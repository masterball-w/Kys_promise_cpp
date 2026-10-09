# Pascal 实现映射 (C++ 重构参考)

> **进度真源已迁移：** 请优先查阅 [FEATURE_PORT_LIST.md](FEATURE_PORT_LIST.md) 与 [PROGRESS.md](PROGRESS.md)。  
> 本文保留早期「任务→Pascal 符号」映射作辅参考；行号可能漂移，勿当作完成度清单。

**项目原则**: 除非明确需要改进，否则逻辑必须严格复刻 Pascal 的行为。

---

## 🔴 第一阶段: 核心功能 (Phase 1: Core Features)

### C-00: ESC 菜单 (视觉修复)
*   **目标**: 修复贴图切片和按钮逻辑。
*   **Pascal 文件**: `kys_engine.pas`
*   **关键函数**: `NewMenuEsc` (第 5066 行)
*   **辅助函数**: `showNewMenuEsc` (第 5305 行)
*   **实现细节**:
    *   使用 `Menuesc_PIC` (Index 6) 和 `MenuescBack_PIC` (Index 7)。
    *   **切片逻辑**:
        ```pascal
        // i = 菜单索引 (0..5)
        srcX := (i mod 3) * 100;
        srcY := (i div 3) * 100 + 200; // +200 意味着使用第 2-3 行的“未选中”状态
        // 选中状态通常在 0-1 行 (Y=0 或 Y=100)
        ```
    *   **按钮布局**: 6 个按钮呈圆形/六边形排列。

### C-01: UI 系统补全
*   **目标**: 物品栏、系统菜单、存读档、开场动画。
*   **Pascal 文件**: `kys_engine.pas`, `kys_main.pas`
*   **关键函数**:
    *   **开场动画**:
        *   `PlayBeginningMovie` (kys_engine.pas 第 5575 行): 加载 `Begin.Pic` (MOVIE_file) 的每一帧并全屏显示。
        *   **逻辑说明**: 循环读取帧，每帧延迟 20ms，支持 ESC/Space/Enter 跳过。
        *   **移植实现**: `UIManager::PlayTitleAnimation()`。动画结束后加载 `Background.Pic` Index 1 作为背景。
    *   **物品栏**:
        *   `NewMenuItem` (第 5974 行): 物品栏主循环。
        *   `showNewItemMenu` (第 6068 行): 渲染逻辑。
        *   `SelectItemUser` (第 6099 行): 选择使用物品的角色。
    *   **系统菜单**:
        *   `NewMenuSystem` (第 4360 行): 主循环。
        *   `NewShowMenuSystem` (第 4486 行): 渲染逻辑。
    *   **存读档**:
        *   `NewMenuSave` (第 4532 行)。
        *   `NewMenuLoad` (第 4622 行)。
        *   **注意**: Pascal 使用 `AssignFile`, `Rewrite`, `BlockWrite` 进行二进制序列化。C++ 实现必须匹配 `R*.grp` 文件的结构布局。

### C-02: 战斗交互
*   **目标**: 玩家控制 (移动、攻击、武功、物品) 和 AI。
*   **Pascal 文件**: `kys_battle.pas`
*   **关键函数**:
    *   **战斗菜单**: `BattleMenu` (第 1142 行)。
        *   使用位掩码 `menustatus` 控制按钮的可用性 (移动、攻击、武功、物品)。
    *   **移动逻辑**: `MoveRole` (第 1395 行) & `CalMoveAbility` (第 1089 行)。
    *   **攻击逻辑**: `Attack` (第 2430 行) & `AttackAction` (第 2601 行)。
    *   **武功逻辑**: `SelectMagic` (第 2657 行)。
    *   **AI 逻辑**: `AutoBattle` (第 4508 行) & `Auto` (第 6608 行)。

### C-03: 事件系统与指令 (详细映射)
*   **目标**: 补全指令集并修复事件流。
*   **Pascal 文件**:
    *   `kys_main.pas`: **解释器循环** (约第 4900 行)。
    *   `kys_event.pas`: 指令具体实现。

#### ⚠️ 关键指令差异与风险
*   **Instruct_23 (Opcode 23)**:
    *   **Pascal**: `procedure instruct_23(rnum, Poision: integer);`
    *   **功能**: 设置角色中毒/用毒属性 (`UsePoi`)。
    *   **参数数量**: 2 个 (rnum, Poison)。
    *   **注意**: 绝对不是动画指令。如果按动画指令解析(4参数)会导致参数错位。
*   **Instruct_27 (Opcode 27)**:
    *   **Pascal**: `procedure instruct_27(enum, beginpic, endpic: integer);`
    *   **功能**: 播放动画。

#### 指令表 (基于 kys_main.pas 解析)
| Opcode | 函数名 | 参数数量 (不含Op) | 逻辑说明 |
| :--- | :--- | :--- | :--- |
| 0 | `instruct_0` | 0 | 空指令/重画 |
| 1 | `instruct_1` | 3 | 对话 (TalkNum, HeadNum, DisMode) |
| 2 | `instruct_2` | 2 | 获得/失去物品 (ItemNum, Amount) |
| 3 | `instruct_3` | 13 | 修改场景/事件 (数组参数) |
| 4 | `instruct_4` | 3 | 询问使用物品 -> 跳转 (Ret: Offset) |
| 5 | `instruct_5` | 2 | 询问战斗 -> 跳转 |
| 6 | `instruct_6` | 4 | 战斗 (BattleNum, Jump1, Jump2, GetExp) |
| ... | ... | ... | ... |
| 19 | `instruct_19` | 2 | 瞬移 (X, Y) |
| 23 | `instruct_23` | 2 | **中毒/用毒** (Role, Poison) |
| 25 | `instruct_25` | 4 | 移动镜头/角色? (x1, y1, x2, y2) |
| 27 | `instruct_27` | 3 | **动画** (EventNum, BeginPic, EndPic) |
| 50 | `instruct_50` | 7 | **复杂逻辑** (Ret: JumpOffset 或 修改变量) |

**解释器逻辑**:
Pascal 使用 `case e[i] of` 跳转，每个 case 执行完后 `i := i + N`，其中 N = 参数数量 + 1 (Opcode本身)。
例如 Opcode 1: `instruct_1(e[i+1], e[i+2], e[i+3]); i := i + 4;`
**注意**: `instruct_50` 返回值 `p` 会影响指令指针 `i` (跳转) 或者修改内存 `e` (变量赋值)，这是实现脚本逻辑分支的关键。

### C-04: 视觉特效 (VFX)
*   **目标**: 渲染武功/攻击动画、天气、色调。
*   **Pascal 文件**: `kys_gfx.pas`, `kys_battle.pas`
*   **关键函数**:
    *   **武功动画**: `PlayMagicAmination` (kys_battle.pas 第 2892 行)。
    *   **图形变换**: `rotozoomSurfaceXY` (kys_gfx.pas) - 用于旋转缩放。
    *   **天气**: `DrawClouds` (kys_gfx.pas) - 云层/雨雪效果。
    *   **色调**: `DrawCPic` (kys_gfx.pas) - 支持 alpha 混合和阴影。

### C-05: 物品与装备逻辑
*   **目标**: 实现物品使用效果和装备属性加成。
*   **Pascal 文件**: `kys_event.pas`, `kys_engine.pas`
*   **关键函数**:
    *   **使用物品**: `instruct_32` (增加物品), `instruct_2` (显示获得物品)。
    *   **装备属性**: `GetRoleAttack`, `GetRoleDefence` (kys_event.pas 第 118 行起) - 计算装备后的最终属性。
    *   **属性计算**: 必须包含 `Equip` 带来的加成。

---

## 🟡 第二阶段: 架构优化 (Phase 2: Architecture Optimization)

### O-01: 输入系统重构
*   **目标**: 集中输入处理 (移除 `SDL_PollEvent` 循环)。
*   **Pascal 文件**: `kys_engine.pas`
*   **关键函数**: `CheckBasicEvent` (第 7137 行)。
*   **当前 Pascal 模式**: 每个菜单函数 (`NewMenuEsc`, `BattleMenu`) 都有自己的 `while (SDL_WaitEvent)` 循环。C++ 应迁移到基于帧的 `Update()` 模型。

### O-02: 音频系统
*   **目标**: 从 Windows MCI 迁移到 SDL_mixer。
*   **Pascal 文件**: `kys_engine.pas`
*   **关键函数**:
    *   `PlayMP3` (第 279 行): 处理背景音乐。
    *   `PlaySound` (第 359 行): 处理音效 (SFX)。
