"""Ranger.grp header layout probing (team / money / inventory offsets)."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Sequence, Tuple

# Promise / Pascal header: sface, ship_face, game_time, then team at byte 30.
PROMISE_TEAM_OFFSET = 30
# Classic KYS / kys-cpp / kys-awaken (836-byte role table): team directly after Encode.
CLASSIC_TEAM_OFFSET = 24
STANDARD_TEAM_OFFSET = PROMISE_TEAM_OFFSET
MAX_TEAM_SLOTS = 6


@dataclass(frozen=True)
class RangerHeaderLayout:
    """Byte layout of the ranger save header before role table."""

    team_offset: int = STANDARD_TEAM_OFFSET
    team_count: int = 6
    money_offset: int = -1  # -1 if no dedicated money word
    inventory_base: int = 42

    @property
    def has_money_word(self) -> bool:
        return self.money_offset >= 0


def _score_team_block(header: bytes, team_off: int, count: int, role_count: int) -> int:
    score = 0
    for i in range(count):
        off = team_off + i * 2
        if off + 2 > len(header):
            return -999
        v = struct.unpack_from("<h", header, off)[0]
        if v == -1:
            score += 4
        elif v == 0:
            score += 1
        elif 0 < v < role_count:
            score += 5
        elif v > 0:
            score -= 3
    return score


def _team_count_before_inventory(inv_base: int, money_offset: int) -> int:
    """Team list length on disk (engine always uses 6 slots)."""
    _ = inv_base, money_offset
    return MAX_TEAM_SLOTS


def _team_offset_candidates(role_offset: int, magic_words: int) -> Tuple[int, ...]:
    """Classic 836-byte headers place Team[0] at byte 24 (kys-cpp Save::BaseInfo)."""
    if role_offset == 836 and magic_words == 68:
        return (CLASSIC_TEAM_OFFSET, PROMISE_TEAM_OFFSET)
    if role_offset >= 1600 and magic_words in (111, 93):
        return (PROMISE_TEAM_OFFSET,)
    return (CLASSIC_TEAM_OFFSET, PROMISE_TEAM_OFFSET)


def probe_ranger_header_layout(
    role_offset: int,
    magic_words: int,
    header: bytes,
    *,
    role_count: int = 300,
) -> RangerHeaderLayout:
    """Pick team / money / inventory offsets from the ranger header prefix."""
    header = header[: max(64, min(len(header), role_offset))]
    best_score = -10**9
    best = RangerHeaderLayout()

    inv_options: list[Tuple[int, int]] = []
    for inv_base in range(32, 52, 2):
        nbytes = role_offset - inv_base
        if nbytes < 0 or nbytes % 4 != 0:
            continue
        inv_options.append((inv_base, nbytes // 4))

    for inv_base, slots in inv_options:
        slot_score = 0
        if slots == 200 and magic_words == 68:
            slot_score += 25
        if slots in (400, 401) and magic_words in (111, 93):
            slot_score += 25
        if slots in (198, 199, 200, 201, 400, 401):
            slot_score += 8

        money_candidates: Sequence[int] = (-1,)
        if inv_base == 44:
            money_candidates = (42, -1)
        elif inv_base == 46:
            money_candidates = (44, -1)

        for money_off in money_candidates:
            if money_off >= 0 and money_off + 2 > inv_base:
                continue
            team_cnt = _team_count_before_inventory(inv_base, money_off)
            if team_cnt <= 0:
                continue
            for team_off in _team_offset_candidates(role_offset, magic_words):
                ts = _score_team_block(header, team_off, team_cnt, role_count)
                total = slot_score + ts
                if inv_base == 42 and slots in (400, 401) and team_cnt == 6:
                    total += 12
                if (
                    inv_base == 44
                    and slots in (198, 199, 200)
                    and money_off == 42
                    and team_cnt == 6
                    and team_off == CLASSIC_TEAM_OFFSET
                ):
                    total += 25
                if role_offset == 836 and inv_base == 44 and money_off == 42:
                    total += 30
                if role_offset == 836 and inv_base == 36:
                    # inv@36 treats the money word (byte 42) as bag data.
                    total -= 60
                if (
                    inv_base == 44
                    and slots in (198, 199, 200)
                    and money_off == 42
                    and team_cnt == 6
                    and team_off == PROMISE_TEAM_OFFSET
                ):
                    total += 5
                if inv_base == 36 and team_off == PROMISE_TEAM_OFFSET:
                    # inv@36 overlaps team[3..5] when team starts at byte 30.
                    total -= 40
                if role_offset == 836 and team_off == PROMISE_TEAM_OFFSET:
                    # 836-byte classic headers keep team at byte 24, not 30.
                    total -= 30
                if total > best_score:
                    best_score = total
                    best = RangerHeaderLayout(
                        team_offset=team_off,
                        team_count=team_cnt,
                        money_offset=money_off,
                        inventory_base=inv_base,
                    )
    return best


def probe_ranger_inventory_base(role_offset: int, magic_words: int) -> int:
    layout = probe_ranger_header_layout(role_offset, magic_words, b"\xff\xff" * 32)
    return layout.inventory_base
