"""Shared data-root context for editor UI."""

from __future__ import annotations

from pathlib import Path
from typing import Optional

from PySide6.QtCore import QObject, Signal

from kys_formats.ranger import RangerArchive
from kys_formats.scene_data import SceneEventData, SceneMapData
from kys_formats.kdef import KdefArchive
from kys_formats.talk import TalkArchive, NameArchive
from kys_formats.war import WarArchive, WarFieldArchive
from kys_formats.pic_png import PicArchive


class EditorContext(QObject):
    dataRootChanged = Signal(str)
    statusMessage = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self.data_root: Optional[Path] = None
        self.ranger: Optional[RangerArchive] = None
        self.save_slot: int = 0
        self.events: Optional[SceneEventData] = None
        self.maps: Optional[SceneMapData] = None
        self.kdef: Optional[KdefArchive] = None
        self.talk: Optional[TalkArchive] = None
        self.names: Optional[NameArchive] = None
        self.war: Optional[WarArchive] = None
        self.warfld: Optional[WarFieldArchive] = None
        self.heads: Optional[PicArchive] = None
        self.items_pic: Optional[PicArchive] = None

    @property
    def save_dir(self) -> Path:
        return self.data_root / "save"

    @property
    def resource_dir(self) -> Path:
        return self.data_root / "resource"

    def set_data_root(self, path: str | Path) -> None:
        self.data_root = Path(path)
        self.reload_all()
        self.dataRootChanged.emit(str(self.data_root))

    def reload_all(self) -> None:
        if not self.data_root:
            return
        try:
            self.ranger = RangerArchive()
            self.ranger.load(self.save_dir, self.save_slot)
            self.statusMessage.emit(f"Loaded save slot {self.save_slot}")
        except Exception as e:
            self.ranger = None
            self.statusMessage.emit(f"Save load error: {e}")

        try:
            self.events = SceneEventData()
            self.events.load(self.save_dir / "alldef.grp")
        except Exception:
            self.events = None

        try:
            self.maps = SceneMapData()
            self.maps.load(self.save_dir / "allsin.grp")
        except Exception:
            self.maps = None

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
            self.war = WarArchive()
            self.war.load(self.resource_dir)
        except Exception:
            self.war = None

        try:
            self.warfld = WarFieldArchive()
            self.warfld.load(self.resource_dir)
        except Exception:
            self.warfld = None

        try:
            self.heads = PicArchive()
            heads_path = self.resource_dir / "Heads.Pic"
            if not heads_path.is_file():
                heads_path = self.resource_dir / "heads.pic"
            self.heads.load(heads_path)
        except Exception:
            self.heads = None

        try:
            self.items_pic = PicArchive()
            items_path = self.resource_dir / "Items.Pic"
            if not items_path.is_file():
                items_path = self.resource_dir / "items.pic"
            self.items_pic.load(items_path)
        except Exception:
            self.items_pic = None

    def load_save_slot(self, slot: int) -> None:
        self.save_slot = slot
        if not self.data_root:
            return
        self.ranger = RangerArchive()
        self.ranger.load(self.save_dir, slot)
        self.statusMessage.emit(f"Loaded save slot {slot}")
