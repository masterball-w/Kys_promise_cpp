"""War.sta and warfld.idx/grp codecs."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

from .backup import atomic_write, backup_file
from .ranger import decode_fixed_name, encode_fixed_name

WAR_WORDS = 156
WAR_BYTES = WAR_WORDS * 2  # 312
FIELD_SIZE = 64
FIELD_LAYERS = 2
FIELD_BYTES = FIELD_LAYERS * FIELD_SIZE * FIELD_SIZE * 2  # 16384


@dataclass
class WarRecord:
    data: List[int] = field(default_factory=lambda: [0] * WAR_WORDS)

    def get(self, i: int) -> int:
        return self.data[i]

    def set(self, i: int, v: int) -> None:
        self.data[i] = int(v)

    @property
    def battle_num(self) -> int:
        return self.data[0]

    @battle_num.setter
    def battle_num(self, v: int) -> None:
        self.data[0] = int(v)

    @property
    def name(self) -> str:
        raw = b"".join(struct.pack("<h", self.data[i]) for i in range(1, 6))
        return decode_fixed_name(raw)

    @name.setter
    def name(self, text: str) -> None:
        raw = encode_fixed_name(text, 10)
        for i in range(5):
            self.data[1 + i] = struct.unpack_from("<h", raw, i * 2)[0]

    @property
    def battle_map(self) -> int:
        return self.data[6]

    @battle_map.setter
    def battle_map(self, v: int) -> None:
        self.data[6] = int(v)

    @property
    def exp(self) -> int:
        return self.data[7]

    @exp.setter
    def exp(self, v: int) -> None:
        self.data[7] = int(v)

    @property
    def music(self) -> int:
        return self.data[8]

    @music.setter
    def music(self, v: int) -> None:
        self.data[8] = int(v)

    def mate(self, i: int) -> int:
        return self.data[9 + i]

    def set_mate(self, i: int, v: int) -> None:
        self.data[9 + i] = int(v)

    def auto_mate(self, i: int) -> int:
        return self.data[21 + i]

    def set_auto_mate(self, i: int, v: int) -> None:
        self.data[21 + i] = int(v)

    def mate_x(self, i: int) -> int:
        return self.data[33 + i]

    def set_mate_x(self, i: int, v: int) -> None:
        self.data[33 + i] = int(v)

    def mate_y(self, i: int) -> int:
        return self.data[45 + i]

    def set_mate_y(self, i: int, v: int) -> None:
        self.data[45 + i] = int(v)

    def enemy(self, i: int) -> int:
        return self.data[57 + i]

    def set_enemy(self, i: int, v: int) -> None:
        self.data[57 + i] = int(v)

    def enemy_x(self, i: int) -> int:
        return self.data[87 + i]

    def set_enemy_x(self, i: int, v: int) -> None:
        self.data[87 + i] = int(v)

    def enemy_y(self, i: int) -> int:
        return self.data[117 + i]

    def set_enemy_y(self, i: int, v: int) -> None:
        self.data[117 + i] = int(v)

    @property
    def bout_event(self) -> int:
        return self.data[147]

    @bout_event.setter
    def bout_event(self, v: int) -> None:
        self.data[147] = int(v)

    @property
    def operation_event(self) -> int:
        return self.data[148]

    @operation_event.setter
    def operation_event(self, v: int) -> None:
        self.data[148] = int(v)

    def get_kongfu(self, i: int) -> int:
        return self.data[149 + i]

    def set_kongfu(self, i: int, v: int) -> None:
        self.data[149 + i] = int(v)

    def get_items(self, i: int) -> int:
        return self.data[152 + i]

    def set_items(self, i: int, v: int) -> None:
        self.data[152 + i] = int(v)

    @property
    def get_money(self) -> int:
        return self.data[155]

    @get_money.setter
    def get_money(self, v: int) -> None:
        self.data[155] = int(v)

    def enemy_count(self) -> int:
        return sum(1 for i in range(30) if self.enemy(i) >= 0)

    def mate_count(self) -> int:
        return sum(1 for i in range(12) if self.mate(i) >= 0)

    def clear(self) -> None:
        self.data = [-1] * WAR_WORDS
        self.data[0] = 0
        for i in range(1, 6):
            self.data[i] = 0
        self.data[6] = 0
        self.data[7] = 0
        self.data[8] = 0


class WarArchive:
    def __init__(self) -> None:
        self.path: Optional[Path] = None
        self.records: List[WarRecord] = []

    @property
    def count(self) -> int:
        return len(self.records)

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
        if len(raw) % WAR_BYTES != 0:
            raise ValueError(f"War.sta size {len(raw)} not multiple of {WAR_BYTES}")
        self.records = []
        for i in range(len(raw) // WAR_BYTES):
            words = list(struct.unpack_from(f"<{WAR_WORDS}h", raw, i * WAR_BYTES))
            self.records.append(WarRecord(words))

    def find_by_num(self, battle_num: int) -> Optional[WarRecord]:
        for r in self.records:
            if r.battle_num == battle_num:
                return r
        if 0 <= battle_num < len(self.records):
            return self.records[battle_num]
        return None

    def append_copy(self, src_index: int = 0) -> WarRecord:
        src = self.records[src_index] if self.records else WarRecord()
        rec = WarRecord(list(src.data))
        max_num = max((r.battle_num for r in self.records), default=0)
        rec.battle_num = max_num + 1
        self.records.append(rec)
        return rec

    def to_bytes(self) -> bytes:
        out = bytearray()
        for r in self.records:
            data = r.data[:WAR_WORDS] + [0] * max(0, WAR_WORDS - len(r.data))
            out.extend(struct.pack(f"<{WAR_WORDS}h", *data[:WAR_WORDS]))
        return bytes(out)

    def save(self, backup: bool = True) -> None:
        if not self.path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.path)
        atomic_write(self.path, self.to_bytes())


class WarFieldArchive:
    """warfld.idx + warfld.grp — battle terrain 64x64 x 2 layers."""

    def __init__(self) -> None:
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.offsets: List[int] = []
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
        self.offsets = list(struct.unpack(f"<{len(idx_data)//4}i", idx_data))
        # Infer field count from grp size if idx is sparse
        nfields = len(grp_data) // FIELD_BYTES
        self.fields = []
        for i in range(nfields):
            off = self.offsets[i] if i < len(self.offsets) else i * FIELD_BYTES
            if i == 0 and off != 0 and len(self.offsets) > 0:
                # field 0 often starts at 0 regardless
                off = 0 if i == 0 else off
            # Prefer sequential layout matching engine
            off = i * FIELD_BYTES
            layers = []
            for layer in range(FIELD_LAYERS):
                grid = [[0] * FIELD_SIZE for _ in range(FIELD_SIZE)]
                layer_off = off + layer * FIELD_SIZE * FIELD_SIZE * 2
                for x in range(FIELD_SIZE):
                    for y in range(FIELD_SIZE):
                        o = layer_off + (x * FIELD_SIZE + y) * 2
                        if o + 2 <= len(grp_data):
                            grid[x][y] = struct.unpack_from("<h", grp_data, o)[0]
                layers.append(grid)
            self.fields.append(layers)

    def get(self, field: int, layer: int, x: int, y: int) -> int:
        return self.fields[field][layer][x][y]

    def set(self, field: int, layer: int, x: int, y: int, value: int) -> None:
        self.fields[field][layer][x][y] = int(value)

    def to_bytes(self) -> tuple[bytes, bytes]:
        grp = bytearray()
        offsets = []
        for i, layers in enumerate(self.fields):
            offsets.append(len(grp))
            for layer in range(FIELD_LAYERS):
                grid = layers[layer]
                for x in range(FIELD_SIZE):
                    for y in range(FIELD_SIZE):
                        grp.extend(struct.pack("<h", grid[x][y]))
        idx = struct.pack(f"<{len(offsets)}i", *offsets)
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
