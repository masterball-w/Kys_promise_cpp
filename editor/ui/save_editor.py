"""Save / ranger data editor."""

from __future__ import annotations

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QFormLayout,
    QSpinBox, QPushButton, QComboBox, QTableWidget,
    QTableWidgetItem, QLabel, QMessageBox, QHeaderView,
)

from ui.context import EditorContext
from ui.magic_editor import MagicEditorPanel
from ui.role_editor import RoleEditorPanel
from ui.item_editor import ItemEditorPanel


class SaveEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        layout = QVBoxLayout(self)

        top = QHBoxLayout()
        top.addWidget(QLabel("存档槽:"))
        self.slot_combo = QComboBox()
        self.slot_combo.addItem("Ranger (新游戏模板)", 0)
        for i in range(1, 7):
            self.slot_combo.addItem(f"R{i}", i)
        self.slot_combo.currentIndexChanged.connect(self._on_slot)
        top.addWidget(self.slot_combo)
        reload_btn = QPushButton("重新加载")
        reload_btn.clicked.connect(self.refresh)
        top.addWidget(reload_btn)
        save_btn = QPushButton("保存到磁盘")
        save_btn.clicked.connect(self.save)
        top.addWidget(save_btn)
        top.addStretch()
        layout.addLayout(top)

        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)

        self._build_header_tab()
        self._build_roles_tab()
        self._build_items_tab()
        self._build_magics_tab()
        self._build_inventory_tab()
        self._build_shops_tab()
        self._build_scenes_tab()

        ctx.dataRootChanged.connect(lambda _: self.refresh())
        ctx.encodingChanged.connect(lambda _: self.refresh())
        ctx.saveSlotChanged.connect(self._on_save_slot_changed)
        ctx.saveSlotChanged.connect(lambda _: self.refresh())

    def _on_slot(self) -> None:
        slot = self.slot_combo.currentData()
        if slot is None or not self.ctx.data_root:
            return
        try:
            self.ctx.set_save_slot(int(slot))
            self.refresh()
        except Exception as e:
            QMessageBox.warning(self, "加载失败", str(e))

    def _on_save_slot_changed(self, slot: int) -> None:
        idx = self.slot_combo.findData(slot)
        if idx >= 0 and self.slot_combo.currentIndex() != idx:
            self.slot_combo.blockSignals(True)
            self.slot_combo.setCurrentIndex(idx)
            self.slot_combo.blockSignals(False)
        self.refresh()

    def refresh(self) -> None:
        arc = self.ctx.ranger
        if not arc:
            return
        h = arc.header
        self.sp_where.setValue(h.where)
        self.sp_mx.setValue(h.mx)
        self.sp_my.setValue(h.my)
        self.sp_sx.setValue(h.sx)
        self.sp_sy.setValue(h.sy)
        self.sp_gametime.setValue(h.game_time)
        prof = self.ctx.profile
        team_n = prof.ranger_team_count if prof else 6
        team_off = prof.ranger_team_offset if prof else 30
        for i, sp in enumerate(self.team_spins):
            vis = i < team_n
            sp.setVisible(vis)
            self._team_labels[i].setVisible(vis)
            if vis:
                self._team_labels[i].setText(f"Team[{i}] @byte{team_off + i * 2}")
                sp.setValue(h.team[i] if i < len(h.team) else -1)
        has_money = bool(prof and prof.ranger_has_money_word)
        self._money_label.setVisible(has_money)
        self.sp_money.setVisible(has_money)
        if has_money and prof:
            self._money_label.setText(f"银两 Money[byte {prof.ranger_money_offset}]")
            self.sp_money.setValue(h.money)

        self.role_editor.refresh()
        self.item_editor.refresh()
        self.magic_editor.refresh()

        inv = arc.header.inventory
        self.inv_table.setRowCount(len(inv))
        for i, slot in enumerate(inv):
            name = arc.item_name(slot.number) if slot.number >= 0 else ""
            for c, v in enumerate([str(i), str(slot.number), name, str(slot.amount)]):
                self.inv_table.setItem(i, c, QTableWidgetItem(v))

        self.shop_table.blockSignals(True)
        words = arc.shops.words
        headers = ["店"] + [f"W{j}" for j in range(words)]
        self.shop_table.setColumnCount(len(headers))
        self.shop_table.setHorizontalHeaderLabels(headers)
        self.shop_table.setRowCount(arc.shops.count)
        for i in range(arc.shops.count):
            row = [str(i)] + [str(arc.shops.get(i, j)) for j in range(words)]
            for c, v in enumerate(row):
                self.shop_table.setItem(i, c, QTableWidgetItem(v))
        self.shop_table.blockSignals(False)

        self.scene_table.setRowCount(arc.scenes.count)
        for i in range(arc.scenes.count):
            s = arc.scenes.records[i]
            vals = [str(i), arc.scene_name(i), str(s[9]), str(s[6]), str(s[7]), str(s[23])]
            for c, v in enumerate(vals):
                self.scene_table.setItem(i, c, QTableWidgetItem(v))

    def _build_header_tab(self) -> None:
        w = QWidget()
        form = QFormLayout(w)
        self.sp_where = QSpinBox(); self.sp_where.setRange(-1, 999)
        self.sp_mx = QSpinBox(); self.sp_mx.setRange(0, 479)
        self.sp_my = QSpinBox(); self.sp_my.setRange(0, 479)
        self.sp_sx = QSpinBox(); self.sp_sx.setRange(0, 63)
        self.sp_sy = QSpinBox(); self.sp_sy.setRange(0, 63)
        self.sp_gametime = QSpinBox(); self.sp_gametime.setRange(0, 99)
        form.addRow("where (场景/-1大地图)", self.sp_where)
        form.addRow("Mx", self.sp_mx)
        form.addRow("My", self.sp_my)
        form.addRow("Sx", self.sp_sx)
        form.addRow("Sy", self.sp_sy)
        form.addRow("gametime", self.sp_gametime)
        self.team_spins = []
        self._team_labels: list[QLabel] = []
        for i in range(8):
            sp = QSpinBox(); sp.setRange(-1, 999)
            lbl = QLabel(f"Team[{i}]")
            self.team_spins.append(sp)
            self._team_labels.append(lbl)
            form.addRow(lbl, sp)
        self.sp_money = QSpinBox(); self.sp_money.setRange(-32768, 32767)
        self._money_label = QLabel("银两 Money[42]")
        form.addRow(self._money_label, self.sp_money)
        apply = QPushButton("应用总览修改")
        apply.clicked.connect(self._apply_header)
        form.addRow(apply)
        self.tabs.addTab(w, "总览")

    def _apply_header(self) -> None:
        arc = self.ctx.ranger
        if not arc:
            return
        h = arc.header
        h.where = self.sp_where.value()
        h.mx = self.sp_mx.value()
        h.my = self.sp_my.value()
        h.sx = self.sp_sx.value()
        h.sy = self.sp_sy.value()
        h.game_time = self.sp_gametime.value()
        team_n = self.ctx.profile.ranger_team_count if self.ctx.profile else len(self.team_spins)
        while len(h.team) < team_n:
            h.team.append(-1)
        for i in range(team_n):
            h.team[i] = self.team_spins[i].value()
        if self.ctx.profile and self.ctx.profile.ranger_has_money_word:
            h.money = self.sp_money.value()
        self.ctx.statusMessage.emit("总览已应用到内存（请点保存）")

    def _build_roles_tab(self) -> None:
        self.role_editor = RoleEditorPanel(self.ctx)
        self.tabs.addTab(self.role_editor, "人物")

    def _build_items_tab(self) -> None:
        self.item_editor = ItemEditorPanel(self.ctx)
        self.tabs.addTab(self.item_editor, "物品定义")

    def _build_magics_tab(self) -> None:
        self.magic_editor = MagicEditorPanel(self.ctx)
        self.tabs.addTab(self.magic_editor, "武功")

    def _build_inventory_tab(self) -> None:
        self.inv_table = QTableWidget(0, 4)
        self.inv_table.setHorizontalHeaderLabels(["槽", "物品ID", "名称", "数量"])
        self.inv_table.cellChanged.connect(self._inv_cell_changed)
        self.tabs.addTab(self.inv_table, "背包")

    def _inv_cell_changed(self, row: int, col: int) -> None:
        if not self.ctx.ranger or col not in (1, 3):
            return
        item = self.inv_table.item(row, col)
        if not item:
            return
        try:
            val = int(item.text())
            slot = self.ctx.ranger.header.inventory[row]
            if col == 1:
                slot.number = val
                name = self.ctx.ranger.item_name(val) if val >= 0 else ""
                self.inv_table.blockSignals(True)
                self.inv_table.setItem(row, 2, QTableWidgetItem(name))
                self.inv_table.blockSignals(False)
            else:
                slot.amount = val
        except ValueError:
            pass

    def _build_shops_tab(self) -> None:
        # Column count is refreshed from arc.shops.words (15 classic / 18 Promise)
        self.shop_table = QTableWidget(0, 1)
        self.shop_table.setHorizontalHeaderLabels(["店"])
        self.shop_table.cellChanged.connect(self._shop_cell_changed)
        self.tabs.addTab(self.shop_table, "商店")

    def _shop_cell_changed(self, row: int, col: int) -> None:
        if not self.ctx.ranger or col < 1:
            return
        item = self.shop_table.item(row, col)
        if not item:
            return
        word = col - 1
        if word >= self.ctx.ranger.shops.words:
            return
        try:
            self.ctx.ranger.shops.set(row, word, int(item.text()))
        except ValueError:
            pass

    def _build_scenes_tab(self) -> None:
        self.scene_table = QTableWidget(0, 6)
        self.scene_table.setHorizontalHeaderLabels(["ID", "名称", "EnCondition", "入场音乐", "出场音乐", "mapnum"])
        self.scene_table.cellChanged.connect(self._scene_cell_changed)
        self.tabs.addTab(self.scene_table, "场景元数据")

    def _scene_cell_changed(self, row: int, col: int) -> None:
        if not self.ctx.ranger:
            return
        item = self.scene_table.item(row, col)
        if not item:
            return
        text = item.text()
        try:
            if col == 1:
                self.ctx.ranger.scenes.set_name(row, text, 1, 5)
            elif col == 2:
                self.ctx.ranger.scenes.set(row, 9, int(text))
            elif col == 3:
                self.ctx.ranger.scenes.set(row, 6, int(text))
            elif col == 4:
                self.ctx.ranger.scenes.set(row, 7, int(text))
            elif col == 5:
                self.ctx.ranger.scenes.set(row, 23, int(text))
        except ValueError:
            pass

    def save(self) -> None:
        if not self.ctx.ranger:
            QMessageBox.warning(self, "保存", "未加载存档")
            return
        try:
            self._apply_header()
            self.ctx.ranger.save(backup=True)
            QMessageBox.information(self, "保存", f"已写入 {self.ctx.ranger.grp_path}（已备份 .bak）")
            self.ctx.statusMessage.emit("存档已保存")
        except Exception as e:
            QMessageBox.critical(self, "保存失败", str(e))
