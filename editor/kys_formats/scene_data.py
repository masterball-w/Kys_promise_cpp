"""Scene event (alldef/D*.grp) and map (allsin/S*.grp) codecs."""

from __future__ import annotations

import struct
from pathlib import Path
from typing import List, Optional

from .backup import atomic_write, backup_file

EVENTS_PER_SCENE = 200
EVENT_WORDS = 11
EVENT_BYTES = EVENTS_PER_SCENE * EVENT_WORDS * 2  # 4400

MAP_LAYERS = 6
MAP_SIZE = 64
SCENE_MAP_BYTES = MAP_LAYERS * MAP_SIZE * MAP_SIZE * 2  # 49152


class SceneEventData:
    """DData: scenes × 200 events × 11 int16."""

    def __init__(self) -> None:
        self.path: Optional[Path] = None
        self.scenes: List[List[List[int]]] = []  # [scene][event][word]

    @staticmethod
    def resolve_path(save_dir: Path, slot: int) -> Path:
        save_dir = Path(save_dir)
        if slot <= 0:
            for name in ("alldef.grp", "Alldef.grp"):
                p = save_dir / name
                if p.is_file():
                    return p
            return save_dir / "alldef.grp"
        for name in (f"D{slot}.grp", f"d{slot}.grp"):
            p = save_dir / name
            if p.is_file():
                return p
        raise FileNotFoundError(f"D{slot}.grp not found in {save_dir}")

    @property
    def scene_count(self) -> int:
        return len(self.scenes)

    def load(self, path: str | Path) -> None:
        path = Path(path)
        self.path = path
        raw = path.read_bytes()
        if len(raw) % EVENT_BYTES != 0:
            raise ValueError(f"alldef size {len(raw)} not multiple of {EVENT_BYTES}")
        count = len(raw) // EVENT_BYTES
        self.scenes = []
        for s in range(count):
            base = s * EVENT_BYTES
            events = []
            for e in range(EVENTS_PER_SCENE):
                off = base + e * EVENT_WORDS * 2
                events.append(list(struct.unpack_from(f"<{EVENT_WORDS}h", raw, off)))
            self.scenes.append(events)

    def get(self, scene: int, event: int, word: int) -> int:
        return self.scenes[scene][event][word]

    def set(self, scene: int, event: int, word: int, value: int) -> None:
        self.scenes[scene][event][word] = int(value)

    def to_bytes(self) -> bytes:
        out = bytearray()
        for events in self.scenes:
            for ev in events:
                words = ev[:EVENT_WORDS] + [0] * max(0, EVENT_WORDS - len(ev))
                out.extend(struct.pack(f"<{EVENT_WORDS}h", *words[:EVENT_WORDS]))
        return bytes(out)

    def save(self, backup: bool = True) -> None:
        if not self.path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.path)
        atomic_write(self.path, self.to_bytes())

    def find_free_event(self, scene: int) -> int:
        """Return first event index with all scripts <=0 and pic==0, or -1."""
        for e, ev in enumerate(self.scenes[scene]):
            if ev[2] <= 0 and ev[3] <= 0 and ev[4] <= 0 and ev[5] == 0:
                return e
        return -1


class SceneMapData:
    """SData: scenes × 6 layers × 64 × 64 int16 (index = x*64+y)."""

    def __init__(self) -> None:
        self.path: Optional[Path] = None
        # [scene][layer][x][y]
        self.maps: List[List[List[List[int]]]] = []

    @staticmethod
    def resolve_path(save_dir: Path, slot: int) -> Path:
        save_dir = Path(save_dir)
        if slot <= 0:
            for name in ("allsin.grp", "Allsin.grp"):
                p = save_dir / name
                if p.is_file():
                    return p
            return save_dir / "allsin.grp"
        for name in (f"S{slot}.grp", f"s{slot}.grp"):
            p = save_dir / name
            if p.is_file():
                return p
        raise FileNotFoundError(f"S{slot}.grp not found in {save_dir}")

    @property
    def scene_count(self) -> int:
        return len(self.maps)

    def load(self, path: str | Path) -> None:
        path = Path(path)
        self.path = path
        raw = path.read_bytes()
        if len(raw) % SCENE_MAP_BYTES != 0:
            raise ValueError(f"allsin size {len(raw)} not multiple of {SCENE_MAP_BYTES}")
        count = len(raw) // SCENE_MAP_BYTES
        self.maps = []
        for s in range(count):
            base = s * SCENE_MAP_BYTES
            layers = []
            for layer in range(MAP_LAYERS):
                grid = [[0] * MAP_SIZE for _ in range(MAP_SIZE)]
                layer_off = base + layer * MAP_SIZE * MAP_SIZE * 2
                for x in range(MAP_SIZE):
                    for y in range(MAP_SIZE):
                        off = layer_off + (x * MAP_SIZE + y) * 2
                        grid[x][y] = struct.unpack_from("<h", raw, off)[0]
                layers.append(grid)
            self.maps.append(layers)

    def get(self, scene: int, layer: int, x: int, y: int) -> int:
        return self.maps[scene][layer][x][y]

    def set(self, scene: int, layer: int, x: int, y: int, value: int) -> None:
        self.maps[scene][layer][x][y] = int(value)

    def to_bytes(self) -> bytes:
        out = bytearray()
        for layers in self.maps:
            for layer in range(MAP_LAYERS):
                grid = layers[layer]
                for x in range(MAP_SIZE):
                    for y in range(MAP_SIZE):
                        out.extend(struct.pack("<h", grid[x][y]))
        return bytes(out)

    def save(self, backup: bool = True) -> None:
        if not self.path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.path)
        atomic_write(self.path, self.to_bytes())
