"""Picture / tile asset editor with index preview."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QListWidget, QListWidgetItem,
    QPushButton, QLabel, QFileDialog, QMessageBox, QComboBox, QSpinBox,
    QFormLayout, QInputDialog,
)

from kys_formats.pic_png import PicArchive
from kys_formats.rle_tile import RleTilePack, load_palette
from ui.context import EditorContext


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
        self.list = QListWidget()
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

    def load_path(self, path: Path) -> None:
        self.path = path
        self.archive = PicArchive()
        self.archive.load(path)
        self.list.clear()
        for i in range(self.archive.count):
            self.list.addItem(QListWidgetItem(f"{i}"))
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
            self._preview(row)

    def append_frame(self) -> None:
        if not self.archive:
            QMessageBox.warning(self, "追加", "请先打开 .Pic")
            return
        path, _ = QFileDialog.getOpenFileName(self, "选择 PNG", "", "PNG (*.png)")
        if path:
            idx = self.archive.append_frame(path)
            self.list.addItem(QListWidgetItem(f"{idx}"))
            self.list.setCurrentRow(idx)

    def delete_frame(self) -> None:
        row = self.list.currentRow()
        if not self.archive or row < 0:
            return
        self.archive.delete_frame(row)
        self.list.takeItem(row)

    def save_file(self) -> None:
        if not self.archive:
            return
        try:
            self.archive.save(backup=True)
            QMessageBox.information(self, "保存", f"已保存 {self.archive.path}")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))


class AssetEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        layout = QVBoxLayout(self)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)

        # Quick open common packs
        quick = QWidget()
        qlay = QVBoxLayout(quick)
        row = QHBoxLayout()
        for label, name in [
            ("Heads.Pic", "Heads.Pic"),
            ("Items.Pic", "Items.Pic"),
            ("Begin.Pic", "Begin.Pic"),
            ("Background.Pic", "Background.Pic"),
        ]:
            b = QPushButton(f"打开 {label}")
            b.clicked.connect(lambda _=False, n=name: self._open_named(n))
            row.addWidget(b)
        qlay.addLayout(row)
        self.heads_panel = PicPackPanel(ctx, "Heads")
        qlay.addWidget(self.heads_panel)
        self.tabs.addTab(quick, "常用贴图包")

        self.items_panel = PicPackPanel(ctx, "Items")
        self.tabs.addTab(self.items_panel, "物品图")

        # Fight actions
        fight = QWidget()
        flay = QFormLayout(fight)
        self.fight_head = QSpinBox(); self.fight_head.setRange(0, 999)
        self.fight_mode = QSpinBox(); self.fight_mode.setRange(0, 4)
        flay.addRow("HeadNum 目录", self.fight_head)
        flay.addRow("mode (武功类型)", self.fight_mode)
        open_fight = QPushButton("打开 fight/NNN/MM.pic")
        open_fight.clicked.connect(self._open_fight)
        flay.addRow(open_fight)
        self.fight_panel = PicPackPanel(ctx, "Fight")
        flay.addRow(self.fight_panel)
        self.tabs.addTab(fight, "战斗动作")

        # EFT
        eft = QWidget()
        elay = QFormLayout(eft)
        self.eft_num = QSpinBox(); self.eft_num.setRange(0, 999)
        elay.addRow("AmiNum", self.eft_num)
        open_eft = QPushButton("打开 eft/eftNNN.pic")
        open_eft.clicked.connect(self._open_eft)
        elay.addRow(open_eft)
        link = QPushButton("从当前存档武功 AmiNum 填充")
        link.clicked.connect(self._fill_ami_from_magic)
        elay.addRow(link)
        self.eft_panel = PicPackPanel(ctx, "Eft")
        elay.addRow(self.eft_panel)
        self.tabs.addTab(eft, "特效")

        # RLE tiles
        tile = QWidget()
        tlay = QVBoxLayout(tile)
        brow = QHBoxLayout()
        open_smp = QPushButton("打开 smp/sdx")
        open_smp.clicked.connect(self._open_smp)
        brow.addWidget(open_smp)
        tlay.addLayout(brow)
        self.tile_list = QListWidget()
        self.tile_list.currentRowChanged.connect(self._preview_tile)
        tlay.addWidget(self.tile_list)
        self.tile_preview = QLabel("砖预览")
        self.tile_preview.setMinimumHeight(128)
        self.tile_preview.setAlignment(Qt.AlignCenter)
        self.tile_preview.setStyleSheet("background:#111;")
        tlay.addWidget(self.tile_preview)
        self.tabs.addTab(tile, "场景砖(RLE)")
        self._tile_pack: RleTilePack | None = None
        self._palette = None

        # Index linker
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
            "物品图标索引通常等于物品 ID（Items.Pic）。\n"
            "武功 AmiNum 对应 eft/eft{AmiNum:03d}.pic。\n"
            "DData[5] 为场景事件贴图（常为 pic×2）。"
        )
        hint.setWordWrap(True)
        llay.addRow(hint)
        self.tabs.addTab(link_tab, "索引联动")

        ctx.dataRootChanged.connect(lambda _: self._autoload_common())

    def _autoload_common(self) -> None:
        if not self.ctx.data_root:
            return
        heads = self.ctx.resource_dir / "Heads.Pic"
        if heads.is_file():
            self.heads_panel.load_path(heads)
        items = self.ctx.resource_dir / "Items.Pic"
        if items.is_file():
            self.items_panel.load_path(items)

    def _open_named(self, name: str) -> None:
        if not self.ctx.data_root:
            QMessageBox.warning(self, "打开", "请先选择 game_data 目录")
            return
        path = self.ctx.resource_dir / name
        if not path.is_file():
            QMessageBox.warning(self, "打开", f"找不到 {path}")
            return
        self.heads_panel.load_path(path)
        self.tabs.setCurrentIndex(0)

    def _open_fight(self) -> None:
        if not self.ctx.data_root:
            return
        path = (
            self.ctx.data_root
            / "fight"
            / f"{self.fight_head.value():03d}"
            / f"{self.fight_mode.value():02d}.pic"
        )
        if not path.is_file():
            QMessageBox.warning(self, "打开", f"找不到 {path}")
            return
        self.fight_panel.load_path(path)

    def _open_eft(self) -> None:
        if not self.ctx.data_root:
            return
        path = self.ctx.data_root / "eft" / f"eft{self.eft_num.value():03d}.pic"
        if not path.is_file():
            # try without zero pad variants
            alt = self.ctx.data_root / "eft" / f"eft{self.eft_num.value()}.pic"
            path = alt if alt.is_file() else path
        if not path.is_file():
            QMessageBox.warning(self, "打开", f"找不到 {path}")
            return
        self.eft_panel.load_path(path)

    def _fill_ami_from_magic(self) -> None:
        if not self.ctx.ranger:
            return
        mid, ok = QInputDialog.getInt(self, "武功", "Magic ID", 0, 0, self.ctx.ranger.magics.count - 1)
        if not ok:
            return
        ami = self.ctx.ranger.magics.get(mid, 13)
        self.eft_num.setValue(ami)
        self._open_eft()

    def _open_smp(self) -> None:
        if not self.ctx.data_root:
            return
        res = self.ctx.resource_dir
        idx = res / "sdx"
        grp = res / "smp"
        if not idx.is_file() or not grp.is_file():
            QMessageBox.warning(self, "打开", "需要 resource/sdx 与 resource/smp")
            return
        pal_path = res / "MMAP.COL"
        if not pal_path.is_file():
            pal_path = res / "pallet.col"
        try:
            self._palette = load_palette(pal_path) if pal_path.is_file() else None
            self._tile_pack = RleTilePack()
            self._tile_pack.load(idx, grp)
            self.tile_list.clear()
            for i in range(self._tile_pack.count):
                self.tile_list.addItem(str(i))
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    def _preview_tile(self, row: int) -> None:
        if not self._tile_pack or not self._palette or row < 0:
            return
        try:
            img = self._tile_pack.decode_tile(row, self._palette)
            if img is None:
                self.tile_preview.setText("无法解码")
                return
            self.tile_preview.setPixmap(
                pil_to_pixmap(img).scaled(128, 128, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        except Exception as e:
            self.tile_preview.setText(str(e))

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
                img = self.ctx.heads.frames[hid].to_image()
                if img:
                    self.link_preview.setPixmap(
                        pil_to_pixmap(img).scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                    )
            except Exception:
                pass
        self.ctx.statusMessage.emit(f"角色 {rid} HeadNum -> {hid}（记得在存档页保存）")
