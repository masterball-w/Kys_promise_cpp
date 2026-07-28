"""Editable combo for resource IDs with Chinese name labels (magic / item / role / scene)."""

from __future__ import annotations

import re
from typing import List, Optional, Tuple

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import QComboBox, QCompleter, QWidget


def format_named_id(oid: int, name: str, none_value: int = -1) -> str:
    if oid == none_value or oid < 0 and oid == none_value:
        return f"{none_value} — (无)"
    label = (name or "").strip() or "(无名)"
    return f"{oid} — {label}"


def parse_named_id(text: str, none_value: int = -1) -> int:
    text = (text or "").strip()
    if not text:
        return none_value
    m = re.match(r"^(-?\d+)", text)
    if not m:
        raise ValueError(f"无法解析 ID: {text!r}")
    return int(m.group(1))


class NamedIdCombo(QComboBox):
    """Dropdown showing `id — 中文名`; editable so numbers can be typed directly."""

    idChanged = Signal(int)

    def __init__(
        self,
        kind: str,
        *,
        allow_none: bool = True,
        none_value: int = -1,
        parent: Optional[QWidget] = None,
    ) -> None:
        super().__init__(parent)
        if kind not in ("magic", "item", "role", "scene"):
            raise ValueError(kind)
        self.kind = kind
        self.allow_none = allow_none
        self.none_value = none_value
        self.max_count: Optional[int] = None
        self.setEditable(True)
        self.setInsertPolicy(QComboBox.NoInsert)
        self.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
        self.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToMinimumContentsLengthWithIcon)
        self.setMinimumContentsLength(18)
        completer = QCompleter(self.model(), self)
        completer.setCompletionMode(QCompleter.CompletionMode.PopupCompletion)
        completer.setFilterMode(Qt.MatchFlag.MatchContains)
        completer.setCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
        self.setCompleter(completer)
        self.activated.connect(self._emit_committed)
        le = self.lineEdit()
        if le is not None:
            le.editingFinished.connect(self._emit_committed)
        self._block_emit = False

    def rebuild(self, options: List[Tuple[int, str]]) -> None:
        """options: list of (id, name). Always includes none_value when allow_none."""
        cur = self.get_id(silent=True)
        self._block_emit = True
        self.blockSignals(True)
        self.clear()
        seen = set()
        if self.allow_none:
            self.addItem(format_named_id(self.none_value, "(无)", self.none_value), self.none_value)
            seen.add(self.none_value)
        for oid, name in options:
            if oid in seen:
                continue
            self.addItem(format_named_id(oid, name, self.none_value), oid)
            seen.add(oid)
        self.blockSignals(False)
        self._block_emit = False
        self.set_id(cur)

    def set_id(self, value: int) -> None:
        self._block_emit = True
        self.blockSignals(True)
        idx = self.findData(value)
        if idx >= 0:
            self.setCurrentIndex(idx)
        else:
            # Unknown id: show bare number (still editable)
            self.setEditText(str(value))
        self.blockSignals(False)
        self._block_emit = False

    def get_id(self, silent: bool = False) -> int:
        try:
            return parse_named_id(self.currentText(), self.none_value)
        except ValueError:
            if silent:
                return self.none_value
            raise

    def _emit_committed(self, *_args) -> None:
        if self._block_emit:
            return
        try:
            oid = self.get_id()
        except ValueError:
            return
        # Normalize label when known
        idx = self.findData(oid)
        self._block_emit = True
        self.blockSignals(True)
        if idx >= 0:
            self.setCurrentIndex(idx)
        else:
            self.setEditText(str(oid))
        self.blockSignals(False)
        self._block_emit = False
        self.idChanged.emit(oid)


def collect_magic_options(ctx) -> List[Tuple[int, str]]:
    arc = getattr(ctx, "ranger", None)
    if not arc:
        return []
    return [(i, arc.magic_name(i)) for i in range(arc.magics.count)]


def collect_item_options(ctx) -> List[Tuple[int, str]]:
    arc = getattr(ctx, "ranger", None)
    if not arc:
        return []
    return [(i, arc.item_name(i)) for i in range(arc.items.count)]


def collect_role_options(ctx) -> List[Tuple[int, str]]:
    arc = getattr(ctx, "ranger", None)
    if not arc:
        return []
    return [(i, arc.role_name(i)) for i in range(arc.roles.count)]


def collect_scene_options(ctx, max_count: Optional[int] = None) -> List[Tuple[int, str]]:
    arc = getattr(ctx, "template_ranger", None) or getattr(ctx, "ranger", None)
    if not arc:
        return []
    count = arc.scenes.count
    if max_count is not None:
        count = min(count, max_count)
    return [(i, arc.scene_name(i)) for i in range(max(0, count))]


def rebuild_named_combos(combos: List[NamedIdCombo], ctx) -> None:
    magic_opts = item_opts = role_opts = scene_opts = None
    for cb in combos:
        if cb.kind == "magic":
            if magic_opts is None:
                magic_opts = collect_magic_options(ctx)
            cb.rebuild(magic_opts)
        elif cb.kind == "item":
            if item_opts is None:
                item_opts = collect_item_options(ctx)
            cb.rebuild(item_opts)
        elif cb.kind == "role":
            if role_opts is None:
                role_opts = collect_role_options(ctx)
            cb.rebuild(role_opts)
        elif cb.kind == "scene":
            if scene_opts is None:
                scene_opts = collect_scene_options(ctx, max_count=cb.max_count)
            cb.rebuild(scene_opts)
