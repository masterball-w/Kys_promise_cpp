"""talk.idx/grp and name.idx/grp codecs (XOR 0xFF text)."""

from __future__ import annotations

import struct
from pathlib import Path
from typing import List, Optional

from .backup import atomic_write, backup_file
from .encoding import decode_talk_payload, encode_talk_payload, normalize_encoding


def xor_ff(data: bytes) -> bytes:
    return bytes(b ^ 0xFF for b in data)


def decode_talk_bytes(raw: bytes, encoding: str = "auto") -> str:
    """XOR 0xFF then decode. Auto mode prefers GBK before Big5 (see encoding.py)."""
    return decode_talk_payload(raw, encoding)


def encode_talk_text(text: str, encoding: str = "auto") -> bytes:
    return encode_talk_payload(text, encoding)


class _IdxGrpTextArchive:
    def __init__(self, kind: str) -> None:
        self.kind = kind
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.offsets: List[int] = []
        self.entries: List[bytes] = []  # raw encrypted bytes per entry
        self.text_encoding: str = "auto"

    @property
    def count(self) -> int:
        return len(self.entries)

    def load(self, resource_dir: str | Path) -> None:
        resource_dir = Path(resource_dir)
        names = {
            "talk": [("talk.idx", "Talk.idx"), ("talk.grp", "Talk.grp")],
            "name": [("name.idx", "Name.idx"), ("name.grp", "Name.grp")],
        }[self.kind]
        idx = grp = None
        for n in names[0]:
            p = resource_dir / n
            if p.is_file():
                idx = p
                break
        for n in names[1]:
            p = resource_dir / n
            if p.is_file():
                grp = p
                break
        if not idx or not grp:
            raise FileNotFoundError(f"{self.kind} idx/grp not found")
        self.idx_path = idx
        self.grp_path = grp
        idx_data = idx.read_bytes()
        grp_data = grp.read_bytes()
        self.offsets = list(struct.unpack(f"<{len(idx_data)//4}i", idx_data))
        self.entries = []
        for i, off in enumerate(self.offsets):
            end = self.offsets[i + 1] if i + 1 < len(self.offsets) else len(grp_data)
            self.entries.append(grp_data[off:end])

    def get_text(self, entry_id: int) -> str:
        """1-based id."""
        if entry_id <= 0 or entry_id > len(self.entries):
            return ""
        return decode_talk_bytes(self.entries[entry_id - 1], self.text_encoding)

    def set_text(self, entry_id: int, text: str) -> None:
        if entry_id <= 0 or entry_id > len(self.entries):
            raise IndexError(entry_id)
        self.entries[entry_id - 1] = encode_talk_text(text, self.text_encoding)

    def append_text(self, text: str) -> int:
        self.entries.append(encode_talk_text(text, self.text_encoding))
        return len(self.entries)

    def _rebuild_offsets(self) -> None:
        offsets = []
        cursor = 0
        for e in self.entries:
            offsets.append(cursor)
            cursor += len(e)
        self.offsets = offsets

    def to_idx_bytes(self) -> bytes:
        self._rebuild_offsets()
        return struct.pack(f"<{len(self.offsets)}i", *self.offsets)

    def to_grp_bytes(self) -> bytes:
        return b"".join(self.entries)

    def save(self, backup: bool = True) -> None:
        if not self.idx_path or not self.grp_path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.idx_path)
            backup_file(self.grp_path)
        atomic_write(self.idx_path, self.to_idx_bytes())
        atomic_write(self.grp_path, self.to_grp_bytes())


class TalkArchive(_IdxGrpTextArchive):
    def __init__(self) -> None:
        super().__init__("talk")


class NameArchive(_IdxGrpTextArchive):
    def __init__(self) -> None:
        super().__init__("name")
