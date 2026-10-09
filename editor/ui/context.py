"""Shared data-root context for editor UI."""

from __future__ import annotations

from pathlib import Path
from typing import List, Optional, Tuple

from PySide6.QtCore import QObject, Signal

from kys_formats.assets import ImageBank, EmptyImageBank, load_heads_bank, load_items_bank
from kys_formats.encoding import normalize_encoding
from kys_formats.profile import GameProfile, detect_profile
from kys_formats.ranger import RangerArchive, RangerLayout
from kys_formats.scene_data import SceneEventData, SceneMapData
from kys_formats.kdef import KdefArchive
from kys_formats.talk import TalkArchive, NameArchive
from kys_formats.war import WarArchive, WarFieldArchive
from kys_formats.rle_tile import (
    RleTilePack,
    find_palette,
    load_palette,
    load_tile_pack_pair,
)
from kys_formats.world_map import WorldMapBundle


class EditorContext(QObject):
    dataRootChanged = Signal(str)
    encodingChanged = Signal(str)
    profileChanged = Signal(str)
    saveSlotChanged = Signal(int)
    statusMessage = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self.data_root: Optional[Path] = None
        self.profile: Optional[GameProfile] = None
        self.ranger: Optional[RangerArchive] = None
        self.template_ranger: Optional[RangerArchive] = None
        self.save_slot: int = 0
        self.text_encoding: str = "auto"
        self.events: Optional[SceneEventData] = None
        self.maps: Optional[SceneMapData] = None
        self.event_template: Optional[SceneEventData] = None
        self.map_template: Optional[SceneMapData] = None
        self.kdef: Optional[KdefArchive] = None
        self.talk: Optional[TalkArchive] = None
        self.names: Optional[NameArchive] = None
        self.war: Optional[WarArchive] = None
        self.warfld: Optional[WarFieldArchive] = None
        self.heads: ImageBank = EmptyImageBank()
        self.items_pic: ImageBank = EmptyImageBank()
        self.palette: Optional[List[Tuple[int, int, int]]] = None
        self.scene_tiles: Optional[RleTilePack] = None  # sdx + smp
        self.battle_tiles: Optional[RleTilePack] = None  # wdx + wmp
        self.mmap_tiles: Optional[RleTilePack] = None  # mmap.idx + mmap.grp
        self.world_map: Optional[WorldMapBundle] = None

    @property
    def save_dir(self) -> Path:
        sub = self.profile.save_subdir if self.profile else "save"
        return self.data_root / sub

    @property
    def resource_dir(self) -> Path:
        sub = self.profile.resource_subdir if self.profile else "resource"
        return self.data_root / sub

    def set_data_root(self, path: str | Path, profile: Optional[GameProfile] = None) -> None:
        self.data_root = Path(path)
        self.profile = profile or detect_profile(self.data_root)
        if self.profile.default_text_encoding and self.profile.default_text_encoding != "auto":
            if self.text_encoding == "auto":
                self.text_encoding = self.profile.default_text_encoding
        self.reload_all()
        self.profileChanged.emit(self.profile.display_name)
        self.dataRootChanged.emit(str(self.data_root))

    def set_text_encoding(self, encoding: str) -> None:
        enc = normalize_encoding(encoding)
        if enc == self.text_encoding:
            self.apply_text_encoding()
            return
        self.text_encoding = enc
        self.apply_text_encoding()
        self.encodingChanged.emit(enc)
        self.statusMessage.emit(f"文本编码: {enc}")

    def apply_text_encoding(self) -> None:
        enc = normalize_encoding(self.text_encoding)
        for arc in (self.talk, self.names, self.ranger, self.template_ranger):
            if arc is None:
                continue
            if hasattr(arc, "text_encoding"):
                arc.text_encoding = enc
            if isinstance(arc, RangerArchive):
                arc._sync_table_encodings()

    def _ranger_layout(self) -> RangerLayout:
        if self.profile:
            return RangerLayout.from_profile(self.profile)
        return RangerLayout()

    def reload_all(self) -> None:
        if not self.data_root:
            return
        if self.profile is None:
            self.profile = detect_profile(self.data_root)

        lay = self._ranger_layout()
        try:
            self.ranger = RangerArchive(lay)
            self.ranger.load(self.save_dir, self.save_slot)
        except Exception as e:
            self.ranger = None
            self.statusMessage.emit(f"Save load error: {e}")

        try:
            self.template_ranger = RangerArchive(lay)
            self.template_ranger.load(self.save_dir, 0)
        except Exception:
            self.template_ranger = self.ranger

        if self.ranger:
            self.statusMessage.emit(
                f"已加载存档槽 {self.save_slot} · {self.profile.display_name} "
                f"(Magic={self.profile.magic_words} Shop={self.profile.shop_words} "
                f"War={self.profile.war.words})"
            )

        try:
            self.event_template = SceneEventData()
            self.event_template.load(SceneEventData.resolve_path(self.save_dir, 0))
        except Exception:
            self.event_template = None

        try:
            self.map_template = SceneMapData()
            self.map_template.load(SceneMapData.resolve_path(self.save_dir, 0))
        except Exception:
            self.map_template = None

        self._load_scene_progress(self.save_slot)

        try:
            self.kdef = KdefArchive()
            self.kdef.load(self.resource_dir)
        except Exception:
            self.kdef = None

        try:
            self.talk = TalkArchive()
            self.talk.load(self.resource_dir)
        except Exception:
            self.talk = None

        try:
            self.names = NameArchive()
            self.names.load(self.resource_dir)
        except Exception:
            self.names = None

        try:
            self.war = WarArchive(self.profile.war)
            self.war.load(self.resource_dir)
        except Exception:
            self.war = None

        try:
            self.warfld = WarFieldArchive()
            self.warfld.load(self.resource_dir)
        except Exception:
            self.warfld = None

        try:
            self.heads = load_heads_bank(self.data_root, self.profile.assets)
        except Exception:
            self.heads = EmptyImageBank()

        try:
            self.items_pic = load_items_bank(self.data_root, self.profile.assets)
        except Exception:
            self.items_pic = EmptyImageBank()

        # Tile libraries + palette for map overview
        self.palette = None
        self.scene_tiles = None
        self.battle_tiles = None
        self.mmap_tiles = None
        self.world_map = None
        try:
            pal_path = find_palette(self.resource_dir)
            if pal_path:
                self.palette = load_palette(pal_path)
        except Exception:
            self.palette = None
        try:
            self.scene_tiles = load_tile_pack_pair(
                self.resource_dir, ("sdx", "SDX"), ("smp", "SMP")
            )
        except Exception:
            self.scene_tiles = None
        try:
            self.battle_tiles = load_tile_pack_pair(
                self.resource_dir, ("wdx", "WDX"), ("wmp", "WMP")
            )
        except Exception:
            self.battle_tiles = None
        try:
            self.mmap_tiles = load_tile_pack_pair(
                self.resource_dir,
                ("mmap.idx", "MMAP.idx", "Mmap.idx"),
                ("mmap.grp", "MMAP.grp", "Mmap.grp"),
            )
        except Exception:
            self.mmap_tiles = None
        try:
            self.world_map = WorldMapBundle()
            self.world_map.load(self.resource_dir)
            if not self.world_map.layers:
                self.world_map = None
        except Exception:
            self.world_map = None

        self.apply_text_encoding()
        self.saveSlotChanged.emit(self.save_slot)

    def _load_scene_progress(self, slot: int) -> None:
        """Load DData/SData for save slot (0 = template alldef/allsin)."""
        try:
            self.events = SceneEventData()
            self.events.load(SceneEventData.resolve_path(self.save_dir, slot))
        except Exception as e:
            self.events = None
            if slot > 0:
                self.statusMessage.emit(f"DData load error (slot {slot}): {e}")

        try:
            self.maps = SceneMapData()
            self.maps.load(SceneMapData.resolve_path(self.save_dir, slot))
        except Exception as e:
            self.maps = None
            if slot > 0:
                self.statusMessage.emit(f"SData load error (slot {slot}): {e}")

    def load_save_slot(self, slot: int) -> None:
        """Load ranger + per-slot DData/SData (alias for set_save_slot)."""
        self.set_save_slot(slot)

    def set_save_slot(self, slot: int) -> None:
        """Switch active save slot; reload ranger and D{n}/S{n} progress files."""
        slot = int(slot)
        self.save_slot = slot
        if not self.data_root:
            self.saveSlotChanged.emit(slot)
            return

        lay = self._ranger_layout()
        try:
            self.ranger = RangerArchive(lay)
            self.ranger.load(self.save_dir, slot)
        except Exception as e:
            self.ranger = None
            self.statusMessage.emit(f"Save load error (slot {slot}): {e}")

        self._load_scene_progress(slot)
        self.apply_text_encoding()
        dname, sname = self.progress_filenames()
        self.statusMessage.emit(
            f"存档槽 {slot} · 剧情进度 {dname} + {sname}"
        )
        self.saveSlotChanged.emit(slot)

    def progress_filenames(self) -> tuple[str, str]:
        from kys_formats.event_progress import progress_file_labels

        return progress_file_labels(self.save_slot)
