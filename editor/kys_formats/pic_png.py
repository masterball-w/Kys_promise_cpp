"""PNG-in-.Pic archive codec (PicLoader end-offset layout)."""

from __future__ import annotations

import io
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

from .backup import atomic_write, backup_file

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None  # type: ignore


@dataclass
class PicFrame:
    x: int = 0
    y: int = 0
    black: int = 0
    png_bytes: bytes = b""

    def to_image(self):
        if Image is None:
            raise RuntimeError("Pillow required")
        if not self.png_bytes:
            return None
        return Image.open(io.BytesIO(self.png_bytes)).convert("RGBA")

    @classmethod
    def from_image(cls, img, x: int = 0, y: int = 0, black: int = 0) -> "PicFrame":
        if Image is None:
            raise RuntimeError("Pillow required")
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        return cls(x=x, y=y, black=black, png_bytes=buf.getvalue())


class PicArchive:
    def __init__(self) -> None:
        self.path: Optional[Path] = None
        self.frames: List[PicFrame] = []

    @property
    def count(self) -> int:
        return len(self.frames)

    def load(self, path: str | Path) -> None:
        path = Path(path)
        self.path = path
        raw = path.read_bytes()
        if len(raw) < 4:
            raise ValueError("empty pic")
        (count,) = struct.unpack_from("<i", raw, 0)
        if count <= 0:
            self.frames = []
            return
        if len(raw) < 4 + count * 4:
            raise ValueError("pic header truncated")
        offsets = list(struct.unpack_from(f"<{count}i", raw, 4))
        header_size = (count + 1) * 4
        self.frames = []
        for i in range(count):
            start = header_size if i == 0 else offsets[i - 1]
            end = offsets[i]
            if start < 0 or end > len(raw) or end < start + 12:
                self.frames.append(PicFrame())
                continue
            x, y, black = struct.unpack_from("<iii", raw, start)
            png = raw[start + 12 : end]
            self.frames.append(PicFrame(x, y, black, bytes(png)))

    def to_bytes(self) -> bytes:
        count = len(self.frames)
        header_size = (count + 1) * 4
        chunks: List[bytes] = []
        offsets: List[int] = []
        cursor = header_size
        for fr in self.frames:
            chunk = struct.pack("<iii", fr.x, fr.y, fr.black) + fr.png_bytes
            cursor += len(chunk)
            offsets.append(cursor)
            chunks.append(chunk)
        out = bytearray()
        out.extend(struct.pack("<i", count))
        if count:
            out.extend(struct.pack(f"<{count}i", *offsets))
        for c in chunks:
            out.extend(c)
        return bytes(out)

    def save(self, path: str | Path | None = None, backup: bool = True) -> None:
        path = Path(path) if path else self.path
        if not path:
            raise RuntimeError("no path")
        self.path = path
        if backup and path.is_file():
            backup_file(path)
        atomic_write(path, self.to_bytes())

    def replace_frame(self, index: int, image_path: str | Path, x: int = 0, y: int = 0) -> None:
        if Image is None:
            raise RuntimeError("Pillow required")
        img = Image.open(image_path).convert("RGBA")
        frame = PicFrame.from_image(img, x=x, y=y)
        if index < 0 or index >= len(self.frames):
            raise IndexError(index)
        # preserve black from old frame
        frame.black = self.frames[index].black
        self.frames[index] = frame

    def append_frame(self, image_path: str | Path, x: int = 0, y: int = 0) -> int:
        if Image is None:
            raise RuntimeError("Pillow required")
        img = Image.open(image_path).convert("RGBA")
        self.frames.append(PicFrame.from_image(img, x=x, y=y))
        return len(self.frames) - 1

    def export_frame(self, index: int, out_path: str | Path) -> None:
        img = self.frames[index].to_image()
        if img is None:
            raise ValueError("empty frame")
        img.save(out_path)

    def delete_frame(self, index: int) -> None:
        del self.frames[index]
