"""Chinese opcode dictionary + argument tooltip resolution for Kdef scripts."""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Callable, List, Optional, Sequence

# opcode -> 中文名称（名称栏展示）
OPCODE_ZH: dict[int, str] = {
    -1: "结束",
    0: "重绘场景",
    1: "显示对话",
    2: "获得/失去物品",
    3: "修改场景事件(DData)",
    4: "判断是否使用某物品?",
    5: "询问是否战斗?",
    6: "进入战斗",
    7: "中断脚本",
    8: "更换音乐/表情",
    9: "询问是否加入?",
    10: "加入队伍",
    11: "询问是否休息?",
    12: "休息",
    13: "淡入",
    14: "淡出",
    15: "游戏失败",
    16: "判断是否在队?",
    17: "设置场景贴图",
    18: "判断物品数量?",
    19: "瞬移坐标",
    20: "判断队伍是否已满?",
    21: "离队",
    22: "内力清零",
    23: "角色中毒",
    24: "空指令",
    25: "镜头平移",
    26: "累加事件参数",
    27: "播放事件动画",
    28: "判断道德?",
    29: "判断攻击力?",
    30: "角色行走",
    31: "判断金钱?",
    32: "静默增减物品",
    33: "学会武功",
    34: "增加资质",
    35: "设置武功栏",
    36: "判断性别?",
    37: "增加道德",
    38: "更换场景贴图",
    39: "开启场景",
    40: "设置面向",
    41: "设置角色携带物品",
    42: "判断队中有女性?",
    43: "有某物品则跳转?",
    44: "双人动画",
    45: "增加轻功",
    46: "增加内力上限",
    47: "增加攻击",
    48: "增加生命上限",
    49: "设置内力属性",
    50: "扩展指令50e",
    51: "软星对话",
    52: "显示道德",
    53: "显示声望",
    54: "华山论剑",
    55: "判断事件贴图?",
    56: "增加声望",
    57: "空",
    58: "全员离队",
    59: "屏幕震动",
    60: "判断事件是否存在?",
    61: "跳转脚本",
    62: "游戏结束",
    63: "设置性别",
    64: "打开商店",
    65: "空",
    66: "播放音乐",
    67: "播放音效",
    68: "新对话(NewTalk)",
    69: "重设名称",
    70: "显示标题",
    71: "跳转场景",
    83: "扩展空操作(83)",
}


@dataclass(frozen=True)
class ArgSpec:
    """One script argument."""
    name: str
    kind: str = "int"  # talk|item|role|battle|magic|scene|head|name|jump|ddata|face|mp_type|flag|int


# opcode -> list of ArgSpec (length should match OPCODE_ARGC)
OPCODE_ARGS: dict[int, List[ArgSpec]] = {
    1: [
        ArgSpec("对话ID", "talk"),
        ArgSpec("头像ID", "head"),
        ArgSpec("显示模式(0左上/1右下/2无头像…)", "int"),
    ],
    2: [ArgSpec("物品ID", "item"), ArgSpec("数量(负=失去)", "int")],
    3: [
        ArgSpec("场景号(-2=当前)", "scene"),
        ArgSpec("事件号(-2=当前)", "int"),
        ArgSpec("DData[0] 条件", "int"),
        ArgSpec("DData[1]", "int"),
        ArgSpec("DData[2] 手动脚本", "jump"),
        ArgSpec("DData[3] 物品脚本", "jump"),
        ArgSpec("DData[4] 踩上脚本", "jump"),
        ArgSpec("DData[5] 贴图当前(偶数代码, /2=smp)", "pic"),
        ArgSpec("DData[6] 贴图结束", "pic"),
        ArgSpec("DData[7] 贴图起始", "pic"),
        ArgSpec("DData[8]", "int"),
        ArgSpec("DData[9] 坐标Y", "int"),
        ArgSpec("DData[10] 坐标X", "int"),
    ],
    4: [ArgSpec("物品ID", "item"), ArgSpec("是→跳转", "jump"), ArgSpec("否→跳转", "jump")],
    5: [ArgSpec("是→跳转", "jump"), ArgSpec("否→跳转", "jump")],
    6: [
        ArgSpec("战斗ID(War.sta)", "battle"),
        ArgSpec("胜→跳转", "jump"),
        ArgSpec("负→跳转", "jump"),
        ArgSpec("是否得经验", "flag"),
    ],
    8: [ArgSpec("音乐/参数", "int")],
    9: [ArgSpec("是→跳转", "jump"), ArgSpec("否→跳转", "jump")],
    10: [ArgSpec("角色ID", "role")],
    11: [ArgSpec("是→跳转", "jump"), ArgSpec("否→跳转", "jump")],
    16: [ArgSpec("角色ID", "role"), ArgSpec("在队→跳转", "jump"), ArgSpec("不在→跳转", "jump")],
    17: [
        ArgSpec("场景号", "scene"),
        ArgSpec("图层", "int"),
        ArgSpec("Y", "int"),
        ArgSpec("X", "int"),
        ArgSpec("贴图值", "int"),
    ],
    18: [ArgSpec("物品ID", "item"), ArgSpec("有→跳转", "jump"), ArgSpec("无→跳转", "jump")],
    19: [ArgSpec("X", "int"), ArgSpec("Y", "int")],
    20: [ArgSpec("满→跳转", "jump"), ArgSpec("未满→跳转", "jump")],
    21: [ArgSpec("角色ID", "role")],
    23: [ArgSpec("角色ID", "role"), ArgSpec("毒值", "int")],
    25: [ArgSpec("X1", "int"), ArgSpec("Y1", "int"), ArgSpec("X2", "int"), ArgSpec("Y2", "int")],
    26: [
        ArgSpec("场景号", "scene"),
        ArgSpec("事件号", "int"),
        ArgSpec("DData[2]增量", "int"),
        ArgSpec("DData[3]增量", "int"),
        ArgSpec("DData[4]增量", "int"),
    ],
    27: [ArgSpec("事件号", "int"), ArgSpec("起始贴图", "int"), ArgSpec("结束贴图", "int")],
    28: [
        ArgSpec("角色ID", "role"),
        ArgSpec("下限", "int"),
        ArgSpec("上限", "int"),
        ArgSpec("在范围→跳转", "jump"),
        ArgSpec("否则→跳转", "jump"),
    ],
    29: [
        ArgSpec("角色ID", "role"),
        ArgSpec("下限", "int"),
        ArgSpec("上限", "int"),
        ArgSpec("在范围→跳转", "jump"),
        ArgSpec("否则→跳转", "jump"),
    ],
    30: [ArgSpec("X1", "int"), ArgSpec("Y1", "int"), ArgSpec("X2", "int"), ArgSpec("Y2", "int")],
    31: [ArgSpec("金钱阈值", "int"), ArgSpec("够→跳转", "jump"), ArgSpec("不够→跳转", "jump")],
    32: [ArgSpec("物品ID", "item"), ArgSpec("数量", "int")],
    33: [ArgSpec("角色ID", "role"), ArgSpec("武功ID", "magic"), ArgSpec("显示模式", "int")],
    34: [ArgSpec("角色ID", "role"), ArgSpec("资质增量", "int")],
    35: [
        ArgSpec("角色ID", "role"),
        ArgSpec("武功栏位", "int"),
        ArgSpec("武功ID", "magic"),
        ArgSpec("经验", "int"),
    ],
    36: [ArgSpec("性别(0男1女)", "int"), ArgSpec("是→跳转", "jump"), ArgSpec("否→跳转", "jump")],
    37: [ArgSpec("道德增量", "int")],
    38: [
        ArgSpec("场景号", "scene"),
        ArgSpec("图层", "int"),
        ArgSpec("旧贴图", "int"),
        ArgSpec("新贴图", "int"),
    ],
    39: [ArgSpec("场景号", "scene")],
    40: [ArgSpec("面向(0-3)", "face")],
    41: [ArgSpec("角色ID", "role"), ArgSpec("物品ID", "item"), ArgSpec("数量", "int")],
    42: [ArgSpec("有→跳转", "jump"), ArgSpec("无→跳转", "jump")],
    43: [ArgSpec("物品ID", "item"), ArgSpec("有→跳转", "jump"), ArgSpec("无→跳转", "jump")],
    44: [
        ArgSpec("事件1", "int"),
        ArgSpec("起始图1", "int"),
        ArgSpec("结束图1", "int"),
        ArgSpec("事件2", "int"),
        ArgSpec("起始图2", "int"),
        ArgSpec("结束图2", "int"),
    ],
    45: [ArgSpec("角色ID", "role"), ArgSpec("轻功增量", "int")],
    46: [ArgSpec("角色ID", "role"), ArgSpec("内力增量", "int")],
    47: [ArgSpec("角色ID", "role"), ArgSpec("攻击增量", "int")],
    48: [ArgSpec("角色ID", "role"), ArgSpec("生命增量", "int")],
    49: [ArgSpec("角色ID", "role"), ArgSpec("内力属性(0阴1阳2调和)", "mp_type")],
    50: [
        ArgSpec("子码 code", "int"),
        ArgSpec("e1", "int"),
        ArgSpec("e2", "int"),
        ArgSpec("e3", "int"),
        ArgSpec("e4", "int"),
        ArgSpec("e5", "int"),
        ArgSpec("e6", "int"),
    ],
    55: [
        ArgSpec("事件号", "int"),
        ArgSpec("贴图值", "int"),
        ArgSpec("相等→跳转", "jump"),
        ArgSpec("不等→跳转", "jump"),
    ],
    56: [ArgSpec("声望增量", "int")],
    60: [
        ArgSpec("场景号", "scene"),
        ArgSpec("事件号", "int"),
        ArgSpec("贴图", "int"),
        ArgSpec("存在→跳转", "jump"),
        ArgSpec("不存在→跳转", "jump"),
    ],
    61: [ArgSpec("目标脚本相对偏移?", "jump"), ArgSpec("备用", "int")],
    63: [ArgSpec("角色ID", "role"), ArgSpec("性别", "int")],
    66: [ArgSpec("音乐号", "int")],
    67: [ArgSpec("音效号", "int")],
    68: [
        ArgSpec("头像ID", "head"),
        ArgSpec("对话ID", "talk"),
        ArgSpec("姓名条目(-2=跟头像)", "name"),
        ArgSpec("位置(0左1右…)", "int"),
        ArgSpec("是否显示头像(0显示)", "flag"),
        ArgSpec("颜色/色板", "int"),
        ArgSpec("边框", "int"),
    ],
    69: [ArgSpec("类型", "int"), ArgSpec("目标ID", "int"), ArgSpec("新名称条目", "name")],
    70: [ArgSpec("标题对话ID?", "talk"), ArgSpec("参数", "int")],
    71: [ArgSpec("场景号", "scene"), ArgSpec("X", "int"), ArgSpec("Y", "int")],
    83: [],
}


def opcode_display_name(opcode: int) -> str:
    if opcode < 0:
        return "结束(END)"
    zh = OPCODE_ZH.get(opcode)
    if zh:
        return zh
    return f"未知指令({opcode})"


def format_opcode_choice(opcode: int) -> str:
    """Dropdown / display label: `编码 — 中文释义`."""
    return f"{opcode} — {opcode_display_name(opcode)}"


def parse_opcode_choice(text: str) -> int:
    """Parse opcode from typed number or `编码 — 中文` label."""
    text = (text or "").strip()
    m = re.match(r"^(-?\d+)", text)
    if not m:
        raise ValueError(f"无法解析 opcode: {text!r}")
    return int(m.group(1))


def known_opcodes() -> List[int]:
    """Sorted opcode list for editor dropdowns (-1 END + documented ops)."""
    from kys_formats.kdef import OPCODE_ARGC

    ops = set(OPCODE_ZH.keys()) | set(OPCODE_ARGC.keys()) | {-1}
    return sorted(ops)


def default_args_for_opcode(opcode: int) -> List[int]:
    """Sensible starter args when inserting a new instruction."""
    from kys_formats.kdef import OPCODE_ARGC

    presets = {
        1: [1, 0, 0],
        2: [0, 1],
        4: [0, 1, 1],
        5: [1, 1],
        6: [0, 0, 0, 1],
        9: [1, 1],
        11: [1, 1],
        16: [0, 1, 1],
        18: [0, 1, 1],
        20: [1, 1],
        68: [0, 1, -2, 0, 0, 28515, 0],
    }
    if opcode in presets:
        return list(presets[opcode])
    if opcode < 0:
        return []
    return [0] * OPCODE_ARGC.get(opcode, 0)


def arg_specs(opcode: int) -> List[ArgSpec]:
    return list(OPCODE_ARGS.get(opcode, []))


def _clip(text: str, n: int = 80) -> str:
    t = text.replace("\n", " ").replace("\r", " ").strip()
    if len(t) <= n:
        return t
    return t[: n - 1] + "…"


def resolve_arg_value(ctx, kind: str, value: int) -> str:
    """Resolve a single argument value to human-readable text using EditorContext."""
    if value == -2 and kind in ("scene", "int", "jump", "name"):
        return "特殊值 -2（常=当前场景/事件/自动）"
    if value == -1 and kind in ("item", "role", "magic", "battle", "talk"):
        return "无(-1)"

    try:
        if kind == "talk" and ctx.talk and value >= 0:
            return f"对话「{_clip(ctx.talk.get_text(value))}」"
        if kind == "name" and ctx.names and value > 0:
            return f"姓名「{_clip(ctx.names.get_text(value), 40)}」"
        if kind == "item" and ctx.ranger and value >= 0:
            name = ctx.ranger.item_name(value)
            return f"物品「{name}」(#{value})" if name else f"物品#{value}"
        if kind == "role" and ctx.ranger and value >= 0:
            name = ctx.ranger.role_name(value)
            return f"角色「{name}」(#{value})" if name else f"角色#{value}"
        if kind == "magic" and ctx.ranger and value >= 0:
            name = ctx.ranger.magic_name(value)
            return f"武功「{name}」(#{value})" if name else f"武功#{value}"
        if kind == "scene" and ctx.ranger and value >= 0:
            name = ctx.ranger.scene_name(value)
            return f"场景「{name}」(#{value})" if name else f"场景#{value}"
        if kind == "battle" and ctx.war and value >= 0:
            rec = ctx.war.find_by_num(value)
            if rec:
                return f"战斗「{rec.name}」(BattleNum={value})"
            return f"战斗#{value}"
        if kind == "head":
            return f"头像帧 #{value}（Heads.Pic）"
        if kind == "jump":
            return (
                f"相对跳过 {value} 个「字」(int16)，"
                f"从本指令结束后的下一条起始处再偏移（不是地图坐标，也不是跳过 N 条指令）"
            )
        if kind == "face":
            faces = {0: "左", 1: "上", 2: "右", 3: "下"}
            return f"面向 {faces.get(value, value)}"
        if kind == "mp_type":
            return {0: "阴性", 1: "阳性", 2: "调和"}.get(value, str(value))
        if kind == "flag":
            return "是/开" if value else "否/关"
        if kind == "pic":
            if value == -2:
                return "保持原值(-2)"
            if value == 0:
                return "清除贴图(0)"
            from kys_formats.rle_tile import format_pic_code

            return format_pic_code(value)
    except Exception as e:
        return f"(解析失败: {e})"
    return str(value)


def format_args_tooltip(ctx, opcode: int, args: Sequence[int]) -> str:
    """Full tooltip for the parameters cell."""
    specs = arg_specs(opcode)
    lines = [f"指令 {opcode}: {opcode_display_name(opcode)}", ""]
    if opcode < 0:
        lines.append("脚本结束标记（opcode < 0）")
        return "\n".join(lines)
    if not specs:
        if not args:
            lines.append("无参数")
        else:
            lines.append("参数（未登记释义，按原始数值）:")
            for i, a in enumerate(args):
                lines.append(f"  [{i}] = {a}")
        return "\n".join(lines)

    lines.append("参数释义:")
    for i, spec in enumerate(specs):
        val = args[i] if i < len(args) else "(缺省)"
        if isinstance(val, int):
            resolved = resolve_arg_value(ctx, spec.kind, val)
            lines.append(f"  [{i}] {spec.name} = {val}")
            if resolved != str(val):
                lines.append(f"       → {resolved}")
        else:
            lines.append(f"  [{i}] {spec.name} = {val}")
    if len(args) > len(specs):
        lines.append("多余参数: " + ",".join(str(a) for a in args[len(specs) :]))
    if opcode == 6 and len(args) >= 3:
        win, lose = args[1], args[2]
        # Battle occupies 5 words; after it IP is at next instr. Then +win/+lose words.
        lines.append("")
        lines.append("跳转落点（相对本指令之后）:")
        lines.append(f"  胜利: 再跳过 {win} 字 → 常用于跳过紧随其后的「游戏失败」")
        lines.append(f"  失败: 再跳过 {lose} 字 → 0 表示直接执行下一条（多为游戏失败）")
        lines.append("  典型写法: 战斗(…, 3, 0, …) / 游戏失败 / …胜利剧情…")
    return "\n".join(lines)


def format_name_tooltip(opcode: int) -> str:
    eng = ""
    try:
        from kys_formats.kdef import OPCODE_NAMES
        eng = OPCODE_NAMES.get(opcode, "")
    except Exception:
        pass
    zh = opcode_display_name(opcode)
    specs = arg_specs(opcode)
    lines = [zh]
    if eng:
        lines.append(f"英文标识: {eng}")
    lines.append(f"Opcode: {opcode}")
    if specs:
        lines.append("参数: " + ", ".join(s.name for s in specs))
    elif opcode >= 0:
        lines.append("无参数")
    return "\n".join(lines)
