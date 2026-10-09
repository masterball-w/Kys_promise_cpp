"""Rollback triggered scene events using template DData/SData + kdef script graph."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Set, Tuple

from .kdef import KdefArchive
from .scene_data import EVENTS_PER_SCENE, SceneEventData, SceneMapData

SceneEventKey = Tuple[int, int]  # (scene_id, event_id)


@dataclass
class RollbackResult:
    scenes_touched: Set[int]
    events_reset: Set[SceneEventKey]
    scripts_scanned: Set[int]


def resolve_modify_scene(scene_arg: int, context_scene: int) -> int:
    """Match EventManager::Instruct_ModifyEvent scene resolution (-2 / -1 → current)."""
    if scene_arg in (-2, -1):
        return context_scene
    return scene_arg


def _script_ids_on_event(ev: List[int]) -> List[int]:
    out: List[int] = []
    for word in (2, 3, 4):
        sid = int(ev[word])
        if sid > 0:
            out.append(sid)
    return out


def collect_modify_targets_from_script(
    kdef: KdefArchive,
    script_id: int,
    context_scene: int,
    context_event: int,
) -> Set[SceneEventKey]:
    """All (scene, event) pairs touched by ModifyEvent in this script (linear scan)."""
    if script_id <= 0 or script_id > kdef.script_count:
        return set()

    targets: Set[SceneEventKey] = set()
    try:
        script = kdef.get_script(script_id)
    except IndexError:
        return set()

    for ins in script.instructions:
        if ins.opcode != 3 or len(ins.args) < 2:
            continue
        scene = resolve_modify_scene(int(ins.args[0]), context_scene)
        event_id = int(ins.args[1])
        if event_id == -2:
            event_id = context_event
        if scene < 0 or event_id < 0 or event_id >= EVENTS_PER_SCENE:
            continue
        targets.add((scene, event_id))
    return targets


def collect_related_rollback_targets(
    kdef: KdefArchive,
    scene: int,
    event_id: int,
    events: SceneEventData,
    *,
    include_script_graph: bool = True,
) -> Tuple[Set[SceneEventKey], Set[int]]:
    """Events to restore: the slot itself + ModifyEvent targets from linked scripts."""
    targets: Set[SceneEventKey] = {(scene, event_id)}
    scripts: Set[int] = set()
    if event_id < 0 or event_id >= EVENTS_PER_SCENE or scene >= events.scene_count:
        return targets, scripts

    ev = events.scenes[scene][event_id]
    root_scripts = _script_ids_on_event(ev)
    scripts.update(root_scripts)

    if include_script_graph and kdef:
        visited_scripts: Set[int] = set()
        queue = list(root_scripts)
        while queue:
            sid = queue.pop(0)
            if sid in visited_scripts or sid <= 0:
                continue
            visited_scripts.add(sid)
            targets |= collect_modify_targets_from_script(
                kdef, sid, scene, event_id
            )
            try:
                script = kdef.get_script(sid)
            except IndexError:
                continue
            for ins in script.instructions:
                if ins.opcode != 3 or len(ins.args) < 7:
                    continue
                # args[2+i] → DData[i]; script ids live at DData[2..4] → args[4..6]
                for arg_idx in (4, 5, 6):
                    val = int(ins.args[arg_idx])
                    if val > 0 and val not in visited_scripts:
                        queue.append(val)

    return targets, scripts


def restore_events_from_template(
    events: SceneEventData,
    maps: Optional[SceneMapData],
    template_events: SceneEventData,
    template_maps: Optional[SceneMapData],
    targets: Iterable[SceneEventKey],
) -> RollbackResult:
    """Copy DData (+ SData layer 3 per scene) from template for listed events."""
    scenes_touched: Set[int] = set()
    reset: Set[SceneEventKey] = set()

    for scene, eid in targets:
        if scene >= events.scene_count or eid >= EVENTS_PER_SCENE:
            continue
        if scene >= template_events.scene_count:
            continue
        events.scenes[scene][eid] = list(template_events.scenes[scene][eid])
        scenes_touched.add(scene)
        reset.add((scene, eid))

    if maps and template_maps:
        for scene in scenes_touched:
            if scene >= maps.scene_count or scene >= template_maps.scene_count:
                continue
            for x in range(64):
                for y in range(64):
                    maps.maps[scene][3][x][y] = template_maps.maps[scene][3][x][y]

    return RollbackResult(scenes_touched=scenes_touched, events_reset=reset, scripts_scanned=set())


def rollback_event(
    kdef: Optional[KdefArchive],
    events: SceneEventData,
    maps: Optional[SceneMapData],
    template_events: SceneEventData,
    template_maps: Optional[SceneMapData],
    scene: int,
    event_id: int,
    *,
    include_related: bool = True,
) -> RollbackResult:
    targets, scripts = collect_related_rollback_targets(
        kdef, scene, event_id, events, include_script_graph=include_related
    )
    result = restore_events_from_template(
        events, maps, template_events, template_maps, targets
    )
    result.scripts_scanned = scripts
    return result
