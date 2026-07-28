"""Detailed item definition editor panel for the save editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QFormLayout, QGridLayout,
    QListWidget, QListWidgetItem, QLabel, QLineEdit, QSpinBox, QComboBox,
    QPushButton, QGroupBox, QScrollArea, QSplitter, QMessageBox, QTextEdit,
)

from kys_formats.item_meta import (
    ITEM_TYPES, EQUIP_TYPES, NEED_SEX, NEED_MP_TYPES, CHANGE_MP_TYPES,
    BATTLE_STATES, item_summary, item_type_display,
)
from ui.context import EditorContext
from ui.id_combo import NamedIdCombo, rebuild_named_combos


def _pil_to_pixmap(img) -> QPixmap:
    data = img.convert("RGBA").tobytes("raw", "RGBA")
    qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)


class ItemEditorPanel(QWidget):
    """List + detail form for Ranger Item table (95 words)."""

    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self._iid = -1
        self._loading = False
        self._named_combos: list[NamedIdCombo] = []

        root = QHBoxLayout(self)
        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        left = QWidget()
        left_lay = QVBoxLayout(left)
        self.filter_edit = QLineEdit()
        self.filter_edit.setPlaceholderText("过滤名称/ID/类型…")
        self.filter_edit.textChanged.connect(self._rebuild_list)
        left_lay.addWidget(self.filter_edit)
        self.list = QListWidget()
        self.list.currentRowChanged.connect(self._on_select_row)
        left_lay.addWidget(self.list)
        splitter.addWidget(left)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        detail = QWidget()
        scroll.setWidget(detail)
        self.detail_lay = QVBoxLayout(detail)
        self._build_detail_form()
        splitter.addWidget(scroll)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 3)

    def _spin(self, lo: int, hi: int) -> QSpinBox:
        sp = QSpinBox()
        sp.setRange(lo, hi)
        sp.valueChanged.connect(self._on_field_changed)
        return sp

    def _combo(self, mapping: dict) -> QComboBox:
        cb = QComboBox()
        for k in sorted(mapping.keys()):
            cb.addItem(f"{k}: {mapping[k]}", k)
        cb.currentIndexChanged.connect(self._on_field_changed)
        return cb

    def _id_combo(self, kind: str) -> NamedIdCombo:
        cb = NamedIdCombo(kind, allow_none=True, none_value=-1)
        cb.idChanged.connect(self._on_field_changed)
        self._named_combos.append(cb)
        return cb

    def _set_combo(self, cb: QComboBox, value: int) -> None:
        idx = cb.findData(value)
        if idx < 0:
            cb.addItem(f"{value}: (未登记)", value)
            idx = cb.findData(value)
        cb.setCurrentIndex(max(0, idx))

    def _build_detail_form(self) -> None:
        head = QHBoxLayout()
        self.lbl_id = QLabel("ID: -")
        self.lbl_type = QLabel("类型: -")
        head.addWidget(self.lbl_id)
        head.addWidget(self.lbl_type)
        head.addStretch()
        self.icon_preview = QLabel("图标")
        self.icon_preview.setFixedSize(96, 96)
        self.icon_preview.setAlignment(Qt.AlignCenter)
        self.icon_preview.setStyleSheet("background:#1a1a1a;color:#888;")
        head.addWidget(self.icon_preview)
        self.detail_lay.addLayout(head)

        basic = QGroupBox("基本")
        bf = QFormLayout(basic)
        self.ed_name = QLineEdit()
        self.ed_name.editingFinished.connect(self._on_field_changed)
        self.ed_intro = QTextEdit()
        self.ed_intro.setMaximumHeight(72)
        self.ed_intro.setPlaceholderText("物品说明 Introduction[21..35]，约 30 字节")
        self.cb_type = self._combo(ITEM_TYPES)
        self.cb_equip = self._combo(EQUIP_TYPES)
        self.sp_price = self._spin(0, 30000)
        self.sp_inv = self._spin(-1, 9999)
        self.sp_event = self._spin(-1, 9999)
        self.sp_magic = self._id_combo("magic")
        self.sp_ami = self._spin(-1, 999)
        self.sp_user = self._spin(-1, 999)
        self.sp_show_intro = self._spin(0, 1)
        self.sp_list = self._spin(-1, 9999)
        bf.addRow("名称 Name[1..10]", self.ed_name)
        bf.addRow("说明 Introduction", self.ed_intro)
        bf.addRow("物品类型 ItemType[41]", self.cb_type)
        bf.addRow("装备部位 EquipType[39]", self.cb_equip)
        bf.addRow("价格 price[43]", self.sp_price)
        bf.addRow("模板库存 inventory[42]", self.sp_inv)
        bf.addRow("使用事件 EventNum[44]", self.sp_event)
        bf.addRow("关联武功 Magic[36]", self.sp_magic)
        bf.addRow("动画/AmiNum[37]", self.sp_ami)
        bf.addRow("使用者 User[38]", self.sp_user)
        bf.addRow("显示说明 ShowIntro[40]", self.sp_show_intro)
        bf.addRow("列表号 ListNum[0]", self.sp_list)
        self.detail_lay.addWidget(basic)

        special = QGroupBox("特效 / 限制")
        sf = QFormLayout(special)
        self.cb_battle = self._combo(BATTLE_STATES)
        self.sp_wine = self._spin(0, 100)
        self.sp_set = self._spin(-1, 99)
        self.sp_exp_magic = self._spin(0, 30000)
        self.cb_need_sex = self._combo(NEED_SEX)
        sf.addRow("装备战斗特效 BattleEffect[13]", self.cb_battle)
        sf.addRow("酒效应 WineEffect[14]", self.sp_wine)
        sf.addRow("套装号 SetNum[12]", self.sp_set)
        sf.addRow("秘籍经验 ExpOfMagic[11]", self.sp_exp_magic)
        sf.addRow("性别限制 needSex[15]", self.cb_need_sex)
        self.detail_lay.addWidget(special)

        adds = QGroupBox("属性加成 Add*（装备/丹药生效）")
        ag = QGridLayout(adds)
        self.sp_add_hp = self._spin(-9999, 9999)
        self.sp_add_max_hp = self._spin(-9999, 9999)
        self.sp_add_poi = self._spin(-100, 100)
        self.sp_add_phy = self._spin(-100, 100)
        self.cb_change_mp = self._combo(CHANGE_MP_TYPES)
        self.sp_add_mp = self._spin(-9999, 9999)
        self.sp_add_max_mp = self._spin(-9999, 9999)
        self.sp_add_att = self._spin(-100, 100)
        self.sp_add_spd = self._spin(-100, 100)
        self.sp_add_def = self._spin(-100, 100)
        self.sp_add_med = self._spin(-100, 100)
        self.sp_add_usepoi = self._spin(-100, 100)
        self.sp_add_medpoi = self._spin(-100, 100)
        self.sp_add_defpoi = self._spin(-100, 100)
        self.sp_add_fist = self._spin(-100, 100)
        self.sp_add_sword = self._spin(-100, 100)
        self.sp_add_knife = self._spin(-100, 100)
        self.sp_add_unusual = self._spin(-100, 100)
        self.sp_add_hid = self._spin(-100, 100)
        self.sp_add_know = self._spin(-100, 100)
        self.sp_add_ethics = self._spin(-100, 100)
        self.sp_add_twice = self._spin(0, 1)
        self.sp_add_attpoi = self._spin(-100, 100)
        add_fields = [
            ("当前生命[45]", self.sp_add_hp),
            ("生命上限[46]", self.sp_add_max_hp),
            ("解毒/加毒[47]", self.sp_add_poi),
            ("体力[48]", self.sp_add_phy),
            ("改内力属性[49]", self.cb_change_mp),
            ("当前内力[50]", self.sp_add_mp),
            ("内力上限[51]", self.sp_add_max_mp),
            ("攻击[52]", self.sp_add_att),
            ("轻功[53]", self.sp_add_spd),
            ("防御[54]", self.sp_add_def),
            ("医疗[55]", self.sp_add_med),
            ("用毒[56]", self.sp_add_usepoi),
            ("解毒技[57]", self.sp_add_medpoi),
            ("抗毒[58]", self.sp_add_defpoi),
            ("拳[59]", self.sp_add_fist),
            ("剑[60]", self.sp_add_sword),
            ("刀[61]", self.sp_add_knife),
            ("奇[62]", self.sp_add_unusual),
            ("暗器[63]", self.sp_add_hid),
            ("学识[64]", self.sp_add_know),
            ("道德[65]", self.sp_add_ethics),
            ("连击[66]", self.sp_add_twice),
            ("攻击带毒[67]", self.sp_add_attpoi),
        ]
        for i, (lab, w) in enumerate(add_fields):
            ag.addWidget(QLabel(lab), i // 3, (i % 3) * 2)
            ag.addWidget(w, i // 3, (i % 3) * 2 + 1)
        self.detail_lay.addWidget(adds)

        needs = QGroupBox("装备/修炼需求 Need*（正=至少，负=至多，0=不限）")
        ng = QGridLayout(needs)
        self.sp_only_role = self._id_combo("role")
        self.cb_need_mp_type = self._combo(NEED_MP_TYPES)
        self.sp_need_mp = self._spin(-30000, 30000)
        self.sp_need_att = self._spin(-100, 100)
        self.sp_need_spd = self._spin(-100, 100)
        self.sp_need_usepoi = self._spin(-100, 100)
        self.sp_need_med = self._spin(-100, 100)
        self.sp_need_medpoi = self._spin(-100, 100)
        self.sp_need_fist = self._spin(-100, 100)
        self.sp_need_sword = self._spin(-100, 100)
        self.sp_need_knife = self._spin(-100, 100)
        self.sp_need_unusual = self._spin(-100, 100)
        self.sp_need_hid = self._spin(-100, 100)
        self.sp_need_apt = self._spin(-100, 100)
        self.sp_need_exp = self._spin(0, 30000)
        need_fields = [
            ("专属角色 OnlyPracRole[68]", self.sp_only_role),
            ("内力属性 NeedMPType[69]", self.cb_need_mp_type),
            ("内力 NeedMP[70]", self.sp_need_mp),
            ("攻击 NeedAttack[71]", self.sp_need_att),
            ("轻功 NeedSpeed[72]", self.sp_need_spd),
            ("用毒 NeedUsePoi[73]", self.sp_need_usepoi),
            ("医疗 NeedMedcine[74]", self.sp_need_med),
            ("解毒 NeedMedPoi[75]", self.sp_need_medpoi),
            ("拳 NeedFist[76]", self.sp_need_fist),
            ("剑 NeedSword[77]", self.sp_need_sword),
            ("刀 NeedKnife[78]", self.sp_need_knife),
            ("奇 NeedUnusual[79]", self.sp_need_unusual),
            ("暗器 NeedHidWeapon[80]", self.sp_need_hid),
            ("资质 NeedAptitude[81]", self.sp_need_apt),
            ("修炼经验 NeedExp[82]", self.sp_need_exp),
        ]
        for i, (lab, w) in enumerate(need_fields):
            ng.addWidget(QLabel(lab), i // 2, (i % 2) * 2)
            ng.addWidget(w, i // 2, (i % 2) * 2 + 1)
        self.detail_lay.addWidget(needs)

        craft = QGroupBox("合成材料 NeedItem[85..89] / NeedMatAmount[90..94]")
        cg = QGridLayout(craft)
        self.sp_count = self._spin(0, 999)
        self.sp_rate = self._spin(0, 100)
        cg.addWidget(QLabel("Count[83]"), 0, 0)
        cg.addWidget(self.sp_count, 0, 1)
        cg.addWidget(QLabel("Rate[84]"), 0, 2)
        cg.addWidget(self.sp_rate, 0, 3)
        cg.addWidget(QLabel("材料物品"), 1, 1)
        cg.addWidget(QLabel("数量"), 1, 2)
        self.sp_need_item = []
        self.sp_need_amt = []
        for i in range(5):
            iid = self._id_combo("item")
            amt = self._spin(0, 9999)
            self.sp_need_item.append(iid)
            self.sp_need_amt.append(amt)
            cg.addWidget(QLabel(f"槽{i}"), i + 2, 0)
            cg.addWidget(iid, i + 2, 1)
            cg.addWidget(amt, i + 2, 2)
        self.detail_lay.addWidget(craft)

        apply = QPushButton("应用当前物品修改到内存")
        apply.clicked.connect(self._apply_current)
        self.detail_lay.addWidget(apply)
        self.detail_lay.addStretch()

    def refresh(self) -> None:
        rebuild_named_combos(self._named_combos, self.ctx)
        self._rebuild_list()

    def _rebuild_list(self) -> None:
        arc = self.ctx.ranger
        self.list.blockSignals(True)
        self.list.clear()
        if not arc:
            self.list.blockSignals(False)
            return
        filt = self.filter_edit.text().strip().lower()
        keep = self._iid
        select_row = 0
        for i in range(arc.items.count):
            name = arc.item_name(i)
            rec = arc.items.records[i]
            text = f"{i}: {item_summary(rec, name)}"
            if filt and filt not in text.lower() and filt not in str(i):
                continue
            item = QListWidgetItem(text)
            item.setData(Qt.UserRole, i)
            self.list.addItem(item)
            if i == keep:
                select_row = self.list.count() - 1
        self.list.blockSignals(False)
        if self.list.count() > 0:
            self.list.setCurrentRow(select_row)
        else:
            self._iid = -1

    def _on_select_row(self, row: int) -> None:
        if row < 0:
            return
        item = self.list.item(row)
        if not item:
            return
        self._load_item(int(item.data(Qt.UserRole)))

    def _load_item(self, iid: int) -> None:
        arc = self.ctx.ranger
        if not arc or iid < 0 or iid >= arc.items.count:
            return
        self._loading = True
        self._iid = iid
        rec = arc.items.records[iid]
        name = arc.item_name(iid)
        intro = arc.items.get_name(iid, 21, 15)
        self.lbl_id.setText(f"ID: {iid}")
        self.lbl_type.setText(f"类型: {item_type_display(rec[41])}")

        self.ed_name.setText(name)
        self.ed_intro.setPlainText(intro)
        self._set_combo(self.cb_type, rec[41])
        self._set_combo(self.cb_equip, rec[39])
        self.sp_price.setValue(rec[43])
        self.sp_inv.setValue(rec[42])
        self.sp_event.setValue(rec[44])
        self.sp_magic.set_id(rec[36])
        self.sp_ami.setValue(rec[37])
        self.sp_user.setValue(rec[38])
        self.sp_show_intro.setValue(rec[40])
        self.sp_list.setValue(rec[0])

        self._set_combo(self.cb_battle, rec[13])
        self.sp_wine.setValue(rec[14])
        self.sp_set.setValue(rec[12])
        self.sp_exp_magic.setValue(rec[11])
        self._set_combo(self.cb_need_sex, rec[15])

        self.sp_add_hp.setValue(rec[45])
        self.sp_add_max_hp.setValue(rec[46])
        self.sp_add_poi.setValue(rec[47])
        self.sp_add_phy.setValue(rec[48])
        self._set_combo(self.cb_change_mp, rec[49])
        self.sp_add_mp.setValue(rec[50])
        self.sp_add_max_mp.setValue(rec[51])
        self.sp_add_att.setValue(rec[52])
        self.sp_add_spd.setValue(rec[53])
        self.sp_add_def.setValue(rec[54])
        self.sp_add_med.setValue(rec[55])
        self.sp_add_usepoi.setValue(rec[56])
        self.sp_add_medpoi.setValue(rec[57])
        self.sp_add_defpoi.setValue(rec[58])
        self.sp_add_fist.setValue(rec[59])
        self.sp_add_sword.setValue(rec[60])
        self.sp_add_knife.setValue(rec[61])
        self.sp_add_unusual.setValue(rec[62])
        self.sp_add_hid.setValue(rec[63])
        self.sp_add_know.setValue(rec[64])
        self.sp_add_ethics.setValue(rec[65])
        self.sp_add_twice.setValue(rec[66])
        self.sp_add_attpoi.setValue(rec[67])

        self.sp_only_role.set_id(rec[68])
        self._set_combo(self.cb_need_mp_type, rec[69])
        self.sp_need_mp.setValue(rec[70])
        self.sp_need_att.setValue(rec[71])
        self.sp_need_spd.setValue(rec[72])
        self.sp_need_usepoi.setValue(rec[73])
        self.sp_need_med.setValue(rec[74])
        self.sp_need_medpoi.setValue(rec[75])
        self.sp_need_fist.setValue(rec[76])
        self.sp_need_sword.setValue(rec[77])
        self.sp_need_knife.setValue(rec[78])
        self.sp_need_unusual.setValue(rec[79])
        self.sp_need_hid.setValue(rec[80])
        self.sp_need_apt.setValue(rec[81])
        self.sp_need_exp.setValue(rec[82])

        self.sp_count.setValue(rec[83])
        self.sp_rate.setValue(rec[84])
        for i in range(5):
            self.sp_need_item[i].set_id(rec[85 + i])
            self.sp_need_amt[i].setValue(rec[90 + i])

        self._loading = False
        self._refresh_icon()

    def _on_field_changed(self, *_args) -> None:
        if self._loading:
            return
        t = self.cb_type.currentData()
        if t is not None:
            self.lbl_type.setText(f"类型: {item_type_display(int(t))}")

    def _refresh_icon(self) -> None:
        bank = self.ctx.items_pic
        iid = self._iid
        if bank is None or iid < 0 or iid >= bank.count:
            self.icon_preview.setText("无图")
            return
        try:
            img = bank.get_image(iid)
            if img is None:
                self.icon_preview.setText("空帧")
                return
            self.icon_preview.setPixmap(
                _pil_to_pixmap(img).scaled(90, 90, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        except Exception as e:
            self.icon_preview.setText(str(e))

    def _apply_current(self) -> None:
        arc = self.ctx.ranger
        if not arc or self._iid < 0:
            return
        iid = self._iid
        try:
            arc.items.set_name(iid, self.ed_name.text(), 1, 10)
            arc.items.set_name(iid, self.ed_intro.toPlainText().replace("\n", ""), 21, 15)
            sets = {
                0: self.sp_list.value(),
                11: self.sp_exp_magic.value(),
                12: self.sp_set.value(),
                13: int(self.cb_battle.currentData()),
                14: self.sp_wine.value(),
                15: int(self.cb_need_sex.currentData()),
                36: self.sp_magic.get_id(),
                37: self.sp_ami.value(),
                38: self.sp_user.value(),
                39: int(self.cb_equip.currentData()),
                40: self.sp_show_intro.value(),
                41: int(self.cb_type.currentData()),
                42: self.sp_inv.value(),
                43: self.sp_price.value(),
                44: self.sp_event.value(),
                45: self.sp_add_hp.value(),
                46: self.sp_add_max_hp.value(),
                47: self.sp_add_poi.value(),
                48: self.sp_add_phy.value(),
                49: int(self.cb_change_mp.currentData()),
                50: self.sp_add_mp.value(),
                51: self.sp_add_max_mp.value(),
                52: self.sp_add_att.value(),
                53: self.sp_add_spd.value(),
                54: self.sp_add_def.value(),
                55: self.sp_add_med.value(),
                56: self.sp_add_usepoi.value(),
                57: self.sp_add_medpoi.value(),
                58: self.sp_add_defpoi.value(),
                59: self.sp_add_fist.value(),
                60: self.sp_add_sword.value(),
                61: self.sp_add_knife.value(),
                62: self.sp_add_unusual.value(),
                63: self.sp_add_hid.value(),
                64: self.sp_add_know.value(),
                65: self.sp_add_ethics.value(),
                66: self.sp_add_twice.value(),
                67: self.sp_add_attpoi.value(),
                68: self.sp_only_role.get_id(),
                69: int(self.cb_need_mp_type.currentData()),
                70: self.sp_need_mp.value(),
                71: self.sp_need_att.value(),
                72: self.sp_need_spd.value(),
                73: self.sp_need_usepoi.value(),
                74: self.sp_need_med.value(),
                75: self.sp_need_medpoi.value(),
                76: self.sp_need_fist.value(),
                77: self.sp_need_sword.value(),
                78: self.sp_need_knife.value(),
                79: self.sp_need_unusual.value(),
                80: self.sp_need_hid.value(),
                81: self.sp_need_apt.value(),
                82: self.sp_need_exp.value(),
                83: self.sp_count.value(),
                84: self.sp_rate.value(),
            }
            for i in range(5):
                sets[85 + i] = self.sp_need_item[i].get_id()
                sets[90 + i] = self.sp_need_amt[i].value()
            for w, v in sets.items():
                arc.items.set(iid, w, v)
            self.ctx.statusMessage.emit(f"物品 {iid} 已写入内存（请点「保存到磁盘」）")
            self._rebuild_list()
        except Exception as e:
            QMessageBox.critical(self, "应用失败", str(e))
