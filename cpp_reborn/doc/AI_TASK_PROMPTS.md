# KYS C++ 重制版开发全景文档 (Master Development Doc)

**最后更新**: 2026-07-19
**版本**: Alpha 0.5
**状态**: 核心游戏循环修复与系统补全阶段

**角色**: 高级 C++ 游戏开发工程师 / 架构师
**项目目标**: 将《金庸群侠前传》(Pascal) 重构为 C++ (SDL3)，保持对原版二进制数据文件 (`.grp`, `.idx`) 的 **1:1 兼容性**，并实现现代化的代码架构。

---

## 1. 项目状态总览 (Status Overview)

### 核心原则
1.  **二进制兼容**: 直接读取原版资源，不修改文件结构。
2.  **逻辑保真**: 战斗公式、事件指令、AI 逻辑严格对齐 Pascal 源码。
3.  **架构现代化**: 单例模式管理子系统 (`GameManager`, `SceneManager` 等)，分离逻辑与渲染。

### 📅 最近重大更新 (Changelog)
*   **2026-07-19 (Core Loop & Input Fixes)**:
    *   **朝向修复**: 场景内统一使用 `m_subMapFace`（Pascal SFace 编码），空格交互改为 `KEY_UP` 触发，与原版一致。
    *   **对话修复**: `ShowDialogue` 增加 150ms 消抖，改用 `KEY_UP` 翻页；`Instruct_25` 镜头平移后清空输入队列。
    *   **文本编码**: 新增 `TextManager::talkToUtf8`，对话文本优先 Big5 解码。
    *   **InputManager**: 新增统一输入管理器，集中处理按键状态与事件冲刷。
    *   **大地图闭环**: 大地图移动至入口格自动尝试 `TryEnterScene`；空格触发 `CheckWorldEntrance`。
    *   **UI 补全**: 物品菜单统一走 `SelectShowItem`；新增 `ShowShop` 商店界面；实现 `GameManager::Rest()`。
    *   **事件增强**: 小游戏 Opcode 提供自动通过占位；`Instruct_27` 接入 `GameSpeed`。
*   **2026-01-24 (Critical Fixes & Tools)**:
    *   **事件系统修复**: 修正了 `Instruct_ModifyEvent` 中的自动触发逻辑。增加了距离检测，防止修改远端事件（如场景出口）属性时错误地立即触发该事件（解决了“大厅对话后直接跳过行走阶段”的问题）。
    *   **开局流程修正**: 将新游戏起始坐标修正为 Scene 0 (38, 38)，并修复了 `InitNewGame` 中摄像机未同步的问题。
    *   **资源加载优化**: 实现了 `GameManager::getHead` 和 `UIManager` 的**懒加载机制**，修复了对话框头像不显示和物品栏图标/简介缺失的问题。
    *   **调试工具链**: 新增 `save_inspector.exe` (存档/全剧数据查看器) 和 `analyze_data.exe` (事件定义分析器)，用于逆向分析 `.grp` 文件。
*   **2026-01-23 (UI & Event System)**:
    *   **ESC 菜单重构**: 初步实现环形菜单。
    *   **多帧动画**: 实现了 `instruct_23`。

---

## 2. 详细开发任务清单 (Task List)

### 🔴 Phase 1: 核心功能完善 (Priority: High)

#### 🚨 C-00: ESC 菜单视觉修复 (Critical)
*   **状态**: 已修复（`RenderMenuSystem` 切片逻辑对齐 Pascal `showNewMenuEsc`）

#### C-01: UI 系统补全
*   **当前状态**:
    *   [x] 物品列表显示（图标 + 简介）。
    *   [x] **物品使用/装备逻辑**: `SelectShowItem` 已实现使用/装备。
    *   [x] **系统菜单**: 存读档界面 UI 已有。
    *   [ ] 存读档二进制写入需持续用 `save_inspector` 验证。

#### C-02: 战斗系统交互
*   **状态**: `BattleMenu` 已实现，玩家可手动操作战斗。

#### C-03: 事件系统增强
*   **状态**: 小游戏 Opcode 有自动通过占位；`Instruct_27` 已接入 GameSpeed。

#### C-04: 视觉特效 (VFX)
*   **状态**: `PlayMagicAmination` 已有基础实现；`.eft` 完整特效待深化。

---

### 🟡 Phase 2: 架构优化 (Priority: Medium)

#### O-01: 输入系统重构
*   **状态**: `InputManager` 已创建并接入漫游/对话/镜头平移。

#### O-02: 音频系统迁移
*   **状态**: Windows 仍用 MCI；非 Windows 尝试 WAV 回退；可选 `USE_SDL2_MIXER` CMake 开关。

---

## 3. 常用调试工具 (New)

*   **save_inspector.exe**:
    *   用途: 查看 `save/` 目录下 `.grp` 文件的内部数据（角色属性、物品、全局状态）。
    *   用法: `save_inspector.exe [save_dir]`
*   **analyze_data.exe**:
    *   用途: 解析 `alldef.grp` (事件定义) 和 `allsin.grp` (地图定义)。
    *   用法: 交互式选择分析对象。
*   **dump_events.exe**:
    *   用途: 打印指定 Event ID 的指令序列。

---

## 4. 技术规范速查 (Reference)

### 4.1 坐标系统
*   **Scene 0 (圣堂)** 起始坐标: `(38, 38)`
*   **镜头逻辑**: `setMainMapPosition(x, y)` 会同步更新 `m_mainMapX/Y` 和 `m_cameraX/Y`。

### 4.2 指令修正记录
*   **Instruct_ModifyEvent**: 仅当事件位于玩家当前坐标时，或为无坐标的系统事件时，才允许脚本变更触发自动运行。禁止远程修改导致的瞬间传送/触发。

---

**执行提示**: 开发前请运行 `save_inspector` 确认数据状态，修改代码后更新本文件。
