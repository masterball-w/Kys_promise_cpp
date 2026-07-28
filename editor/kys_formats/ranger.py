"""Ranger.grp / R*.grp + ranger.idx codec."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple, TYPE_CHECKING

from .backup import atomic_write, backup_file
from .encoding import decode_bytes, encode_text, normalize_encoding

if TYPE_CHECKING:
    from .profile import GameProfile

# Defaults match 金庸群侠前传; prefer GameProfile / RangerLayout at runtime.
ROLE_WORDS = 91
ITEM_WORDS = 95
SCENE_WORDS = 26
MAGIC_WORDS = 111
SHOP_WORDS = 18

ROLE_BYTES = ROLE_WORDS * 2
ITEM_BYTES = ITEM_WORDS * 2
SCENE_BYTES = SCENE_WORDS * 2
MAGIC_BYTES = MAGIC_WORDS * 2
SHOP_BYTES = SHOP_WORDS * 2

INVENTORY_SLOTS = 400  # Promise disk pad; classic games often ~200


@dataclass(frozen=True)
class RangerLayout:
    role_words: int = ROLE_WORDS
    item_words: int = ITEM_WORDS
    scene_words: int = SCENE_WORDS
    magic_words: int = MAGIC_WORDS
    shop_words: int = SHOP_WORDS
    inventory_slots: int = INVENTORY_SLOTS

    @classmethod
    def from_profile(cls, profile: "GameProfile") -> "RangerLayout":
        return cls(
            role_words=profile.role_words,
            item_words=profile.item_words,
            scene_words=profile.scene_words,
            magic_words=profile.magic_words,
            shop_words=profile.shop_words,
            inventory_slots=profile.inventory_slots,
        )


def _i16(data: bytes, off: int) -> int:
    return struct.unpack_from("<h", data, off)[0]


def _u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def _set_i16(buf: bytearray, off: int, value: int) -> None:
    struct.pack_into("<h", buf, off, int(value))


def decode_fixed_name(raw: bytes, encoding: str = "auto") -> str:
    """Decode fixed-width ANSI/GBK/Big5 name used inside ranger records."""
    raw = raw.split(b"\x00")[0]
    while raw and raw[-1:] in (b"\x00", b"\xff", b" "):
        raw = raw[:-1]
    return decode_bytes(raw, encoding)


def encode_fixed_name(text: str, nbytes: int, encoding: str = "auto") -> bytes:
    return encode_text(text, encoding, nbytes=nbytes)


@dataclass
class InventorySlot:
    number: int = -1
    amount: int = 0


@dataclass
class RangerHeader:
    in_ship: int = 0
    where: int = -1
    my: int = 0
    mx: int = 0
    sy: int = 0
    sx: int = 0
    mface: int = 0
    ship_x: int = 0
    ship_y: int = 0
    time: int = 0
    time_event: int = 0
    random_event: int = 0
    sface: int = 0
    ship_face: int = 0
    game_time: int = 0
    team: List[int] = field(default_factory=lambda: [-1] * 6)
    inventory: List[InventorySlot] = field(default_factory=list)


@dataclass
class RecordTable:
    words: int
    records: List[List[int]] = field(default_factory=list)
    text_encoding: str = "auto"

    @property
    def count(self) -> int:
        return len(self.records)

    def get_name(self, index: int, start_word: int = 1, word_count: int = 5) -> str:
        if index < 0 or index >= len(self.records):
            return ""
        rec = self.records[index]
        raw = b"".join(struct.pack("<h", rec[start_word + i]) for i in range(word_count))
        return decode_fixed_name(raw, self.text_encoding)

    def set_name(self, index: int, name: str, start_word: int = 1, word_count: int = 5) -> None:
        raw = encode_fixed_name(name, word_count * 2, self.text_encoding)
        for i in range(word_count):
            self.records[index][start_word + i] = struct.unpack_from("<h", raw, i * 2)[0]

    def get(self, index: int, word: int) -> int:
        return self.records[index][word]

    def set(self, index: int, word: int, value: int) -> None:
        self.records[index][word] = int(value)


class RangerArchive:
    """Load/save one ranger slot (Ranger.grp or Rn.grp)."""

    def __init__(self, layout: Optional[RangerLayout] = None) -> None:
        self.layout = layout or RangerLayout()
        self.text_encoding: str = "auto"
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.role_offset = 0
        self.item_offset = 0
        self.scene_offset = 0
        self.magic_offset = 0
        self.shop_offset = 0
        self.total_len = 0
        self.header = RangerHeader()
        self.roles = RecordTable(self.layout.role_words)
        self.items = RecordTable(self.layout.item_words)
        self.scenes = RecordTable(self.layout.scene_words)
        self.magics = RecordTable(self.layout.magic_words)
        self.shops = RecordTable(self.layout.shop_words)
        self._raw: bytes = b""

    @staticmethod
    def find_idx(save_dir: Path) -> Path:
        for name in ("ranger.idx", "Ranger.idx"):
            p = save_dir / name
            if p.is_file():
                return p
        raise FileNotFoundError(f"ranger.idx not found in {save_dir}")

    @staticmethod
    def resolve_grp(save_dir: Path, slot: int) -> Path:
        if slot <= 0:
            for name in ("Ranger.grp", "ranger.grp"):
                p = save_dir / name
                if p.is_file():
                    return p
            raise FileNotFoundError(f"Ranger.grp not found in {save_dir}")
        for name in (f"R{slot}.grp", f"r{slot}.grp"):
            p = save_dir / name
            if p.is_file():
                return p
        raise FileNotFoundError(f"R{slot}.grp not found in {save_dir}")

    def load(self, save_dir: str | Path, slot: int = 0) -> None:
        save_dir = Path(save_dir)
        self.idx_path = self.find_idx(save_dir)
        self.grp_path = self.resolve_grp(save_dir, slot)
        idx = self.idx_path.read_bytes()
        if len(idx) < 24:
            raise ValueError("ranger.idx too small")
        (
            self.role_offset,
            self.item_offset,
            self.scene_offset,
            self.magic_offset,
            self.shop_offset,
            self.total_len,
        ) = struct.unpack_from("<6i", idx, 0)
        self._raw = self.grp_path.read_bytes()
        if len(self._raw) < self.total_len:
            raise ValueError(f"grp size {len(self._raw)} < TotalLen {self.total_len}")
        self._parse_header()
        lay = self.layout
        self.roles = self._parse_table(self.role_offset, self.item_offset, lay.role_words)
        self.items = self._parse_table(self.item_offset, self.scene_offset, lay.item_words)
        self.scenes = self._parse_table(self.scene_offset, self.magic_offset, lay.scene_words)
        self.magics = self._parse_table(self.magic_offset, self.shop_offset, lay.magic_words)
        self.shops = self._parse_table(self.shop_offset, self.total_len, lay.shop_words)
        self._sync_table_encodings()

    def _sync_table_encodings(self) -> None:
        enc = normalize_encoding(self.text_encoding)
        for table in (self.roles, self.items, self.scenes, self.magics, self.shops):
            table.text_encoding = enc

    def _parse_header(self) -> None:
        d = self._raw
        h = RangerHeader()
        h.in_ship = _i16(d, 0)
        h.where = _i16(d, 2)
        h.my = _i16(d, 4)
        h.mx = _i16(d, 6)
        h.sy = _i16(d, 8)
        h.sx = _i16(d, 10)
        h.mface = _i16(d, 12)
        h.ship_x = _i16(d, 14)
        h.ship_y = _i16(d, 16)
        h.time = _i16(d, 18)
        h.time_event = _i16(d, 20)
        h.random_event = _i16(d, 22)
        h.sface = _i16(d, 24)
        h.ship_face = _i16(d, 26)
        h.game_time = _i16(d, 28)
        h.team = [_i16(d, 30 + i * 2) for i in range(6)]
        inv_bytes = max(0, self.role_offset - 42)
        slots = inv_bytes // 4
        h.inventory = []
        for i in range(slots):
            off = 42 + i * 4
            h.inventory.append(InventorySlot(_i16(d, off), _i16(d, off + 2)))
        # Pad to profile inventory_slots for editor convenience
        while len(h.inventory) < self.layout.inventory_slots:
            h.inventory.append(InventorySlot(-1, 0))
        self.header = h

    def _parse_table(self, start: int, end: int, words: int) -> RecordTable:
        byte_size = words * 2
        count = (end - start) // byte_size
        table = RecordTable(words)
        for i in range(count):
            off = start + i * byte_size
            rec = list(struct.unpack_from(f"<{words}h", self._raw, off))
            table.records.append(rec)
        return table

    def to_bytes(self) -> bytes:
        """Serialize full grp matching original idx offsets."""
        # Keep original section sizes; truncate/pad tables to fit.
        role_bytes = self.item_offset - self.role_offset
        item_bytes = self.scene_offset - self.item_offset
        scene_bytes = self.magic_offset - self.scene_offset
        magic_bytes = self.shop_offset - self.magic_offset
        shop_bytes = self.total_len - self.shop_offset

        header = bytearray(self.role_offset)
        h = self.header
        _set_i16(header, 0, h.in_ship)
        _set_i16(header, 2, h.where)
        _set_i16(header, 4, h.my)
        _set_i16(header, 6, h.mx)
        _set_i16(header, 8, h.sy)
        _set_i16(header, 10, h.sx)
        _set_i16(header, 12, h.mface)
        _set_i16(header, 14, h.ship_x)
        _set_i16(header, 16, h.ship_y)
        _set_i16(header, 18, h.time)
        _set_i16(header, 20, h.time_event)
        _set_i16(header, 22, h.random_event)
        _set_i16(header, 24, h.sface)
        _set_i16(header, 26, h.ship_face)
        _set_i16(header, 28, h.game_time)
        for i in range(6):
            _set_i16(header, 30 + i * 2, h.team[i] if i < len(h.team) else -1)
        inv_slots = (self.role_offset - 42) // 4
        for i in range(inv_slots):
            slot = h.inventory[i] if i < len(h.inventory) else InventorySlot(-1, 0)
            _set_i16(header, 42 + i * 4, slot.number)
            _set_i16(header, 42 + i * 4 + 2, slot.amount)

        def pack_table(table: RecordTable, nbytes: int) -> bytes:
            words = table.words
            count = nbytes // (words * 2)
            out = bytearray()
            for i in range(count):
                if i < len(table.records):
                    rec = table.records[i]
                else:
                    rec = [0] * words
                # pad/truncate record
                if len(rec) < words:
                    rec = rec + [0] * (words - len(rec))
                out.extend(struct.pack(f"<{words}h", *rec[:words]))
            if len(out) < nbytes:
                out.extend(b"\x00" * (nbytes - len(out)))
            return bytes(out[:nbytes])

        parts = [
            bytes(header),
            pack_table(self.roles, role_bytes),
            pack_table(self.items, item_bytes),
            pack_table(self.scenes, scene_bytes),
            pack_table(self.magics, magic_bytes),
            pack_table(self.shops, shop_bytes),
        ]
        data = b"".join(parts)
        if len(data) < self.total_len:
            data += b"\x00" * (self.total_len - len(data))
        return data[: self.total_len]

    def save(self, backup: bool = True) -> None:
        if not self.grp_path:
            raise RuntimeError("No grp loaded")
        if backup:
            backup_file(self.grp_path)
        atomic_write(self.grp_path, self.to_bytes())

    def role_name(self, index: int) -> str:
        return self.roles.get_name(index, 4, 5)

    def item_name(self, index: int) -> str:
        return self.items.get_name(index, 1, 10)

    def magic_name(self, index: int) -> str:
        return self.magics.get_name(index, 1, 5)

    def scene_name(self, index: int) -> str:
        return self.scenes.get_name(index, 1, 5)
