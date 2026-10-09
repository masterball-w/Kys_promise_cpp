# 金庸群侠前传 C++ 重构：Pascal 对齐进度清单

**更新日期:** 2026-08-09  
**对齐原则:** 以 `kys_*.pas` 玩法级函数为单元；C++ 实现主要在 `cpp_reborn/src/`。状态以**源码实测**为准。  
**权威功能对照 list:** [FEATURE_PORT_LIST.md](FEATURE_PORT_LIST.md)  
**Pascal 早期映射（辅参考）:** [PASCAL_MAPPING.md](PASCAL_MAPPING.md)  
**待办与分阶段计划:** [../TODO_LIST.md](../TODO_LIST.md)

**状态图例**

| 标记 | 含义 |
|------|------|
| `[x]` Done | 可用且主路径对齐 |
| `[~]` Partial | 有实现，但缺分支 / 保真度 / 接入点 |
| `[ ]` Stub / Missing | 占位或未实现 |

---

## A. 启动 / 标题 / 存读档（`kys_main` + `kys_engine`）

| 状态 | Pascal 单元 | C++ 对应 | 备注 |
|------|-------------|----------|------|
| [x] | `Run` / `Quit` / `ReadFiles` | `GameManager::Init/Run/Quit` | |
| [x] | `Start` + 标题三项 | `UpdateTitleScreen` / `DrawTitleMenu` | |
| [x] | `PlayBeginningMovie` | `UIManager::PlayTitleAnimation` | |
| [~] | `InitialRole` / `RandomAttribute` / 建角 | 建角 + `InitNewGame` 重载模板 | 难度菜单未见；开场强制重载 alldef/allsin |
| [x] | `LoadR` / `SaveR` | `LoadGame` / `SaveGame` | slot0 **不**写回 `alldef`/`allsin` 模板（2026-07-28） |
| [x] | `MenuLoadAtBeginning` | `ShowSaveLoadMenu(false)` | 已竖排对齐 CommonMenu |
| [x] | `NewMenuSave` / `NewMenuLoad` | 系统菜单内横排存读 | |
| [~] | `MenuSave`（旧竖排） | `ShowSaveLoadMenu(true)` 布局预留 | 标题侧基本不走存档 |
| [~] | `ShowSaveSuccess` / `QuitConfirm` | 对话提示 / 系统退出 | |

---

## B. 主地图 / 场景漫游（`kys_main` + `kys_engine`）

| 状态 | Pascal 单元 | C++ 对应 | 备注 |
|------|-------------|----------|------|
| [x] | `Walk` / `CanWalk` | 大地图漫游 + 碰撞 | |
| [x] | `CheckEntrance` / `ReSetEntrance` | `TryEnterScene` / `CheckWorldEntrance` | |
| [x] | `WalkInScene` / `CanWalkInScene` | 场景漫游 | |
| [x] | `CheckEvent3` | 场景事件触发 | |
| [x] | `DrawMMap` / `DrawScene` / `InitialScene` | `SceneManager` | InitialScene 保留层3；场景内不画云；开场见 REGRESSION_SCRIPT_101 |
| [x] | `DrawClouds` / `CloudCreate*` | 云层 | |
| [~] | `JmpScene` / `SetScene`（雾雨雪） | `Instruct_JmpScene`；天气 | `SetScene` 氛围未完整 |
| [~] | `findway` / `Moveman` | `Instruct_25` / `Instruct_30` | |
| [x] | `ShowSceneName` | `UIManager::ShowSceneName` | |
| [ ] | `ShowMap` | — | Missing |

---

## C. 事件解释器与指令（`kys_main.CallEvent` + `kys_event.instruct_*`）

解释器循环：`EventManager::ExecuteEvent` — **Done（骨架）**。缺口在单条 opcode。  
完整对照见 [FEATURE_PORT_LIST.md](FEATURE_PORT_LIST.md) §4。

### C0. 已知 Missing（高优先，2026-08-07 核实）

- [ ] **`7` 结束事件**（Pascal `break`；C++ 无 case）
- [ ] **`61` 相对跳转**（Pascal `i += e[i+1]; i += 3`；C++ 无 case）

### C1. 已对齐（主路径可用）

- [x] `0` 重画
- [x] `1` 对话
- [x] `2` 得物
- [x] `3` 改事件
- [x] `4` / `5` 询问跳转
- [x] `6` 战斗
- [x] `8` 音乐
- [x] `9` 询问入队
- [x] `10` 入队
- [x] `11` / `12` 住宿
- [x] `13` / `14` 亮/黑屏
- [x] `15` 失败回标题 · `17` 改场景图层
- [x] `20` 队伍已满 · `21` 离队 · `22` 内力清零 · `24` 空
- [x] `23` 用毒属性（**不是动画**）
- [x] `25` 镜头
- [x] `26` 事件参数加减
- [x] `27` 动画
- [x] `28` / `29` 道德/攻击区间
- [x] `30` 主角走动
- [x] `31` 金钱判定
- [x] `32` 静默加减物品
- [x] `33`–`35` 学武功 / 资质 / 写武功栏
- [x] `36` 性别/扩展条件
- [x] `37` 增减道德
- [x] `38` 换贴图
- [x] `39` 开放场景
- [x] `40` 朝向
- [x] `41` 随身物品 · `42` 队中有女
- [~] `43` 子功能/调事件（**内含小游戏占位**）
- [x] `44` 双动画
- [x] `45`–`49` 轻功/内力/武力/生命/内力属性
- [~] `50` → `50e` 扩展（需逐项验收）
- [x] `51`–`56` · `58`–`60` · `63`/`64` · `66`/`67`
- [x] `62` 游戏结束
- [x] `68` NewTalk
- [x] `69` ReSetName
- [x] `70` ShowTitle
- [x] `71` JmpScene

### C2. Stub / 吞参数 — 已补齐（2026-07-20）

- [x] `33` `instruct_33` 学武功（`StudyMagic`）
- [x] `34` `instruct_34` 加资质
- [x] `35` `instruct_35` 写武功栏
- [x] `37` `instruct_37` 增减道德
- [x] `41` `instruct_41` 改随身物品
- [x] `42` `instruct_42` 队中有女（跳转）
- [x] `69` `ReSetName` — 从 `name.idx/grp` 写入 Role/Item/Scene/Magic/Introduction

### C3. Missing（剩余较少）

- [x] `15` 失败回标题
- [x] `17` 改场景图层（**已修正**：原误绑 Delay）
- [x] `20` 队伍已满
- [x] `22` 内力清零
- [x] `24`（原版空实现）
- [x] `45`–`49` 属性增减 / 内力属性
- [x] `51`–`56` 软星对话、品德/声望显示、开放场景、事件判定、声望
- [x] `58`–`60` 华山论剑脚本、全员离队、事件贴图判定
- [x] `63` / `64` 性别；`64` 重制接入 `ShowShop(0)`（原版 Pascal 空实现）
- [x] `66` / `67` 音乐/音效
- [ ] `instruct_50e` 扩展 code 对照 Pascal 逐项验收
- [ ] `57`（原版注释掉，可不实现）
- [ ] `61`/`65`（若脚本有引用再查）

### C4. 小游戏（经 `instruct_43` 子功能，`kys_littlegame`）

- [x] `Poetry`（-2）— `LittleGameManager::Poetry`（talk 字库 + 交换作答）
- [x] `Acupuncture`（-4）— 序列记忆点穴（`list/Acupuncture.bin` / 回退网格）
- [x] `Puzzle`（540）— `Instruct_Puzzle` 场景推块（不写 `$7000`）
- [x] `ShotEagle`（-9）— 蓄力射雕
- [x] `Lamp`（-31）— Lights Out 黑白棋
- [x] `rotoSpellPicture`（-10）— 5×5 旋转拼图
- [x] `FemaleSnake`（-25）— 贪吃蛇；得分写入 `x50[e3]`
- [x] `-3` `GetPetSkill` 判定
- [x] `instruct_43` / `50e·43` 共用 `HandleInstruct43Sub`

---

## D. 战斗（`kys_battle`）

| 状态 | Pascal 单元 | C++ 对应 | 备注 |
|------|-------------|----------|------|
| [x] | `Battle` / `InitialBField` | `StartBattle` / `RunBattle` | Alpha |
| [x] | `SelectTeamMembers` | `BattleManager::SelectTeamMembers` | |
| [x] | `CountProgress` / `CalMoveAbility` / `MoveRole` | 有 | |
| [x] | `BattleMenu` / 手动移攻武 | `ShowBattleMenu` 等 | |
| [~] | `SelectMagic` / `Attack` / `AttackAction` | 有 | 范围选择族需核对 |
| [~] | `CalHurtValue` / `ShowHurtValue` | 有 | 须 1:1 对照 Pascal |
| [~] | `BattleMenuItem` / 医疗/用毒/暗器 | 有 | |
| [~] | `PlayMagicAmination` / `.eft` | 基础播放 | 深化中 |
| [~] | `AutoBattle` / `Auto` / `SelectAutoMode` | 基础 AI | 复杂 AI / 模式菜单弱 |
| [~] | `PetEffect` | `BattleManager::PetEffect` | 主路径有；分支保真待验 |
| [~] | `AddExp` / `CheckLevelUp` / `CheckBook` | 战后流程 | |
| [ ] | `TeamModeMenu` / `OldBattleMainControl` | — | 旧循环可不迁 |
| [x] | 功体 BattleState 26/27/28 | `BattleEffects` + `BattleManager` | 叠加/失去功体边界待验 |

---

## E. UI 菜单（`kys_engine` 新版为主）

| 状态 | Pascal 单元 | C++ 对应 | 备注 |
|------|-------------|----------|------|
| [x] | `NewMenuEsc` / `showNewMenuEsc` | ESC 环形菜单 | |
| [x] | `SelectShowStatus` / `NewShowStatus` | 状态界面 | |
| [x] | `NewMenuItem` / `SelectItemUser` | `SelectShowItem` | |
| [x] | `SelectShowMagic` / `InModeMagic` | 武功查看 | |
| [x] | `NewMenuSystem` / 音量/退出 | `SelectShowSystem` | |
| [~] | `NewMenuTeammate` | 队友相关 | |
| [ ] | `FourPets` / `PetStatus` / `PetLearnSkill` | — | Missing |
| [~] | `MenuMedcine` / `MenuMedPoision`（漫游） | UIManager 有部分 | |
| [ ] | `CheckHotkey`（1–6 快捷） | — | Missing |
| [x] | 商店（买卖 + Rshop/x50） | `ShowShop` + `instruct_64` | 买/卖；`50e` type4 读写商店 |

---

## F. 图音输入（`kys_engine`）

| 状态 | Pascal 单元 | C++ 对应 | 备注 |
|------|-------------|----------|------|
| [x] | `Draw*` 族 / `Redraw` / `DrawRectangle` | `SceneManager` + `UIManager` | |
| [~] | `PlayMP3` / `StopMP3` | `SoundManager`（Win MCI；非 Win WAV） | 统一解码库待做 |
| [~] | `PlaySound*` | WAV / 部分路径 | |
| [~] | `CheckBasicEvent` | `InputManager` + 阻塞 `PollEvent` | 架构未统一 |
| [~] | `rotozoomSurfaceXY` | 战斗/特效用 | |
| [x] | 虚拟键（方向+Esc） | `VirtualControls` | Android 默认；确认键可选增强 |
| [ ] | `SwitchFullscreen` | — | Missing/Partial |

---

## G. 进度总览

| 模块 | 完成度（粗估） | 阻塞剧情程度 |
|------|----------------|--------------|
| A 启动存读 | ~90% | 低 |
| B 地图场景 | ~85% | 中（氛围/ShowMap） |
| C 事件 opcode | ~90% | 中（`50e` 深化） |
| D 战斗 | ~65–75% | 高（公式/特效/AI；功体 26–28 已落地） |
| E UI | ~80% | 中（宠物/热键/ShowMap） |
| F 音频输入 | ~60% | 中（Android WAV 过渡；统一解码待做） |
| 小游戏 | ~85% | 主路径已迁；细节保真可继续打磨 |
| Android 双端 | ~40% | 脚手架+虚拟键已有；真机可玩闭环未完成 |
| 制作器 | ~70% | 与引擎解耦；场景可视化等可增强 |

---

## H. 建议优先补齐顺序

详见 [TODO_LIST.md](../TODO_LIST.md) **「下一步：完整开发计划」**（P0–P6）。摘要：

1. **P0 战斗保真** — `CalHurtValue` / 范围 / `.eft` / AI。
2. **P1 `instruct_50e` + 剧本抽样**。
3. **P2 Android 真机可玩闭环**。
4. **P3** 宠物 / 热键 / ShowMap / 氛围。
5. **P4** 音频统一 + InputManager 收敛。
6. **P5–P6** 制作器/工程化 → Beta。

---

## I. 已知文档矛盾（已纠正）

| 旧说法 | 正确口径 |
|--------|----------|
| 商店未实现 | `ShowShop` 买卖已接入；`instruct_64` + `50e` Rshop |
| `instruct_23` = 动画 | `23` = 用毒；`27` = 动画 |
| 物品/系统存读 UI 未做 | 已实现 |
| 起始坐标说法不一 | 以 `InitNewGame` Scene 0 `(38,38)` 为准 |

---

*与 [PROJECT_STATUS.md](../PROJECT_STATUS.md)、[TODO_LIST.md](../TODO_LIST.md) 同步维护。*
