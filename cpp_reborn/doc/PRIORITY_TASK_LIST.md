# C++ 重构未完成任务优先清单

**更新日期：** 2026-08-07  
**排序原则：** 脚本正确性与主线阻塞 → 战斗保真 → 回归和存档 → UI 体验 → Android → 架构与工具。

## P0：脚本与开场正确性（立即处理）

- 在 [`EventManager.cpp`](../src/EventManager.cpp) 补齐 opcode **7**（立即结束当前事件）与 **61**（Pascal 相对跳转），严格对照 [`kys_main.pas`](../../kys_main.pas) 的 `CallEvent`。
- 增加解释器单测：正常结束、正/负偏移跳转、越界保护、嵌套事件返回。
- 完成脚本 101 目视回归：8284 + 8268 → 8282，并复查对话闪过与残留输入。
- 验收：当前 `kdef` 抽样无 PC 错乱、死循环或静默吞指令。

## P1：战斗主线保真

- 对照 [`kys_battle.pas`](../../kys_battle.pas) 验证范围选择族、伤害边界、医疗/用毒/暗器、功体状态叠加顺序。
- 完善 `.eft`/AOE 时序、音效和坐标；补强 `AutoBattle`、`AutoBattle2` 风格的试走与攻击评分。
- 核对战后升级、秘籍与 `PetEffect` 分支。
- 验收：固定前中期战役可稳定通过，固定输入下关键数值与 Pascal 一致，或差异有明确说明。

## P2：事件扩展与数据安全网

- 按 [`FEATURE_PORT_LIST.md`](FEATURE_PORT_LIST.md) 完成 `instruct_50e` 边角：41 场景图、50 改名 UI、51 `InputAmount`、子码 60；核对 `instruct_43` 与 `50e·43` 一致性。
- 建立原版/DOS 存档互读矩阵，脚本化 `save_inspector` 校验；保护 slot0 模板不回退。
- 建立冒烟路径：新游戏 → 首战 → 商店 → 小游戏 → 存档 → 读档。
- 验收：改档和剧情数据无布局损坏，关键流程可重复通过。

## P3：UI 与漫游功能补齐

- 实现 `ShowMap`、`FourPets/PetStatus/PetLearnSkill`、数字键 1–6 热键。
- 收尾队友换位、漫游医疗/解毒、`MenuDifficult`（确认资源确有使用时）。
- 实现 `SetScene` 雾/雨/雪与全屏切换；继续核对船和特殊地表规则。
- 验收：日常漫游菜单与 Pascal 信息量、操作路径基本一致。

## P4：Android 可玩闭环

- 在无 TSD 环境完成 `assembleDebug`，验证标题、新游戏、读档、存档。
- 验证外置 `game_data`、分区存储权限、CJK 字体、横屏安全区和虚拟键手感。
- 做低端设备帧率/内存检查，覆盖大地图与战斗特效。
- 验收：真机可完成开场、首战与存读档。

## P5：音频与输入架构

- 选定 SDL3 兼容音频方案，统一 Windows/Android 的 MIDI/OGG/WAV 播放与音量控制。
- 分批把对话、菜单、战斗中的阻塞 `PollEvent` 收敛到 `InputManager`。
- 验收：双端使用同一资源格式，输入无重复、丢键或虚拟键分裂。

## P6：制作器、工程质量与 Beta 收敛

- 深化场景可视化编辑；加入改档后自动格式校验和 CI 固定运行 `editor/tests`。
- 清理类型转换警告，统一硬编码 UI 坐标和 Present 路径。
- 完成全剧本冒烟、性能基线、Android Release 与已知问题冻结。
- 每阶段同步 [`FEATURE_PORT_LIST.md`](FEATURE_PORT_LIST.md)、[`PROGRESS.md`](PROGRESS.md)、[`TODO_LIST.md`](../TODO_LIST.md) 和 [`PROJECT_STATUS.md`](../PROJECT_STATUS.md)。
