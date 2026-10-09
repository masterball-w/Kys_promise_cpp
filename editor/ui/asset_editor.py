"""Picture / tile asset editor with index preview."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QListWidgetItem,
    QPushButton, QLabel, QFileDialog, QMessageBox, QComboBox, QSpinBox,
    QFormLayout, QInputDialog, QCheckBox,
)

from kys_formats.pic_png import PicArchive
from kys_formats.rle_tile import (
    RleTilePack,
    load_palette,
    find_palette,
    parse_tile_filename,
    load_tile_pack_pair,
)
from ui.context import EditorContext
from ui.thumb_list import LazyThumbList

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None  # type: ignore


def pil_to_pixmap(img) -> QPixmap:
    data = img.convert("RGBA").tobytes("raw", "RGBA")
    qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)


class PicPackPanel(QWidget):
    def __init__(self, ctx: EditorContext, title: str) -> None:
        super().__init__()
        self.ctx = ctx
        self.title = title
        self.archive: PicArchive | None = None
        self.path: Path | None = None
        lay = QHBoxLayout(self)
        left = QVBoxLayout()
        thumb_row = QHBoxLayout()
        self.chk_thumbs = QCheckBox("缩略图")
        self.chk_thumbs.setChecked(True)
        self.chk_thumbs.setToolTip("仅加载可见行缩略图，带缓存上限，可关闭以省内存")
        self.chk_thumbs.toggled.connect(self._on_thumbs_toggled)
        thumb_row.addWidget(self.chk_thumbs)
        thumb_row.addWidget(QLabel("尺寸"))
        self.sp_thumb = QSpinBox()
        self.sp_thumb.setRange(32, 96)
        self.sp_thumb.setValue(48)
        self.sp_thumb.valueChanged.connect(self._on_thumb_size)
        thumb_row.addWidget(self.sp_thumb)
        thumb_row.addStretch()
        left.addLayout(thumb_row)
        self.list = LazyThumbList(thumb_size=48, cache_limit=280)
        self.list.set_thumb_loader(self._load_thumb)
        self.list.currentRowChanged.connect(self._preview)
        left.addWidget(self.list)
        btns = QHBoxLayout()
        for text, slot in [
            ("打开", self.open_file),
            ("导出帧", self.export_frame),
            ("替换帧", self.replace_frame),
            ("追加帧", self.append_frame),
            ("删除帧", self.delete_frame),
            ("保存", self.save_file),
        ]:
            b = QPushButton(text)
            b.clicked.connect(slot)
            btns.addWidget(b)
        left.addLayout(btns)
        lay.addLayout(left, 1)
        self.preview = QLabel("预览")
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setMinimumSize(256, 256)
        self.preview.setStyleSheet("background:#111;color:#888;")
        lay.addWidget(self.preview, 2)

    def _on_thumbs_toggled(self, on: bool) -> None:
        self.list.set_thumbs_enabled(on)

    def _on_thumb_size(self, v: int) -> None:
        self.list.set_thumb_size(v)

    def _load_thumb(self, index: int) -> QPixmap | None:
        if not self.archive or index < 0 or index >= self.archive.count:
            return None
        try:
            img = self.archive.frames[index].to_image()
        except Exception:
            return None
        if img is None:
            return None
        return pil_to_pixmap(img)

    def load_path(self, path: Path) -> None:
        self.path = path
        self.archive = PicArchive()
        self.archive.load(path)
        self.list.rebuild_items([str(i) for i in range(self.archive.count)])
        self.ctx.statusMessage.emit(f"已打开 {path.name} ({self.archive.count} 帧)")

    def open_file(self) -> None:
        start = str(self.ctx.resource_dir) if self.ctx.data_root else ""
        path, _ = QFileDialog.getOpenFileName(self, "打开 .Pic", start, "Pic (*.Pic *.pic)")
        if path:
            self.load_path(Path(path))

    def _preview(self, row: int) -> None:
        if not self.archive or row < 0 or row >= self.archive.count:
            return
        try:
            img = self.archive.frames[row].to_image()
            if img is None:
                self.preview.setText("空帧")
                return
            self.preview.setPixmap(
                pil_to_pixmap(img).scaled(240, 240, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        except Exception as e:
            self.preview.setText(str(e))

    def export_frame(self) -> None:
        row = self.list.currentRow()
        if not self.archive or row < 0:
            return
        path, _ = QFileDialog.getSaveFileName(self, "导出 PNG", f"frame_{row}.png", "PNG (*.png)")
        if path:
            self.archive.export_frame(row, path)

    def replace_frame(self) -> None:
        row = self.list.currentRow()
        if not self.archive or row < 0:
            return
        path, _ = QFileDialog.getOpenFileName(self, "选择 PNG", "", "PNG (*.png)")
        if path:
            self.archive.replace_frame(row, path)
            self.list.invalidate(row)
            self._preview(row)

    def append_frame(self) -> None:
        if not self.archive:
            QMessageBox.warning(self, "追加", "请先打开 .Pic")
            return
        path, _ = QFileDialog.getOpenFileName(self, "选择 PNG", "", "PNG (*.png)")
        if path:
            idx = self.archive.append_frame(path)
            self.list.rebuild_items([str(i) for i in range(self.archive.count)])
            self.list.setCurrentRow(idx)

    def delete_frame(self) -> None:
        row = self.list.currentRow()
        if not self.archive or row < 0:
            return
        self.archive.delete_frame(row)
        self.list.rebuild_items([str(i) for i in range(self.archive.count)])

    def save_file(self) -> None:
        if not self.archive:
            return
        try:
            self.archive.save(backup=True)
            QMessageBox.information(self, "保存", f"已保存 {self.archive.path}")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))


class RleTilePanel(QWidget):
    """Edit mmap / smp / wmp RLE tile packs with single & batch PNG I/O."""

    PACKS = (
        ("mmap（大地图/建筑）", ("mmap.idx", "MMAP.idx", "Mmap.idx"), ("mmap.grp", "MMAP.grp", "Mmap.grp")),
        ("smp（场景砖）", ("sdx", "SDX", "Sdx"), ("smp", "SMP", "Smp")),
        ("wmp（战场砖）", ("wdx", "WDX", "Wdx"), ("wmp", "WMP", "Wmp")),
    )

    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self.pack: RleTilePack | None = None
        self.palette = None
        self._kind = "mmap"

        lay = QVBoxLayout(self)
        top = QHBoxLayout()
        top.addWidget(QLabel("砖库"))
        self.pack_combo = QComboBox()
        for label, _i, _g in self.PACKS:
            self.pack_combo.addItem(label)
        self.pack_combo.currentIndexChanged.connect(self._load_selected_pack)
        top.addWidget(self.pack_combo)
        btn_reload = QPushButton("重新加载")
        btn_reload.clicked.connect(self._load_selected_pack)
        top.addWidget(btn_reload)
        self.chk_thumbs = QCheckBox("缩略图")
        self.chk_thumbs.setChecked(True)
        self.chk_thumbs.setToolTip("仅加载可见行缩略图，带缓存上限，可关闭以省内存")
        self.chk_thumbs.toggled.connect(self._on_thumbs_toggled)
        top.addWidget(self.chk_thumbs)
        top.addWidget(QLabel("尺寸"))
        self.sp_thumb = QSpinBox()
        self.sp_thumb.setRange(32, 96)
        self.sp_thumb.setValue(48)
        self.sp_thumb.valueChanged.connect(self._on_thumb_size)
        top.addWidget(self.sp_thumb)
        top.addStretch()
        lay.addLayout(top)

        body = QHBoxLayout()
        left = QVBoxLayout()
        self.list = LazyThumbList(thumb_size=48, cache_limit=320)
        self.list.set_thumb_loader(self._load_thumb)
        self.list.currentRowChanged.connect(self._preview)
        left.addWidget(self.list)
        btns = QHBoxLayout()
        for text, slot in [
            ("导出当前", self.export_one),
            ("导入替换", self.import_one),
            ("追加 PNG", self.append_one),
            ("批量导出", self.export_batch),
            ("批量导入", self.import_batch),
            ("保存砖库", self.save_pack),
        ]:
            b = QPushButton(text)
            b.clicked.connect(slot)
            btns.addWidget(b)
        left.addLayout(btns)
        body.addLayout(left, 1)
        right = QVBoxLayout()
        self.preview = QLabel("砖预览")
        self.preview.setMinimumSize(256, 256)
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setStyleSheet("background:#111;color:#888;")
        right.addWidget(self.preview)
        self.lbl_info = QLabel(
            "列表缩略图按可见区域懒加载（约 300 张缓存）。\n"
            "PNG 导入会按调色板量化并编码为 RLE8。\n"
            "批量导入文件名：00012.png / tile_12.png / mmap_12.png。\n"
            "大地图建筑层使用 mmap 砖号×2 作为贴图码。"
        )
        self.lbl_info.setWordWrap(True)
        right.addWidget(self.lbl_info)
        right.addStretch()
        body.addLayout(right, 1)
        lay.addLayout(body)

        ctx.dataRootChanged.connect(lambda _: self._load_selected_pack())

    def _on_thumbs_toggled(self, on: bool) -> None:
        self.list.set_thumbs_enabled(on)

    def _on_thumb_size(self, v: int) -> None:
        self.list.set_thumb_size(v)

    def _load_thumb(self, index: int) -> QPixmap | None:
        if not self.pack or not self.palette or index < 0:
            return None
        if index >= self.pack.count or not self.pack.tiles[index]:
            return None
        try:
            img = self.pack.decode_tile(index, self.palette, use_cache=True)
        except Exception:
            return None
        if img is None:
            return None
        return pil_to_pixmap(img)

    def _labels(self) -> list[str]:
        if not self.pack:
            return []
        return [
            f"{i}" + (" (空)" if not self.pack.tiles[i] else "")
            for i in range(self.pack.count)
        ]

    def _load_selected_pack(self) -> None:
        if not self.ctx.data_root:
            return
        idx = self.pack_combo.currentIndex()
        if idx < 0 or idx >= len(self.PACKS):
            return
        label, idx_names, grp_names = self.PACKS[idx]
        self._kind = label.split("（", 1)[0]
        res = self.ctx.resource_dir
        pal_path = find_palette(res)
        try:
            self.palette = load_palette(pal_path) if pal_path else self.ctx.palette
            pack = load_tile_pack_pair(res, idx_names, grp_names)
            if pack is None:
                self.pack = None
                self.list.rebuild_items([])
                self.preview.setText(f"未找到 {label}")
                return
            self.pack = pack
            if idx == 0 and self.ctx.mmap_tiles:
                self.pack = self.ctx.mmap_tiles
            elif idx == 1 and self.ctx.scene_tiles:
                self.pack = self.ctx.scene_tiles
            elif idx == 2 and self.ctx.battle_tiles:
                self.pack = self.ctx.battle_tiles
            self.list.rebuild_items(self._labels())
            self.ctx.statusMessage.emit(f"已加载 {label}：{self.pack.count} 块")
            if self.list.count():
                self.list.setCurrentRow(0)
        except Exception as e:
            QMessageBox.critical(self, "加载失败", str(e))

    def _preview(self, row: int) -> None:
        if not self.pack or not self.palette or row < 0:
            return
        try:
            img = self.pack.decode_tile(row, self.palette)
            if img is None:
                self.preview.setText("空/无法解码")
                return
            xs, ys = self.pack.get_hotspot(row)
            self.preview.setPixmap(
                pil_to_pixmap(img).scaled(240, 240, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
            self.lbl_info.setText(
                f"索引 {row}  尺寸 {img.width}×{img.height}  热点 ({xs},{ys})\n"
                f"引擎贴图码参考：{row * 2}\n"
                "列表缩略图懒加载；PNG 导入按调色板量化为 RLE8。"
            )
        except Exception as e:
            self.preview.setText(str(e))

    def export_one(self) -> None:
        row = self.list.currentRow()
        if not self.pack or not self.palette or row < 0:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "导出 PNG", f"{self._kind}_{row:04d}.png", "PNG (*.png)"
        )
        if not path:
            return
        try:
            self.pack.export_png(row, path, self.palette)
            self.ctx.statusMessage.emit(f"已导出 {path}")
        except Exception as e:
            QMessageBox.critical(self, "导出失败", str(e))

    def import_one(self) -> None:
        row = self.list.currentRow()
        if not self.pack or not self.palette or row < 0:
            return
        if Image is None:
            QMessageBox.warning(self, "导入", "需要 Pillow")
            return
        path, _ = QFileDialog.getOpenFileName(self, "选择 PNG", "", "PNG (*.png)")
        if not path:
            return
        try:
            img = Image.open(path)
            self.pack.replace_from_image(row, img, self.palette)
            self.list.rebuild_items(self._labels())
            self.list.setCurrentRow(row)
            self._preview(row)
            self.ctx.statusMessage.emit(f"已替换砖 {row}（记得保存砖库）")
        except Exception as e:
            QMessageBox.critical(self, "导入失败", str(e))

    def append_one(self) -> None:
        if not self.pack or not self.palette:
            return
        if Image is None:
            QMessageBox.warning(self, "追加", "需要 Pillow")
            return
        path, _ = QFileDialog.getOpenFileName(self, "选择 PNG", "", "PNG (*.png)")
        if not path:
            return
        try:
            img = Image.open(path)
            idx = self.pack.append_from_image(img, self.palette)
            self.list.rebuild_items(self._labels())
            self.list.setCurrentRow(idx)
            self.ctx.statusMessage.emit(f"已追加砖 {idx}")
        except Exception as e:
            QMessageBox.critical(self, "追加失败", str(e))

    def export_batch(self) -> None:
        if not self.pack or not self.palette:
            return
        folder = QFileDialog.getExistingDirectory(self, "选择导出目录")
        if not folder:
            return
        out = Path(folder)
        ok = fail = 0
        for i in range(self.pack.count):
            if not self.pack.tiles[i]:
                continue
            try:
                self.pack.export_png(i, out / f"{self._kind}_{i:04d}.png", self.palette)
                ok += 1
            except Exception:
                fail += 1
        QMessageBox.information(self, "批量导出", f"成功 {ok}，失败/跳过 {fail}\n目录：{out}")

    def import_batch(self) -> None:
        if not self.pack or not self.palette:
            return
        if Image is None:
            QMessageBox.warning(self, "导入", "需要 Pillow")
            return
        folder = QFileDialog.getExistingDirectory(self, "选择含 PNG 的目录")
        if not folder:
            return
        files = sorted(Path(folder).glob("*.png")) + sorted(Path(folder).glob("*.PNG"))
        ok = skip = 0
        errors: list[str] = []
        for fp in files:
            idx = parse_tile_filename(fp.name)
            if idx is None:
                skip += 1
                continue
            try:
                img = Image.open(fp)
                if idx >= self.pack.count:
                    while self.pack.count <= idx:
                        self.pack.append_raw(b"")
                self.pack.replace_from_image(idx, img, self.palette)
                ok += 1
            except Exception as e:
                errors.append(f"{fp.name}: {e}")
        cur = self.list.currentRow()
        self.list.rebuild_items(self._labels())
        if 0 <= cur < self.list.count():
            self.list.setCurrentRow(cur)
        msg = f"导入 {ok} 张，跳过 {skip}"
        if errors:
            msg += "\n" + "\n".join(errors[:8])
        QMessageBox.information(self, "批量导入", msg + "\n记得点「保存砖库」")

    def save_pack(self) -> None:
        if not self.pack:
            return
        try:
            self.pack.save(backup=True)
            QMessageBox.information(
                self,
                "保存",
                f"已保存\n{self.pack.idx_path}\n{self.pack.grp_path}",
            )
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))


class AssetEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        layout = QVBoxLayout(self)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)

        quick = QWidget()
        qlay = QVBoxLayout(quick)
        row = QHBoxLayout()
        self._quick_buttons: list[QPushButton] = []
        for label, name in [
            ("Heads.Pic", "Heads.Pic"),
            ("Items.Pic", "Items.Pic"),
            ("Begin.Pic", "Begin.Pic"),
            ("Background.Pic", "Background.Pic"),
        ]:
            b = QPushButton(f"打开 {label}")
            b.clicked.connect(lambda _=False, n=name: self._open_named(n))
            row.addWidget(b)
            self._quick_buttons.append(b)
        qlay.addLayout(row)
        self.heads_panel = PicPackPanel(ctx, "Heads")
        qlay.addWidget(self.heads_panel)
        self.png_hint = QLabel("")
        self.png_hint.setWordWrap(True)
        qlay.addWidget(self.png_hint)
        self.tabs.addTab(quick, "常用贴图包")

        self.items_panel = PicPackPanel(ctx, "Items")
        self.tabs.addTab(self.items_panel, "物品图")

        fight = QWidget()
        flay = QFormLayout(fight)
        self.fight_head = QSpinBox(); self.fight_head.setRange(0, 999)
        self.fight_mode = QSpinBox(); self.fight_mode.setRange(0, 4)
        flay.addRow("HeadNum 目录", self.fight_head)
        flay.addRow("mode (武功类型)", self.fight_mode)
        open_fight = QPushButton("打开战斗贴图")
        open_fight.clicked.connect(self._open_fight)
        flay.addRow(open_fight)
        self.fight_panel = PicPackPanel(ctx, "Fight")
        flay.addRow(self.fight_panel)
        self.tabs.addTab(fight, "战斗动作")

        eft = QWidget()
        elay = QFormLayout(eft)
        self.eft_num = QSpinBox(); self.eft_num.setRange(0, 999)
        elay.addRow("AmiNum", self.eft_num)
        open_eft = QPushButton("打开特效贴图")
        open_eft.clicked.connect(self._open_eft)
        elay.addRow(open_eft)
        link = QPushButton("从当前存档武功 AmiNum 填充")
        link.clicked.connect(self._fill_ami_from_magic)
        elay.addRow(link)
        self.eft_panel = PicPackPanel(ctx, "Eft")
        elay.addRow(self.eft_panel)
        self.tabs.addTab(eft, "特效")

        self.rle_panel = RleTilePanel(ctx)
        self.tabs.addTab(self.rle_panel, "RLE砖库(mmap/场景/战场)")

        link_tab = QWidget()
        llay = QFormLayout(link_tab)
        self.link_role = QSpinBox(); self.link_role.setRange(0, 999)
        self.link_head = QSpinBox(); self.link_head.setRange(0, 999)
        apply_head = QPushButton("写入角色 HeadNum 并预览")
        apply_head.clicked.connect(self._apply_role_head)
        llay.addRow("角色 ID", self.link_role)
        llay.addRow("HeadNum", self.link_head)
        llay.addRow(apply_head)
        self.link_preview = QLabel()
        self.link_preview.setFixedSize(128, 128)
        self.link_preview.setStyleSheet("background:#222;")
        llay.addRow("预览", self.link_preview)
        hint = QLabel(
            "物品/头像索引通常等于对应 ID。\n"
            "前传：Items.Pic / Heads.Pic / fight/NNN/MM.pic / eft/eftNNN.pic\n"
            "经典：item/*.png / head/*.png / fight/fightNNN.grp / resource/eft.idx"
        )
        hint.setWordWrap(True)
        llay.addRow(hint)
        self.tabs.addTab(link_tab, "索引联动")

        ctx.dataRootChanged.connect(lambda _: self._autoload_common())

    def _autoload_common(self) -> None:
        if not self.ctx.data_root:
            return
        assets = self.ctx.profile.assets if self.ctx.profile else None
        if assets and assets.heads_mode == "png_dir":
            for b in self._quick_buttons:
                b.setEnabled(False)
            self.png_hint.setText(
                f"当前配置档使用散图：{assets.heads_dir}/{{id}}.png 、"
                f"{assets.items_dir}/{{id}}.png（请在资源管理器中直接替换 PNG）"
            )
            return
        for b in self._quick_buttons:
            b.setEnabled(True)
        self.png_hint.setText("")
        heads = self.ctx.resource_dir / "Heads.Pic"
        if heads.is_file():
            self.heads_panel.load_path(heads)
        items = self.ctx.resource_dir / "Items.Pic"
        if items.is_file():
            self.items_panel.load_path(items)

    def _open_named(self, name: str) -> None:
        if not self.ctx.data_root:
            QMessageBox.warning(self, "打开", "请先选择数据根目录")
            return
        path = self.ctx.resource_dir / name
        if not path.is_file():
            QMessageBox.warning(self, "打开", f"找不到 {path}")
            return
        self.heads_panel.load_path(path)
        self.tabs.setCurrentIndex(0)

    def _open_fight(self) -> None:
        if not self.ctx.data_root or not self.ctx.profile:
            return
        assets = self.ctx.profile.assets
        head = self.fight_head.value()
        mode = self.fight_mode.value()
        if assets.fight_mode == "pic_tree":
            path = self.ctx.data_root / assets.fight_pic_fmt.format(head=head, mode=mode)
            if not path.is_file():
                QMessageBox.warning(self, "打开", f"找不到 {path}")
                return
            self.fight_panel.load_path(path)
            return
        if assets.fight_mode == "idx_grp":
            path = self.ctx.data_root / assets.fight_grp_fmt.format(head=head)
            QMessageBox.information(
                self,
                "战斗贴图",
                f"当前为 idx+grp 包：\n{path}\n"
                f"（以及对应 .idx）\n"
                "经典 RLE 战斗包暂不支持在此面板编辑，请用外部工具。",
            )
            return
        QMessageBox.warning(self, "打开", "当前配置档未定义战斗贴图布局")

    def _open_eft(self) -> None:
        if not self.ctx.data_root or not self.ctx.profile:
            return
        assets = self.ctx.profile.assets
        ami = self.eft_num.value()
        if assets.eft_mode == "pic_file":
            path = self.ctx.data_root / assets.eft_pic_fmt.format(ami=ami)
            if not path.is_file():
                alt = self.ctx.data_root / assets.eft_pic_fmt_alt.format(ami=ami)
                path = alt if alt.is_file() else path
            if not path.is_file():
                QMessageBox.warning(self, "打开", f"找不到 {path}")
                return
            self.eft_panel.load_path(path)
            return
        if assets.eft_mode == "idx_grp":
            QMessageBox.information(
                self,
                "特效",
                f"当前为 resource/eft.idx + eft.grp（AmiNum={ami} 对应帧号）。\n"
                "经典 RLE 特效包暂不支持在此面板编辑。",
            )
            return
        QMessageBox.warning(self, "打开", "当前配置档未定义特效布局")

    def _fill_ami_from_magic(self) -> None:
        if not self.ctx.ranger:
            return
        mid, ok = QInputDialog.getInt(self, "武功", "Magic ID", 0, 0, self.ctx.ranger.magics.count - 1)
        if not ok:
            return
        ami = self.ctx.ranger.magics.get(mid, 13)
        self.eft_num.setValue(ami)
        self._open_eft()

    def _apply_role_head(self) -> None:
        if not self.ctx.ranger:
            QMessageBox.warning(self, "联动", "请先加载存档")
            return
        rid = self.link_role.value()
        hid = self.link_head.value()
        if rid >= self.ctx.ranger.roles.count:
            QMessageBox.warning(self, "联动", "角色 ID 越界")
            return
        self.ctx.ranger.roles.set(rid, 1, hid)
        if self.ctx.heads and 0 <= hid < self.ctx.heads.count:
            try:
                img = self.ctx.heads.get_image(hid)
                if img:
                    self.link_preview.setPixmap(
                        pil_to_pixmap(img).scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                    )
            except Exception:
                pass
        self.ctx.statusMessage.emit(f"角色 {rid} HeadNum -> {hid}（记得在存档页保存）")
