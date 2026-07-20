# KYS-Promise 重构项目待办事项清单 (TODO List)

**Last Updated:** 2026-07-20  
**完整勾选进度:** [doc/PROGRESS.md](doc/PROGRESS.md)  
**状态摘要:** [PROJECT_STATUS.md](PROJECT_STATUS.md)

> **核心原则**
> 1. **二进制兼容**: `GameObject` 子类与 DOS `.grp` 布局一致。
> 2. **逻辑还原**: 优先对齐 `kys_*.pas`，除非明确改进。
> 3. 下列条目按剧情阻塞与对齐优先级排序（对应 PROGRESS §H）。
>
> **2026-07-20 已交付**：事件 Stub/Missing 大部、商店接入、`LittleGameManager` 小游戏主路径。  
> **下一优先**：战斗保真 → `instruct_50e` 验收 → 小游戏像素打磨。

---

## 1. 事件缺口（最高优先）

### 1.1 Stub → 真实现（C2）— 已完成 2026-07-20
- [x] `instruct_33` 学武功（`StudyMagic`）
- [x] `instruct_34` 加资质
- [x] `instruct_35` 写武功栏
- [x] `instruct_37` 增减道德
- [x] `instruct_41` 改随身物品
- [x] `instruct_42` 队中有女（跳转）
- [x] `Instruct_ReSetName`（opcode 69）

### 1.2 Missing opcodes（C3）
- [x] `15` 失败回标题
- [x] `17` 改场景图层（已修正误绑 Delay）
- [x] `20` 队伍已满
- [x] `22` 内力清零
- [x] `45`–`49` 属性增减 / 内力属性
- [x] `51`–`56` / `58`–`60` / `63`/`64` / `66`/`67`
- [ ] `instruct_50e` 扩展 code 对照 Pascal 逐项验收
- [x] 小游戏：`LittleGameManager` 迁入像素玩法 + `FemaleSnake`（-25）+ `Puzzle`（540）

### 1.3 小游戏（`instruct_43` / `kys_littlegame`）
- [x] `Poetry` / `Acupuncture` / `ShotEagle` / `Lamp` / `rotoSpellPicture` / `FemaleSnake`
- [x] `Puzzle`（场景推块，非 UI 挑战）
- [ ] 与原版像素级细节打磨（箭轨迹、穴位高亮 crop、场景贴图 DrawSPic 等）

---

## 2. 商店接入

- [x] `instruct_64` → `UIManager::ShowShop(0)`（原版空；重制接入）
- [x] `50e` 16/17 `e2==4` 读写 `Rshop`（`getShopData`/`setShopData`）
- [x] 库存：x50 `0x5000`/`0x5100` → `Rshop` → 回退样品
- [x] 买/卖切换与金钱结算

---

## 3. 战斗保真（`kys_battle.pas`）

- [ ] `CalHurtValue` / 治疗 / 命中 / 暴击 与 Pascal 1:1
- [ ] 攻击范围选择族（`SelectRange` / `Cross` / `Line` / …）核对
- [ ] `PlayMagicAmination` / `.eft` 深化（含 AOE 表现）
- [ ] `AutoBattle` / `Auto` / 自动模式菜单
- [ ] `PetEffect`；战后 `AddExp` / `CheckLevelUp` / `CheckBook` 完善

---

## 4. UI / 漫游补齐

- [ ] `FourPets` / `PetStatus` / `PetLearnSkill`
- [ ] `CheckHotkey`（数字键 1–6 快捷）
- [ ] `ShowMap` / `ShowSceneName`
- [ ] `SetScene` 雾/雨/雪等氛围
- [ ] `NewMenuTeammate` / 漫游医疗解毒菜单收尾
- [ ] 建角难度 `MenuDifficult`（若资源需要）

---

## 5. 音频与输入

- [ ] 以 SDL_mixer / SDL3 等价方案替代 Windows MCI 播 BGM
- [ ] 音效覆盖与音量路径对齐 `PlaySound*`
- [ ] 将剩余 UI 阻塞 `PollEvent` 收敛到 `InputManager`

---

## 6. 存档与工程质量

- [ ] 存档写盘 + 原版/DOS 互读验证（`save_inspector`）
- [ ] 清理 `size_t`→`int` 等编译警告
- [ ] 收敛硬编码 UI 坐标

---

## 已完成（勿再当作 TODO）

以下旧清单项**已完成**，勿回退为未做：

- [x] 物品栏分类 / 使用 / 装备路径（`SelectShowItem`）
- [x] 系统菜单存读档 UI（`NewMenuSave` / `NewMenuLoad` 风格）
- [x] 标题读档竖排（`MenuLoadAtBeginning`）
- [x] ESC 环形菜单切片（`NewMenuEsc`）
- [x] 战斗手动操作菜单；战斗内物品/医疗/用毒基础路径
- [x] 大地图入口闭环（`TryEnterScene`）
- [x] `instruct_27` 动画（勿与 `instruct_23` 混淆）
- [x] `ShowShop` 买卖 UI + `instruct_64` / Rshop 接入（见 §2）

---
*与 [PROJECT_STATUS.md](PROJECT_STATUS.md)、[doc/PROGRESS.md](doc/PROGRESS.md) 同步。*
