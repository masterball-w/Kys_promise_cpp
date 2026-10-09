# KYS C++ 重构工程开发文档（踩坑 / 缺陷记录）

> **状态与功能对照请看：** [FEATURE_PORT_LIST.md](FEATURE_PORT_LIST.md)、[PROGRESS.md](PROGRESS.md)、[../TODO_LIST.md](../TODO_LIST.md)。  
> 本文保留历史踩坑与已修缺陷说明；页内旧日期仅为记录时间点。

**文档维护提示（2026-08-07）：** 小游戏主路径、功体 26–28、商店、`SaveGame(0)` 模板保护等已落地；§4 中若仍写「未集成小游戏」等旧表述，以 FEATURE_PORT_LIST 为准。

---

## 1. 重构核心原则与历史

### 1.1 核心原则
- **二进制兼容性**：直接读取原版 `.grp`, `.idx`, `.smp`, `mmap.grp` 等资源文件。
- **逻辑保真度**：核心算法（战斗公式、事件指令）严格对齐 Pascal 源码 (`kys_*.pas`)。
- **模块化管理**：采用单例模式管理 `GameManager`, `SceneManager`, `EventManager`, `UIManager` 等核心子系统。

### 1.2 重大改动历史
- **2026-01-16**：完成基础架构搭建，实现地图渲染与基础角色移动。
- **2026-01-21**：修复了制约重构进度的渲染与贴图 Bug（Issue 1 & 2）。剧情对话闪过问题（Issue 3）目前仍未解决，需后续调查。

---

## 2. 关键问题修复逻辑 (Correction Logic)

针对 ISSUE_LOG_20260119.md 中提到的阻塞性问题，目前的修复状态如下：

### 2.1 Gold 先生卧床贴图切换失败 (Issue 1) - **已修复**
*   **问题现象**：执行剧情指令修改地图贴图（如床铺切换）时，屏幕无反应或逻辑跳过。
*   **根本原因**：
    1.  **坐标边界错误**：在 `EventManager::Instruct_ModifyEvent` 中，判断坐标是否有效的逻辑使用了 `currentX > 0`，导致 (0,0) 坐标（床铺常用位置）被忽略。
    2.  **事件 ID 逻辑冲突**：SceneManager 对 Layer 3 (事件层) 的空值处理与指令系统不统一。
    3.  **渲染刷新缺失**：指令执行后未强制触发 `Redraw`。
*   **修正逻辑**：
    -   将 [EventManager.cpp](file:///e:/Github/kys-promise-main/cpp_reborn/src/EventManager.cpp) 中的坐标有效性判断修改为 `currentX >= 0`。
    -   统一 Layer 3 存储逻辑：`-1` 代表无事件，`0..199` 代表有效事件 ID。
    -   在 `Instruct_ModifyEvent` 和 `Instruct_Redraw` 中补全了 `RenderScreenTo` 调用，确保修改立即同步到 GPU 纹理。

### 2.2 孔霹雳引用贴图错误 (Issue 2) - **已修复**
*   **问题现象**：NPC（如孔霹雳）显示的贴图不是对应的角色模型，而是其他静态物体的贴图。
*   **根本原因**：
    1.  **资源空间冲突**：原版将静态物体 (smp) 和 角色模型 (mmap.grp) 分开存储。旧版代码统一调用 `DrawSprite`，导致索引在 smp 资源中越界或错位。
    2.  **索引计算偏差**：Pascal 中 `DData[..., 5]` (贴图索引) 为正值时引用 smp，为负值时引用角色模型，且计算方式涉及 `div 2`。
*   **修正逻辑**：
    -   **渲染分流**：在 [SceneManager.cpp](file:///e:/Github/kys-promise-main/cpp_reborn/src/SceneManager.cpp) 中引入 `DrawSmpSprite` 和 `DrawMmapSprite`。
    -   **对齐 Pascal 公式**：
        -   若 `eventPic > 0`，调用 `DrawSmpSprite( (eventPic / 2) - 1 )`。
        -   若 `eventPic < 0`，调用 `DrawMmapSprite( (-eventPic / 2) - 1 )`。
    -   这种映射方式精确还原了原版资源文件的索引偏移逻辑。

### 2.3 脚本 101 / 开场面向与事件修改 (2026-07-24) - **已修复**
*   **问题现象**：脚本 101 中 PC 190 设置面向后主角朝向与原版相反；事件修改后贴图刷新不稳定。
*   **根本原因**：
    1.  `Instruct_40` 只改 `m_mainMapFace`，未改场景用的 `m_subMapFace`（Pascal `Sface`）。
    2.  `DrawScene` 重复绘制且主角第二遍使用 `mainMapFace` 错误映射。
    3.  `Instruct_ModifyEvent` 写 `SData` 条件与 Pascal 不一致（`X>0` / `Y>=0`）。
*   **回归文档**：见 [REGRESSION_SCRIPT_101.md](REGRESSION_SCRIPT_101.md)。

### 2.4 开场卧室事件层「丢失」——实为模板被覆盖 (2026-07-28) - **已修复**
*   **问题现象**：新游戏卧室看不到躺床贴图 **8284**、看不到孔霹雳 **8268**；两句对话后改成 **8282** 却能显示。控制台曾显示事件1在 `(14,28)`（大厅方向）且 `eventsDrawn` 在卧室镜头外。
*   **根本原因（数据，非绘制公式）**：
    1.  仓库 `game_data/save/alldef.grp` / `allsin.grp` 被**事后剧情状态**覆盖：事件0 `pic` 被清 0；事件1 被挪到 `(Y=28,X=14)` 并带上脚本 `104/1701`——与脚本 **101 跑完后的内存态**一致。
    2.  稳定发布版 `前传64位-sdl3` 模板正确：事件0=`8284@(38,40)`，事件1=`8268@(40,37)`（卧室旁），`allsin` 层3 `(37,40)=1`。
    3.  制作器 `SceneEventData`/`SceneMapData` **roundtrip 无损**，不是 editor 保存写坏。
    4.  引擎 `SaveGame(0)` 原会把内存 D/S 写回 `alldef`/`allsin`（与 Softstar slot0=模板约定相同），易永久毁掉新游戏开场。
*   **修正逻辑**：
    -   从稳定版复制权威 `alldef.grp` / `allsin.grp` 到 `game_data/save/`（损坏文件留 `*.corrupt_*.bak`）。
    -   `SaveGame(0)` **不再写** `alldef`/`allsin`（仅写 `ranger`）。
    -   `InitNewGame` 强制重载模板 + Pascal 对齐的 `InitialScene`（保留层3；`DData[5]:=DData[7]`）。
    -   `DrawRLE8` 热点对齐 Pascal（`xs/ys+1`）。
    -   制作器单测 `test_alldef_scene0_opening_pics` 锁定稳定版开场字段。
*   **回归文档**：见 [REGRESSION_SCRIPT_101.md](REGRESSION_SCRIPT_101.md)。

---

## 3. 已知问题与待处理 (Known Issues & Pending)

### 3.1 剧情对话一闪而过 (Issue 3) - **未解决**
*   **问题现象**：在镜头平移指令（Instruct_25）结束后，紧接着的对话框会瞬间消失（闪过）。
*   **现状**：已尝试在 `ShowDialogue` 中增加 200ms 消抖延迟，但根据最新测试结果，该问题依然存在，可能涉及底层指令同步或事件冒泡。
*   **后续调查方向**：
    -   调查 `EventManager::Instruct_25` 是否在平移结束后意外发送了确认信号。
    -   检查 `SDL_PollEvent` 是否在平移期间积压了多个输入事件。
    -   验证 `Instruct_Redraw` 是否干扰了对话框的显示。

### 3.2 开场 101 全流程目视回归 - **待用户确认**
*   稳定模板恢复后：卧室 8284 + 孔霹雳 8268 → 8282 坐起 → 平推等，需在本机再跑一遍签字。

---

## 4. 待改动与功能补充 (Pending Tasks)

### 4.1 核心指令完善
- [ ] **全局贴图替换 (Instruct_38)**：目前已实现逻辑，需验证是否需要在指令结束时自动触发 `Redraw`。
- [ ] **复杂动画指令 (Instruct_27/44)**：目前仅实现单次更新，需支持循环播放与帧序列同步。
- [ ] **小游戏集成**：如开锁、针灸等基于 Pascal 逻辑的 C++ 移植。

### 4.2 战斗系统对齐
- [ ] **伤害公式验证**：需对照 `kys_battle.pas` 重新核对 `Attack` 和 `Defend` 逻辑。
- [ ] **AI 路径规划**：目前 AI 移动较为简单，需移植原版的 A* 变种算法。

---

## 5. 优化建议 (Optimizations)

### 5.1 渲染性能
- **资源缓存**：目前的 RLE8 解压是实时的，对于高频渲染的贴图（如主角、常用地表），应在解压后缓存为 `SDL_Texture` 以减少 CPU 消耗。
- **脏矩形渲染**：目前每帧全屏重绘，可优化为仅重绘变动的 Tile 区域。

### 5.2 架构改进
- **统一输入管理**：目前各模块通过 `SDL_PollEvent` 自行处理输入，建议抽象出 `InputManager` 统一管理按键状态与事件分发。
- **音频引擎迁移**：将基于 Windows MCI 的音乐播放器替换为 `SDL_mixer`，以支持更广泛的音频格式（如 OGG/FLAC）并提高跨平台兼容性。

---
*本文档由 Trae AI Assistant 生成，最后更新于 2026-01-21。*
