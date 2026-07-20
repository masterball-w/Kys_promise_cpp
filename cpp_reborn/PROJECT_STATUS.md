# KYS C++ Refactoring Project Status

**Last Updated:** 2026-07-20  
**Status:** Alpha 0.6 / Core Loop + Shop + Little Games  
**详细对齐清单:** [doc/PROGRESS.md](doc/PROGRESS.md)（以 Pascal 函数为单元）  
**构建约定:** 源码 `cpp_reborn/` → 唯一产物目录仓库根 `build/`（见根目录 `CMakePresets.json` / `README.md`）

## 1. Completed Systems

### Core Architecture
- **GameManager**: Central singleton managing global state, main loop, and cross-system coordination.
- **GameObject**: Base class ensuring binary compatibility with original Pascal data files (`.grp`/`.idx`).
- **FileLoader**: Robust loading/saving of raw binary data.
- **Data Structures**: `Role`, `Item`, `Magic`, `Scene` classes fully mapped to original memory layouts.
- **InputManager**: Frame-level input state; roaming / dialogue / camera paths partially migrated (UI still uses blocking `PollEvent` in places).

### Graphics & Rendering (SDL3)
- **GraphicsUtils**: Encapsulates SDL3 rendering primitives.
- **PicLoader**: Decodes legacy `.pic` resource format (RLE/LZW compressed images).
- **SceneManager**:
    - Isometric map rendering (`DrawScene` / world map).
    - Dynamic sprite rendering with correct depth sorting.
    - Cloud/Weather effects (`DrawClouds`).
    - Palette-based color handling.
    - SMP / MMAP sprite systems split to avoid index collisions.

### Event System (Scripting)
- **EventManager**: KYS script interpreter (`ExecuteEvent`).
- **Opcode coverage**: Core path largely Done (~90% of Pascal `instruct_*`).
    - Includes fail-to-title (`15`), SData tile write (`17`, fixed from misbound Delay), softstar/repute/ethics UI (`51`–`56`), Huashan script (`58`), leave-all (`59`), pic check (`60`), gender (`63`), shop (`64`→`ShowShop`), music/sfx (`66`/`67`).
    - **Little games**: `LittleGameManager` (Poetry/Acupuncture/ShotEagle/Lamp/rotoSpell/FemaleSnake) + `Instruct_Puzzle` (540); wired by `HandleInstruct43Sub` / `50e·43`.
    - Remaining gaps: deeper `instruct_50e` coverage; littlegame pixel fidelity polish.
- **Interaction**: Script triggering via coordinates and keyboard; world-map entrance (`TryEnterScene` / `CheckWorldEntrance`).

### Game Flow
- **Startup**: Title Screen → Character Creation → Game Loop.
- **Character Creation**: Name input + reroll; start at Scene 0 `(38,38)` (see `InitNewGame`). Difficulty menu (`MenuDifficult`) not ported.
- **Roaming**: Tile movement, collision, event triggering, enter/leave scenes.
- **Save/Load**: Backend for `R/S/D/G*.grp`; title load UI aligned with Pascal `MenuLoadAtBeginning` (vertical `CommonMenu`); in-game system menu uses horizontal save/load rows.

### UI System
- **Text**: GBK / UTF-8, shadow text, traditional→simplified display path.
- **Menus**:
    - Title / character creation / dialogue.
    - ESC ring menu (`NewMenuEsc` alignment).
    - Status, inventory (use/equip/train paths), magic view (`InModeMagic`).
    - System menu: save/load slots, volume, quit.
    - **Shop**: `ShowShop` buy/sell; stock from x50 then `Rshop`; wired via `instruct_64` + `50e` shop type 4.

### Battle System (Alpha)
- **BattleManager**: Loads battle data; turn loop; manual `BattleMenu` (move/attack/magic).
- **Items in battle**: Medicine / poison / hidden weapon paths present (fidelity TBD).
- **AI**: Basic `AutoBattle` (not full Pascal AI).
- **VFX**: Basic `PlayMagicAmination` / `.eft` — needs deepening.

## 2. Unfinished / Missing Features

### Event System (highest drama-block risk)
- **Remaining**: `instruct_50e` depth check; littlegame fidelity polish.
- **Done (2026-07-20 littlegames)**: `LittleGameManager` — Poetry/Acupuncture/ShotEagle/Lamp/rotoSpell/FemaleSnake; `Instruct_Puzzle` (540); wired via `HandleInstruct43Sub` including `-25`.
- **Done (2026-07-20 shop+mini)**: `ShowShop` buy/sell + `instruct_64` hook; `50e` Rshop r/w; (superseded stub `RunMiniGame` for main games).
- **Done (2026-07-20 batch 2)**: `15`/`17`/`24`/`51`–`56`/`58`–`60`/`63`/`64`/`66`/`67` (+ fixed `17` misbound as Delay).
- **Done (batch 1)**: Stub `33`/`34`/`35`/`37`/`41`/`42`/`69`; `20`/`22`/`45`–`49`.

### Battle
- Damage/healing formulas vs `kys_battle.pas` 1:1 verification.
- Full range-select family, complex AI, PetEffect, richer `.eft` playback.

### UI / World
- `FourPets` / pet learn UI; `CheckHotkey` (1–6); `ShowMap` / `ShowSceneName`.
- `SetScene` atmosphere (fog/rain/snow) incomplete.
- Littlegame polish: arrow trajectory, acupuncture crop highlights, Lamp `DrawSPic`.

### Audio
- Music still Windows `mciSendString`-centric; non-Windows fallback limited.
- Sound coverage incomplete.

## 3. Areas for Improvement

- Unify remaining UI blocking loops onto `InputManager` / event-driven model.
- Cross-platform audio (`USE_SDL2_MIXER` or SDL3 equivalent).
- Clean `size_t`→`int` warnings; centralize hardcoded UI coordinates.
- Continuous save binary compatibility checks (`save_inspector`).

## 4. Known Issues / Doc Corrections

| Outdated claim | Current fact |
|----------------|--------------|
| Shop UI not implemented | `ShowShop` buy/sell + `instruct_64` + Rshop via `50e` |
| `instruct_23` = animation | `23` = poison; `27` = animation |
| Inventory / system save UI missing | Implemented |
| Start at Scene 70 / (19,20) | Scene 0 `(38,38)` |

---
*Maintained with [doc/PROGRESS.md](doc/PROGRESS.md) and [TODO_LIST.md](TODO_LIST.md).*
