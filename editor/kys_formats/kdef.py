"""Kdef.idx/grp event script codec + disassembler."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .backup import atomic_write, backup_file

# opcode -> number of argument words (Pascal CallEvent)
OPCODE_ARGC: Dict[int, int] = {
    0: 0, 1: 3, 2: 2, 3: 13, 4: 3, 5: 2, 6: 4, 7: 0, 8: 1, 9: 2,
    10: 1, 11: 2, 12: 0, 13: 0, 14: 0, 15: 0, 16: 3, 17: 5, 18: 3, 19: 2,
    20: 2, 21: 1, 22: 0, 23: 2, 24: 0, 25: 4, 26: 5, 27: 3, 28: 5, 29: 5,
    30: 4, 31: 3, 32: 2, 33: 3, 34: 2, 35: 4, 36: 3, 37: 1, 38: 4, 39: 1,
    40: 1, 41: 3, 42: 2, 43: 3, 44: 6, 45: 2, 46: 2, 47: 2, 48: 2, 49: 2,
    50: 7, 51: 0, 52: 0, 53: 0, 54: 0, 55: 4, 56: 1, 57: 0, 58: 0, 59: 0,
    60: 5, 61: 2, 62: 0, 63: 2, 64: 0, 65: 0, 66: 1, 67: 1, 68: 7, 69: 3,
    70: 2, 71: 3,
    # Mods / extended engines (天龙等): appears after GameFail before Break on battle win paths
    83: 0,
}

OPCODE_NAMES: Dict[int, str] = {
    0: "Redraw",
    1: "Dialogue",
    2: "AddItem",
    3: "ModifyEvent",
    4: "HaveItem?",
    5: "AskBattle",
    6: "Battle",
    7: "Break",
    8: "ChangeFace",
    9: "AskJoin?",
    10: "Join",
    11: "AskRest?",
    12: "Rest",
    13: "FadeIn",
    14: "FadeOut",
    15: "GameFail",
    16: "InTeam?",
    17: "SetSceneTile",
    18: "HaveItemAmt?",
    19: "Teleport",
    20: "TeamFull?",
    21: "Leave",
    22: "ZeroMP",
    23: "UsePoi",
    25: "PanCamera",
    26: "AddEventParam",
    27: "Animate",
    28: "Morality?",
    29: "Attack?",
    30: "Walk",
    31: "Money?",
    32: "AddItemSilent",
    33: "LearnMagic",
    34: "AddAptitude",
    35: "SetMagicSlot",
    36: "Sexual?",
    37: "AddMorality",
    38: "ChangePic",
    39: "OpenScene",
    40: "SetFace",
    41: "TakingItem",
    42: "FemaleInTeam?",
    43: "SubFunc",
    44: "DualAnimate",
    45: "AddSpeed",
    46: "AddMP",
    47: "AddAttack",
    48: "AddHP",
    49: "SetMPType",
    50: "50e",
    51: "SoftStarTalk",
    52: "ShowMorality",
    53: "ShowFame",
    54: "Huashan",
    55: "EventPic?",
    56: "AddFame",
    58: "AllLeave",
    59: "Shake",
    60: "EventExist?",
    61: "Jump",
    62: "GameEnd",
    63: "SetSexual",
    64: "Shop",
    66: "PlayMusic",
    67: "PlaySound",
    68: "NewTalk",
    69: "ReSetName",
    70: "ShowTitle",
    71: "JmpScene",
    83: "Nop83",
}


@dataclass
class Instruction:
    opcode: int
    args: List[int] = field(default_factory=list)
    pc: int = 0  # word index within script

    @property
    def name(self) -> str:
        if self.opcode < 0:
            return "END"
        return OPCODE_NAMES.get(self.opcode, f"Op{self.opcode}")

    def to_words(self) -> List[int]:
        if self.opcode < 0:
            return [self.opcode]
        return [self.opcode] + list(self.args)


@dataclass
class Script:
    script_id: int
    instructions: List[Instruction] = field(default_factory=list)
    raw_words: List[int] = field(default_factory=list)

    def disassemble(self) -> List[Instruction]:
        words = self.raw_words
        pc = 0
        out: List[Instruction] = []
        while pc < len(words):
            op = words[pc]
            if op < 0:
                out.append(Instruction(op, [], pc))
                break
            argc = OPCODE_ARGC.get(op)
            if argc is None:
                # Unknown opcode: keep going as 0-arg so the rest of the script
                # remains visible (old behavior stopped the whole listing here).
                out.append(Instruction(op, [], pc))
                pc += 1
                continue
            args = words[pc + 1 : pc + 1 + argc]
            if len(args) < argc:
                args = args + [0] * (argc - len(args))
            out.append(Instruction(op, list(args), pc))
            pc += 1 + argc
        self.instructions = out
        return out

    def assemble(self) -> List[int]:
        words: List[int] = []
        for ins in self.instructions:
            if ins.opcode < 0:
                words.append(ins.opcode)
                break
            argc = OPCODE_ARGC.get(ins.opcode, 0)
            args = list(ins.args[:argc]) + [0] * max(0, argc - len(ins.args))
            words.append(ins.opcode)
            words.extend(args[:argc])
        if not words or words[-1] >= 0:
            words.append(-1)
        self.raw_words = words
        return words


class KdefArchive:
    def __init__(self) -> None:
        self.idx_path: Optional[Path] = None
        self.grp_path: Optional[Path] = None
        self.offsets: List[int] = []  # byte offsets, length = script_count
        self.words: List[int] = []  # entire grp as int16

    @property
    def script_count(self) -> int:
        return len(self.offsets)

    def load(self, resource_dir: str | Path) -> None:
        resource_dir = Path(resource_dir)
        idx = None
        grp = None
        for n in ("Kdef.idx", "kdef.idx"):
            p = resource_dir / n
            if p.is_file():
                idx = p
                break
        for n in ("Kdef.grp", "kdef.grp"):
            p = resource_dir / n
            if p.is_file():
                grp = p
                break
        if not idx or not grp:
            raise FileNotFoundError("Kdef.idx/grp not found")
        self.idx_path = idx
        self.grp_path = grp
        idx_data = idx.read_bytes()
        grp_data = grp.read_bytes()
        self.offsets = list(struct.unpack(f"<{len(idx_data)//4}i", idx_data))
        self.words = list(struct.unpack(f"<{len(grp_data)//2}h", grp_data))

    def get_script(self, script_id: int) -> Script:
        """script_id is 1-based (Pascal CallEvent)."""
        if script_id <= 0 or script_id > len(self.offsets):
            raise IndexError(script_id)
        start_byte = self.offsets[script_id - 1]
        end_byte = (
            self.offsets[script_id]
            if script_id < len(self.offsets)
            else len(self.words) * 2
        )
        start = start_byte // 2
        end = end_byte // 2
        raw = self.words[start:end]
        script = Script(script_id, raw_words=list(raw))
        script.disassemble()
        return script

    def set_script(self, script: Script) -> None:
        """Replace script body; rebuilds entire idx/grp word stream."""
        scripts: List[List[int]] = []
        for sid in range(1, self.script_count + 1):
            if sid == script.script_id:
                scripts.append(script.assemble())
            else:
                scripts.append(self.get_script(sid).raw_words)
        self._rebuild(scripts)

    def append_script(self, words: List[int]) -> int:
        """Append a new script; returns new 1-based script id."""
        if not words or words[-1] >= 0:
            words = list(words) + [-1]
        scripts = [self.get_script(sid).raw_words for sid in range(1, self.script_count + 1)]
        scripts.append(list(words))
        self._rebuild(scripts)
        return len(scripts)

    def _rebuild(self, scripts: List[List[int]]) -> None:
        offsets: List[int] = []
        all_words: List[int] = []
        # Pascal idx[0] for script 1 is often 4 (skip first empty word pair) or 0.
        # Preserve original first offset if present.
        cursor = self.offsets[0] if self.offsets else 0
        if cursor == 4 and not all_words:
            # Keep leading 2 words from original if any
            if self.words:
                all_words.extend(self.words[:2])
            else:
                all_words.extend([0, 0])
            cursor = 4
        for body in scripts:
            offsets.append(cursor)
            all_words.extend(body)
            cursor += len(body) * 2
        self.offsets = offsets
        self.words = all_words

    def to_idx_bytes(self) -> bytes:
        return struct.pack(f"<{len(self.offsets)}i", *self.offsets)

    def to_grp_bytes(self) -> bytes:
        return struct.pack(f"<{len(self.words)}h", *self.words)

    def save(self, backup: bool = True) -> None:
        if not self.idx_path or not self.grp_path:
            raise RuntimeError("not loaded")
        if backup:
            backup_file(self.idx_path)
            backup_file(self.grp_path)
        atomic_write(self.idx_path, self.to_idx_bytes())
        atomic_write(self.grp_path, self.to_grp_bytes())

    def find_battle_refs(self, battle_id: int) -> List[int]:
        """Return script IDs that call instruct_6 with this battle_id."""
        hits = []
        for sid in range(1, self.script_count + 1):
            script = self.get_script(sid)
            for ins in script.instructions:
                if ins.opcode == 6 and ins.args and ins.args[0] == battle_id:
                    hits.append(sid)
                    break
        return hits
