"""Detailed role (人物) editor panel for the save editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QFormLayout, QGridLayout,
    QListWidget, QListWidgetItem, QLabel, QLineEdit, QSpinBox, QComboBox,
    QPushButton, QGroupBox, QScrollArea, QSplitter, QMessageBox,
)

from kys_formats.role_meta import (
    SEXUAL, MP_TYPES, EQUIP_SLOTS, as_u16, to_i16_from_u16, role_summary,
)
from ui.context import EditorContext
from ui.id_combo import NamedIdCombo, rebuild_named_combos


def _pil_to_pixmap(img) -> QPixmap:
    data = img.convert("RGBA").tobytes("raw", "RGBA")
    qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)


class RoleEditorPanel(QWidget):
    """List + detail form for Ranger Role table (91 words)."""

    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self._rid = -1
        self._loading = False
        self._named_combos: list[NamedIdCombo] = []

        root = QHBoxLayout(self)
        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        left = QWidget()
        left_lay = QVBoxLayout(left)
        self.filter_edit = QLineEdit()
        self.filter_edit.setPlaceholderText("过滤姓名/ID…")
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
        self.lbl_summary = QLabel("")
        head.addWidget(self.lbl_id)
        head.addWidget(self.lbl_summary)
        head.addStretch()
        self.head_preview = QLabel("头像")
        self.head_preview.setFixedSize(96, 96)
        self.head_preview.setAlignment(Qt.AlignCenter)
        self.head_preview.setStyleSheet("background:#1a1a1a;color:#888;")
        head.addWidget(self.head_preview)
        self.detail_lay.addLayout(head)

        basic = QGroupBox("基本")
        bf = QFormLayout(basic)
        self.ed_name = QLineEdit()
        self.ed_name.editingFinished.connect(self._on_field_changed)
        self.ed_nick = QLineEdit()
        self.ed_nick.editingFinished.connect(self._on_field_changed)
        self.cb_sex = self._combo(SEXUAL)
        self.sp_head = self._spin(0, 999)
        self.sp_level = self._spin(0, 99)
        self.sp_exp = self._spin(0, 65535)
        self.sp_list = self._spin(-1, 999)
        self.sp_inc_life = self._spin(-100, 100)
        bf.addRow("姓名 Name[4..8]", self.ed_name)
        bf.addRow("绰号 Nick[9..13]", self.ed_nick)
        bf.addRow("性别 Sexual[14]", self.cb_sex)
        bf.addRow("头像 HeadNum[1]", self.sp_head)
        bf.addRow("等级 Level[15]", self.sp_level)
        bf.addRow("经验 Exp[16](uint16)", self.sp_exp)
        bf.addRow("列表号 ListNum[0]", self.sp_list)
        bf.addRow("成长 IncLife[2]", self.sp_inc_life)
        self.detail_lay.addWidget(basic)

        vitals = QGroupBox("生命 / 内力 / 状态")
        vf = QFormLayout(vitals)
        self.sp_hp = self._spin(0, 30000)
        self.sp_max_hp = self._spin(0, 30000)
        self.sp_hurt = self._spin(0, 100)
        self.sp_poi = self._spin(0, 100)
        self.sp_phy = self._spin(0, 100)
        self.cb_mp_type = self._combo(MP_TYPES)
        self.sp_mp = self._spin(0, 30000)
        self.sp_max_mp = self._spin(0, 30000)
        vf.addRow("当前生命 CurrentHP[17]", self.sp_hp)
        vf.addRow("生命上限 MaxHP[18]", self.sp_max_hp)
        vf.addRow("内伤 Hurt[19]", self.sp_hurt)
        vf.addRow("中毒 Poision[20]", self.sp_poi)
        vf.addRow("体力 PhyPower[21]", self.sp_phy)
        vf.addRow("内力属性 MPType[40]", self.cb_mp_type)
        vf.addRow("当前内力 CurrentMP[41]", self.sp_mp)
        vf.addRow("内力上限 MaxMP[42]", self.sp_max_mp)
        self.detail_lay.addWidget(vitals)

        combat = QGroupBox("战斗属性")
        cf = QGridLayout(combat)
        self.sp_att = self._spin(0, 200)
        self.sp_spd = self._spin(0, 200)
        self.sp_def = self._spin(0, 200)
        self.sp_apt = self._spin(0, 100)
        self.sp_ethics = self._spin(0, 100)
        self.sp_know = self._spin(0, 100)
        self.sp_repute = self._spin(0, 30000)
        self.sp_att_poi = self._spin(0, 100)
        self.sp_att_twice = self._spin(0, 1)
        rows = [
            ("攻击 Attack[43]", self.sp_att),
            ("轻功 Speed[44]", self.sp_spd),
            ("防御 Defence[45]", self.sp_def),
            ("资质 Aptitude[60]", self.sp_apt),
            ("道德 Ethics[56]", self.sp_ethics),
            ("学识 Knowledge[55]", self.sp_know),
            ("声望 Repute[59]", self.sp_repute),
            ("攻击带毒 AttPoi[57]", self.sp_att_poi),
            ("连击 AttTwice[58]", self.sp_att_twice),
        ]
        for i, (lab, w) in enumerate(rows):
            cf.addWidget(QLabel(lab), i // 3, (i % 3) * 2)
            cf.addWidget(w, i // 3, (i % 3) * 2 + 1)
        self.detail_lay.addWidget(combat)

        skills = QGroupBox("技能熟练度")
        sf = QGridLayout(skills)
        self.sp_med = self._spin(0, 100)
        self.sp_usepoi = self._spin(0, 100)
        self.sp_medpoi = self._spin(0, 100)
        self.sp_defpoi = self._spin(0, 100)
        self.sp_fist = self._spin(0, 100)
        self.sp_sword = self._spin(0, 100)
        self.sp_knife = self._spin(0, 100)
        self.sp_unusual = self._spin(0, 100)
        self.sp_hid = self._spin(0, 100)
        skill_rows = [
            ("医疗 Medcine[46]", self.sp_med),
            ("用毒 UsePoi[47]", self.sp_usepoi),
            ("解毒 MedPoi[48]", self.sp_medpoi),
            ("抗毒 DefPoi[49]", self.sp_defpoi),
            ("拳掌 Fist[50]", self.sp_fist),
            ("剑术 Sword[51]", self.sp_sword),
            ("刀法 Knife[52]", self.sp_knife),
            ("奇门 Unusual[53]", self.sp_unusual),
            ("暗器 HidWeapon[54]", self.sp_hid),
        ]
        for i, (lab, w) in enumerate(skill_rows):
            sf.addWidget(QLabel(lab), i // 3, (i % 3) * 2)
            sf.addWidget(w, i // 3, (i % 3) * 2 + 1)
        self.detail_lay.addWidget(skills)

        equip = QGroupBox("装备 Equip[23..27]（物品，-1=空）")
        ef = QFormLayout(equip)
        self.sp_equip = [self._id_combo("item") for _ in range(5)]
        for i, sp in enumerate(self.sp_equip):
            ef.addRow(f"{EQUIP_SLOTS[i]} Equip[{i}]=[{23 + i}]", sp)
        self.detail_lay.addWidget(equip)

        gongti = QGroupBox("功体 / 队伍扩展")
        gf = QFormLayout(gongti)
        self.sp_gongti = self._id_combo("magic")
        self.sp_gongti_exam = self._spin(0, 65535)
        self.sp_team_state = self._spin(-1, 999)
        self.sp_angry = self._spin(0, 100)
        self.sp_moveable = self._spin(-1, 1)
        self.sp_skill_pt = self._spin(0, 999)
        self.sp_pet = self._spin(0, 99)
        self.sp_impression = self._spin(-100, 100)
        self.sp_reset = self._spin(-1, 99)
        self.sp_diff = self._spin(-1, 99)
        gf.addRow("当前功体武功 Gongti[28]", self.sp_gongti)
        gf.addRow("功体经验 GongtiExam[31](uint16)", self.sp_gongti_exam)
        gf.addRow("TeamState[29]", self.sp_team_state)
        gf.addRow("怒气 Angry[30]", self.sp_angry)
        gf.addRow("可移动 Moveable[32]", self.sp_moveable)
        gf.addRow("技能点 AddSkillPoint[33]", self.sp_skill_pt)
        gf.addRow("宠物数 PetAmount[34]", self.sp_pet)
        gf.addRow("好感 Impression[35]", self.sp_impression)
        gf.addRow("Reset[36]", self.sp_reset)
        gf.addRow("difficulty[37]", self.sp_diff)
        self.detail_lay.addWidget(gongti)

        book = QGroupBox("修炼")
        bkf = QFormLayout(book)
        self.sp_book = self._id_combo("item")
        self.sp_exp_book = self._spin(0, 65535)
        self.sp_exp_item = self._spin(0, 65535)
        bkf.addRow("修炼秘籍 PracticeBook[61]", self.sp_book)
        bkf.addRow("修炼经验 ExpForBook[62](uint16)", self.sp_exp_book)
        bkf.addRow("装备经验 ExpForItem[22](uint16)", self.sp_exp_item)
        self.detail_lay.addWidget(book)

        magic_box = QGroupBox("武功栏 Magic[63..72] / MagLevel[73..82]（等级多为 级×100+进度）")
        mg = QGridLayout(magic_box)
        mg.addWidget(QLabel("栏"), 0, 0)
        mg.addWidget(QLabel("武功"), 0, 1)
        mg.addWidget(QLabel("等级数据"), 0, 2)
        self.sp_magic = []
        self.sp_mag_lv = []
        for i in range(10):
            mid = self._id_combo("magic")
            mlv = self._spin(0, 30000)
            self.sp_magic.append(mid)
            self.sp_mag_lv.append(mlv)
            mg.addWidget(QLabel(str(i)), i + 1, 0)
            mg.addWidget(mid, i + 1, 1)
            mg.addWidget(mlv, i + 1, 2)
        self.detail_lay.addWidget(magic_box)

        take = QGroupBox("随身物品 TakingItem[83..86] / Amount[87..90]")
        tg = QGridLayout(take)
        tg.addWidget(QLabel("槽"), 0, 0)
        tg.addWidget(QLabel("物品"), 0, 1)
        tg.addWidget(QLabel("数量"), 0, 2)
        self.sp_take_id = []
        self.sp_take_amt = []
        for i in range(4):
            tid = self._id_combo("item")
            amt = self._spin(0, 9999)
            self.sp_take_id.append(tid)
            self.sp_take_amt.append(amt)
            tg.addWidget(QLabel(str(i)), i + 1, 0)
            tg.addWidget(tid, i + 1, 1)
            tg.addWidget(amt, i + 1, 2)
        self.detail_lay.addWidget(take)

        apply = QPushButton("应用当前人物修改到内存")
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
        keep = self._rid
        select_row = 0
        for i in range(arc.roles.count):
            name = arc.role_name(i)
            rec = arc.roles.records[i]
            text = f"{i}: {role_summary(rec, name)}"
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
            self._rid = -1

    def _on_select_row(self, row: int) -> None:
        if row < 0:
            return
        item = self.list.item(row)
        if not item:
            return
        self._load_role(int(item.data(Qt.UserRole)))

    def _load_role(self, rid: int) -> None:
        arc = self.ctx.ranger
        if not arc or rid < 0 or rid >= arc.roles.count:
            return
        self._loading = True
        self._rid = rid
        rec = arc.roles.records[rid]
        name = arc.role_name(rid)
        nick = arc.roles.get_name(rid, 9, 5)
        self.lbl_id.setText(f"ID: {rid}")
        self.lbl_summary.setText(role_summary(rec, name))

        self.ed_name.setText(name)
        self.ed_nick.setText(nick)
        self._set_combo(self.cb_sex, rec[14])
        self.sp_head.setValue(rec[1])
        self.sp_level.setValue(rec[15])
        self.sp_exp.setValue(as_u16(rec[16]))
        self.sp_list.setValue(rec[0])
        self.sp_inc_life.setValue(rec[2])

        self.sp_hp.setValue(rec[17])
        self.sp_max_hp.setValue(rec[18])
        self.sp_hurt.setValue(rec[19])
        self.sp_poi.setValue(rec[20])
        self.sp_phy.setValue(rec[21])
        self._set_combo(self.cb_mp_type, rec[40])
        self.sp_mp.setValue(rec[41])
        self.sp_max_mp.setValue(rec[42])

        self.sp_att.setValue(rec[43])
        self.sp_spd.setValue(rec[44])
        self.sp_def.setValue(rec[45])
        self.sp_apt.setValue(rec[60])
        self.sp_ethics.setValue(rec[56])
        self.sp_know.setValue(rec[55])
        self.sp_repute.setValue(rec[59])
        self.sp_att_poi.setValue(rec[57])
        self.sp_att_twice.setValue(rec[58])

        self.sp_med.setValue(rec[46])
        self.sp_usepoi.setValue(rec[47])
        self.sp_medpoi.setValue(rec[48])
        self.sp_defpoi.setValue(rec[49])
        self.sp_fist.setValue(rec[50])
        self.sp_sword.setValue(rec[51])
        self.sp_knife.setValue(rec[52])
        self.sp_unusual.setValue(rec[53])
        self.sp_hid.setValue(rec[54])

        for i in range(5):
            self.sp_equip[i].set_id(rec[23 + i])

        self.sp_gongti.set_id(rec[28])
        self.sp_gongti_exam.setValue(as_u16(rec[31]))
        self.sp_team_state.setValue(rec[29])
        self.sp_angry.setValue(rec[30])
        self.sp_moveable.setValue(rec[32])
        self.sp_skill_pt.setValue(rec[33])
        self.sp_pet.setValue(rec[34])
        self.sp_impression.setValue(rec[35])
        self.sp_reset.setValue(rec[36])
        self.sp_diff.setValue(rec[37])

        self.sp_book.set_id(rec[61])
        self.sp_exp_book.setValue(as_u16(rec[62]))
        self.sp_exp_item.setValue(as_u16(rec[22]))

        for i in range(10):
            self.sp_magic[i].set_id(rec[63 + i])
            self.sp_mag_lv[i].setValue(rec[73 + i])
        for i in range(4):
            self.sp_take_id[i].set_id(rec[83 + i])
            self.sp_take_amt[i].setValue(rec[87 + i])

        self._loading = False
        self._refresh_head()

    def _on_field_changed(self, *_args) -> None:
        if self._loading:
            return
        if self.sender() is self.sp_head:
            self._refresh_head()

    def _refresh_head(self) -> None:
        bank = self.ctx.heads
        head = self.sp_head.value()
        if bank is None or head < 0 or head >= bank.count:
            self.head_preview.setText("无图")
            return
        try:
            img = bank.get_image(head)
            if img is None:
                self.head_preview.setText("空帧")
                return
            self.head_preview.setPixmap(
                _pil_to_pixmap(img).scaled(90, 90, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        except Exception as e:
            self.head_preview.setText(str(e))

    def _apply_current(self) -> None:
        arc = self.ctx.ranger
        if not arc or self._rid < 0:
            return
        rid = self._rid
        try:
            arc.roles.set_name(rid, self.ed_name.text(), 4, 5)
            arc.roles.set_name(rid, self.ed_nick.text(), 9, 5)
            sets = {
                0: self.sp_list.value(),
                1: self.sp_head.value(),
                2: self.sp_inc_life.value(),
                14: int(self.cb_sex.currentData()),
                15: self.sp_level.value(),
                16: to_i16_from_u16(self.sp_exp.value()),
                17: self.sp_hp.value(),
                18: self.sp_max_hp.value(),
                19: self.sp_hurt.value(),
                20: self.sp_poi.value(),
                21: self.sp_phy.value(),
                22: to_i16_from_u16(self.sp_exp_item.value()),
                28: self.sp_gongti.get_id(),
                29: self.sp_team_state.value(),
                30: self.sp_angry.value(),
                31: to_i16_from_u16(self.sp_gongti_exam.value()),
                32: self.sp_moveable.value(),
                33: self.sp_skill_pt.value(),
                34: self.sp_pet.value(),
                35: self.sp_impression.value(),
                36: self.sp_reset.value(),
                37: self.sp_diff.value(),
                40: int(self.cb_mp_type.currentData()),
                41: self.sp_mp.value(),
                42: self.sp_max_mp.value(),
                43: self.sp_att.value(),
                44: self.sp_spd.value(),
                45: self.sp_def.value(),
                46: self.sp_med.value(),
                47: self.sp_usepoi.value(),
                48: self.sp_medpoi.value(),
                49: self.sp_defpoi.value(),
                50: self.sp_fist.value(),
                51: self.sp_sword.value(),
                52: self.sp_knife.value(),
                53: self.sp_unusual.value(),
                54: self.sp_hid.value(),
                55: self.sp_know.value(),
                56: self.sp_ethics.value(),
                57: self.sp_att_poi.value(),
                58: self.sp_att_twice.value(),
                59: self.sp_repute.value(),
                60: self.sp_apt.value(),
                61: self.sp_book.get_id(),
                62: to_i16_from_u16(self.sp_exp_book.value()),
            }
            for i in range(5):
                sets[23 + i] = self.sp_equip[i].get_id()
            for i in range(10):
                sets[63 + i] = self.sp_magic[i].get_id()
                sets[73 + i] = self.sp_mag_lv[i].value()
            for i in range(4):
                sets[83 + i] = self.sp_take_id[i].get_id()
                sets[87 + i] = self.sp_take_amt[i].value()
            for w, v in sets.items():
                arc.roles.set(rid, w, v)
            self.ctx.statusMessage.emit(f"人物 {rid} 已写入内存（请点「保存到磁盘」）")
            self._rebuild_list()
        except Exception as e:
            QMessageBox.critical(self, "应用失败", str(e))
