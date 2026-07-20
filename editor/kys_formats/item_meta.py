"""Item field labels and enums aligned with Item.h / TItem / UIManager."""

from __future__ import annotations

# Reuse battle-state names from magic (equipment BattleEffect)
from .magic_meta import BATTLE_STATES

ITEM_TYPES = {
    0: "剧情物品",
    1: "神兵宝甲(装备)",
    2: "武功秘笈",
    3: "灵丹妙药",
    4: "伤人暗器",
}

EQUIP_TYPES = {
    -1: "(非装备/不限)",
    0: "武器",
    1: "身披",
    2: "头戴",
    3: "脚踩",
    4: "第五装备位",
}

NEED_SEX = {
    -1: "不限",
    0: "仅男",
    1: "仅女",
}

NEED_MP_TYPES = {
    -1: "不限",
    0: "需阴内",
    1: "需阳内",
    2: "需调和/不限",
}

CHANGE_MP_TYPES = {
    -1: "不变",
    0: "改为阴",
    1: "改为阳",
    2: "改为调和",
}


def item_type_display(v: int) -> str:
    return ITEM_TYPES.get(v, f"未知类型({v})")


def equip_type_display(v: int) -> str:
    return EQUIP_TYPES.get(v, f"部位({v})")


def item_summary(rec: list, name: str = "") -> str:
    t = rec[41] if len(rec) > 41 else 0
    return f"{name}  [{item_type_display(t)}]"
