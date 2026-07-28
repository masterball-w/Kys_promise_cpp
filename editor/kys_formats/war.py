"""War.sta and warfld.idx/grp codecs."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, TYPE_CHECKING

from .backup import atomic_write, backup_file
from .profile import WAR_LAYOUT_PROMISE, WarLayout
from .ranger import decode_fixed_name, encode_fixed_name

if TYPE_CHECKING:
    from .profile import GameProfile

# Back-compat defaults (Promise)
WAR_WORDS = WAR_LAYOUT_PROMISE.words
WAR_BYTES = WAR_WORDS * 2
FIELD_SIZE = 64
FIELD_LAYERS = 2
FIELD_BYTES = FIELD_LAYERS * FIELD_SIZE * FIELD_SIZE * 2  # 16384


@dataclass
class WarRecord:
    data: List[int] = field(default_factory=list)
    layout: WarLayout = field(default_factory=lambda: WAR_LAYOUT_PROMISE)

    def __post_init__(self) -> None:
        w = self.layout.words
        if not self.data:
            self.data = [0] * w
        elif len(self.data) < w:
            self.data = list(self.data) + [0] * (w - len(self.data))
        elif len(self.data) > w:
            self.data = list(self.data[:w])

    def _get(self, i: int, default: int = 0) -> int:
        if i < 0 or i >= len(self.data):
            return default
        return self.data[i]

    def _set(self, i: int, v: int) -> None:
        if 0 <= i < len(self.data):
            self.data[i] = int(v)

    def get(self, i: int) -> int:
        return self._get(i)

    def set(self, i: int, v: int) -> None:
        self._set(i, v)

    @property
    def battle_num(self) -> int:
        return self._get(0)

    @battle_num.setter
    def battle_num(self, v: int) -> None:
        self._set(0, v)

    @property
    def name(self) -> str:
        raw = b"".join(struct.pack("<h", self._get(i)) for i in range(1, 6))
        return decode_fixed_name(raw)

    @name.setter
    def name(self, text: str) -> None:
        raw = encode_fixed_name(text, 10)
        for i in range(5):
            self._set(1 + i, struct.unpack_from("<h", raw, i * 2)[0])

    @property
    def battle_map(self) -> int:
        return self._get(6)

    @battle_map.setter
    def battle_map(self, v: int) -> None:
        self._set(6, v)

    @property
    def exp(self) -> int:
        return self._get(7)

    @exp.setter
    def exp(self, v: int) -> None:
        self._set(7, v)

    @property
    def music(self) -> int:
        return self._get(8)

    @music.setter
    def music(self, v: int) -> None:
        self._set(8, v)

    def mate(self, i: int) -> int:
        lay = self.layout
        if 0 <= i < lay.mate_count:
            return self._get(lay.mate_off + i, -1)
        return -1

    def set_mate(self, i: int, v: int) -> None:
        lay = self.layout
        if 0 <= i < lay.mate_count:
            self._set(lay.mate_off + i, v)

    def auto_mate(self, i: int) -> int:
        lay = self.layout
        if lay.auto_mate_off < 0 or i < 0 or i >= lay.auto_mate_count:
            return -1
        return self._get(lay.auto_mate_off + i, -1)

    def set_auto_mate(self, i: int, v: int) -> None:
        lay = self.layout
        if lay.auto_mate_off >= 0 and 0 <= i < lay.auto_mate_count:
            self._set(lay.auto_mate_off + i, v)

    def mate_x(self, i: int) -> int:
        return self._get(self.layout.mate_x_off + i)

    def set_mate_x(self, i: int, v: int) -> None:
        if 0 <= i < self.layout.mate_count:
            self._set(self.layout.mate_x_off + i, v)

    def mate_y(self, i: int) -> int:
        return self._get(self.layout.mate_y_off + i)

    def set_mate_y(self, i: int, v: int) -> None:
        if 0 <= i < self.layout.mate_count:
            self._set(self.layout.mate_y_off + i, v)

    def enemy(self, i: int) -> int:
        lay = self.layout
        if 0 <= i < lay.enemy_count:
            return self._get(lay.enemy_off + i, -1)
        return -1

    def set_enemy(self, i: int, v: int) -> None:
        lay = self.layout
        if 0 <= i < lay.enemy_count:
            self._set(lay.enemy_off + i, v)

    def enemy_x(self, i: int) -> int:
        return self._get(self.layout.enemy_x_off + i)

    def set_enemy_x(self, i: int, v: int) -> None:
        if 0 <= i < self.layout.enemy_count:
            self._set(self.layout.enemy_x_off + i, v)

    def enemy_y(self, i: int) -> int:
        return self._get(self.layout.enemy_y_off + i)

    def set_enemy_y(self, i: int, v: int) -> None:
        if 0 <= i < self.layout.enemy_count:
            self._set(self.layout.enemy_y_off + i, v)

    @property
    def bout_event(self) -> int:
        off = self.layout.bout_event_off
        return self._get(off) if off >= 0 else 0

    @bout_event.setter
    def bout_event(self, v: int) -> None:
        if self.layout.bout_event_off >= 0:
            self._set(self.layout.bout_event_off, v)

    @property
    def operation_event(self) -> int:
        off = self.layout.operation_event_off
        return self._get(off) if off >= 0 else 0

    @operation_event.setter
    def operation_event(self, v: int) -> None:
        if self.layout.operation_event_off >= 0:
            self._set(self.layout.operation_event_off, v)

    def get_kongfu(self, i: int) -> int:
        lay = self.layout
        if lay.get_kongfu_off < 0 or i < 0 or i >= lay.get_kongfu_count:
            return -1
        return self._get(lay.get_kongfu_off + i, -1)

    def set_kongfu(self, i: int, v: int) -> None:
        lay = self.layout
        if lay.get_kongfu_off >= 0 and 0 <= i < lay.get_kongfu_count:
            self._set(lay.get_kongfu_off + i, v)

    def get_items(self, i: int) -> int:
        lay = self.layout
        if lay.get_items_off < 0 or i < 0 or i >= lay.get_items_count:
            return -1
        return self._get(lay.get_items_off + i, -1)

    def set_items(self, i: int, v: int) -> None:
        lay = self.layout
        if lay.get_items_off >= 0 and 0 <= i < lay.get_items_count:
            self._set(lay.get_items_off + i, v)

    @property
    def get_money(self) -> int:
        off = self.layout.get_money_off
        return self._get(off) if off >= 0 else 0

    @get_money.setter
    def get_money(self, v: int) -> None:
        if self.layout.get_money_off >= 0:
            self._set(self.layout.get_money_off, v)

    def enemy_count(self) -> int:
        return sum(1 for i in range(self.layout.enemy_count) if self.enemy(i) >= 0)

    def mate_count(self) -> int:
        n = sum(1 for i in range(self.layout.mate_count) if self.mate(i) >= 0)
        if self.layout.auto_mate_off >= 0:
            n += sum(1 for i in range(self.layout.auto_mate_count) if self.auto_mate(i) >= 0)
        return n

    def clear(self) -> None:
        w = self.layout.words
        self.data = [-1] * w
        self.data[0] = 0
        for i in range(1, 6):
            self.data[i] = 0
        self.data[6] = 0
        self.data[7] = 0
        self.data[8] = 0


class WarArchive:
    def __init__(self, layout: Optional[WarLayout] = None) -> None:
        self.layout = layout or WAR_LAYOUT_PROMISE
        self.path: Optional[Path] = None
        self.records: List[WarRecord] = []

    @classmethod
    def from_profile(cls, profile: "GameProfile") -> "WarArchive":
        return cls(layout=profile.war)

    @property
    def count(self) -> int:
        return len(self.records)

    @property
    def war_words(self) -> int:
        return self.layout.words

    @property
    def war_bytes(self) -> int:
        return self.layout.words * 2

    def load(self, resource_dir: str | Path) -> None:
        resource_dir = Path(resource_dir)
        path = None
        for n in ("War.sta", "war.sta"):
            p = resource_dir / n
            if p.is_file():
                path = p
                break
        if not path:
            raise FileNotFoundError("War.sta not found")
        self.path = path
        raw = path.read_bytes()
        wb = self.war_bytes
        ww = self.war_words
        if len(raw) % wb != 0:
            raise ValueError(f"War.sta size {len(raw)} not multiple of {wb} (words={ww})")
        self.records = []
        for i in range(len(raw) // wb):
            words = list(struct.unpack_from(f"<{ww}h", raw, i * wb))
            self.records.append(WarRecord(words, self.layout))

    def find_by_num(self, battle_num: int) -> Optional[WarRecord]:
        for r in self.records:
            if r.battle_num == battle_num:
                return r
        if 0 <= battle_num < len(self.records):
            return self.records[battle_num]
        return None

    def append_copy(self, src_index: int = 0) -> WarRecord:
        src = self.records[src_index] if self.records else WarRecord(layout=self.layout)
        rec = WarRecord(list(src.data), self.layout)
        max_num = max((r.battle_num for r in self.records), default=0)
        rec.battle_num = max_num + 1
        self.records.append(rec)
        return rec

    def to_bytes(self) -> bytes:
        ww = self.war_words
        out = bytearray()
        for r in self.records:
            data = r.data[:ww] + [0] * max(0, ww - len(r.data))
            out.extend(struct.pack(f"<{ww}h", *data[:ww]))
        return bytes(out)

    def save(self, backup: bool = True) -> None:
        if not self.path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.path)
        atomic_write(self.path, self.to_bytes())


class WarFieldArchive:
    """warfld.idx + warfld.grp — battle terrain (variable layers via idx)."""

    def __init__(self) -> None:
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.offsets: List[int] = []
        self.layer_counts: List[int] = []
        # [field][layer][x][y]
        self.fields: List[List[List[List[int]]]] = []

    @property
    def count(self) -> int:
        return len(self.fields)

    def load(self, resource_dir: str | Path) -> None:
        resource_dir = Path(resource_dir)
        idx = grp = None
        for n in ("warfld.idx", "Warfld.idx"):
            p = resource_dir / n
            if p.is_file():
                idx = p
                break
        for n in ("warfld.grp", "Warfld.grp"):
            p = resource_dir / n
            if p.is_file():
                grp = p
                break
        if not idx or not grp:
            raise FileNotFoundError("warfld.idx/grp not found")
        self.idx_path = idx
        self.grp_path = grp
        idx_data = idx.read_bytes()
        grp_data = grp.read_bytes()
        ends = list(struct.unpack(f"<{len(idx_data)//4}i", idx_data))
        self.offsets = []
        self.layer_counts = []
        self.fields = []
        prev = 0
        layer_stride = FIELD_SIZE * FIELD_SIZE * 2
        for end in ends:
            if end <= prev or end > len(grp_data):
                # fall back: treat as sequential 2-layer if corrupt
                if not self.fields and len(grp_data) >= FIELD_BYTES:
                    nfields = len(grp_data) // FIELD_BYTES
                    for i in range(nfields):
                        self._append_field(grp_data, i * FIELD_BYTES, FIELD_LAYERS)
                break
            size = end - prev
            layers = max(1, size // layer_stride)
            self.offsets.append(prev)
            self.layer_counts.append(layers)
            self._append_field(grp_data, prev, layers)
            prev = end
        else:
            return
        # if loop broke early without fields, try fixed layout
        if not self.fields and len(grp_data) >= FIELD_BYTES:
            nfields = len(grp_data) // FIELD_BYTES
            for i in range(nfields):
                self._append_field(grp_data, i * FIELD_BYTES, FIELD_LAYERS)

    def _append_field(self, grp_data: bytes, off: int, layers: int) -> None:
        layer_list = []
        for layer in range(layers):
            grid = [[0] * FIELD_SIZE for _ in range(FIELD_SIZE)]
            layer_off = off + layer * FIELD_SIZE * FIELD_SIZE * 2
            for x in range(FIELD_SIZE):
                for y in range(FIELD_SIZE):
                    o = layer_off + (x * FIELD_SIZE + y) * 2
                    if o + 2 <= len(grp_data):
                        grid[x][y] = struct.unpack_from("<h", grp_data, o)[0]
            layer_list.append(grid)
        self.fields.append(layer_list)

    def get(self, field: int, layer: int, x: int, y: int) -> int:
        return self.fields[field][layer][x][y]

    def set(self, field: int, layer: int, x: int, y: int, value: int) -> None:
        self.fields[field][layer][x][y] = int(value)

    def to_bytes(self) -> tuple[bytes, bytes]:
        grp = bytearray()
        ends = []
        for layers in self.fields:
            for grid in layers:
                for x in range(FIELD_SIZE):
                    for y in range(FIELD_SIZE):
                        grp.extend(struct.pack("<h", grid[x][y]))
            ends.append(len(grp))
        idx = struct.pack(f"<{len(ends)}i", *ends)
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
