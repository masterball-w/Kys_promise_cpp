# KYS C++ Refactoring Project Status

**Last Updated:** 2026-08-07  
**Status:** Alpha ~0.7 / Core Loop + Shop + Little Games + Gongti 26–28 + Dual-build scaffolding  
**权威功能对照:** [doc/FEATURE_PORT_LIST.md](doc/FEATURE_PORT_LIST.md)（Pascal ↔ C++）  
**详细对齐清单:** [doc/PROGRESS.md](doc/PROGRESS.md)（以 Pascal 函数为单元）  
**待办与开发计划:** [TODO_LIST.md](TODO_LIST.md)  
**开场/脚本101回归:** [doc/REGRESSION_SCRIPT_101.md](doc/REGRESSION_SCRIPT_101.md)  
**Android:** [../ANDROID.md](../ANDROID.md)  
**构建约定:** 源码 `cpp_reborn/` → 唯一产物目录仓库根 `build/`（见根目录 `CMakePresets.json` / `README.md`）

## 1. Completed Systems

### Core Architecture
- **GameManager**: Central singleton managing global state, main loop, and cross-system coordination. **`SaveGame(0)` 不再覆盖新游戏模板 `alldef`/`allsin`**（2026-07-28）。
- **GameObject**: Base class ensuring binary compatibility with original Pascal data files (`.grp`/`.idx`).
- **FileLoader**: Robust loading/saving; data root via `PlatformCompat` (`game_data` / Android paths).
- **Data Structures**: `Role`, `Item`, `Magic`, `Scene` classes fully mapped to original memory layouts.
- **InputManager**: Frame-level input state; roaming / dialogue / camera paths partially migrated (UI still uses blocking `PollEvent` in places).
- **PlatformCompat / VirtualControls**: Android BACK mapping; on-screen D-pad + Esc; dual-path Present overlay.

### Graphics & Rendering (SDL3)
- **GraphicsUtils**: Encapsulates SDL3 rendering primitives；`DrawRLE8` 热点对齐 Pascal（`xs/ys+1`）。
- **PicLoader**: Decodes legacy `.pic` resource format (RLE/LZW compressed images).
- **SceneManager**:
    - Isometric map rendering (`DrawScene` / world map).
    - **`InitialScene`**：保留 `allsin` 层3；`DData[5]:=DData[7]`；层空才重建。
    - Dynamic sprite rendering with correct depth sorting.
    - Cloud/Weather effects (`DrawClouds`) — atmosphere SetScene incomplete；**场景漫游不画云**。
    - Palette-based color handling.
    - SMP / MMAP sprite systems split to avoid index collisions.

### Event System (Scripting)
- **EventManager**: KYS script interpreter (`ExecuteEvent`).
- **Opcode coverage**: Core path largely Done (~90% of Pascal `instruct_*`).
    - Includes fail-to-title (`15`), SData tile write (`17`), softstar/repute/ethics UI (`51`–`56`), Huashan script (`58`), leave-all (`59`), pic check (`60`), gender (`63`), shop (`64`→`ShowShop`), music/sfx (`66`/`67`).
    - **Little games**: `LittleGameManager` + `Instruct_Puzzle` (540); wired by `HandleInstruct43Sub` / `50e·43`.
    - Remaining gaps: deeper `instruct_50e` coverage; littlegame pixel fidelity polish.
- **Interaction**: Script triggering via coordinates and keyboard; world-map entrance (`TryEnterScene` / `CheckWorldEntrance`).

### Game Flow
- **Startup**: Title Screen → Character Creation → Game Loop.
- **Character Creation**: Name input + reroll; `InitNewGame` 强制重载模板 `alldef`/`allsin` 后进场景 0（权威数据：事件0=`8284`，孔霹雳事件1=`8268` 在卧室旁）。
- **Roaming**: Tile movement, collision, event triggering, enter/leave scenes; `ShowSceneName` present; `ShowMap` missing.
- **Save/Load**: Backend for `R/S/D/G*.grp`；**slot0 仅写 ranger，保护 alldef/allsin 模板**；title load UI; in-game system menu save/load.

### UI System
- **Text**: Win MultiByte + non-Win `SDL_iconv`; GBK-prefer talk; shadow text.
- **Menus**: Title / character creation / dialogue; ESC ring; status; inventory; magic; system; **Shop**.
- **Editor (decoupled)**: `editor/` PySide6 — ranger/events/battle/assets; 开场字段单测 `test_alldef_scene0_opening_pics`。

### Battle System (Alpha+)
- **BattleManager**: Turn loop; manual menu; items/medicine/poison paths; basic AI; `.eft` basic playback.
- **Gongti BattleState 26/27/28**: per-turn attack stacks, aura poison, med/detox reinforce — implemented; edge cases TBD.
- **PetEffect**: Main path present; fidelity vs Pascal TBD.

### Dual-build (Scheme A)
- Desktop: `kys_cpp` via `cpp_reborn/CMakeLists.txt`.
- Android: `libkys_promise.so` via `kys-promise-androidstudio` jni FetchContent + `add_subdirectory(cpp_reborn)`.
- Audio: Win MCI; non-Win WAV loop (+ convert script). **True device playability still open** (see TODO §5 / plan P2).

## 2. Unfinished / Missing Features

See **[doc/FEATURE_PORT_LIST.md](doc/FEATURE_PORT_LIST.md)** and **[TODO_LIST.md](TODO_LIST.md)**. Headline gaps:

- **Opcode 7 / 61** missing in `ExecuteEvent` (event end / relative jump).
- Battle formula / range / VFX / AI fidelity vs `kys_battle.pas`.
- `instruct_50e` edge codes / rename UI / InputAmount.
- Android real-device closed loop (data, confirm key, music, fonts).
- `ShowMap`, pets UI, hotkeys, SetScene atmosphere.
- Unified cross-platform music (beyond preconverted WAV); InputManager migration.

## 3. Areas for Improvement

- Unify remaining UI blocking loops onto `InputManager`.
- Cross-platform audio (`USE_SDL2_MIXER` or SDL3 equivalent).
- Clean `size_t`/`Uint64` warnings; centralize hardcoded UI coordinates.
- Continuous save binary compatibility checks (`save_inspector`).

## 4. Known Issues / Doc Corrections

| Outdated claim | Current fact |
|----------------|--------------|
| Shop UI not implemented | `ShowShop` buy/sell + `instruct_64` + Rshop via `50e` |
| `instruct_23` = animation | `23` = poison; `27` = animation |
| Inventory / system save UI missing | Implemented |
| Start at Scene 70 / (19,20) | Scene 0 `(38,38)` |
| `ShowSceneName` missing | Implemented; `ShowMap` still missing |
| Android impossible / separate engine | Scheme A dual-build scaffolding in tree; device QA pending |

---
*Maintained with [doc/PROGRESS.md](doc/PROGRESS.md) and [TODO_LIST.md](TODO_LIST.md).*
