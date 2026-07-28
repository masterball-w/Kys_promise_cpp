"""Battle War.sta + warfld editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QTableWidget, QTableWidgetItem,
    QPushButton, QLabel, QSpinBox, QFormLayout, QMessageBox, QSplitter,
    QHeaderView, QLineEdit, QGridLayout,
)

from ui.context import EditorContext


class FormationGrid(QWidget):
    """Simple 64x64 clickable grid for mate/enemy positions."""

    def __init__(self) -> None:
        super().__init__()
        self.setMinimumSize(320, 320)
        self.cell = 5
        self.mates: list[tuple[int, int]] = []
        self.enemies: list[tuple[int, int]] = []
        self.mode = "enemy"  # or mate
        self._paint_field = None  # optional layer0 heights ignored

    def set_positions(self, mates, enemies) -> None:
        self.mates = mates
        self.enemies = enemies
        self.update()

    def paintEvent(self, _event) -> None:
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(40, 40, 40))
        pen = QPen(QColor(70, 70, 70))
        p.setPen(pen)
        for i in range(65):
            p.drawLine(0, i * self.cell, 64 * self.cell, i * self.cell)
            p.drawLine(i * self.cell, 0, i * self.cell, 64 * self.cell)
        for x, y in self.mates:
            p.fillRect(x * self.cell, y * self.cell, self.cell, self.cell, QColor(40, 180, 40))
        for x, y in self.enemies:
            p.fillRect(x * self.cell, y * self.cell, self.cell, self.cell, QColor(200, 60, 60))

    def mousePressEvent(self, event) -> None:
        x = event.position().x() // self.cell
        y = event.position().y() // self.cell
        if not (0 <= x < 64 and 0 <= y < 64):
            return
        pos = (int(x), int(y))
        if self.mode == "mate":
            if pos in self.mates:
                self.mates.remove(pos)
            else:
                self.mates.append(pos)
        else:
            if pos in self.enemies:
                self.enemies.remove(pos)
            else:
                self.enemies.append(pos)
        self.update()
        if hasattr(self, "on_changed"):
            self.on_changed()


class BattleEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self.current_index = 0
        layout = QVBoxLayout(self)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)
        self._build_list_tab()
        self._build_edit_tab()
        self._build_field_tab()
        ctx.dataRootChanged.connect(lambda _: self.refresh())

    def refresh(self) -> None:
        self._refresh_list()
        self._refresh_field_spin()

    def _build_list_tab(self) -> None:
        w = QWidget()
        lay = QVBoxLayout(w)
        row = QHBoxLayout()
        add = QPushButton("复制当前场追加")
        add.clicked.connect(self._append_battle)
        row.addWidget(add)
        clear = QPushButton("清空当前场敌人")
        clear.clicked.connect(self._clear_enemies)
        row.addWidget(clear)
        save = QPushButton("保存 War.sta")
        save.clicked.connect(self._save_war)
        row.addWidget(save)
        refs = QPushButton("检查脚本引用")
        refs.clicked.connect(self._check_refs)
        row.addWidget(refs)
        row.addStretch()
        lay.addLayout(row)
        self.list_table = QTableWidget(0, 7)
        self.list_table.setHorizontalHeaderLabels([
            "下标", "BattleNum", "名称", "地图", "Exp", "我方", "敌人",
        ])
        self.list_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.list_table.currentCellChanged.connect(self._select_battle)
        lay.addWidget(self.list_table)
        self.tabs.addTab(w, "战斗列表")

    def _refresh_list(self) -> None:
        war = self.ctx.war
        if not war:
            return
        self.list_table.setRowCount(war.count)
        for i, r in enumerate(war.records):
            vals = [i, r.battle_num, r.name, r.battle_map, r.exp, r.mate_count(), r.enemy_count()]
            for c, v in enumerate(vals):
                self.list_table.setItem(i, c, QTableWidgetItem(str(v)))

    def _select_battle(self, row: int, *_a) -> None:
        if row < 0 or not self.ctx.war:
            return
        self.current_index = row
        self._load_edit()

    def _build_edit_tab(self) -> None:
        w = QWidget()
        lay = QHBoxLayout(w)
        form_w = QWidget()
        form = QFormLayout(form_w)
        self.ed_num = QSpinBox(); self.ed_num.setRange(0, 9999)
        self.ed_name = QLineEdit()
        self.ed_map = QSpinBox(); self.ed_map.setRange(0, 100)
        self.ed_exp = QSpinBox(); self.ed_exp.setRange(0, 30000)
        self.ed_music = QSpinBox(); self.ed_music.setRange(-1, 200)
        self.ed_bout = QSpinBox(); self.ed_bout.setRange(-1, 9999)
        self.ed_op = QSpinBox(); self.ed_op.setRange(-1, 9999)
        self.ed_money = QSpinBox(); self.ed_money.setRange(0, 30000)
        form.addRow("BattleNum", self.ed_num)
        form.addRow("名称", self.ed_name)
        form.addRow("BattleMap", self.ed_map)
        form.addRow("Exp", self.ed_exp)
        form.addRow("Music", self.ed_music)
        form.addRow("BoutEvent", self.ed_bout)
        form.addRow("OperationEvent", self.ed_op)
        form.addRow("GetMoney", self.ed_money)
        self.reward_table = QTableWidget(3, 2)
        self.reward_table.setHorizontalHeaderLabels(["GetKongfu", "GetItems"])
        for i in range(3):
            self.reward_table.setItem(i, 0, QTableWidgetItem("0"))
            self.reward_table.setItem(i, 1, QTableWidgetItem("0"))
        form.addRow(self.reward_table)
        apply = QPushButton("应用字段到当前场")
        apply.clicked.connect(self._apply_edit)
        form.addRow(apply)
        note = QLabel("胜负条件：原版为双方全滅判定，无额外配置字段。")
        note.setWordWrap(True)
        form.addRow(note)
        lay.addWidget(form_w, 1)

        right = QVBoxLayout()
        mode_row = QHBoxLayout()
        self.mode_mate = QPushButton("放置我方")
        self.mode_enemy = QPushButton("放置敌人")
        self.mode_mate.clicked.connect(lambda: setattr(self.grid, "mode", "mate"))
        self.mode_enemy.clicked.connect(lambda: setattr(self.grid, "mode", "enemy"))
        mode_row.addWidget(self.mode_mate)
        mode_row.addWidget(self.mode_enemy)
        right.addLayout(mode_row)
        self.grid = FormationGrid()
        self.grid.on_changed = self._grid_to_record
        right.addWidget(self.grid)
        self.mate_ids = QLineEdit()
        self.mate_ids.setPlaceholderText("我方 Role ID，逗号分隔")
        self.enemy_ids = QLineEdit()
        self.enemy_ids.setPlaceholderText("敌人 Role ID，逗号分隔")
        right.addWidget(QLabel("我方角色 ID"))
        right.addWidget(self.mate_ids)
        right.addWidget(QLabel("敌人角色 ID"))
        right.addWidget(self.enemy_ids)
        sync = QPushButton("同步 ID 列表到记录")
        sync.clicked.connect(self._ids_to_record)
        right.addWidget(sync)
        lay.addLayout(right, 2)
        self.tabs.addTab(w, "编辑战斗")

    def _load_edit(self) -> None:
        war = self.ctx.war
        if not war or self.current_index >= war.count:
            return
        r = war.records[self.current_index]
        self.ed_num.setValue(r.battle_num)
        self.ed_name.setText(r.name)
        self.ed_map.setValue(r.battle_map)
        self.ed_exp.setValue(r.exp)
        self.ed_music.setValue(r.music)
        self.ed_bout.setValue(r.bout_event)
        self.ed_op.setValue(r.operation_event)
        self.ed_money.setValue(r.get_money)
        for i in range(3):
            self.reward_table.setItem(i, 0, QTableWidgetItem(str(r.get_kongfu(i))))
            self.reward_table.setItem(i, 1, QTableWidgetItem(str(r.get_items(i))))
        mates = []
        mate_ids = []
        lay = r.layout
        for i in range(lay.mate_count):
            mid = r.mate(i)
            if mid < 0 and lay.auto_mate_off >= 0:
                mid = r.auto_mate(i)
            if mid >= 0:
                mates.append((r.mate_x(i), r.mate_y(i)))
                mate_ids.append(str(mid))
        enemies = []
        enemy_ids = []
        for i in range(lay.enemy_count):
            if r.enemy(i) >= 0:
                enemies.append((r.enemy_x(i), r.enemy_y(i)))
                enemy_ids.append(str(r.enemy(i)))
        self.grid.set_positions(mates, enemies)
        self.mate_ids.setText(",".join(mate_ids))
        self.enemy_ids.setText(",".join(enemy_ids))

    def _apply_edit(self) -> None:
        war = self.ctx.war
        if not war:
            return
        r = war.records[self.current_index]
        r.battle_num = self.ed_num.value()
        r.name = self.ed_name.text()
        r.battle_map = self.ed_map.value()
        r.exp = self.ed_exp.value()
        r.music = self.ed_music.value()
        r.bout_event = self.ed_bout.value()
        r.operation_event = self.ed_op.value()
        r.get_money = self.ed_money.value()
        for i in range(3):
            r.set_kongfu(i, int(self.reward_table.item(i, 0).text() or 0))
            r.set_items(i, int(self.reward_table.item(i, 1).text() or 0))
        self._ids_to_record()
        self._grid_to_record()
        self._refresh_list()
        self.ctx.statusMessage.emit("战斗字段已更新（请保存）")

    def _ids_to_record(self) -> None:
        war = self.ctx.war
        if not war:
            return
        r = war.records[self.current_index]
        lay = r.layout
        mates = [int(x) for x in self.mate_ids.text().split(",") if x.strip() != ""]
        enemies = [int(x) for x in self.enemy_ids.text().split(",") if x.strip() != ""]
        for i in range(lay.mate_count):
            r.set_mate(i, mates[i] if i < len(mates) else -1)
        if lay.auto_mate_off >= 0:
            for i in range(lay.auto_mate_count):
                r.set_auto_mate(i, -1)
        for i in range(lay.enemy_count):
            r.set_enemy(i, enemies[i] if i < len(enemies) else -1)

    def _grid_to_record(self) -> None:
        war = self.ctx.war
        if not war:
            return
        r = war.records[self.current_index]
        lay = r.layout
        for i in range(lay.mate_count):
            if i < len(self.grid.mates):
                x, y = self.grid.mates[i]
                r.set_mate_x(i, x)
                r.set_mate_y(i, y)
            else:
                r.set_mate_x(i, 0)
                r.set_mate_y(i, 0)
        for i in range(lay.enemy_count):
            if i < len(self.grid.enemies):
                x, y = self.grid.enemies[i]
                r.set_enemy_x(i, x)
                r.set_enemy_y(i, y)
            else:
                r.set_enemy_x(i, 0)
                r.set_enemy_y(i, 0)

    def _append_battle(self) -> None:
        if not self.ctx.war:
            return
        rec = self.ctx.war.append_copy(self.current_index)
        self._refresh_list()
        self.list_table.setCurrentCell(self.ctx.war.count - 1, 0)
        QMessageBox.information(self, "追加", f"已追加 BattleNum={rec.battle_num}")

    def _clear_enemies(self) -> None:
        if not self.ctx.war:
            return
        r = self.ctx.war.records[self.current_index]
        for i in range(r.layout.enemy_count):
            r.set_enemy(i, -1)
        self._load_edit()

    def _save_war(self) -> None:
        if not self.ctx.war:
            return
        try:
            self.ctx.war.save(backup=True)
            QMessageBox.information(self, "保存", "War.sta 已保存")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    def _check_refs(self) -> None:
        if not self.ctx.war or not self.ctx.kdef:
            QMessageBox.warning(self, "引用", "需要加载 War.sta 与 Kdef")
            return
        r = self.ctx.war.records[self.current_index]
        hits = self.ctx.kdef.find_battle_refs(r.battle_num)
        QMessageBox.information(
            self, "脚本引用",
            f"BattleNum {r.battle_num} 被脚本引用: {hits if hits else '无'}",
        )

    def _build_field_tab(self) -> None:
        from ui.map_view import MapOverviewPanel

        w = QWidget()
        lay = QVBoxLayout(w)
        top = QHBoxLayout()
        top.addWidget(QLabel("战场地图号"))
        self.field_spin = QSpinBox()
        self.field_spin.setRange(0, 100)
        self.field_spin.valueChanged.connect(self._load_field)
        top.addWidget(self.field_spin)
        top.addWidget(QLabel("层"))
        self.layer_spin = QSpinBox()
        self.layer_spin.setRange(0, 2)
        self.layer_spin.valueChanged.connect(self._load_field)
        top.addWidget(self.layer_spin)
        save = QPushButton("保存 warfld")
        save.clicked.connect(self._save_field)
        top.addWidget(save)
        top.addStretch()
        lay.addLayout(top)
        hint = QLabel(
            "俯视图按 wmp 砖主色铺底。调整模式点击写入笔刷值；悬停显示真实战斗贴图块。"
        )
        hint.setWordWrap(True)
        lay.addWidget(hint)

        split = QSplitter(Qt.Horizontal)
        self.field_overview = MapOverviewPanel("战斗俯视图")
        self.field_overview.chk_events.setVisible(False)
        self.field_overview.cellSelected.connect(self._on_field_overview_select)
        self.field_overview.cellEdited.connect(self._on_field_overview_edit)
        split.addWidget(self.field_overview)

        self.field_table = QTableWidget(64, 64)
        self.field_table.horizontalHeader().setDefaultSectionSize(28)
        self.field_table.verticalHeader().setDefaultSectionSize(18)
        self.field_table.cellChanged.connect(self._field_cell_changed)
        self.field_table.cellClicked.connect(self._on_field_table_click)
        split.addWidget(self.field_table)
        split.setStretchFactor(0, 3)
        split.setStretchFactor(1, 2)
        lay.addWidget(split)
        self.tabs.addTab(w, "战场地形")

    def _refresh_field_spin(self) -> None:
        if self.ctx.warfld:
            self.field_spin.setMaximum(max(0, self.ctx.warfld.count - 1))
            # layer max from selected field
            self._load_field()

    def _bind_field_overview(self) -> None:
        fld = self.ctx.warfld
        if not fld:
            return
        fi = self.field_spin.value()
        layer = self.layer_spin.value()
        if fi >= fld.count:
            return
        nlayers = len(fld.fields[fi]) if fi < len(fld.fields) else 1
        self.layer_spin.setMaximum(max(0, nlayers - 1))
        if layer >= nlayers:
            layer = 0
            self.layer_spin.blockSignals(True)
            self.layer_spin.setValue(0)
            self.layer_spin.blockSignals(False)

        def get_code(x: int, y: int) -> int:
            return fld.get(fi, layer, x, y)

        def set_code(x: int, y: int, v: int) -> None:
            fld.set(fi, layer, x, y, v)

        # ground preview prefers layer 0 if available
        def ground(x: int, y: int) -> int:
            return fld.get(fi, 0, x, y)

        self.field_overview.bind(
            64,
            64,
            get_code,
            set_code,
            ground_code=ground,
            event_code=None,
            tile_pack=self.ctx.battle_tiles,
            palette=self.ctx.palette,
        )

    def _on_field_overview_select(self, x: int, y: int) -> None:
        self.field_table.setCurrentCell(x, y)
        item = self.field_table.item(x, y)
        if item:
            try:
                self.field_overview.sp_brush.setValue(int(item.text()))
            except ValueError:
                pass

    def _on_field_overview_edit(self, x: int, y: int, value: int) -> None:
        self.field_table.blockSignals(True)
        self.field_table.setItem(x, y, QTableWidgetItem(str(value)))
        self.field_table.blockSignals(False)

    def _on_field_table_click(self, row: int, col: int) -> None:
        self.field_overview.select_cell(row, col)

    def _load_field(self) -> None:
        fld = self.ctx.warfld
        if not fld:
            return
        fi = self.field_spin.value()
        layer = self.layer_spin.value()
        if fi >= fld.count:
            return
        nlayers = len(fld.fields[fi]) if fi < len(fld.fields) else 1
        if layer >= nlayers:
            layer = 0
        self.field_table.blockSignals(True)
        for x in range(64):
            for y in range(64):
                self.field_table.setItem(x, y, QTableWidgetItem(str(fld.get(fi, layer, x, y))))
        self.field_table.blockSignals(False)
        self._bind_field_overview()

    def _field_cell_changed(self, row: int, col: int) -> None:
        if not self.ctx.warfld:
            return
        item = self.field_table.item(row, col)
        if not item:
            return
        try:
            self.ctx.warfld.set(
                self.field_spin.value(), self.layer_spin.value(), row, col, int(item.text())
            )
            self.field_overview.rebuild()
        except ValueError:
            pass

    def _save_field(self) -> None:
        if not self.ctx.warfld:
            return
        try:
            self.ctx.warfld.save(backup=True)
            QMessageBox.information(self, "保存", "warfld 已保存")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))
