"""Read-only story linkage check for KYS Promise data.

Usage (repo root):
  python .cursor/skills/add-kys-story/scripts/inspect_story.py --scene 0 --event 0
  python .cursor/skills/add-kys-story/scripts/inspect_story.py --scene 0 --event 1 --data-root game_data
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[4]
EDITOR = REPO / "editor"
sys.path.insert(0, str(EDITOR))

from kys_formats.kdef import KdefArchive  # noqa: E402
from kys_formats.scene_data import SceneEventData, SceneMapData  # noqa: E402
from kys_formats.talk import TalkArchive  # noqa: E402

DDATA_NAMES = (
    "cond0",
    "spare1",
    "manual_script",
    "item_script",
    "step_script",
    "pic_now",
    "pic_end",
    "pic_start",
    "spare8",
    "y",
    "x",
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Inspect one scene event linkage")
    parser.add_argument("--scene", type=int, required=True)
    parser.add_argument("--event", type=int, required=True)
    parser.add_argument("--data-root", type=Path, default=REPO / "game_data")
    parser.add_argument("--slot", type=int, default=0)
    args = parser.parse_args()

    root = args.data_root
    save = root / "save"
    resource = root / "resource"
    dpath = SceneEventData.resolve_path(save, args.slot)
    spath = SceneMapData.resolve_path(save, args.slot)

    ddata = SceneEventData()
    ddata.load(dpath)
    smap = SceneMapData()
    smap.load(spath)
    kdef = KdefArchive()
    kdef.load(resource)
    talk = TalkArchive()
    talk.load(resource)

    ev = ddata.scenes[args.scene][args.event]
    print(f"slot={args.slot} scene={args.scene} event={args.event}")
    print(f"ddata={dpath.name} sdata={spath.name}")
    for i, name in enumerate(DDATA_NAMES):
        print(f"  [{i}] {name}={ev[i]}")

    ex, ey = int(ev[10]), int(ev[9])
    cells = []
    for x in range(64):
        for y in range(64):
            if smap.get(args.scene, 3, x, y) == args.event:
                cells.append((x, y))
    print(f"layer3_cells={cells}")
    if (ex, ey) not in cells and any(v > 0 for v in ev[2:5]):
        print(f"MISMATCH ddata_xy=({ex},{ey}) not on layer 3")
    else:
        print("layer3_xy=OK")

    for label, sid in (("manual", ev[2]), ("item", ev[3]), ("step", ev[4])):
        if sid <= 0:
            print(f"script {label}: empty")
            continue
        script = kdef.get_script(int(sid))
        ops = []
        for ins in script.instructions[:12]:
            ops.append(f"{ins.opcode}:{ins.args}")
        print(f"script {label} id={sid} ops={' | '.join(ops)}")
        for ins in script.instructions:
            if ins.opcode == 1 and ins.args:
                tid = int(ins.args[0])
                text = talk.get_text(tid).replace("\n", " / ")
                print(f"  talk {tid}: {text[:60]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
