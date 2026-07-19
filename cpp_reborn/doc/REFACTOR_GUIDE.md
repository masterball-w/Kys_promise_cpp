# KYS Project C++ Refactoring Guide

## 1. Project Overview
This project is a C++ refactoring of the classic "KYS" (Kam Yung's Stories) engine, originally written in Pascal/Delphi.
The goal is to modernize the codebase while maintaining logical consistency with the original data formats and game mechanics.

## 2. Architectural Principles
1.  **Data Compatibility**: The original game relies heavily on specific binary layouts (arrays of `smallint`). C++ classes must maintain the ability to serialize/deserialize from these raw formats to support existing data files (`.grp`, `.idx`, etc.).
2.  **Object-Oriented Design**:
    -   Replace global procedural logic with Class-based encapsulation.
    -   Use `GameManager` (Singleton) for global state.
    -   Use `SceneManager` for map handling.
    -   Use `BattleManager` for combat.
3.  **Extensibility**:
    -   Use inheritance where appropriate (e.g., `GameObject` -> `Character`, `Item`).
    -   Support polymorphism for game entities to allow future modding capabilities.
4.  **No Hallucinations**: Code logic must be derived strictly from the existing Pascal source files (`kys_*.pas`).

## 3. Directory Structure
-   `src/`: Source code (.cpp)
-   `include/`: Header files (.h)
-   `doc/`: Documentation and Guides
-   `data/`: Game resources (images, scripts, data files)

## 4. Task List
- [ ] **Phase 1: Foundation**
    - [x] Create Directory Structure
    - [ ] Setup CMake Build System
    - [ ] Define Basic Types (`GameTypes.h`)
    - [ ] Implement Core Data Classes (`Role`, `Item`, `Magic`, `Scene`)
- [ ] **Phase 2: Engine Core**
    - [ ] Initialize SDL3
    - [ ] Resource Loading (Images, Sprites)
    - [ ] Font/Text Rendering (Big5/GBK handling)
- [ ] **Phase 3: Game Logic**
    - [ ] Main Game Loop
    - [ ] Event System (Port `kys_event.pas`)
    - [ ] Scene Navigation
- [ ] **Phase 4: Battle System**
    - [ ] Turn-based Battle Logic (Port `kys_battle.pas`)
    - [ ] AI Logic

## 5. Type Mapping (Pascal -> C++)
| Pascal | C++ | Notes |
|Str | Str | Notes |
|---|---|---|
| `smallint` | `int16_t` | Critical for binary compatibility |
| `integer` | `int32_t` | |
| `uint16` / `word` | `uint16_t` | |
| `TRole` | `class Role` | |
| `TItem` | `class Item` | |

## 6. Implementation Rules
-   **Variant Records**: Use `union` or internal `std::vector<int16_t>` buffer with accessors to mimic Pascal's `case` records.
-   **Strings**: Convert C-style fixed char arrays to `std::string` for internal logic, but keep raw arrays for I/O if needed.
