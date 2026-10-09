"""Big-map (.002) grid codecs — 480×480 int16 tile codes."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, TYPE_CHECKING
import json

from .backup import atomic_write, backup_file

if TYPE_CHECKING:
    from .ranger import RangerArchive

WORLD_SIZE = 480
WORLD_BYTES = WORLD_SIZE * WORLD_SIZE * 2  # 460800

# Scene record word indices (Pascal TScene / cpp Scene.h)
_SCENE_MAIN_ENTRANCE_Y1 = 10
_SCENE_MAIN_ENTRANCE_X1 = 11
_SCENE_MAIN_ENTRANCE_Y2 = 12
_SCENE_MAIN_ENTRANCE_X2 = 13


@dataclass
class SceneEntrance:
    """One big-map entrance cell for a scene (MainEntrance1 or 2)."""

    scene_id: int
    name: str
    which: int  # 1 or 2
    x: int  # MainEntranceX — engine / .002 X axis
    y: int  # MainEntranceY

    @property
    def label(self) -> str:
        tag = f"#{self.which}" if self.which > 1 else ""
        return f"{self.scene_id}:{self.name}{tag} ({self.x},{self.y})"


def collect_scene_entrances(
    ranger: "RangerArchive",
    *,
    map_size: int = WORLD_SIZE,
) -> List[SceneEntrance]:
    """Collect valid MainEntranceX/Y points from ranger scene metadata."""
    out: List[SceneEntrance] = []
    if ranger is None or ranger.scenes.count == 0:
        return out
    for i in range(ranger.scenes.count):
        rec = ranger.scenes.records[i]
        if len(rec) <= _SCENE_MAIN_ENTRANCE_X2:
            continue
        name = ranger.scene_name(i).strip() or f"场景{i}"
        pairs = (
            (1, rec[_SCENE_MAIN_ENTRANCE_X1], rec[_SCENE_MAIN_ENTRANCE_Y1]),
            (2, rec[_SCENE_MAIN_ENTRANCE_X2], rec[_SCENE_MAIN_ENTRANCE_Y2]),
        )
        for which, x, y in pairs:
            if not (0 <= x < map_size and 0 <= y < map_size):
                continue
            # Skip duplicate MainEntrance2 when it coincides with #1.
            if which == 2 and out and out[-1].scene_id == i and out[-1].x == x and out[-1].y == y:
                continue
            out.append(SceneEntrance(i, name, which, int(x), int(y)))
    return out


class WorldLayerGrid:
    """One 480×480 int16 layer (earth / surface / building / …)."""

    def __init__(self) -> None:
        self.path: Optional[Path] = None
        self.size = WORLD_SIZE
        # [x][y]
        self.grid: List[List[int]] = []

    def load(self, path: str | Path) -> None:
        path = Path(path)
        self.path = path
        raw = path.read_bytes()
        if len(raw) < WORLD_BYTES:
            raise ValueError(f"{path.name} size {len(raw)} < {WORLD_BYTES}")
        self.grid = [[0] * WORLD_SIZE for _ in range(WORLD_SIZE)]
        for x in range(WORLD_SIZE):
            for y in range(WORLD_SIZE):
                off = (x * WORLD_SIZE + y) * 2
                self.grid[x][y] = struct.unpack_from("<h", raw, off)[0]

    def get(self, x: int, y: int) -> int:
        return self.grid[x][y]

    def set(self, x: int, y: int, value: int) -> None:
        self.grid[x][y] = int(value)

    def to_bytes(self) -> bytes:
        out = bytearray()
        for x in range(WORLD_SIZE):
            for y in range(WORLD_SIZE):
                out.extend(struct.pack("<h", self.grid[x][y]))
        return bytes(out)

    def save(self, backup: bool = True) -> None:
        if not self.path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.path)
        atomic_write(self.path, self.to_bytes())

    def copy_rect(self, x0: int, y0: int, x1: int, y1: int) -> List[List[int]]:
        """Inclusive rectangle → [dx][dy] codes (engine axes)."""
        xa, xb = sorted((int(x0), int(x1)))
        ya, yb = sorted((int(y0), int(y1)))
        out: List[List[int]] = []
        for x in range(xa, xb + 1):
            col = []
            for y in range(ya, yb + 1):
                if 0 <= x < self.size and 0 <= y < self.size:
                    col.append(self.get(x, y))
                else:
                    col.append(-1)
            out.append(col)
        return out

    def paste_rect(
        self,
        x0: int,
        y0: int,
        data: List[List[int]],
        *,
        skip_negative: bool = False,
    ) -> int:
        """Paste [dx][dy] at engine (x0,y0). Returns cells written."""
        written = 0
        for dx, col in enumerate(data):
            for dy, val in enumerate(col):
                if skip_negative and int(val) < 0:
                    continue
                x, y = x0 + dx, y0 + dy
                if 0 <= x < self.size and 0 <= y < self.size:
                    self.set(x, y, int(val))
                    written += 1
        return written

    def fill_rect(self, x0: int, y0: int, x1: int, y1: int, value: int) -> int:
        xa, xb = sorted((int(x0), int(x1)))
        ya, yb = sorted((int(y0), int(y1)))
        n = 0
        for x in range(xa, xb + 1):
            for y in range(ya, yb + 1):
                if 0 <= x < self.size and 0 <= y < self.size:
                    self.set(x, y, int(value))
                    n += 1
        return n


def export_layer_region_json(
    layer_key: str,
    grid: WorldLayerGrid,
    x0: int,
    y0: int,
    x1: int,
    y1: int,
) -> dict:
    xa, xb = sorted((int(x0), int(x1)))
    ya, yb = sorted((int(y0), int(y1)))
    data = grid.copy_rect(xa, ya, xb, yb)
    return {
        "format": "kys_world_layer_region_v1",
        "layer": layer_key,
        "x0": xa,
        "y0": ya,
        "x1": xb,
        "y1": yb,
        "width": xb - xa + 1,
        "height": yb - ya + 1,
        "data": data,
    }


def save_layer_region_json(path: str | Path, payload: dict) -> None:
    Path(path).write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def load_layer_region_json(path: str | Path) -> dict:
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(payload, dict) or "data" not in payload:
        raise ValueError("invalid region json")
    return payload


class WorldMapBundle:
    """Load common big-map layers from resource/."""

    LAYER_FILES = (
        ("earth", ("earth.002", "Earth.002")),
        ("surface", ("surface.002", "Surface.002")),
        ("building", ("building.002", "Building.002")),
        ("buildx", ("buildx.002", "Buildx.002")),
        ("buildy", ("buildy.002", "Buildy.002")),
    )

    def __init__(self) -> None:
        self.layers: dict[str, WorldLayerGrid] = {}

    def load(self, resource_dir: str | Path) -> None:
        resource_dir = Path(resource_dir)
        self.layers = {}
        for key, names in self.LAYER_FILES:
            path = None
            for n in names:
                p = resource_dir / n
                if p.is_file():
                    path = p
                    break
            if path is None:
                continue
            grid = WorldLayerGrid()
            grid.load(path)
            self.layers[key] = grid

    @property
    def has_earth(self) -> bool:
        return "earth" in self.layers
