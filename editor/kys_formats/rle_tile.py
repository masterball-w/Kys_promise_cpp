"""RLE8 tile packs (smp/sdx, mmap, wmp) + palette."""

from __future__ import annotations

import struct
from pathlib import Path
from typing import List, Optional, Tuple

from .backup import atomic_write, backup_file

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None  # type: ignore


def load_palette(path: str | Path) -> List[Tuple[int, int, int]]:
    """Load MMAP.COL / pallet.col — 256 RGB triples, often 6-bit (*4)."""
    raw = Path(path).read_bytes()
    # Prefer first 768 bytes as one palette
    if len(raw) < 768:
        raise ValueError("palette too small")
    colors = []
    for i in range(256):
        r, g, b = raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2]
        if r <= 63 and g <= 63 and b <= 63:
            r, g, b = r * 4, g * 4, b * 4
        colors.append((r, g, b))
    return colors


class RleTilePack:
    def __init__(self) -> None:
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.offsets: List[int] = []
        self.tiles: List[bytes] = []  # raw rle blocks including header

    @property
    def count(self) -> int:
        return len(self.tiles)

    def load(self, idx_path: str | Path, grp_path: str | Path) -> None:
        self.idx_path = Path(idx_path)
        self.grp_path = Path(grp_path)
        idx = self.idx_path.read_bytes()
        grp = self.grp_path.read_bytes()
        self.offsets = list(struct.unpack(f"<{len(idx)//4}i", idx))
        self.tiles = []
        for i, off in enumerate(self.offsets):
            end = self.offsets[i + 1] if i + 1 < len(self.offsets) else len(grp)
            if off < 0 or end > len(grp) or end < off:
                self.tiles.append(b"")
            else:
                self.tiles.append(grp[off:end])

    def decode_tile(
        self, index: int, palette: List[Tuple[int, int, int]]
    ):
        """Return PIL Image or None."""
        if Image is None:
            raise RuntimeError("Pillow required")
        block = self.tiles[index]
        if len(block) < 8:
            return None
        w, h, xs, ys = struct.unpack_from("<hhhh", block, 0)
        if w <= 0 or h <= 0 or w > 512 or h > 512:
            return None
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        pixels = img.load()
        pos = 8
        for y in range(h):
            if pos >= len(block):
                break
            # Pascal RLE: each row starts with a length or packet stream
            # Simplified: read until row filled
            x = 0
            while x < w and pos < len(block):
                skip = block[pos]
                pos += 1
                x += skip
                if x >= w or pos >= len(block):
                    break
                run = block[pos]
                pos += 1
                for _ in range(run):
                    if pos >= len(block) or x >= w:
                        break
                    idx = block[pos]
                    pos += 1
                    if 0 <= idx < len(palette):
                        r, g, b = palette[idx]
                        pixels[x, y] = (r, g, b, 255)
                    x += 1
        return img

    def to_bytes(self) -> tuple[bytes, bytes]:
        offsets = []
        grp = bytearray()
        for t in self.tiles:
            offsets.append(len(grp))
            grp.extend(t)
        idx = struct.pack(f"<{len(offsets)}i", *offsets) if offsets else b""
        return idx, bytes(grp)

    def save(self, backup: bool = True) -> None:
        if not self.idx_path or not self.grp_path:
            raise RuntimeError("not loaded")
        idx, grp = self.to_bytes()
        if backup:
            backup_file(self.idx_path)
            backup_file(self.grp_path)
        atomic_write(self.idx_path, idx)
        atomic_write(self.grp_path, grp)

    def replace_raw(self, index: int, block: bytes) -> None:
        self.tiles[index] = block
