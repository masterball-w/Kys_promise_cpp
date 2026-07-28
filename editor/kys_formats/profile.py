"""Game / mod profiles: record widths, paths, and asset layouts.

The editor targets the shared KYS-family engine. Individual games differ in
Ranger table word sizes, War.sta layout, inventory length, and how portraits /
item icons / fight / eft art are stored.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class WarLayout:
    """Offsets into one War.sta record (int16 words)."""

    words: int
    # mate slots: either a single list, or auto + manual (classic)
    mate_count: int
    enemy_count: int
    mate_off: int
    mate_x_off: int
    mate_y_off: int
    enemy_off: int
    enemy_x_off: int
    enemy_y_off: int
    # optional second mate list (classic AutoTeamMate); -1 if unused
    auto_mate_off: int = -1
    auto_mate_count: int = 0
    bout_event_off: int = -1
    operation_event_off: int = -1
    get_kongfu_off: int = -1
    get_kongfu_count: int = 0
    get_items_off: int = -1
    get_items_count: int = 0
    get_money_off: int = -1


# 金庸群侠前传 / Kys Promise
WAR_LAYOUT_PROMISE = WarLayout(
    words=156,
    mate_count=12,
    enemy_count=30,
    mate_off=9,
    auto_mate_off=21,
    auto_mate_count=12,
    mate_x_off=33,
    mate_y_off=45,
    enemy_off=57,
    enemy_x_off=87,
    enemy_y_off=117,
    bout_event_off=147,
    operation_event_off=148,
    get_kongfu_off=149,
    get_kongfu_count=3,
    get_items_off=152,
    get_items_count=3,
    get_money_off=155,
)

# 经典 KYS（含本仓库天龙八部 / GodsDevils）
WAR_LAYOUT_CLASSIC = WarLayout(
    words=93,
    mate_count=6,
    enemy_count=20,
    auto_mate_off=9,
    auto_mate_count=6,
    mate_off=15,
    mate_x_off=21,
    mate_y_off=27,
    enemy_off=33,
    enemy_x_off=53,
    enemy_y_off=73,
)


@dataclass(frozen=True)
class AssetPaths:
    """How art is located relative to data_root."""

    # "pic" = resource/Heads.Pic style; "png_dir" = head/0.png; "none" = unavailable
    heads_mode: str = "pic"
    heads_pic: str = "resource/Heads.Pic"
    heads_pic_alt: str = "resource/heads.pic"
    heads_dir: str = "head"

    items_mode: str = "pic"
    items_pic: str = "resource/Items.Pic"
    items_pic_alt: str = "resource/items.pic"
    items_dir: str = "item"

    # "pic_tree" = fight/NNN/MM.pic; "idx_grp" = fight/fightNNN.idx+grp
    fight_mode: str = "pic_tree"
    fight_pic_fmt: str = "fight/{head:03d}/{mode:02d}.pic"
    fight_idx_fmt: str = "fight/fight{head:03d}.idx"
    fight_grp_fmt: str = "fight/fight{head:03d}.grp"

    # "pic_file" = eft/eftNNN.pic; "idx_grp" = resource/eft.idx+grp (frame index = AmiNum)
    eft_mode: str = "pic_file"
    eft_pic_fmt: str = "eft/eft{ami:03d}.pic"
    eft_pic_fmt_alt: str = "eft/eft{ami}.pic"
    eft_idx: str = "resource/eft.idx"
    eft_grp: str = "resource/eft.grp"

    quick_pics: Tuple[str, ...] = (
        "Heads.Pic",
        "Items.Pic",
        "Begin.Pic",
        "Background.Pic",
    )


@dataclass(frozen=True)
class GameProfile:
    id: str
    display_name: str
    role_words: int = 91
    item_words: int = 95
    scene_words: int = 26
    magic_words: int = 111
    shop_words: int = 18
    inventory_slots: int = 400
    war: WarLayout = field(default_factory=lambda: WAR_LAYOUT_PROMISE)
    save_subdir: str = "save"
    resource_subdir: str = "resource"
    assets: AssetPaths = field(default_factory=AssetPaths)
    # Prefer this encoding when auto-detecting text (hint only)
    default_text_encoding: str = "auto"


PROFILE_PROMISE = GameProfile(
    id="promise",
    display_name="金庸群侠前传 (Kys Promise)",
    magic_words=111,
    shop_words=18,
    inventory_slots=400,
    war=WAR_LAYOUT_PROMISE,
    assets=AssetPaths(
        heads_mode="pic",
        items_mode="pic",
        fight_mode="pic_tree",
        eft_mode="pic_file",
    ),
)

PROFILE_CLASSIC = GameProfile(
    id="classic",
    display_name="经典 KYS (散图/idx+grp)",
    magic_words=68,
    shop_words=15,
    inventory_slots=200,
    war=WAR_LAYOUT_CLASSIC,
    assets=AssetPaths(
        heads_mode="png_dir",
        items_mode="png_dir",
        fight_mode="idx_grp",
        eft_mode="idx_grp",
        quick_pics=(),
    ),
    default_text_encoding="big5",
)

# Alias for this repository's game
PROFILE_GODSDEVILS = replace(
    PROFILE_CLASSIC,
    id="godsdevils",
    display_name="天龙八部 / GodsDevils",
)

PROFILES: dict[str, GameProfile] = {
    p.id: p
    for p in (PROFILE_PROMISE, PROFILE_CLASSIC, PROFILE_GODSDEVILS)
}


def _readable_name_score(raw: bytes) -> int:
    """Heuristic: higher is more likely a real GBK/Big5 name."""
    raw = raw.split(b"\x00")[0].rstrip(b"\xff ").strip()
    if len(raw) < 2:
        return 0
    score = 0
    for enc in ("gbk", "big5"):
        try:
            text = raw.decode(enc)
        except UnicodeDecodeError:
            continue
        han = sum(1 for ch in text if "\u4e00" <= ch <= "\u9fff")
        if han:
            score = max(score, han * 3 + len(text))
    return score


def _probe_table_words(
    grp: bytes,
    start: int,
    end: int,
    candidates: Sequence[int],
    name_word: int = 1,
    name_words: int = 5,
) -> Optional[int]:
    size = end - start
    best_w: Optional[int] = None
    best_score = -1
    for w in candidates:
        b = w * 2
        if b <= 0 or size % b != 0:
            continue
        count = size // b
        if count <= 0:
            continue
        score = 0
        sample = min(count, 12)
        for i in range(sample):
            off = start + i * b + name_word * 2
            chunk = grp[off : off + name_words * 2]
            score += _readable_name_score(chunk)
        # prefer exact division with decent names
        score += count // 10
        if score > best_score:
            best_score = score
            best_w = w
    return best_w


def _probe_war_words(raw: bytes, candidates: Sequence[int] = (93, 156)) -> Optional[int]:
    best_w: Optional[int] = None
    best_score = -1
    for w in candidates:
        b = w * 2
        if len(raw) % b != 0:
            continue
        count = len(raw) // b
        score = count
        for i in range(min(count, 8)):
            chunk = raw[i * b + 2 : i * b + 12]
            score += _readable_name_score(chunk) * 2
        if score > best_score:
            best_score = score
            best_w = w
    return best_w


def find_data_root_candidates(start: Path) -> List[Path]:
    """Paths that look like a KYS data root (have save/ + resource/)."""
    start = start.resolve()
    out: List[Path] = []
    for cand in (start, start / "game_data", start.parent, start.parent / "game_data"):
        if (cand / "save").is_dir() and (cand / "resource").is_dir():
            if cand not in out:
                out.append(cand)
    return out


def detect_profile(data_root: str | Path) -> GameProfile:
    """Inspect save/resource layout and pick the best matching profile."""
    root = Path(data_root)
    save = root / "save"
    res = root / "resource"

    base = PROFILE_PROMISE
    magic_w = PROFILE_PROMISE.magic_words
    shop_w = PROFILE_PROMISE.shop_words
    inv = PROFILE_PROMISE.inventory_slots
    war_layout = WAR_LAYOUT_PROMISE
    assets = PROFILE_PROMISE.assets
    encoding = "auto"
    display = "KYS 自动探测"
    pid = "auto"

    # Ranger table widths
    idx_path = None
    for n in ("ranger.idx", "Ranger.idx"):
        p = save / n
        if p.is_file():
            idx_path = p
            break
    grp_path = None
    for n in ("Ranger.grp", "ranger.grp"):
        p = save / n
        if p.is_file():
            grp_path = p
            break

    if idx_path and grp_path:
        idx = idx_path.read_bytes()
        if len(idx) >= 24:
            role_o, item_o, scene_o, magic_o, shop_o, total = struct.unpack_from("<6i", idx, 0)
            inv = max(0, (role_o - 42) // 4)
            grp = grp_path.read_bytes()
            mw = _probe_table_words(grp, magic_o, shop_o, (68, 93, 111))
            if mw:
                magic_w = mw
            sw = _probe_table_words(grp, shop_o, total, (15, 18), name_word=0, name_words=1)
            # shop has no name; prefer exact rem=0 with plausible count
            shop_bytes = total - shop_o
            for cand in (15, 18):
                if shop_bytes % (cand * 2) == 0 and shop_bytes // (cand * 2) >= 1:
                    sw = cand
                    break
            if sw:
                shop_w = sw

    # War.sta
    war_path = None
    for n in ("War.sta", "war.sta"):
        p = res / n
        if p.is_file():
            war_path = p
            break
    if war_path:
        ww = _probe_war_words(war_path.read_bytes())
        if ww == 93:
            war_layout = WAR_LAYOUT_CLASSIC
        elif ww == 156:
            war_layout = WAR_LAYOUT_PROMISE

    # Assets
    heads_pic = (res / "Heads.Pic").is_file() or (res / "heads.pic").is_file()
    items_pic = (res / "Items.Pic").is_file() or (res / "items.pic").is_file()
    heads_dir = (root / "head").is_dir() and any((root / "head").glob("*.png"))
    items_dir = (root / "item").is_dir() and any((root / "item").glob("*.png"))
    fight_tree = (root / "fight").is_dir() and any((root / "fight").glob("*/*.pic"))
    fight_pack = (root / "fight").is_dir() and any((root / "fight").glob("fight*.grp"))
    eft_pic = (root / "eft").is_dir() and any((root / "eft").glob("eft*.pic"))
    eft_pack = (res / "eft.idx").is_file() and (res / "eft.grp").is_file()

    assets = AssetPaths(
        heads_mode="pic" if heads_pic else ("png_dir" if heads_dir else "none"),
        items_mode="pic" if items_pic else ("png_dir" if items_dir else "none"),
        fight_mode="pic_tree" if fight_tree else ("idx_grp" if fight_pack else "none"),
        eft_mode="pic_file" if eft_pic else ("idx_grp" if eft_pack else "none"),
        quick_pics=PROFILE_PROMISE.assets.quick_pics if heads_pic else (),
    )

    if magic_w == 68 or war_layout.words == 93 or assets.heads_mode == "png_dir":
        encoding = "big5"
        pid = "classic"
        display = "经典 KYS (自动探测)"
        if (root / "list" / "start.txt").is_file() or (root / "item").is_dir():
            pid = "godsdevils"
            display = "天龙八部 / GodsDevils (自动探测)"
    else:
        pid = "promise"
        display = "金庸群侠前传 (自动探测)"

    return GameProfile(
        id=pid,
        display_name=display,
        role_words=91,
        item_words=95,
        scene_words=26,
        magic_words=magic_w,
        shop_words=shop_w,
        inventory_slots=max(inv, 1),
        war=war_layout,
        assets=assets,
        default_text_encoding=encoding,
    )


def resolve_path(data_root: Path, rel: str) -> Path:
    return data_root / rel.replace("\\", "/")
