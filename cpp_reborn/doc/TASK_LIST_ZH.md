# KYS C++ 重构任务列表

## ✅ 已完成任务 (Completed)

### 基础系统与 UI
- [x] **阅读原始代码**: 分析 `kys_engine.pas` 以了解 UI 绘制逻辑 (`ShowMenu`, `DrawRectangle`)。
- [x] **UI 管理器**: 创建 `UIManager` 类，实现基础绘图功能。
- [x] **菜单系统**: 实现 `ShowMenu` 及状态栏显示逻辑。
- [x] **系统集成**: 将 `UIManager` 集成到 `GameManager` 并通过验证。

### 战斗系统核心 (Battle System)
- [x] **数据结构**: 实现 `BattleManager`, `BattleRole`, `WarData` 等核心类。
- [x] **战斗循环**: 实现 `RunBattle` 主循环及 `CountProgress` (集气/行动条) 逻辑。
- [x] **移动逻辑**: 实现基于 BFS (广度优先搜索) 的移动范围计算 `CalSelectableArea`。
- [x] **伤害计算**: 实现基础伤害公式 `CalHurtValue`，并加入武功威力支持。
- [x] **自动战斗 AI**: 实现 `BattleManager::AutoBattle`，支持移动评分与攻击选择。
- [x] **动作执行**: 实现角色在战场上的移动 (`MoveRole`) 和攻击 (`Attack`) 逻辑。

## 📝 待办任务 (Pending)

### 战斗系统完善
- [x] **玩家交互**: `BattleMenu` 已实现移动、攻击、药品等手动操作。
- [ ] **战斗效果**: `.eft` 完整特效资源待深化。
- [x] **物品与状态**: 战斗中使用物品及中毒/受伤等待效果已实现。

### 引擎功能补全
- [ ] **资源加载**: 完善图像 (`.grp`, `.idx`) 和精灵系统的加载与渲染。
- [ ] **事件系统**: 完整移植 `kys_event.pas` 中的事件指令处理。
- [ ] **构建系统**: 优化 CMake 配置，确保跨平台编译顺畅。

## 📊 当前进度摘要
核心战斗逻辑与玩家手动操作界面均已完成。当前重点为对话/交互稳定性验证，以及 `.eft` 武功特效与存档二进制兼容性测试。
