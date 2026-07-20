"""Event script + talk + DData editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QListWidget, QListWidgetItem,
    QTableWidget, QTableWidgetItem, QPushButton, QLabel, QSpinBox, QComboBox,
    QTextEdit, QFormLayout, QMessageBox, QSplitter, QHeaderView, QLineEdit,
    QAbstractItemView, QCompleter,
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
from ui.context import EditorContext


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
        self.scene_spin = QSpinBox()
        self.scene_spin.setRange(0, 200)
        self.scene_spin.valueChanged.connect(self._load_ddata)
        top.addWidget(self.scene_spin)
        top.addStretch()
        save = QPushButton("保存 alldef.grp")
        save.clicked.connect(self._save_ddata)
        top.addWidget(save)
        lay.addLayout(top)
        self.ddata_table = QTableWidget(0, 8)
        self.ddata_table.setHorizontalHeaderLabels([
            "事件", "条件[0]", "手动脚本[2]", "物品脚本[3]", "踩上脚本[4]",
            "贴图[5]", "Y[9]", "X[10]",
        ])
        self.ddata_table.cellChanged.connect(self._ddata_changed)
        lay.addWidget(self.ddata_table)
        self.tabs.addTab(w, "场景事件挂接")

    def _refresh_ddata_scenes(self) -> None:
        if self.ctx.events:
            self.scene_spin.setMaximum(max(0, self.ctx.events.scene_count - 1))
            self._load_ddata()

    def _load_ddata(self) -> None:
        if not self.ctx.events:
            return
        scene = self.scene_spin.value()
        if scene >= self.ctx.events.scene_count:
            return
        self.ddata_table.blockSignals(True)
        self.ddata_table.setRowCount(200)
        for e in range(200):
            ev = self.ctx.events.scenes[scene][e]
            vals = [e, ev[0], ev[2], ev[3], ev[4], ev[5], ev[9], ev[10]]
            for c, v in enumerate(vals):
                self.ddata_table.setItem(e, c, QTableWidgetItem(str(v)))
        self.ddata_table.blockSignals(False)

    def _ddata_changed(self, row: int, col: int) -> None:
        if not self.ctx.events or col == 0:
            return
        item = self.ddata_table.item(row, col)
        if not item:
            return
        word_map = {1: 0, 2: 2, 3: 3, 4: 4, 5: 5, 6: 9, 7: 10}
        w = word_map.get(col)
        if w is None:
            return
        try:
            self.ctx.events.set(self.scene_spin.value(), row, w, int(item.text()))
        except ValueError:
            pass

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
        w = QWidget()
        lay = QVBoxLayout(w)
        top = QHBoxLayout()
        top.addWidget(QLabel("场景"))
        self.sdata_scene = QSpinBox()
        self.sdata_scene.setRange(0, 200)
        self.sdata_scene.valueChanged.connect(self._load_sdata)
        top.addWidget(self.sdata_scene)
        top.addWidget(QLabel("层(事件层=3)"))
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
        hint = QLabel("层 3 存事件号；双击或点下方按钮跳到对应 DData 行。")
        lay.addWidget(hint)
        self.sdata_table = QTableWidget(64, 64)
        self.sdata_table.horizontalHeader().setDefaultSectionSize(28)
        self.sdata_table.verticalHeader().setDefaultSectionSize(18)
        self.sdata_table.cellChanged.connect(self._sdata_changed)
        self.sdata_table.cellDoubleClicked.connect(lambda *_: self._jump_sdata_to_ddata())
        lay.addWidget(self.sdata_table)
        self.tabs.addTab(w, "SData 事件层")

    def _refresh_sdata(self) -> None:
        if self.ctx.maps:
            self.sdata_scene.setMaximum(max(0, self.ctx.maps.scene_count - 1))
            self._load_sdata()

    def _load_sdata(self) -> None:
        maps = self.ctx.maps
        if not maps:
            return
        scene = self.sdata_scene.value()
        layer = self.sdata_layer.value()
        if scene >= maps.scene_count:
            return
        self.sdata_table.blockSignals(True)
        for x in range(64):
            for y in range(64):
                self.sdata_table.setItem(x, y, QTableWidgetItem(str(maps.get(scene, layer, x, y))))
        self.sdata_table.blockSignals(False)

    def _sdata_changed(self, row: int, col: int) -> None:
        if not self.ctx.maps:
            return
        item = self.sdata_table.item(row, col)
        if not item:
            return
        try:
            self.ctx.maps.set(
                self.sdata_scene.value(), self.sdata_layer.value(), row, col, int(item.text())
            )
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
            return
        try:
            eid = int(item.text())
        except ValueError:
            return
        if eid < 0:
            return
        self.scene_spin.setValue(self.sdata_scene.value())
        self.tabs.setCurrentIndex(2)  # DData tab
        self.ddata_table.selectRow(min(eid, 199))
        self.ddata_table.scrollToItem(self.ddata_table.item(min(eid, 199), 0))
