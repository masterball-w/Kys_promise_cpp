"""Round-trip tests against local game_data (skipped if missing)."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
GAME_DATA = ROOT / "game_data"
SAVE = GAME_DATA / "save"
RES = GAME_DATA / "resource"

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from kys_formats.ranger import RangerArchive
from kys_formats.war import WarArchive, WAR_BYTES
from kys_formats.kdef import KdefArchive
from kys_formats.pic_png import PicArchive
from kys_formats.scene_data import SceneEventData, EVENT_BYTES
from kys_formats.talk import TalkArchive


pytestmark = pytest.mark.skipif(
    not SAVE.is_dir() or not RES.is_dir(),
    reason="game_data not present",
)


def test_ranger_roundtrip(tmp_path):
    arc = RangerArchive()
    arc.load(SAVE, 0)
    assert arc.roles.count > 0
    assert arc.items.count > 0
    original = arc.grp_path.read_bytes()
    rebuilt = arc.to_bytes()
    assert len(rebuilt) == len(original)
    assert rebuilt == original
    # mutate and restore
    name0 = arc.role_name(0)
    arc.roles.set(0, 15, 99)  # level
    out = tmp_path / "Ranger.grp"
    out.write_bytes(arc.to_bytes())
    arc2 = RangerArchive()
    # need idx alongside
    import shutil

    shutil.copy(SAVE / "ranger.idx", tmp_path / "ranger.idx")
    arc2.load(tmp_path, 0)
    assert arc2.roles.get(0, 15) == 99


def test_war_roundtrip():
    war = WarArchive()
    war.load(RES)
    assert war.count > 0
    original = war.path.read_bytes()
    assert len(original) == war.count * WAR_BYTES
    assert war.to_bytes() == original
    rec = war.records[0]
    assert isinstance(rec.battle_num, int)


def test_kdef_load_disassemble():
    kdef = KdefArchive()
    kdef.load(RES)
    assert kdef.script_count > 100
    script = kdef.get_script(101)
    assert script.instructions
    assert script.instructions[0].opcode >= 0
    # round-trip bytes without rebuild
    assert kdef.to_grp_bytes() == kdef.grp_path.read_bytes()
    assert kdef.to_idx_bytes() == kdef.idx_path.read_bytes()


def test_pic_roundtrip():
    heads = None
    for n in ("Heads.Pic", "heads.pic"):
        p = RES / n
        if p.is_file():
            heads = p
            break
    if heads is None:
        pytest.skip("Heads.Pic missing")
    pic = PicArchive()
    pic.load(heads)
    assert pic.count > 0
    original = heads.read_bytes()
    rebuilt = pic.to_bytes()
    # Allow minor size difference only if empty trailing; prefer exact
    assert rebuilt == original


def test_alldef_roundtrip():
    path = SAVE / "alldef.grp"
    if not path.is_file():
        pytest.skip("alldef.grp missing")
    d = SceneEventData()
    d.load(path)
    assert d.scene_count > 0
    assert d.to_bytes() == path.read_bytes()


def test_talk_decode():
    talk = TalkArchive()
    talk.load(RES)
    assert talk.count > 100
    t = talk.get_text(1)
    assert isinstance(t, str)


def test_talk_gbk_not_big5_mojibake():
    """GBK talk bytes must not be shown as Big5 mojibake (e.g. id 2764)."""
    talk = TalkArchive()
    talk.load(RES)
    if talk.count < 2764:
        pytest.skip("talk archive too short")
    text = talk.get_text(2764)
    assert "斕岆掞" not in text
    assert "你是" in text
    assert "地方" in text
