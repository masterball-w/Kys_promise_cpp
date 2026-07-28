"""RLE8 tile packs (smp/sdx, mmap, wmp) + palette.

KYS-family idx files for scene/battle/mmap tiles store **end offsets**
(cumulative), matching warfld.idx. Empty entries may be 0 or equal to prev.
"""

from __future__ import annotations

import struct
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .backup import atomic_write, backup_file

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None  # type: ignore


def load_palette(path: str | Path) -> List[Tuple[int, int, int]]:
    """Load MMAP.COL / pallet.col — 256 RGB triples, often 6-bit (*4)."""
    raw = Path(path).read_bytes()
    if len(raw) < 768:
        raise ValueError("palette too small")
    colors = []
    for i in range(256):
        r, g, b = raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2]
        if r <= 63 and g <= 63 and b <= 63:
            r, g, b = r * 4, g * 4, b * 4
        colors.append((r, g, b))
    return colors


def find_palette(resource_dir: Path) -> Optional[Path]:
    for name in ("mmap.col", "MMAP.COL", "pallet.col", "Pallet.col"):
        p = resource_dir / name
        if p.is_file():
            return p
    return None


class RleTilePack:
    def __init__(self) -> None:
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.offsets: List[int] = []  # end-offsets as on disk
        self.tiles: List[bytes] = []
        self._color_cache: Dict[int, Tuple[int, int, int]] = {}
        self._img_cache: Dict[int, object] = {}

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
        self._color_cache.clear()
        self._img_cache.clear()
        prev = 0
        for end in self.offsets:
            if end <= 0 or end < prev or end > len(grp):
                self.tiles.append(b"")
                continue
            self.tiles.append(grp[prev:end])
            prev = end

    def _iter_row_pixels(self, block: bytes, w: int, h: int):
        """Yield (x, y, palette_index) for opaque pixels (correct KYS RLE)."""
        data = block[8:]
        pos = 0
        for y in range(h):
            if pos >= len(data):
                break
            row_nbytes = data[pos]
            pos += 1
            row_end = min(pos + row_nbytes, len(data))
            if row_nbytes <= 0:
                continue
            x = 0
            while pos < row_end and x < w:
                skip = data[pos]
                pos += 1
                x += skip
                if pos >= row_end or x >= w:
                    break
                count = data[pos]
                pos += 1
                for _ in range(count):
                    if pos >= row_end or x >= w:
                        break
                    yield x, y, data[pos]
                    pos += 1
                    x += 1
            pos = row_end

    def decode_tile(
        self, index: int, palette: List[Tuple[int, int, int]], *, use_cache: bool = True
    ):
        """Return PIL Image or None.

        Per-row layout: ``[nbytes][skip][count][color×count]…``
        ``nbytes`` is the size of the following bytes for that row (0 = blank).
        """
        if Image is None:
            raise RuntimeError("Pillow required")
        if use_cache and index in self._img_cache:
            return self._img_cache[index]
        if index < 0 or index >= len(self.tiles):
            return None
        block = self.tiles[index]
        if len(block) < 8:
            return None
        w, h, _xs, _ys = struct.unpack_from("<hhhh", block, 0)
        if w <= 0 or h <= 0 or w > 512 or h > 512:
            return None
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        pixels = img.load()
        for x, y, idx in self._iter_row_pixels(block, w, h):
            if 0 <= idx < len(palette):
                r, g, b = palette[idx]
                pixels[x, y] = (r, g, b, 255)
        if use_cache:
            self._img_cache[index] = img
        return img

    def average_color(
        self, index: int, palette: List[Tuple[int, int, int]]
    ) -> Tuple[int, int, int]:
        """Fast representative RGB for overview painting (cached)."""
        if index in self._color_cache:
            return self._color_cache[index]
        if index < 0 or index >= len(self.tiles):
            c = (40, 40, 40)
            self._color_cache[index] = c
            return c
        block = self.tiles[index]
        if len(block) < 8:
            c = (40, 40, 40)
            self._color_cache[index] = c
            return c
        w, h, _xs, _ys = struct.unpack_from("<hhhh", block, 0)
        if w <= 0 or h <= 0 or w > 512 or h > 512:
            c = (40, 40, 40)
            self._color_cache[index] = c
            return c
        rs = gs = bs = n = 0
        for _x, _y, idx in self._iter_row_pixels(block, w, h):
            if 0 <= idx < len(palette):
                r, g, b = palette[idx]
                if r + g + b > 12:
                    rs += r
                    gs += g
                    bs += b
                    n += 1
        if n <= 0:
            c = (55, 55, 55)
        else:
            c = (rs // n, gs // n, bs // n)
        self._color_cache[index] = c
        return c

    def to_bytes(self) -> tuple[bytes, bytes]:
        """Serialize as end-offset idx + grp (engine layout)."""
        ends = []
        grp = bytearray()
        for t in self.tiles:
            if not t:
                # keep empty slot as duplicate end (zero-length) or 0 if still at start
                ends.append(len(grp) if grp or ends else 0)
            else:
                grp.extend(t)
                ends.append(len(grp))
        idx = struct.pack(f"<{len(ends)}i", *ends) if ends else b""
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
        self._color_cache.pop(index, None)
        self._img_cache.pop(index, None)


def code_to_tile_index(code: int) -> int:
    """Map a KYS even pic/tile *code* to smp frame index.

    Pascal ``DrawSPic(code div 2)`` / ``InitialSPic`` uses ``num = code div 2``.
    With end-offset ``sdx`` packs (this module), that ``num`` indexes ``tiles[num]``.
    (C++ engines that keep *start* offsets use ``(code/2)-1`` into the idx array;
    both resolve to the same sprite bytes.)
    """
    if code == 0:
        return -1
    # Negative codes select mmap / ScenePic paths; caller should branch first.
    if code < 0:
        return (-code) // 2
    return code // 2


def format_pic_code(code: int) -> str:
    """Human-readable DData pic: raw code + smp index."""
    if code == 0:
        return "0"
    idx = code_to_tile_index(code)
    sign = "-" if code < 0 else ""
    return f"{code} → smp[{sign}{idx}]"


def load_tile_pack_pair(
    resource_dir: Path, idx_names: tuple[str, ...], grp_names: tuple[str, ...]
) -> Optional[RleTilePack]:
    idx = grp = None
    for n in idx_names:
        p = resource_dir / n
        if p.is_file():
            idx = p
            break
    for n in grp_names:
        p = resource_dir / n
        if p.is_file():
            grp = p
            break
    if not idx or not grp:
        return None
    pack = RleTilePack()
    pack.load(idx, grp)
    return pack
