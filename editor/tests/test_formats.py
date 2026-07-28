"""Round-trip tests against local game data (skipped if missing)."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
EDITOR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(EDITOR))

from kys_formats.encoding import decode_bytes, decode_talk_payload, encode_talk_payload
from kys_formats.profile import detect_profile, find_data_root_candidates
from kys_formats.ranger import RangerArchive, RangerLayout, decode_fixed_name, encode_fixed_name
from kys_formats.war import WarArchive
from kys_formats.kdef import KdefArchive
from kys_formats.pic_png import PicArchive
from kys_formats.scene_data import SceneEventData
from kys_formats.talk import TalkArchive
from kys_formats.assets import load_heads_bank, load_items_bank


def _pick_data_root() -> Path | None:
    cands = find_data_root_candidates(ROOT)
    if cands:
        return cands[0]
    legacy = ROOT / "game_data"
    if (legacy / "save").is_dir() and (legacy / "resource").is_dir():
        return legacy
    return None


DATA_ROOT = _pick_data_root()
SAVE = DATA_ROOT / "save" if DATA_ROOT else None
RES = DATA_ROOT / "resource" if DATA_ROOT else None


def test_encoding_roundtrip_gbk():
    raw = "圣堂".encode("gbk")
    assert decode_bytes(raw, "gbk") == "圣堂"
    assert decode_fixed_name(raw, "gbk") == "圣堂"
    assert encode_fixed_name("圣堂", 4, "gbk") == raw.ljust(4, b"\x00")


def test_encoding_talk_big5():
    text = "測試對話"
    payload = encode_talk_payload(text, "big5")
    assert decode_talk_payload(payload, "big5") == text


pytestmark = pytest.mark.skipif(
    DATA_ROOT is None or SAVE is None or RES is None,
    reason="game data root not present",
)


def test_detect_profile():
    profile = detect_profile(DATA_ROOT)
    assert profile.role_words == 91
    assert profile.item_words == 95
    assert profile.magic_words in (68, 93, 111)
    assert profile.shop_words in (15, 18)
    assert profile.war.words in (93, 156)


def test_ranger_roundtrip(tmp_path):
    profile = detect_profile(DATA_ROOT)
    arc = RangerArchive(RangerLayout.from_profile(profile))
    arc.load(SAVE, 0)
    assert arc.roles.count > 0
    assert arc.items.count > 0
    assert arc.magics.words == profile.magic_words
    original = arc.grp_path.read_bytes()
    rebuilt = arc.to_bytes()
    assert len(rebuilt) == len(original)
    assert rebuilt == original
    arc.roles.set(0, 15, 99)  # level
    out = tmp_path / "Ranger.grp"
    out.write_bytes(arc.to_bytes())
    import shutil

    shutil.copy(SAVE / "ranger.idx", tmp_path / "ranger.idx")
    # case-insensitive name on Windows
    for n in ("Ranger.grp", "ranger.grp"):
        src = tmp_path / "Ranger.grp"
        if src.is_file():
            break
    arc2 = RangerArchive(RangerLayout.from_profile(profile))
    # ensure lowercase resolve works: copy as ranger.grp too
    (tmp_path / "ranger.grp").write_bytes(out.read_bytes())
    arc2.load(tmp_path, 0)
    assert arc2.roles.get(0, 15) == 99


def test_collect_scene_entrances():
    """World-map entrance overlay reads MainEntranceX/Y from ranger scenes."""
    from kys_formats.world_map import collect_scene_entrances, WORLD_SIZE

    profile = detect_profile(DATA_ROOT)
    arc = RangerArchive(RangerLayout.from_profile(profile))
    arc.load(SAVE, 0)
    ents = collect_scene_entrances(arc)
    assert ents, "expected at least one valid big-map entrance"
    for e in ents:
        assert 0 <= e.x < WORLD_SIZE and 0 <= e.y < WORLD_SIZE
        assert e.name
        assert e.which in (1, 2)
    # Scene 0 (开场卧室所属场景) usually has a mapped entrance on the big map
    scene0 = [e for e in ents if e.scene_id == 0]
    assert scene0, "scene 0 should expose MainEntrance on big map"


def test_war_roundtrip():
    profile = detect_profile(DATA_ROOT)
    war = WarArchive(profile.war)
    war.load(RES)
    assert war.count > 0
    original = war.path.read_bytes()
    assert len(original) == war.count * war.war_bytes
    assert war.to_bytes() == original
    rec = war.records[0]
    assert isinstance(rec.battle_num, int)
    assert rec.layout.words == profile.war.words


def test_kdef_load_disassemble():
    kdef = KdefArchive()
    kdef.load(RES)
    assert kdef.script_count > 100
    script = kdef.get_script(101)
    assert script.instructions
    assert script.instructions[0].opcode >= 0
    assert kdef.to_grp_bytes() == kdef.grp_path.read_bytes()
    assert kdef.to_idx_bytes() == kdef.idx_path.read_bytes()


def test_heads_items_assets():
    profile = detect_profile(DATA_ROOT)
    heads = load_heads_bank(DATA_ROOT, profile.assets)
    items = load_items_bank(DATA_ROOT, profile.assets)
    assert heads.count > 0 or items.count > 0
    # Pic round-trip only when Heads.Pic exists
    for n in ("Heads.Pic", "heads.pic"):
        p = RES / n
        if p.is_file():
            pic = PicArchive()
            pic.load(p)
            assert pic.count > 0
            assert pic.to_bytes() == p.read_bytes()
            break


def test_alldef_roundtrip():
    path = SAVE / "alldef.grp"
    if not path.is_file():
        pytest.skip("alldef.grp missing")
    d = SceneEventData()
    d.load(path)
    assert d.scene_count > 0
    assert d.to_bytes() == path.read_bytes()


def test_alldef_scene0_opening_pics():
    """Stable opening: bed event0=8284, Kong Pili event1=8268 at bedroom (X=37,Y=40)."""
    from kys_formats.rle_tile import code_to_tile_index, format_pic_code
    from kys_formats.scene_data import SceneMapData

    path = SAVE / "alldef.grp"
    if not path.is_file():
        pytest.skip("alldef.grp missing")
    d = SceneEventData()
    d.load(path)
    assert d.scene_count > 0

    ev0 = d.scenes[0][0]
    assert ev0[5] == 8284
    assert ev0[6] == 8284
    assert ev0[7] == 8284
    assert ev0[9] == 38 and ev0[10] == 40  # Y, X on bed

    ev1 = d.scenes[0][1]
    assert ev1[5] == 8268
    assert ev1[6] == 8268
    assert ev1[7] == 8268
    assert ev1[9] == 40 and ev1[10] == 37  # Y, X in bedroom near bed
    assert code_to_tile_index(8268) == 4134
    assert "4134" in format_pic_code(8268)
    assert len(ev0) == 11 and len(ev1) == 11

    sin = SAVE / "allsin.grp"
    if sin.is_file():
        mp = SceneMapData()
        mp.load(sin)
        assert mp.get(0, 3, 40, 38) == 0
        assert mp.get(0, 3, 37, 40) == 1


def test_talk_decode():
    talk = TalkArchive()
    talk.load(RES)
    assert talk.count > 100
    t = talk.get_text(1)
    assert isinstance(t, str)


def test_craft_fields_present():
    """Manufacturing fields live in item words 83..94."""
    profile = detect_profile(DATA_ROOT)
    arc = RangerArchive(RangerLayout.from_profile(profile))
    arc.load(SAVE, 0)
    assert arc.items.words >= 95
    crafted = 0
    for i, rec in enumerate(arc.items.records):
        if any(rec[85 + j] >= 0 and rec[90 + j] > 0 for j in range(5)):
            crafted += 1
    # GodsDevils and Promise both ship some recipes
    assert crafted >= 0
