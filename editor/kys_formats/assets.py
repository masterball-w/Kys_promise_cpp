"""Unified image access for heads / items / eft across game layouts."""

from __future__ import annotations

from pathlib import Path
from typing import List, Optional, Protocol, runtime_checkable

from .pic_png import PicArchive

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None  # type: ignore


@runtime_checkable
class ImageBank(Protocol):
    @property
    def count(self) -> int: ...

    def get_image(self, index: int): ...


class PicImageBank:
    """Frames inside a PNG-in-.Pic archive."""

    def __init__(self, archive: PicArchive) -> None:
        self.archive = archive

    @property
    def count(self) -> int:
        return self.archive.count

    def get_image(self, index: int):
        if index < 0 or index >= self.count:
            return None
        return self.archive.frames[index].to_image()


class PngDirImageBank:
    """Scattered `{id}.png` files under a directory (GodsDevils head/item)."""

    def __init__(self, directory: Path) -> None:
        self.directory = Path(directory)
        self._ids: List[int] = []
        if self.directory.is_dir():
            ids = []
            for p in self.directory.glob("*.png"):
                try:
                    ids.append(int(p.stem))
                except ValueError:
                    continue
            self._ids = sorted(ids)
        self._max = max(self._ids) + 1 if self._ids else 0

    @property
    def count(self) -> int:
        return self._max

    def get_image(self, index: int):
        if Image is None:
            raise RuntimeError("Pillow required")
        path = self.directory / f"{index}.png"
        if not path.is_file():
            return None
        return Image.open(path).convert("RGBA")


class EmptyImageBank:
    @property
    def count(self) -> int:
        return 0

    def get_image(self, index: int):
        return None


def load_heads_bank(data_root: Path, assets) -> ImageBank:
    root = Path(data_root)
    if assets.heads_mode == "pic":
        for rel in (assets.heads_pic, assets.heads_pic_alt):
            path = root / rel
            if path.is_file():
                pic = PicArchive()
                pic.load(path)
                return PicImageBank(pic)
    if assets.heads_mode == "png_dir":
        d = root / assets.heads_dir
        if d.is_dir():
            return PngDirImageBank(d)
    # fallback probe
    for rel in ("resource/Heads.Pic", "resource/heads.pic"):
        path = root / rel
        if path.is_file():
            pic = PicArchive()
            pic.load(path)
            return PicImageBank(pic)
    d = root / "head"
    if d.is_dir() and any(d.glob("*.png")):
        return PngDirImageBank(d)
    return EmptyImageBank()


def load_items_bank(data_root: Path, assets) -> ImageBank:
    root = Path(data_root)
    if assets.items_mode == "pic":
        for rel in (assets.items_pic, assets.items_pic_alt):
            path = root / rel
            if path.is_file():
                pic = PicArchive()
                pic.load(path)
                return PicImageBank(pic)
    if assets.items_mode == "png_dir":
        d = root / assets.items_dir
        if d.is_dir():
            return PngDirImageBank(d)
    for rel in ("resource/Items.Pic", "resource/items.pic"):
        path = root / rel
        if path.is_file():
            pic = PicArchive()
            pic.load(path)
            return PicImageBank(pic)
    d = root / "item"
    if d.is_dir() and any(d.glob("*.png")):
        return PngDirImageBank(d)
    return EmptyImageBank()


def resolve_eft_pic_path(data_root: Path, assets, ami: int) -> Optional[Path]:
    """Return path to eft AmiNum .pic when eft_mode is pic_file."""
    root = Path(data_root)
    if assets.eft_mode != "pic_file":
        return None
    for fmt in (assets.eft_pic_fmt, assets.eft_pic_fmt_alt):
        path = root / fmt.format(ami=ami)
        if path.is_file():
            return path
    return None


def load_eft_preview_image(data_root: Path, assets, ami: int):
    """Load first-frame (or single frame) preview for magic AmiNum."""
    root = Path(data_root)
    if assets.eft_mode == "pic_file":
        path = resolve_eft_pic_path(root, assets, ami)
        if path is None:
            return None
        pic = PicArchive()
        pic.load(path)
        if pic.count <= 0:
            return None
        return pic.frames[0].to_image()

    if assets.eft_mode == "idx_grp":
        # Classic eft.idx/grp is RLE/palette art — no PNG. Skip decode for now;
        # surface a clear absence rather than crashing.
        idx = root / assets.eft_idx
        grp = root / assets.eft_grp
        if idx.is_file() and grp.is_file():
            return None  # caller shows "需调色板解码"
        return None

    return None
