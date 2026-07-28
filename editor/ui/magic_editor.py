"""Detailed magic (武功) editor panel for the save editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QFormLayout, QGridLayout,
    QListWidget, QListWidgetItem, QLabel, QLineEdit, QSpinBox, QComboBox,
    QPushButton, QGroupBox, QScrollArea, QSplitter, QMessageBox,
)

from kys_formats.magic_meta import (
    MAGIC_TYPES, HURT_TYPES, ATT_AREA_TYPES, BATTLE_STATES,
    cal_new_hurt_value, dominant_modulus, category_display,
    GROWTH_CURVE_HELP, hurt_table,
)
from kys_formats.pic_png import PicArchive
from ui.context import EditorContext


def _pil_to_pixmap(img) -> QPixmap:
    data = img.convert("RGBA").tobytes("raw", "RGBA")
    qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)


class MagicEditorPanel(QWidget):
    """List + detail form for Ranger Magic table."""

    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self._mid = -1
        self._loading = False
        self._eft_cache: PicArchive | None = None
        self._eft_ami = -1

        root = QHBoxLayout(self)
        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        left = QWidget()
        left_lay = QVBoxLayout(left)
        self.filter_edit = QLineEdit()
        self.filter_edit.setPlaceholderText("过滤名称/ID…")
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

    def _build_detail_form(self) -> None:
        # Header
        head = QHBoxLayout()
        self.lbl_id = QLabel("ID: -")
        self.lbl_category = QLabel("类别: -")
        head.addWidget(self.lbl_id)
        head.addWidget(self.lbl_category)
        head.addStretch()
        self.detail_lay.addLayout(head)

        # Basic
        basic = QGroupBox("基本")
        bf = QFormLayout(basic)
        self.ed_name = QLineEdit()
        self.ed_name.editingFinished.connect(self._on_field_changed)
        self.cb_type = self._combo(MAGIC_TYPES)
        self.cb_hurt = self._combo(HURT_TYPES)
        self.sp_ami = self._spin(0, 999)
        self.sp_need_mp = self._spin(0, 9999)
        self.sp_need_hp = self._spin(0, 9999)
        self.sp_sound = self._spin(-1, 999)
        self.sp_event = self._spin(-1, 9999)
        self.sp_max_lv = self._spin(0, 2)
        self.sp_poison = self._spin(0, 100)
        bf.addRow("名称", self.ed_name)
        bf.addRow("类别 MagicType[12]", self.cb_type)
        bf.addRow("伤害类型 HurtType[14]", self.cb_hurt)
        bf.addRow("特效索引 AmiNum[13]", self.sp_ami)
        bf.addRow("耗内力 NeedMP[16]", self.sp_need_mp)
        bf.addRow("耗生命 NeedHP[7]", self.sp_need_hp)
        bf.addRow("音效 SoundNum[11]", self.sp_sound)
        bf.addRow("事件 EventNum[10]", self.sp_event)
        bf.addRow("内功最高级 MaxLevel[80]", self.sp_max_lv)
        bf.addRow("带毒 Poision[17]", self.sp_poison)
        self.detail_lay.addWidget(basic)

        # Effect preview
        prev = QGroupBox("特效预览 (AmiNum → eft)")
        pl = QHBoxLayout(prev)
        self.eft_preview = QLabel("无预览")
        self.eft_preview.setFixedSize(160, 160)
        self.eft_preview.setAlignment(Qt.AlignCenter)
        self.eft_preview.setStyleSheet("background:#1a1a1a;color:#888;")
        pl.addWidget(self.eft_preview)
        reload_eft = QPushButton("刷新特效预览")
        reload_eft.clicked.connect(self._refresh_eft_preview)
        pl.addWidget(reload_eft)
        pl.addStretch()
        self.detail_lay.addWidget(prev)

        # Power
        power = QGroupBox("威力与成长曲线 (MinHurt / MaxHurt / HurtModulus)")
        pf = QFormLayout(power)
        self.sp_min_hurt = self._spin(0, 30000)
        self.sp_max_hurt = self._spin(0, 30000)
        self.sp_hurt_mod = self._spin(0, 30000)
        self.lbl_p1 = QLabel("-")
        self.lbl_p10 = QLabel("-")
        self.lbl_curve_table = QLabel("-")
        self.lbl_curve_table.setWordWrap(True)
        self.lbl_curve_table.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.lbl_curve_help = QLabel(GROWTH_CURVE_HELP)
        self.lbl_curve_help.setWordWrap(True)
        self.lbl_curve_help.setStyleSheet("color:#555; font-family: Consolas, 'Courier New', monospace;")
        self.lbl_curve_help.setTextInteractionFlags(Qt.TextSelectableByMouse)
        pf.addRow("一级最小伤害 MinHurt[18]", self.sp_min_hurt)
        pf.addRow("十级最大伤害 MaxHurt[19]", self.sp_max_hurt)
        pf.addRow("成长曲线 HurtModulus[20]", self.sp_hurt_mod)
        pf.addRow("推算一级威力", self.lbl_p1)
        pf.addRow("推算十级威力", self.lbl_p10)
        pf.addRow("1～10 级威力表", self.lbl_curve_table)
        pf.addRow("计算公式", self.lbl_curve_help)
        self.detail_lay.addWidget(power)

        # Bonus mode
        bonus = QGroupBox("加成模式 (伤害权重，见 BattleManager::CalHurtValue)")
        bg = QGridLayout(bonus)
        self.sp_mod_att = self._spin(0, 100)
        self.sp_mod_mp = self._spin(0, 100)
        self.sp_mod_spd = self._spin(0, 100)
        self.sp_mod_wpn = self._spin(0, 100)
        self.lbl_mod_sum = QLabel("-")
        bg.addWidget(QLabel("攻击型 AttackModulus[21]"), 0, 0)
        bg.addWidget(self.sp_mod_att, 0, 1)
        bg.addWidget(QLabel("内力型 MPModulus[22]"), 1, 0)
        bg.addWidget(self.sp_mod_mp, 1, 1)
        bg.addWidget(QLabel("轻功型 SpeedModulus[23]"), 2, 0)
        bg.addWidget(self.sp_mod_spd, 2, 1)
        bg.addWidget(QLabel("兵器型 WeaponModulus[24]"), 3, 0)
        bg.addWidget(self.sp_mod_wpn, 3, 1)
        bg.addWidget(QLabel("权重说明"), 4, 0)
        bg.addWidget(self.lbl_mod_sum, 4, 1)
        hint = QLabel(
            "p = 攻击×6 + 内力 + 轻功×2 + 兵器×2；各分量按权重分摊基础伤害。"
            "兵器型会按 MagicType 取拳/剑/刀/奇门数值。"
        )
        hint.setWordWrap(True)
        bg.addWidget(hint, 5, 0, 1, 2)
        self.detail_lay.addWidget(bonus)

        # Range
        rng = QGroupBox("移动 / 攻击范围")
        rf = QFormLayout(rng)
        self.cb_area = self._combo(ATT_AREA_TYPES)
        self.sp_min_step = self._spin(0, 63)
        self.sp_move1 = self._spin(0, 63)
        self.sp_move10 = self._spin(0, 63)
        self.sp_att1 = self._spin(0, 63)
        self.sp_att10 = self._spin(0, 63)
        rf.addRow("攻击范围模式 AttAreaType[15]", self.cb_area)
        rf.addRow("最小步数 MinStep[8]", self.sp_min_step)
        rf.addRow("一级移动范围 MoveDistance[0]=[28]", self.sp_move1)
        rf.addRow("十级移动范围 MoveDistance[9]=[37]", self.sp_move10)
        rf.addRow("一级攻击范围 AttDistance[0]=[38]", self.sp_att1)
        rf.addRow("十级攻击范围 AttDistance[9]=[47]", self.sp_att10)
        self.detail_lay.addWidget(rng)

        # Scales
        scale = QGroupBox("攻击吸血/吸内比例 (武功自身字段，非 BattleState)")
        sf = QFormLayout(scale)
        self.sp_mp_scale = self._spin(0, 100)
        self.sp_hp_scale = self._spin(0, 100)
        sf.addRow("吸内力% AddMpScale[26]", self.sp_mp_scale)
        sf.addRow("吸血% AddHpScale[27]", self.sp_hp_scale)
        self.detail_lay.addWidget(scale)

        # Gongti
        self.gongti_box = QGroupBox("内功 / 功体属性")
        gf = QFormLayout(self.gongti_box)
        self.cb_battle = self._combo(BATTLE_STATES)
        self.sp_need_exp0 = self._spin(0, 50000)
        self.sp_need_exp1 = self._spin(0, 50000)
        self.sp_need_exp2 = self._spin(0, 50000)
        # AddHP/MP/Att/Def/Spd for levels 0,1,2
        self.sp_add_hp = [self._spin(-999, 9999) for _ in range(3)]
        self.sp_add_mp = [self._spin(-999, 9999) for _ in range(3)]
        self.sp_add_att = [self._spin(-999, 9999) for _ in range(3)]
        self.sp_add_def = [self._spin(-999, 9999) for _ in range(3)]
        self.sp_add_spd = [self._spin(-999, 9999) for _ in range(3)]
        self.sp_add_med = self._spin(-100, 100)
        self.sp_add_usepoi = self._spin(-100, 100)
        self.sp_add_medpoi = self._spin(-100, 100)
        self.sp_add_defpoi = self._spin(-100, 100)
        self.sp_add_fist = self._spin(-100, 100)
        self.sp_add_sword = self._spin(-100, 100)
        self.sp_add_knife = self._spin(-100, 100)
        self.sp_add_unusual = self._spin(-100, 100)
        self.sp_add_hid = self._spin(-100, 100)
        gf.addRow("功体特效 BattleState[76]", self.cb_battle)
        gf.addRow("升至精纯所需经验 NeedExp[1]=[78]", self.sp_need_exp1)
        gf.addRow("升至化境所需经验 NeedExp[2]=[79]", self.sp_need_exp2)
        gf.addRow("NeedExp[0]=[77](备用)", self.sp_need_exp0)
        for i, name in enumerate(("熟练(0)", "精纯(1)", "化境(2)")):
            row = QHBoxLayout()
            row.addWidget(QLabel("生命"))
            row.addWidget(self.sp_add_hp[i])
            row.addWidget(QLabel("内力"))
            row.addWidget(self.sp_add_mp[i])
            row.addWidget(QLabel("攻击"))
            row.addWidget(self.sp_add_att[i])
            row.addWidget(QLabel("防御"))
            row.addWidget(self.sp_add_def[i])
            row.addWidget(QLabel("轻功"))
            row.addWidget(self.sp_add_spd[i])
            gf.addRow(f"加成·{name}", row)
        skills = QHBoxLayout()
        for label, sp in [
            ("医疗", self.sp_add_med), ("用毒", self.sp_add_usepoi),
            ("解毒", self.sp_add_medpoi), ("抗毒", self.sp_add_defpoi),
            ("拳", self.sp_add_fist), ("剑", self.sp_add_sword),
            ("刀", self.sp_add_knife), ("奇", self.sp_add_unusual),
            ("暗器", self.sp_add_hid),
        ]:
            skills.addWidget(QLabel(label))
            skills.addWidget(sp)
        gf.addRow("满级技能加成", skills)
        self.detail_lay.addWidget(self.gongti_box)

        apply = QPushButton("应用当前武功修改到内存")
        apply.clicked.connect(self._apply_current)
        self.detail_lay.addWidget(apply)
        self.detail_lay.addStretch()

    def refresh(self) -> None:
        self._rebuild_list()

    def _rebuild_list(self) -> None:
        arc = self.ctx.ranger
        self.list.blockSignals(True)
        self.list.clear()
        if not arc:
            self.list.blockSignals(False)
            return
        filt = self.filter_edit.text().strip().lower()
        keep_mid = self._mid
        select_row = 0
        for i in range(arc.magics.count):
            name = arc.magic_name(i)
            rec = arc.magics.records[i]
            cat = category_display(rec)
            text = f"{i}: {name}  [{cat}]"
            if filt and filt not in text.lower() and filt not in str(i):
                continue
            item = QListWidgetItem(text)
            item.setData(Qt.UserRole, i)
            self.list.addItem(item)
            if i == keep_mid:
                select_row = self.list.count() - 1
        self.list.blockSignals(False)
        if self.list.count() > 0:
            self.list.setCurrentRow(select_row)
        else:
            self._mid = -1

    def _on_select_row(self, row: int) -> None:
        if row < 0:
            return
        item = self.list.item(row)
        if not item:
            return
        mid = item.data(Qt.UserRole)
        self._load_magic(int(mid))

    def _set_combo(self, cb: QComboBox, value: int) -> None:
        idx = cb.findData(value)
        if idx < 0:
            # allow unknown values
            cb.addItem(f"{value}: (未登记)", value)
            idx = cb.findData(value)
        cb.setCurrentIndex(max(0, idx))

    def _word(self, rec: list, index: int, default: int = 0) -> int:
        return rec[index] if 0 <= index < len(rec) else default

    def _load_magic(self, mid: int) -> None:
        arc = self.ctx.ranger
        if not arc or mid < 0 or mid >= arc.magics.count:
            return
        self._loading = True
        self._mid = mid
        rec = arc.magics.records[mid]
        words = len(rec)
        self.lbl_id.setText(f"ID: {mid}  (记录字宽 {words})")
        self.lbl_category.setText(f"类别: {category_display(rec)}")

        self.ed_name.setText(arc.magic_name(mid))
        self._set_combo(self.cb_type, self._word(rec, 12))
        self._set_combo(self.cb_hurt, self._word(rec, 14))
        self.sp_ami.setValue(self._word(rec, 13))
        self.sp_need_mp.setValue(self._word(rec, 16))
        self.sp_need_hp.setValue(self._word(rec, 7))
        self.sp_sound.setValue(self._word(rec, 11))
        self.sp_event.setValue(self._word(rec, 10))
        self.sp_max_lv.setValue(self._word(rec, 80))
        self.sp_poison.setValue(self._word(rec, 17))

        self.sp_min_hurt.setValue(self._word(rec, 18))
        self.sp_max_hurt.setValue(self._word(rec, 19))
        self.sp_hurt_mod.setValue(self._word(rec, 20))
        self._update_power_labels()

        self.sp_mod_att.setValue(self._word(rec, 21))
        self.sp_mod_mp.setValue(self._word(rec, 22))
        self.sp_mod_spd.setValue(self._word(rec, 23))
        self.sp_mod_wpn.setValue(self._word(rec, 24))
        self._update_mod_label()

        self._set_combo(self.cb_area, self._word(rec, 15))
        self.sp_min_step.setValue(self._word(rec, 8))
        self.sp_move1.setValue(self._word(rec, 28))
        self.sp_move10.setValue(self._word(rec, 37))
        self.sp_att1.setValue(self._word(rec, 38))
        self.sp_att10.setValue(self._word(rec, 47))

        self.sp_mp_scale.setValue(self._word(rec, 26))
        self.sp_hp_scale.setValue(self._word(rec, 27))

        self._set_combo(self.cb_battle, self._word(rec, 76))
        self.sp_need_exp0.setValue(self._word(rec, 77))
        self.sp_need_exp1.setValue(self._word(rec, 78))
        self.sp_need_exp2.setValue(self._word(rec, 79))
        for i in range(3):
            self.sp_add_hp[i].setValue(self._word(rec, 48 + i))
            self.sp_add_mp[i].setValue(self._word(rec, 51 + i))
            self.sp_add_att[i].setValue(self._word(rec, 54 + i))
            self.sp_add_def[i].setValue(self._word(rec, 57 + i))
            self.sp_add_spd[i].setValue(self._word(rec, 60 + i))
        self.sp_add_med.setValue(self._word(rec, 67))
        # words 68+ may be absent in classic (68-word) magic records
        self.sp_add_usepoi.setValue(self._word(rec, 68))
        self.sp_add_medpoi.setValue(self._word(rec, 69))
        self.sp_add_defpoi.setValue(self._word(rec, 70))
        self.sp_add_fist.setValue(self._word(rec, 71))
        self.sp_add_sword.setValue(self._word(rec, 72))
        self.sp_add_knife.setValue(self._word(rec, 73))
        self.sp_add_unusual.setValue(self._word(rec, 74))
        self.sp_add_hid.setValue(self._word(rec, 75))

        beyond = words <= 68
        self.sp_max_lv.setEnabled(not beyond)
        self.gongti_box.setEnabled(True)
        self.gongti_box.setTitle(
            "内功 / 功体属性"
            + ("（经典字宽：高位字段可能无效）" if beyond else "")
        )

        is_neigong = self._word(rec, 12) == 5
        if not beyond:
            self.gongti_box.setTitle(
                "内功 / 功体属性" + ("" if is_neigong else "（当前非内功，字段仍可改）")
            )

        self._loading = False
        self._refresh_eft_preview()

    def _update_power_labels(self) -> None:
        mn = self.sp_min_hurt.value()
        mx = self.sp_max_hurt.value()
        mod = self.sp_hurt_mod.value()
        p1 = cal_new_hurt_value(0, mn, mx, mod)
        p10 = cal_new_hurt_value(9, mn, mx, mod)
        self.lbl_p1.setText(str(p1))
        self.lbl_p10.setText(str(p10))
        rows = hurt_table(mn, mx, mod)
        # Compact table: Lv1=.. Lv2=.. …
        line1 = "  ".join(f"Lv{lv}={h}" for lv, h in rows[:5])
        line2 = "  ".join(f"Lv{lv}={h}" for lv, h in rows[5:])
        effective = 100 if mod == 0 else mod
        self.lbl_curve_table.setText(
            f"{line1}\n{line2}\n"
            f"(HurtModulus 实际参与计算值 = {effective}，p = {effective / 1000:g})"
        )

    def _update_mod_label(self) -> None:
        self.lbl_mod_sum.setText(
            dominant_modulus(
                self.sp_mod_att.value(),
                self.sp_mod_mp.value(),
                self.sp_mod_spd.value(),
                self.sp_mod_wpn.value(),
            )
        )

    def _on_field_changed(self, *_args) -> None:
        if self._loading:
            return
        self._update_power_labels()
        self._update_mod_label()
        # live category label
        mt = self.cb_type.currentData()
        ht = self.cb_hurt.currentData()
        fake = [0] * 111
        if mt is not None:
            fake[12] = int(mt)
        if ht is not None:
            fake[14] = int(ht)
        self.lbl_category.setText(f"类别: {category_display(fake)}")
        if self.sender() is self.sp_ami:
            self._refresh_eft_preview()

    def _refresh_eft_preview(self) -> None:
        ami = self.sp_ami.value()
        self.eft_preview.setText("加载中…")
        if not self.ctx.data_root or not self.ctx.profile:
            self.eft_preview.setText("无 data_root")
            return
        from kys_formats.assets import load_eft_preview_image, resolve_eft_pic_path

        assets = self.ctx.profile.assets
        try:
            if assets.eft_mode == "pic_file":
                path = resolve_eft_pic_path(self.ctx.data_root, assets, ami)
                if path is None:
                    self.eft_preview.setText(f"找不到\neft{ami:03d}.pic")
                    return
                if self._eft_ami != ami or self._eft_cache is None:
                    self._eft_cache = PicArchive()
                    self._eft_cache.load(path)
                    self._eft_ami = ami
                if self._eft_cache.count <= 0:
                    self.eft_preview.setText("空包")
                    return
                img = self._eft_cache.frames[0].to_image()
            else:
                img = load_eft_preview_image(self.ctx.data_root, assets, ami)
                if img is None and assets.eft_mode == "idx_grp":
                    self.eft_preview.setText(f"eft.idx/grp\n帧 {ami}\n(RLE 暂不预览)")
                    return
            if img is None:
                self.eft_preview.setText("空帧")
                return
            self.eft_preview.setPixmap(
                _pil_to_pixmap(img).scaled(150, 150, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        except Exception as e:
            self.eft_preview.setText(str(e))

    def _apply_current(self) -> None:
        arc = self.ctx.ranger
        if not arc or self._mid < 0:
            return
        mid = self._mid
        try:
            name = self.ed_name.text()
            arc.magics.set_name(mid, name, 1, 5)
            sets = {
                12: int(self.cb_type.currentData()),
                14: int(self.cb_hurt.currentData()),
                13: self.sp_ami.value(),
                16: self.sp_need_mp.value(),
                7: self.sp_need_hp.value(),
                11: self.sp_sound.value(),
                10: self.sp_event.value(),
                80: self.sp_max_lv.value(),
                17: self.sp_poison.value(),
                18: self.sp_min_hurt.value(),
                19: self.sp_max_hurt.value(),
                20: self.sp_hurt_mod.value(),
                21: self.sp_mod_att.value(),
                22: self.sp_mod_mp.value(),
                23: self.sp_mod_spd.value(),
                24: self.sp_mod_wpn.value(),
                15: int(self.cb_area.currentData()),
                8: self.sp_min_step.value(),
                28: self.sp_move1.value(),
                37: self.sp_move10.value(),
                38: self.sp_att1.value(),
                47: self.sp_att10.value(),
                26: self.sp_mp_scale.value(),
                27: self.sp_hp_scale.value(),
                76: int(self.cb_battle.currentData()),
                77: self.sp_need_exp0.value(),
                78: self.sp_need_exp1.value(),
                79: self.sp_need_exp2.value(),
                67: self.sp_add_med.value(),
                68: self.sp_add_usepoi.value(),
                69: self.sp_add_medpoi.value(),
                70: self.sp_add_defpoi.value(),
                71: self.sp_add_fist.value(),
                72: self.sp_add_sword.value(),
                73: self.sp_add_knife.value(),
                74: self.sp_add_unusual.value(),
                75: self.sp_add_hid.value(),
            }
            for i in range(3):
                sets[48 + i] = self.sp_add_hp[i].value()
                sets[51 + i] = self.sp_add_mp[i].value()
                sets[54 + i] = self.sp_add_att[i].value()
                sets[57 + i] = self.sp_add_def[i].value()
                sets[60 + i] = self.sp_add_spd[i].value()
            for w, v in sets.items():
                if 0 <= w < arc.magics.words:
                    arc.magics.set(mid, w, v)
            # Linear fill MoveDistance[1..8] / AttDistance[1..8] between Lv1 and Lv10
            m0, m9 = self.sp_move1.value(), self.sp_move10.value()
            a0, a9 = self.sp_att1.value(), self.sp_att10.value()
            for lv in range(10):
                if lv == 0:
                    mv, av = m0, a0
                elif lv == 9:
                    mv, av = m9, a9
                else:
                    mv = m0 + (m9 - m0) * lv // 9
                    av = a0 + (a9 - a0) * lv // 9
                if 28 + lv < arc.magics.words:
                    arc.magics.set(mid, 28 + lv, mv)
                if 38 + lv < arc.magics.words:
                    arc.magics.set(mid, 38 + lv, av)
            self.ctx.statusMessage.emit(f"武功 {mid} 已写入内存（请点「保存到磁盘」）")
            # refresh list item text
            self._rebuild_list()
        except Exception as e:
            QMessageBox.critical(self, "应用失败", str(e))
