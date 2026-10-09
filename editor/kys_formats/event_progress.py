"""Per-save event progress: compare slot DData to new-game template (alldef)."""

from __future__ import annotations

from typing import List, Optional, Tuple

from .scene_data import EVENT_WORDS, SceneEventData

# Words that define static layout in template; changes elsewhere imply story progress.
_LAYOUT_WORDS = frozenset({9, 10})


def event_row_equal(a: List[int], b: List[int]) -> bool:
    if len(a) < EVENT_WORDS or len(b) < EVENT_WORDS:
        return list(a[:EVENT_WORDS]) == list(b[:EVENT_WORDS])
    return a[:EVENT_WORDS] == b[:EVENT_WORDS]


def event_runtime_changed(
    template_ev: List[int],
    current_ev: List[int],
    *,
    ignore_layout: bool = True,
) -> bool:
    """True if save row differs from template in any non-layout word."""
    for w in range(EVENT_WORDS):
        if ignore_layout and w in _LAYOUT_WORDS:
            continue
        if int(template_ev[w]) != int(current_ev[w]):
            return True
    return False


def event_progress_flag(
    template: Optional[SceneEventData],
    current: Optional[SceneEventData],
    scene: int,
    event_id: int,
) -> int:
    """0 = same as template (not advanced); 1 = save differs (story touched)."""
    if not template or not current:
        return -1
    if scene >= template.scene_count or scene >= current.scene_count:
        return -1
    if event_id < 0 or event_id >= 200:
        return -1
    tpl = template.scenes[scene][event_id]
    cur = current.scenes[scene][event_id]
    return 1 if event_runtime_changed(tpl, cur) else 0


def format_condition_hint(condition: int) -> str:
    """Engine: DData[0] — 0 常配合踩上脚本自动执行；1 为常见挂接态。"""
    c = int(condition)
    if c == 0:
        return "条件=0（可自动执行：踩上脚本[4]>0 时引擎会跑）"
    if c == 1:
        return "条件=1（常见初始/挂接态，非「未发生」专用位）"
    return f"条件={c}"


def progress_file_labels(slot: int) -> Tuple[str, str]:
    if slot <= 0:
        return "alldef.grp", "allsin.grp"
    return f"D{slot}.grp", f"S{slot}.grp"
