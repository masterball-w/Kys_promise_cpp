#!/usr/bin/env python3
"""Capture UI screenshots for README (requires local game data next to editor/)."""

from __future__ import annotations

import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from PySide6.QtWidgets import QApplication, QTabWidget

from main import MainWindow
from kys_formats.profile import find_data_root_candidates

OUT = ROOT / "docs" / "screenshots"


def _pump(app: QApplication, seconds: float = 0.15) -> None:
    end = time.time() + seconds
    while time.time() < end:
        app.processEvents()
        time.sleep(0.01)


def _grab(widget, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pix = widget.grab()
    ok = pix.save(str(path), "PNG")
    print(("OK" if ok else "FAIL"), path.name, f"{pix.width()}x{pix.height()}")


def main() -> int:
    # Clean previous set
    if OUT.is_dir():
        for p in OUT.glob("*.png"):
            p.unlink()

    app = QApplication.instance() or QApplication(sys.argv)
    win = MainWindow()
    win.resize(1280, 860)
    win.show()
    _pump(app, 0.25)

    for cand in find_data_root_candidates(ROOT.parent):
        win._apply_root(cand)
        break
    _pump(app, 0.5)

    def snap(name: str) -> None:
        _pump(app, 0.18)
        _grab(win, OUT / name)

    snap("00_main_overview.png")

    main_names = [
        "01_save_data",
        "02_events",
        "03_battle",
        "04_world_map",
        "05_assets",
        "06_crossref",
    ]
    for i, base in enumerate(main_names):
        if i >= win.tabs.count():
            break
        win.tabs.setCurrentIndex(i)
        _pump(app, 0.25)
        snap(f"{base}.png")

    # Save sub-pages (ASCII names only)
    win.tabs.setCurrentIndex(0)
    st = win.save_editor.tabs
    mapping = {
        "人物": "01_save__roles.png",
        "物品": "01_save__items.png",
        "武功": "01_save__magic.png",
        "背包": "01_save__inventory.png",
        "商店": "01_save__shops.png",
        "场景": "01_save__scenes.png",
        "Header": "01_save__header.png",
        "头": "01_save__header.png",
    }
    for j in range(st.count()):
        st.setCurrentIndex(j)
        text = st.tabText(j)
        for key, fname in mapping.items():
            if key in text:
                snap(fname)
                break

    # Events: script 81 + SData map
    win.tabs.setCurrentIndex(1)
    et = win.event_editor.tabs
    for j in range(et.count()):
        if "脚本" in et.tabText(j):
            et.setCurrentIndex(j)
            if win.event_editor.script_list.count() > 80:
                win.event_editor.script_list.setCurrentRow(80)
            snap("02_events__script.png")
        if "对话" in et.tabText(j):
            et.setCurrentIndex(j)
            snap("02_events__talk.png")
        if "挂接" in et.tabText(j) or "DData" in et.tabText(j) or "场景事件" in et.tabText(j):
            et.setCurrentIndex(j)
            snap("02_events__ddata.png")
        if "SData" in et.tabText(j) or "事件层" in et.tabText(j):
            et.setCurrentIndex(j)
            _pump(app, 0.35)
            snap("02_events__sdata_map.png")

    # Battle field map
    win.tabs.setCurrentIndex(2)
    bt = win.battle_editor.tabs
    for j in range(bt.count()):
        if "列表" in bt.tabText(j):
            bt.setCurrentIndex(j)
            snap("03_battle__list.png")
        if "编辑" in bt.tabText(j):
            bt.setCurrentIndex(j)
            snap("03_battle__edit.png")
        if "地形" in bt.tabText(j):
            bt.setCurrentIndex(j)
            _pump(app, 0.35)
            snap("03_battle__field_map.png")

    # World map
    win.tabs.setCurrentIndex(3)
    _pump(app, 0.45)
    snap("04_world_map.png")

    # Assets / crossref already covered as main tabs; re-snap assets after open smp if possible
    win.tabs.setCurrentIndex(4)
    at = win.asset_editor.tabs
    for j in range(at.count()):
        at.setCurrentIndex(j)
        text = at.tabText(j)
        if "常用" in text or "贴图包" in text:
            snap("05_assets__common.png")
        elif "物品" in text:
            snap("05_assets__items.png")
        elif "战斗" in text:
            snap("05_assets__fight.png")
        elif "特效" in text:
            snap("05_assets__eft.png")
        elif "砖" in text or "RLE" in text:
            snap("05_assets__tiles.png")
        elif "联动" in text:
            snap("05_assets__link.png")

    win.tabs.setCurrentIndex(5)
    snap("06_crossref.png")

    win.close()
    print(f"done -> {OUT} ({len(list(OUT.glob('*.png')))} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
