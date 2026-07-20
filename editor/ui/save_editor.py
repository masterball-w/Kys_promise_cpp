"""Save / ranger data editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap, QImage
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QFormLayout,
    QSpinBox, QLineEdit, QPushButton, QComboBox, QTableWidget,
    QTableWidgetItem, QLabel, QMessageBox, QSplitter, QHeaderView,
)

from ui.context import EditorContext
from ui.magic_editor import MagicEditorPanel


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

    def _on_slot(self) -> None:
        slot = self.slot_combo.currentData()
        if slot is None or not self.ctx.data_root:
            return
        try:
            self.ctx.load_save_slot(int(slot))
            self.refresh()
        except Exception as e:
            QMessageBox.warning(self, "加载失败", str(e))

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
        for i, sp in enumerate(self.team_spins):
            sp.setValue(h.team[i] if i < len(h.team) else -1)

        self.role_table.setRowCount(arc.roles.count)
        for i in range(arc.roles.count):
            r = arc.roles.records[i]
            vals = [
                str(i),
                arc.role_name(i),
                str(r[1]),
                str(r[15]),
                str(r[17]),
                str(r[18]),
                str(r[40]),
                str(r[41]),
                str(r[43]),
                str(r[44]),
                str(r[45]),
                str(r[56]) if len(r) > 56 else "0",
            ]
            for c, v in enumerate(vals):
                self.role_table.setItem(i, c, QTableWidgetItem(v))

        self.item_table.setRowCount(arc.items.count)
        for i in range(arc.items.count):
            it = arc.items.records[i]
            vals = [str(i), arc.item_name(i), str(it[41]), str(it[43]), str(it[44]), str(it[42])]
            for c, v in enumerate(vals):
                self.item_table.setItem(i, c, QTableWidgetItem(v))

        self.magic_editor.refresh()

        inv = arc.header.inventory
        self.inv_table.setRowCount(len(inv))
        for i, slot in enumerate(inv):
            name = arc.item_name(slot.number) if slot.number >= 0 else ""
            for c, v in enumerate([str(i), str(slot.number), name, str(slot.amount)]):
                self.inv_table.setItem(i, c, QTableWidgetItem(v))

        self.shop_table.setRowCount(arc.shops.count)
        for i in range(arc.shops.count):
            row = [str(i)] + [str(arc.shops.get(i, j)) for j in range(18)]
            for c, v in enumerate(row):
                self.shop_table.setItem(i, c, QTableWidgetItem(v))

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
        for i in range(6):
            sp = QSpinBox(); sp.setRange(-1, 999)
            self.team_spins.append(sp)
            form.addRow(f"Team[{i}]", sp)
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
        h.team = [sp.value() for sp in self.team_spins]
        self.ctx.statusMessage.emit("总览已应用到内存（请点保存）")

    def _build_roles_tab(self) -> None:
        w = QWidget()
        lay = QHBoxLayout(w)
        self.role_table = QTableWidget(0, 12)
        self.role_table.setHorizontalHeaderLabels([
            "ID", "姓名", "HeadNum", "等级", "HP", "MaxHP", "MP", "MaxMP",
            "攻击", "速度", "防御", "资质",
        ])
        self.role_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self.role_table.cellChanged.connect(self._role_cell_changed)
        self.role_table.currentCellChanged.connect(self._role_selected)
        lay.addWidget(self.role_table, 3)
        side = QVBoxLayout()
        self.head_preview = QLabel("头像预览")
        self.head_preview.setFixedSize(128, 128)
        self.head_preview.setAlignment(Qt.AlignCenter)
        self.head_preview.setStyleSheet("background:#222;color:#aaa;")
        side.addWidget(self.head_preview)
        side.addStretch()
        lay.addLayout(side, 1)
        self.tabs.addTab(w, "角色")
        self._role_updating = False

    def _role_selected(self, row: int, *_args) -> None:
        if row < 0 or not self.ctx.ranger or not self.ctx.heads:
            return
        try:
            head = int(self.role_table.item(row, 2).text())
        except Exception:
            return
        self._show_head(head)

    def _show_head(self, head_num: int) -> None:
        pic = self.ctx.heads
        if not pic or head_num < 0 or head_num >= pic.count:
            self.head_preview.setText("无图")
            return
        try:
            img = pic.frames[head_num].to_image()
            if img is None:
                self.head_preview.setText("空帧")
                return
            data = img.tobytes("raw", "RGBA")
            qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
            self.head_preview.setPixmap(QPixmap.fromImage(qimg).scaled(
                120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        except Exception as e:
            self.head_preview.setText(str(e))

    def _role_cell_changed(self, row: int, col: int) -> None:
        if self._role_updating or not self.ctx.ranger:
            return
        arc = self.ctx.ranger
        item = self.role_table.item(row, col)
        if not item or row >= arc.roles.count:
            return
        text = item.text()
        field_map = {2: 1, 3: 15, 4: 17, 5: 18, 6: 40, 7: 41, 8: 43, 9: 44, 10: 45, 11: 56}
        try:
            if col == 1:
                arc.roles.set_name(row, text, 4, 5)
            elif col in field_map:
                arc.roles.set(row, field_map[col], int(text))
                if col == 2:
                    self._show_head(int(text))
        except ValueError:
            pass

    def _build_items_tab(self) -> None:
        self.item_table = QTableWidget(0, 6)
        self.item_table.setHorizontalHeaderLabels(["ID", "名称", "类型", "价格", "EventNum", "数量库存"])
        self.item_table.cellChanged.connect(self._item_cell_changed)
        self.tabs.addTab(self.item_table, "物品定义")
        self._item_updating = False

    def _item_cell_changed(self, row: int, col: int) -> None:
        if not self.ctx.ranger:
            return
        arc = self.ctx.ranger
        item = self.item_table.item(row, col)
        if not item:
            return
        text = item.text()
        try:
            if col == 1:
                arc.items.set_name(row, text, 1, 10)
            elif col == 2:
                arc.items.set(row, 41, int(text))
            elif col == 3:
                arc.items.set(row, 43, int(text))
            elif col == 4:
                arc.items.set(row, 44, int(text))
            elif col == 5:
                arc.items.set(row, 42, int(text))
        except ValueError:
            pass

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
        headers = ["店"] + [f"Item{i}" for i in range(18)]
        self.shop_table = QTableWidget(0, len(headers))
        self.shop_table.setHorizontalHeaderLabels(headers)
        self.shop_table.cellChanged.connect(self._shop_cell_changed)
        self.tabs.addTab(self.shop_table, "商店")

    def _shop_cell_changed(self, row: int, col: int) -> None:
        if not self.ctx.ranger or col < 1:
            return
        item = self.shop_table.item(row, col)
        if not item:
            return
        try:
            self.ctx.ranger.shops.set(row, col - 1, int(item.text()))
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
