"""Event script + talk + DData editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QListWidget, QListWidgetItem,
    QTableWidget, QTableWidgetItem, QPushButton, QLabel, QSpinBox, QComboBox,
    QTextEdit, QFormLayout, QMessageBox, QSplitter, QHeaderView, QLineEdit,
    QAbstractItemView, QCompleter, QCheckBox,
)

from kys_formats.kdef import OPCODE_ARGC, Instruction, Script
from kys_formats.opcode_zh import (
    opcode_display_name,
    format_args_tooltip,
    format_name_tooltip,
    format_opcode_choice,
    parse_opcode_choice,
    known_opcodes,
    default_args_for_opcode,
)
from kys_formats.rle_tile import code_to_tile_index, format_pic_code
from ui.context import EditorContext
from ui.id_combo import NamedIdCombo, collect_scene_options, rebuild_named_combos

# DData word labels (Pascal DData[scene, event, 0..10])
_DDATA_WORD_LABELS = [
    "条件[0]",
    "备用[1]",
    "手动脚本[2]",
    "物品脚本[3]",
    "踩上脚本[4]",
    "贴图当前[5]",
    "贴图结束[6]",
    "贴图起始[7]",
    "备用[8]",
    "Y[9]",
    "X[10]",
]


class EventEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self.current_script_id = 1
        self._opcode_choices = known_opcodes()
        layout = QVBoxLayout(self)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)
        self._build_script_tab()
        self._build_talk_tab()
        self._build_ddata_tab()
        self._build_sdata_tab()
        ctx.dataRootChanged.connect(lambda _: self.refresh())
        ctx.encodingChanged.connect(lambda _: self.refresh())

    def refresh(self) -> None:
        self._refresh_script_list()
        self._refresh_talk()
        self._refresh_ddata_scenes()
        self._refresh_sdata()

    # ----- scripts -----
    def _build_script_tab(self) -> None:
        w = QWidget()
        lay = QHBoxLayout(w)
        left = QVBoxLayout()
        self.script_list = QListWidget()
        self.script_list.currentRowChanged.connect(self._load_script)
        left.addWidget(QLabel("脚本 ID"))
        left.addWidget(self.script_list)
        btn_row = QHBoxLayout()
        add_btn = QPushButton("新建空脚本")
        add_btn.clicked.connect(self._append_script)
        btn_row.addWidget(add_btn)
        left.addLayout(btn_row)
        lay.addLayout(left, 1)

        right = QVBoxLayout()
        hint = QLabel(
            "Opcode 栏可输入数字或下拉选择「编码 — 中文」。"
            "底部可从全部指令中搜索并插入；悬停名称/参数可查看映射。"
        )
        hint.setWordWrap(True)
        right.addWidget(hint)
        self.ins_table = QTableWidget(0, 4)
        self.ins_table.setHorizontalHeaderLabels(
            ["PC", "Opcode（输入/下拉）", "名称(中文)", "参数(逗号分隔)"]
        )
        self.ins_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.ins_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.ins_table.setMouseTracking(True)
        self.ins_table.setWordWrap(True)
        self.ins_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.ins_table.cellChanged.connect(self._on_ins_cell_changed)
        self.ins_table.currentCellChanged.connect(self._on_ins_row_selected)
        right.addWidget(self.ins_table, 3)

        self.arg_preview = QTextEdit()
        self.arg_preview.setReadOnly(True)
        self.arg_preview.setPlaceholderText("选中一行指令后，此处显示参数详细释义…")
        self.arg_preview.setMaximumHeight(140)
        right.addWidget(self.arg_preview)

        ops = QHBoxLayout()
        ops.addWidget(QLabel("插入指令:"))
        self.add_op_combo = QComboBox()
        self.add_op_combo.setEditable(True)
        self.add_op_combo.setInsertPolicy(QComboBox.NoInsert)
        self.add_op_combo.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
        self.add_op_combo.setMinimumContentsLength(22)
        self.add_op_combo.setSizeAdjustPolicy(
            QComboBox.SizeAdjustPolicy.AdjustToMinimumContentsLengthWithIcon
        )
        for op in self._opcode_choices:
            if op < 0:
                continue  # END 一般由脚本自带，不从这里插
            self.add_op_combo.addItem(format_opcode_choice(op), op)
        completer = QCompleter(self.add_op_combo.model(), self.add_op_combo)
        completer.setCompletionMode(QCompleter.CompletionMode.PopupCompletion)
        completer.setFilterMode(Qt.MatchFlag.MatchContains)
        completer.setCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
        self.add_op_combo.setCompleter(completer)
        # default to 显示对话
        idx = self.add_op_combo.findData(1)
        if idx >= 0:
            self.add_op_combo.setCurrentIndex(idx)
        ops.addWidget(self.add_op_combo, 1)

        insert_btn = QPushButton("插入")
        insert_btn.setToolTip("在当前行之后插入；若选中结束行或未选中，则插在结束指令之前")
        insert_btn.clicked.connect(self._insert_from_combo)
        ops.addWidget(insert_btn)
        self.add_op_combo.lineEdit().returnPressed.connect(self._insert_from_combo)

        del_btn = QPushButton("删除行")
        del_btn.clicked.connect(self._del_ins)
        ops.addWidget(del_btn)
        right.addLayout(ops)
        save_btn = QPushButton("应用并写回 Kdef")
        save_btn.clicked.connect(self._save_script)
        right.addWidget(save_btn)
        lay.addLayout(right, 3)
        self.tabs.addTab(w, "事件脚本")

    def _refresh_script_list(self) -> None:
        self.script_list.clear()
        if not self.ctx.kdef:
            return
        for sid in range(1, min(self.ctx.kdef.script_count, 3000) + 1):
            self.script_list.addItem(QListWidgetItem(str(sid)))

    def _parse_args_cell(self, text: str) -> list[int]:
        args: list[int] = []
        if text and text.strip():
            for x in text.split(","):
                x = x.strip()
                if x == "":
                    continue
                args.append(int(x))
        return args

    def _combo_row(self, cb: QComboBox) -> int:
        for r in range(self.ins_table.rowCount()):
            if self.ins_table.cellWidget(r, 1) is cb:
                return r
        return -1

    def _make_opcode_combo(self, opcode: int) -> QComboBox:
        cb = QComboBox()
        cb.setEditable(True)
        cb.setInsertPolicy(QComboBox.NoInsert)
        cb.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
        cb.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToContents)
        for op in self._opcode_choices:
            cb.addItem(format_opcode_choice(op), op)
        # Type number or Chinese fragment to filter the list
        completer = QCompleter(cb.model(), cb)
        completer.setCompletionMode(QCompleter.CompletionMode.PopupCompletion)
        completer.setFilterMode(Qt.MatchFlag.MatchContains)
        completer.setCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
        cb.setCompleter(completer)
        idx = cb.findData(opcode)
        if idx >= 0:
            cb.setCurrentIndex(idx)
        else:
            cb.setEditText(str(opcode))
        cb.activated.connect(lambda _i, c=cb: self._on_opcode_combo_committed(c))
        le = cb.lineEdit()
        if le is not None:
            le.editingFinished.connect(lambda c=cb: self._on_opcode_combo_committed(c))
        return cb

    def _row_opcode(self, row: int) -> int:
        w = self.ins_table.cellWidget(row, 1)
        if isinstance(w, QComboBox):
            return parse_opcode_choice(w.currentText())
        item = self.ins_table.item(row, 1)
        if item is None:
            raise ValueError("no opcode")
        return parse_opcode_choice(item.text())

    def _set_row_opcode_combo(self, row: int, opcode: int) -> None:
        old = self.ins_table.cellWidget(row, 1)
        if old is not None:
            old.deleteLater()
        self.ins_table.setCellWidget(row, 1, self._make_opcode_combo(opcode))

    def _on_opcode_combo_committed(self, cb: QComboBox) -> None:
        row = self._combo_row(cb)
        if row < 0:
            return
        try:
            opcode = parse_opcode_choice(cb.currentText())
        except ValueError:
            return
        data_idx = cb.findData(opcode)
        cb.blockSignals(True)
        if data_idx >= 0:
            cb.setCurrentIndex(data_idx)
        else:
            cb.setEditText(str(opcode))
        cb.blockSignals(False)

        self.ins_table.blockSignals(True)
        name_item = self.ins_table.item(row, 2)
        if name_item is None:
            name_item = QTableWidgetItem()
            name_item.setFlags(name_item.flags() & ~Qt.ItemIsEditable)
            self.ins_table.setItem(row, 2, name_item)
        name_item.setText(opcode_display_name(opcode))
        name_item.setToolTip(format_name_tooltip(opcode))

        argc = OPCODE_ARGC.get(opcode, 0) if opcode >= 0 else 0
        args_item = self.ins_table.item(row, 3)
        try:
            args = self._parse_args_cell(args_item.text() if args_item else "")
        except ValueError:
            args = []
        if opcode >= 0:
            args = (args + [0] * argc)[:argc]
            if args_item is None:
                args_item = QTableWidgetItem()
                self.ins_table.setItem(row, 3, args_item)
            args_item.setText(",".join(str(a) for a in args))
            args_item.setToolTip(format_args_tooltip(self.ctx, opcode, args))
        self.ins_table.blockSignals(False)
        if row == self.ins_table.currentRow():
            self._on_ins_row_selected(row, 0, -1, -1)

    def _fill_instruction_row(self, row: int, opcode: int, args: list[int], pc: str = "") -> None:
        self.ins_table.setItem(row, 0, QTableWidgetItem(pc))
        self._set_row_opcode_combo(row, opcode)
        name_item = QTableWidgetItem(opcode_display_name(opcode))
        name_item.setFlags(name_item.flags() & ~Qt.ItemIsEditable)
        name_item.setToolTip(format_name_tooltip(opcode))
        self.ins_table.setItem(row, 2, name_item)
        args_item = QTableWidgetItem(",".join(str(a) for a in args))
        args_item.setToolTip(format_args_tooltip(self.ctx, opcode, args))
        self.ins_table.setItem(row, 3, args_item)

    def _refresh_row_tooltips(self, row: int) -> None:
        if row < 0 or row >= self.ins_table.rowCount():
            return
        try:
            opcode = self._row_opcode(row)
        except Exception:
            return
        args_text = self.ins_table.item(row, 3).text() if self.ins_table.item(row, 3) else ""
        try:
            args = self._parse_args_cell(args_text)
        except ValueError:
            args = []
        name_item = self.ins_table.item(row, 2)
        if name_item:
            name_item.setText(opcode_display_name(opcode))
            name_item.setToolTip(format_name_tooltip(opcode))
        args_item = self.ins_table.item(row, 3)
        if args_item:
            args_item.setToolTip(format_args_tooltip(self.ctx, opcode, args))

    def _on_ins_cell_changed(self, row: int, col: int) -> None:
        if col == 3:
            self.ins_table.blockSignals(True)
            self._refresh_row_tooltips(row)
            self.ins_table.blockSignals(False)
            if row == self.ins_table.currentRow():
                self._on_ins_row_selected(row, 0, -1, -1)

    def _on_ins_row_selected(self, row: int, _c: int, _pr: int, _pc: int) -> None:
        if row < 0:
            self.arg_preview.clear()
            return
        try:
            opcode = self._row_opcode(row)
            args_text = self.ins_table.item(row, 3).text() if self.ins_table.item(row, 3) else ""
            args = self._parse_args_cell(args_text)
        except Exception:
            self.arg_preview.setPlainText("无法解析本行")
            return
        self.arg_preview.setPlainText(format_args_tooltip(self.ctx, opcode, args))

    def _load_script(self, row: int) -> None:
        if row < 0 or not self.ctx.kdef:
            return
        sid = row + 1
        self.current_script_id = sid
        script = self.ctx.kdef.get_script(sid)
        self.ins_table.blockSignals(True)
        self.ins_table.setRowCount(0)
        for ins in script.instructions:
            r = self.ins_table.rowCount()
            self.ins_table.insertRow(r)
            self._fill_instruction_row(r, ins.opcode, list(ins.args), str(ins.pc))
        self.ins_table.blockSignals(False)
        self.arg_preview.clear()

    def _collect_script(self) -> Script:
        instructions = []
        for r in range(self.ins_table.rowCount()):
            try:
                op = self._row_opcode(r)
            except Exception:
                continue
            args_text = self.ins_table.item(r, 3).text() if self.ins_table.item(r, 3) else ""
            args = []
            if args_text.strip():
                args = [int(x.strip()) for x in args_text.split(",") if x.strip() != ""]
            argc = OPCODE_ARGC.get(op, 0)
            if op >= 0:
                args = (args + [0] * argc)[:argc]
            instructions.append(Instruction(op, args))
        return Script(self.current_script_id, instructions=instructions)

    def _add_op(self, opcode: int, args: list | None = None) -> None:
        if args is None:
            args = default_args_for_opcode(opcode)
        cur = self.ins_table.currentRow()
        r = self.ins_table.rowCount()
        if cur >= 0:
            # Insert after current row; if current is END, insert before it
            try:
                if self._row_opcode(cur) < 0:
                    r = cur
                else:
                    r = cur + 1
            except Exception:
                r = cur + 1
        elif r > 0:
            try:
                last_op = self._row_opcode(r - 1)
                if last_op < 0:
                    r = r - 1
            except Exception:
                pass
        self.ins_table.blockSignals(True)
        self.ins_table.insertRow(r)
        self._fill_instruction_row(r, opcode, list(args), "")
        self.ins_table.blockSignals(False)
        self.ins_table.setCurrentCell(r, 3)

    def _insert_from_combo(self) -> None:
        try:
            opcode = parse_opcode_choice(self.add_op_combo.currentText())
        except ValueError:
            QMessageBox.warning(self, "插入", "无法解析所选指令")
            return
        if opcode < 0:
            QMessageBox.information(self, "插入", "结束指令一般保留在脚本末尾，请直接改某行为结束")
            return
        # Snap combo to known label when possible
        idx = self.add_op_combo.findData(opcode)
        if idx >= 0:
            self.add_op_combo.setCurrentIndex(idx)
        self._add_op(opcode)

    def _del_ins(self) -> None:
        r = self.ins_table.currentRow()
        if r >= 0:
            self.ins_table.removeRow(r)

    def _save_script(self) -> None:
        if not self.ctx.kdef:
            return
        try:
            script = self._collect_script()
            self.ctx.kdef.set_script(script)
            self.ctx.kdef.save(backup=True)
            QMessageBox.information(self, "保存", f"脚本 {script.script_id} 已写回")
            self._load_script(script.script_id - 1)
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    def _append_script(self) -> None:
        if not self.ctx.kdef:
            return
        sid = self.ctx.kdef.append_script([0, -1])
        self.ctx.kdef.save(backup=True)
        self._refresh_script_list()
        self.script_list.setCurrentRow(sid - 1)
        QMessageBox.information(self, "新建", f"已追加脚本 ID={sid}")

    # ----- talk -----
    def _build_talk_tab(self) -> None:
        w = QWidget()
        lay = QHBoxLayout(w)
        left = QVBoxLayout()
        self.talk_list = QListWidget()
        self.talk_list.currentRowChanged.connect(self._load_talk)
        left.addWidget(QLabel("对话 ID"))
        filter_row = QHBoxLayout()
        self.talk_filter = QLineEdit()
        self.talk_filter.setPlaceholderText("过滤…")
        self.talk_filter.textChanged.connect(self._refresh_talk)
        filter_row.addWidget(self.talk_filter)
        left.addLayout(filter_row)
        left.addWidget(self.talk_list)
        add = QPushButton("新增对话")
        add.clicked.connect(self._add_talk)
        left.addWidget(add)
        lay.addLayout(left, 1)
        right = QVBoxLayout()
        enc_hint = QLabel(
            "对话文本受工具栏「文本编码」影响；切换编码后列表会自动刷新。"
            "打开其它同引擎游戏时若乱码，请尝试 Big5 或 GBK。"
        )
        enc_hint.setWordWrap(True)
        right.addWidget(enc_hint)
        self.talk_edit = QTextEdit()
        right.addWidget(self.talk_edit)
        save = QPushButton("保存当前对话")
        save.clicked.connect(self._save_talk)
        right.addWidget(save)
        lay.addLayout(right, 2)
        self.tabs.addTab(w, "对话库")
        self._talk_ids: list[int] = []

    def _refresh_talk(self) -> None:
        self.talk_list.clear()
        self._talk_ids = []
        if not self.ctx.talk:
            return
        filt = self.talk_filter.text().strip()
        for i in range(1, self.ctx.talk.count + 1):
            text = self.ctx.talk.get_text(i)
            preview = text.replace("\n", " ")[:40]
            if filt and filt not in text and filt not in str(i):
                continue
            self._talk_ids.append(i)
            self.talk_list.addItem(f"{i}: {preview}")
        if hasattr(self, "_current_talk_id") and self.ctx.talk:
            self.talk_edit.setPlainText(self.ctx.talk.get_text(self._current_talk_id))

    def _load_talk(self, row: int) -> None:
        if row < 0 or row >= len(self._talk_ids) or not self.ctx.talk:
            return
        tid = self._talk_ids[row]
        self.talk_edit.setPlainText(self.ctx.talk.get_text(tid))
        self._current_talk_id = tid

    def _save_talk(self) -> None:
        if not self.ctx.talk or not hasattr(self, "_current_talk_id"):
            return
        try:
            self.ctx.talk.set_text(self._current_talk_id, self.talk_edit.toPlainText())
            self.ctx.talk.save(backup=True)
            QMessageBox.information(self, "保存", f"对话 {self._current_talk_id} 已保存")
            self._refresh_talk()
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    def _add_talk(self) -> None:
        if not self.ctx.talk:
            return
        tid = self.ctx.talk.append_text("（新对话）")
        self.ctx.talk.save(backup=True)
        self._refresh_talk()
        QMessageBox.information(self, "新增", f"对话 ID={tid}")

    # ----- DData -----
    def _build_ddata_tab(self) -> None:
        w = QWidget()
        lay = QVBoxLayout(w)
        top = QHBoxLayout()
        top.addWidget(QLabel("场景"))
        self.scene_combo = NamedIdCombo("scene", allow_none=False, none_value=0)
        self.scene_combo.setMinimumContentsLength(28)
        self.scene_combo.idChanged.connect(self._load_ddata)
        top.addWidget(self.scene_combo, 1)
        self.ddata_only_used = QCheckBox("仅显示有内容")
        self.ddata_only_used.setChecked(True)
        self.ddata_only_used.setToolTip(
            "隐藏「全 0 / 脚本全空且贴图为 0」的空事件，避免漏看已挂接 NPC（如开场孔霹雳 8268）"
        )
        self.ddata_only_used.toggled.connect(self._load_ddata)
        top.addWidget(self.ddata_only_used)
        top.addStretch()
        save = QPushButton("保存 alldef.grp")
        save.clicked.connect(self._save_ddata)
        top.addWidget(save)
        lay.addLayout(top)

        hint = QLabel(
            "DData 共 11 个 int16。贴图存的是游戏代码（偶数，如 8268）；"
            "引擎 DrawSPic(代码/2)，smp 帧号=代码/2（8268→4134）。"
            "选中行右侧预览贴图当前[5]。"
        )
        hint.setWordWrap(True)
        lay.addWidget(hint)

        split = QSplitter(Qt.Horizontal)
        # columns: 事件 + 11 words + smp(=贴图[5]/2)
        self.ddata_table = QTableWidget(0, 13)
        headers = ["事件"] + _DDATA_WORD_LABELS + ["smp(=贴图[5]/2)"]
        self.ddata_table.setHorizontalHeaderLabels(headers)
        self.ddata_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self.ddata_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.ddata_table.cellChanged.connect(self._ddata_changed)
        self.ddata_table.currentCellChanged.connect(self._on_ddata_row_selected)
        split.addWidget(self.ddata_table)

        side = QWidget()
        side_lay = QVBoxLayout(side)
        self.ddata_info = QLabel("选中事件查看贴图")
        self.ddata_info.setWordWrap(True)
        side_lay.addWidget(self.ddata_info)
        self.ddata_preview = QLabel("贴图预览")
        self.ddata_preview.setFixedSize(160, 160)
        self.ddata_preview.setAlignment(Qt.AlignCenter)
        self.ddata_preview.setStyleSheet("background:#111;color:#888;border:1px solid #333;")
        side_lay.addWidget(self.ddata_preview)
        side_lay.addStretch()
        split.addWidget(side)
        split.setStretchFactor(0, 4)
        split.setStretchFactor(1, 1)
        lay.addWidget(split)
        self.tabs.addTab(w, "场景事件挂接")

    def _refresh_ddata_scenes(self) -> None:
        if not self.ctx.events:
            return
        self.scene_combo.max_count = self.ctx.events.scene_count
        self.scene_combo.rebuild(
            collect_scene_options(self.ctx, max_count=self.ctx.events.scene_count)
        )
        if self.scene_combo.get_id(silent=True) >= self.ctx.events.scene_count:
            self.scene_combo.set_id(0)
        self._load_ddata()

    @staticmethod
    def _event_has_content(ev: list) -> bool:
        """True if event is worth listing (scripts, pics, or non-default coords)."""
        if any(int(ev[i]) > 0 for i in (2, 3, 4)):
            return True
        if int(ev[5]) != 0 or int(ev[6]) != 0 or int(ev[7]) != 0:
            return True
        # keep rows that still occupy a map cell with a condition flag
        if int(ev[0]) != 0 and (int(ev[9]) != 0 or int(ev[10]) != 0):
            return True
        return False

    def _load_ddata(self) -> None:
        if not self.ctx.events:
            return
        scene = self.scene_combo.get_id(silent=True)
        if scene >= self.ctx.events.scene_count:
            return
        only = self.ddata_only_used.isChecked() if hasattr(self, "ddata_only_used") else False
        rows: list[int] = []
        for e in range(200):
            ev = self.ctx.events.scenes[scene][e]
            if only and not self._event_has_content(ev):
                continue
            rows.append(e)

        self.ddata_table.blockSignals(True)
        self.ddata_table.setRowCount(len(rows))
        self._ddata_row_to_event = rows
        for r, e in enumerate(rows):
            ev = self.ctx.events.scenes[scene][e]
            vals = [e] + [int(ev[i]) for i in range(11)]
            pic = int(ev[5])
            smp = code_to_tile_index(pic) if pic != 0 else -1
            vals.append(smp if smp >= 0 else "")
            for c, v in enumerate(vals):
                item = QTableWidgetItem(str(v))
                if c == 0 or c == 12:
                    item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self.ddata_table.setItem(r, c, item)
            # tooltips on pic columns (table cols 6,7,8 → words 5,6,7)
            for word, col in ((5, 6), (6, 7), (7, 8)):
                it = self.ddata_table.item(r, col)
                if it and int(ev[word]) != 0:
                    it.setToolTip(format_pic_code(int(ev[word])))
        self.ddata_table.blockSignals(False)
        if rows:
            self.ddata_table.selectRow(0)
            self._preview_ddata_event(rows[0])

    def _ddata_event_id_at_row(self, row: int) -> int | None:
        mapping = getattr(self, "_ddata_row_to_event", None)
        if mapping is None:
            return row if 0 <= row < 200 else None
        if 0 <= row < len(mapping):
            return mapping[row]
        return None

    def _ddata_changed(self, row: int, col: int) -> None:
        if not self.ctx.events or col == 0 or col == 12:
            return
        item = self.ddata_table.item(row, col)
        if not item:
            return
        # table cols 1..11 → DData words 0..10
        word = col - 1
        if word < 0 or word > 10:
            return
        eid = self._ddata_event_id_at_row(row)
        if eid is None:
            return
        try:
            value = int(item.text())
        except ValueError:
            return
        scene = self.scene_combo.get_id(silent=True)
        self.ctx.events.set(scene, eid, word, value)
        if word == 5:
            # refresh derived smp column
            smp = code_to_tile_index(value) if value != 0 else -1
            self.ddata_table.blockSignals(True)
            self.ddata_table.setItem(
                row, 12, QTableWidgetItem("" if smp < 0 else str(smp))
            )
            self.ddata_table.item(row, 12).setFlags(
                self.ddata_table.item(row, 12).flags() & ~Qt.ItemIsEditable
            )
            item.setToolTip(format_pic_code(value) if value != 0 else "")
            self.ddata_table.blockSignals(False)
            self._preview_ddata_event(eid)

    def _on_ddata_row_selected(self, row: int, _col: int, _prev_row: int, _prev_col: int) -> None:
        eid = self._ddata_event_id_at_row(row)
        if eid is not None:
            self._preview_ddata_event(eid)

    def _preview_ddata_event(self, event_id: int) -> None:
        if not self.ctx.events:
            return
        scene = self.scene_combo.get_id(silent=True)
        if scene >= self.ctx.events.scene_count or event_id < 0 or event_id >= 200:
            return
        ev = self.ctx.events.scenes[scene][event_id]
        pic = int(ev[5])
        smp = code_to_tile_index(pic) if pic != 0 else -1
        lines = [
            f"场景 {scene} 事件 {event_id}",
            f"坐标 Y={ev[9]} X={ev[10]}",
            f"条件={ev[0]} 脚本 手={ev[2]} 物={ev[3]} 踩={ev[4]}",
            f"贴图[5/6/7]={ev[5]}/{ev[6]}/{ev[7]}",
        ]
        if pic != 0:
            lines.append(format_pic_code(pic))
        self.ddata_info.setText("\n".join(lines))

        self.ddata_preview.setPixmap(QPixmap())
        if pic == 0 or smp < 0:
            self.ddata_preview.setText("无贴图")
            return
        if pic < 0:
            self.ddata_preview.setText(f"负贴图\n(mmap/ScenePic)\n{format_pic_code(pic)}")
            return
        pack = self.ctx.scene_tiles
        pal = self.ctx.palette
        if not pack or not pal:
            self.ddata_preview.setText(f"smp[{smp}]\n(未加载 sdx/smp)")
            return
        try:
            img = pack.decode_tile(smp, pal)
        except Exception as e:
            self.ddata_preview.setText(str(e))
            return
        if img is None:
            self.ddata_preview.setText(f"smp[{smp}]\n无法解码")
            return
        data = img.convert("RGBA").tobytes("raw", "RGBA")
        qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
        self.ddata_preview.setPixmap(
            QPixmap.fromImage(qimg).scaled(150, 150, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        )

    def _save_ddata(self) -> None:
        if not self.ctx.events:
            return
        try:
            self.ctx.events.save(backup=True)
            QMessageBox.information(self, "保存", "alldef.grp 已保存")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    # ----- SData layer 3 (event ids on map) -----
    def _build_sdata_tab(self) -> None:
        from ui.map_view import MapOverviewPanel

        w = QWidget()
        lay = QVBoxLayout(w)
        top = QHBoxLayout()
        top.addWidget(QLabel("场景"))
        self.sdata_scene_combo = NamedIdCombo("scene", allow_none=False, none_value=0)
        self.sdata_scene_combo.setMinimumContentsLength(28)
        self.sdata_scene_combo.idChanged.connect(self._load_sdata)
        top.addWidget(self.sdata_scene_combo, 1)
        top.addWidget(QLabel("编辑层(事件=3)"))
        self.sdata_layer = QSpinBox()
        self.sdata_layer.setRange(0, 5)
        self.sdata_layer.setValue(3)
        self.sdata_layer.valueChanged.connect(self._load_sdata)
        top.addWidget(self.sdata_layer)
        save = QPushButton("保存 allsin.grp")
        save.clicked.connect(self._save_sdata)
        top.addWidget(save)
        jump = QPushButton("选中格→编辑 DData")
        jump.clicked.connect(self._jump_sdata_to_ddata)
        top.addWidget(jump)
        top.addStretch()
        lay.addLayout(top)
        hint = QLabel(
            "俯视图用层0地面贴图主色铺底，红色半透明为事件格。"
            "调整模式写入「编辑层」当前值；悬停/点击右侧显示真实砖块。"
        )
        hint.setWordWrap(True)
        lay.addWidget(hint)

        split = QSplitter(Qt.Horizontal)
        self.sdata_overview = MapOverviewPanel("场景俯视图")
        self.sdata_overview.cellSelected.connect(self._on_sdata_overview_select)
        self.sdata_overview.cellEdited.connect(self._on_sdata_overview_edit)
        split.addWidget(self.sdata_overview)

        self.sdata_table = QTableWidget(64, 64)
        self.sdata_table.horizontalHeader().setDefaultSectionSize(28)
        self.sdata_table.verticalHeader().setDefaultSectionSize(18)
        self.sdata_table.cellChanged.connect(self._sdata_changed)
        self.sdata_table.cellDoubleClicked.connect(lambda *_: self._jump_sdata_to_ddata())
        self.sdata_table.cellClicked.connect(self._on_sdata_table_click)
        split.addWidget(self.sdata_table)
        split.setStretchFactor(0, 3)
        split.setStretchFactor(1, 2)
        lay.addWidget(split)
        self.tabs.addTab(w, "SData 事件层")

    def _bind_sdata_overview(self) -> None:
        maps = self.ctx.maps
        if not maps:
            return
        scene = self.sdata_scene_combo.get_id(silent=True)
        if scene >= maps.scene_count:
            return
        layer = self.sdata_layer.value()

        def get_code(x: int, y: int) -> int:
            return maps.get(scene, layer, x, y)

        def set_code(x: int, y: int, v: int) -> None:
            maps.set(scene, layer, x, y, v)

        def ground(x: int, y: int) -> int:
            return maps.get(scene, 0, x, y)

        def event(x: int, y: int) -> int:
            return maps.get(scene, 3, x, y)

        def event_pic(x: int, y: int) -> int:
            if not self.ctx.events or scene >= self.ctx.events.scene_count:
                return 0
            eid = maps.get(scene, 3, x, y)
            if eid < 0 or eid >= 200:
                return 0
            return int(self.ctx.events.scenes[scene][eid][5])

        self.sdata_overview.bind(
            64,
            64,
            get_code,
            set_code,
            ground_code=ground,
            event_code=event,
            event_pic_code=event_pic,
            tile_pack=self.ctx.scene_tiles,
            palette=self.ctx.palette,
        )

    def _on_sdata_overview_select(self, x: int, y: int) -> None:
        self.sdata_table.setCurrentCell(x, y)
        item = self.sdata_table.item(x, y)
        if item:
            try:
                self.sdata_overview.sp_brush.setValue(int(item.text()))
            except ValueError:
                pass

    def _on_sdata_overview_edit(self, x: int, y: int, value: int) -> None:
        self.sdata_table.blockSignals(True)
        self.sdata_table.setItem(x, y, QTableWidgetItem(str(value)))
        self.sdata_table.blockSignals(False)

    def _on_sdata_table_click(self, row: int, col: int) -> None:
        self.sdata_overview.select_cell(row, col)

    def _refresh_sdata(self) -> None:
        if not self.ctx.maps:
            return
        self.sdata_scene_combo.max_count = self.ctx.maps.scene_count
        self.sdata_scene_combo.rebuild(
            collect_scene_options(self.ctx, max_count=self.ctx.maps.scene_count)
        )
        if self.sdata_scene_combo.get_id(silent=True) >= self.ctx.maps.scene_count:
            self.sdata_scene_combo.set_id(0)
        self._load_sdata()

    def _load_sdata(self) -> None:
        maps = self.ctx.maps
        if not maps:
            return
        scene = self.sdata_scene_combo.get_id(silent=True)
        layer = self.sdata_layer.value()
        if scene >= maps.scene_count:
            return
        self.sdata_table.blockSignals(True)
        for x in range(64):
            for y in range(64):
                self.sdata_table.setItem(x, y, QTableWidgetItem(str(maps.get(scene, layer, x, y))))
        self.sdata_table.blockSignals(False)
        self._bind_sdata_overview()

    def _sdata_changed(self, row: int, col: int) -> None:
        if not self.ctx.maps:
            return
        item = self.sdata_table.item(row, col)
        if not item:
            return
        try:
            self.ctx.maps.set(
                self.sdata_scene_combo.get_id(silent=True),
                self.sdata_layer.value(),
                row,
                col,
                int(item.text()),
            )
            self.sdata_overview.rebuild()
        except ValueError:
            pass

    def _save_sdata(self) -> None:
        if not self.ctx.maps:
            return
        try:
            self.ctx.maps.save(backup=True)
            QMessageBox.information(self, "保存", "allsin.grp 已保存")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    def _jump_sdata_to_ddata(self) -> None:
        item = self.sdata_table.currentItem()
        if not item:
            # try overview selection
            sel = self.sdata_overview.canvas.selected
            if sel is None or not self.ctx.maps:
                return
            eid = self.ctx.maps.get(
                self.sdata_scene_combo.get_id(silent=True), 3, sel[0], sel[1]
            )
        else:
            try:
                eid = int(item.text())
            except ValueError:
                return
        if eid < 0:
            return
        self.scene_combo.set_id(self.sdata_scene_combo.get_id(silent=True))
        self.tabs.setCurrentIndex(2)  # DData tab
        # Ensure the event is visible even if "only used" filter would hide empty pics
        if hasattr(self, "ddata_only_used") and self.ddata_only_used.isChecked():
            # force reload; event with map cell usually has content — if not, show all
            self._load_ddata()
        mapping = getattr(self, "_ddata_row_to_event", list(range(200)))
        try:
            row = mapping.index(eid)
        except ValueError:
            self.ddata_only_used.setChecked(False)
            self._load_ddata()
            mapping = self._ddata_row_to_event
            try:
                row = mapping.index(eid)
            except ValueError:
                return
        self.ddata_table.selectRow(row)
        self.ddata_table.scrollToItem(self.ddata_table.item(row, 0))
        self._preview_ddata_event(eid)
