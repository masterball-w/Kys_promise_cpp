"""Magic field labels and helpers aligned with Magic.h / BattleManager."""

from __future__ import annotations

from typing import List, Tuple

# MagicType (word 12)
MAGIC_TYPES = {
    0: "未分类",
    1: "外功·拳掌",
    2: "外功·剑术",
    3: "外功·刀法",
    4: "外功·奇门",
    5: "内功",
}

# HurtType (word 14)
HURT_TYPES = {
    0: "伤气血",
    1: "伤内力(吸星)",
}

# AttAreaType (word 15) — BattleManager::SetAttackArea / Pascal DrawBFieldWithCursor
ATT_AREA_TYPES = {
    0: "点/菱形(目标系)",
    1: "方向直线",
    2: "十字/米字(原地系)",
    3: "方形·面(目标/原地)",
    4: "方向菱形",
    5: "方向方形",
    6: "远程点",
    7: "无定向直线",
}

# Damage modulus words 21-24 — CalHurtValue weights
# p = Attack*6 + MP + Speed*2 + Weapon*2
MODULUS_FIELDS = [
    (21, "攻击型", "AttackModulus", "伤害与攻击力差挂钩 (权重×6)"),
    (22, "内力型", "MPModulus", "伤害与当前内力差挂钩 (权重×1)"),
    (23, "轻功型", "SpeedModulus", "伤害与轻功差挂钩 (权重×2)"),
    (24, "兵器型", "WeaponModulus", "伤害与当前兵器(拳/剑/刀/奇)差挂钩 (权重×2)"),
]

BATTLE_STATES = {
    0: "(无)",
    1: "体力不减",
    2: "女性武功威力加成",
    3: "饮酒功效加倍",
    4: "随机伤害转移",
    5: "随机伤害反噬",
    6: "内伤免疫",
    7: "杀伤体力",
    8: "增加闪躲几率",
    9: "攻击力随等级循环增减",
    10: "内力消耗减少",
    11: "每回合恢复生命",
    12: "负面状态免疫",
    13: "全部武功威力加成",
    14: "随机二次攻击",
    15: "拳掌武功威力加成",
    16: "剑术武功威力加成",
    17: "刀法武功威力加成",
    18: "奇门武功威力加成",
    19: "增加内伤几率",
    20: "增加封穴几率",
    21: "攻击微量吸血",
    22: "攻击距离增加",
    23: "每回合恢复内力",
    24: "使用暗器距离增加",
    25: "附加杀伤吸收内力",
    26: "每回合提高攻击",
    27: "令附近敌人中毒",
    28: "大幅提升医疗和解毒的效果",
}


def cal_new_hurt_value(lv: int, min_val: int, max_val: int, proportion: int) -> int:
    """Port of BattleManager::CalNewHurtValue / Pascal CalNewHurtValue.

    Parameters (引擎约定):
      lv         — 0-based 等级：0=1级 … 9=10级（战斗里传入 level-1）
      min_val    — MinHurt[18] 一级基础伤害
      max_val    — MaxHurt[19] 十级目标伤害上界
      proportion — HurtModulus[20] 成长曲线；为 0 时按 100 处理

    公式:
      p = HurtModulus / 1000
      n = (MaxHurt - MinHurt)^(1/p) / 9
      威力(lv) = round( (lv * n)^p ) + MinHurt

    直观含义:
      - HurtModulus 越大 → p 越大 → 前期涨得慢、后期更陡（偏后期发力）
      - HurtModulus 越小 → p 越小 → 前期涨得更快（偏前期发力）
      - HurtModulus=1000 → p=1 → 近似线性：Min + lv*(Max-Min)/9
    """
    if proportion == 0:
        proportion = 100
    p = proportion / 1000.0
    if max_val <= min_val:
        return int(min_val)
    n = ((max_val - min_val) ** (1.0 / p)) / 9.0
    return int(round((lv * n) ** p) + min_val)


GROWTH_CURVE_HELP = (
    "成长曲线 HurtModulus → 引擎 CalNewHurtValue（战斗实际用等级-1 作为 lv）\n"
    "\n"
    "  p = HurtModulus / 1000\n"
    "  n = (MaxHurt − MinHurt)^(1/p) / 9\n"
    "  威力(等级) = round( ((等级−1) × n)^p ) + MinHurt\n"
    "\n"
    "其中等级取 1…10；一级时 lv=0 → 结果恒为 MinHurt。\n"
    "HurtModulus=0 时按 100 处理。\n"
    "HurtModulus 越大后期越陡；=1000 时接近线性增长。\n"
    "最终战斗伤害还会再乘武学常识等修正，此处仅为招式基础威力。"
)


def hurt_table(min_val: int, max_val: int, proportion: int) -> list[tuple[int, int]]:
    """Return [(level_1_based, hurt), ...] for levels 1..10."""
    return [
        (lv + 1, cal_new_hurt_value(lv, min_val, max_val, proportion))
        for lv in range(10)
    ]



def dominant_modulus(att: int, mp: int, spd: int, wpn: int) -> str:
    """Describe leading damage-bonus mode from the four modulus weights."""
    weighted = [
        ("攻击型", att * 6),
        ("内力型", mp),
        ("轻功型", spd * 2),
        ("兵器型", wpn * 2),
    ]
    total = sum(w for _, w in weighted)
    if total <= 0:
        return "无加成权重"
    parts = [f"{name}{w}/{total}" for name, w in weighted if w > 0]
    top = max(weighted, key=lambda x: x[1])
    return f"主导:{top[0]}  ({', '.join(parts)})"


def modulus_summary(rec: List[int]) -> str:
    return dominant_modulus(
        rec[21] if len(rec) > 21 else 0,
        rec[22] if len(rec) > 22 else 0,
        rec[23] if len(rec) > 23 else 0,
        rec[24] if len(rec) > 24 else 0,
    )


def power_at_level(rec: List[int], level_1_based: int) -> Tuple[int, int]:
    """Return (hurt_at_level, move_dist, att_dist conceptually) — hurt only here."""
    lv = max(0, min(9, level_1_based - 1))
    min_h = rec[18] if len(rec) > 18 else 0
    max_h = rec[19] if len(rec) > 19 else 0
    mod = rec[20] if len(rec) > 20 else 0
    return cal_new_hurt_value(lv, min_h, max_h, mod), lv


def label_magic_type(v: int) -> str:
    return MAGIC_TYPES.get(v, f"未知({v})")


def label_hurt_type(v: int) -> str:
    return HURT_TYPES.get(v, f"未知({v})")


def label_att_area(v: int) -> str:
    return ATT_AREA_TYPES.get(v, f"未知({v})")


def label_battle_state(v: int) -> str:
    return BATTLE_STATES.get(v, f"自定义({v})")


def category_display(rec: List[int]) -> str:
    """外功拳剑刀特 / 内功 / 吸星 (HurtType=1)."""
    mt = rec[12] if len(rec) > 12 else 0
    ht = rec[14] if len(rec) > 14 else 0
    base = label_magic_type(mt)
    if ht == 1 and mt != 5:
        return f"{base} · 吸星(伤内力)"
    if mt == 5:
        return base
    return base
