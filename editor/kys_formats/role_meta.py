"""Role field labels and enums aligned with Role.h / TRole."""

from __future__ import annotations

import struct


def as_u16(signed_word: int) -> int:
    return int(signed_word) & 0xFFFF


def to_i16_from_u16(value: int) -> int:
    return struct.unpack("<h", struct.pack("<H", int(value) & 0xFFFF))[0]


SEXUAL = {
    0: "男",
    1: "女",
}

MP_TYPES = {
    0: "阴",
    1: "阳",
    2: "调和",
}

EQUIP_SLOTS = {
    0: "武器",
    1: "身披",
    2: "头戴",
    3: "脚踩",
    4: "第五装备位",
}


def sexual_display(v: int) -> str:
    return SEXUAL.get(v, f"未知({v})")


def mp_type_display(v: int) -> str:
    return MP_TYPES.get(v, f"未知({v})")


def role_summary(rec: list, name: str = "") -> str:
    """Short summary for list rows."""
    lv = rec[15] if len(rec) > 15 else 0
    sex = sexual_display(rec[14] if len(rec) > 14 else 0)
    return f"{name}  Lv{lv} {sex}"
