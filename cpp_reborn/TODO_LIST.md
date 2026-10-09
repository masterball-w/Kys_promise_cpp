# KYS-Promise 重构项目待办事项清单 (TODO List)

**Last Updated:** 2026-08-07  
**完整勾选进度:** [doc/PROGRESS.md](doc/PROGRESS.md)  
**权威功能对照:** [doc/FEATURE_PORT_LIST.md](doc/FEATURE_PORT_LIST.md)  
**状态摘要:** [PROJECT_STATUS.md](PROJECT_STATUS.md)  
**Android 双端:** [../ANDROID.md](../ANDROID.md)  
**制作器:** [../editor/README.md](../editor/README.md)  
**开场/脚本101回归:** [doc/REGRESSION_SCRIPT_101.md](doc/REGRESSION_SCRIPT_101.md)

> **核心原则**
> 1. **二进制兼容**: `GameObject` 子类与 DOS `.grp` 布局一致。
> 2. **逻辑还原**: 优先对齐 `kys_*.pas`，除非明确改进。
> 3. **一套源码双端**: PC / Android 共用 `cpp_reborn`（方案 A）；平台差异进薄封装。
>
> **当前阶段（Alpha ~0.7）**  
> 核心循环、商店、小游戏主路径、功体状态 26–28、解耦制作器、Android 双端脚手架 + 虚拟按键已落地。  
> **2026-07-28**：开场卧室模板已从稳定版恢复；`SaveGame(0)` 不再覆盖 `alldef`/`allsin`。  
> **下一优先**：开场 101 目视签字 → 新环境 `assembleDebug` → 外置 `game_data` 真机可玩 → UI/小游戏补齐。

---

## 0. 近期已交付

### 0.1 2026-07-28 — 开场事件层 / 模板保护
- [x] 对照稳定版 `前传64位-sdl3`：确认权威 `alldef`/`allsin`（事件0=`8284`，事件1=`8268@(40,37)`）
- [x] 用稳定版覆盖仓库损坏模板；损坏文件保留 `*.corrupt_*.bak`
- [x] 证实制作器 roundtrip 无损（非 editor 写坏）
- [x] `SaveGame(0)` 禁止写回 `alldef`/`allsin`（只写 ranger）
- [x] `InitNewGame` 强制重载模板 + `InitialScene`（保留层3 / `DData[5]:=DData[7]`）
- [x] `DrawRLE8` 热点对齐 Pascal（`xs/ys+1`）
- [x] 制作器单测 `test_alldef_scene0_opening_pics`；文档 `REGRESSION_SCRIPT_101.md` / `REF_DEV_DOC` §2.4
- [ ] **待用户目视**：新游戏卧室 8284 + 孔霹雳 → 8282 坐起全流程

### 0.2 2026-07-20 ~ 07-21 — 勿再当作 TODO

#### 引擎 / 玩法
- [x] 事件 Stub/Missing 大部（含 `15`/`17`/`33`–`35`/`37`/`41`/`42`/`45`–`49`/`51`–`56`/`58`–`60`/`63`/`64`/`66`/`67`/`69`）
- [x] 商店：`ShowShop` + `instruct_64` + `50e` Rshop
- [x] `LittleGameManager` 主路径（作诗/针灸/射雕/灯谜拼图/黑白棋/蛇/推块）
- [x] 功体 BattleState **26 / 27 / 28**（每回合加攻、范围下毒、医疗解毒强化）
- [x] 开场 `WalkInScene` / 标题对齐相关修正
- [x] `ShowSceneName`（已有实现；氛围/ShowMap 仍缺）

#### 跨平台 / Android（方案 A）
- [x] `PlatformCompat`：数据根 / 存档路径发现、BACK→Esc+右键
- [x] `FileLoader` 相对 `game_data`；非 Win 文本 `SDL_iconv`；Win 仍可用 MCI
- [x] 非 Win BGM：WAV 循环；`scripts/convert_music_to_wav.ps1`
- [x] 双端 CMake：桌面 `kys_cpp` / Android `libkys_promise.so`；jni FetchContent SDL3*
- [x] `VirtualControls`：左下方向键、右上 Esc、右下 OK→Space（Android 默认开；桌面 `KYS_VIRTUAL_PAD=1`）
- [x] `ANDROID.md` + assets 占位说明；Windows NDK 需 CMake 3.31.6 + `--target=aarch64-linux-android29`

#### 制作器（`editor/`，与引擎解耦）
- [x] 存档 / 事件 / 战斗 / 贴图 / 交叉引用
- [x] 人物 / 物品 / 武功详情面板；opcode 中文下拉；talk GBK 优先
- [x] 事件「插入指令」全量可搜索 opcode

---

## 1. 事件缺口

### 1.1 Stub → 真实现 — 已完成
- [x] 见 §0 / 原 §1.1 清单

### 1.2 仍待验收 / 补齐
- [ ] **opcode 7**：Pascal `CallEvent` 中 `7: break` 结束事件；C++ `ExecuteEvent` **无 case 7**（落入 default）— **高优先**（2026-08-07 源码核实）
- [ ] **opcode 61**：Pascal 相对跳转 `i += e[i+1]; i += 3`；C++ **无 case 61** — **高优先**
- [x] **`instruct_50e`**：对齐 Pascal — case 0–5/8–12/16–52（含字符串、内存 poke/peek、SelectAim、绘制、Delay/random/菜单、动画/伤显、改名等）；剧本高频 code 已覆盖；未知 code 打日志 — **2026-07-21**
- [x] P1 冒烟：`test_50e` 单元测过；Kdef 抽样统计 code 43/4/3/26/25… 均有实现
- [ ] opcode `65`：原版空走，可与 default 等价；`57` 原版注释可跳过
- [ ] `instruct_43` 子功能边界用例（异常参数、与 `50e·43` 一致性）

### 1.3 小游戏保真
- [x] 主路径已迁入
- [ ] 像素级打磨：射雕轨迹、穴位 crop 高亮、Lamp `DrawSPic`、作诗字库边界等

---

## 2. 战斗保真（`kys_battle.pas`）— 剧情阻塞最高

- [x] `CalHurtValue` 主公式已对齐；`CalHurtRole` 已按 Pascal 补齐（内伤修正、NeedProgress、酒状态、闪躲/暴击、毒/内伤/封穴、耗内耗体、吸血吸内比例等）— **2026-07-21**
- [x] 武功等级约定修正为 `MagLevel/100+1`；`SetAttackArea` 的 step=`MoveDistance` / range=`AttDistance`；十字中心格
- [x] `SelectMagicTarget` 光标层使用 MoveDistance + AttDistance（与 Pascal `DrawBFieldWithCursor` 一致）
- [~] `PlayMagicAmination` / `.eft`：已有 Layer4 铺开；大招时序/音效可继续打磨
- [~] `AutoBattle`：已跳过内功、按等级/射程选招、按 MoveDistance 贴脸 — 复杂 AI 仍可加深
- [x] `PetEffect` 主路径已有 — [~] 对照 Pascal 补齐遗漏分支（可选）
- [x] 战后 `CheckLevelUp`/`LevelUp` 对齐资质成长与奇偶属性、技能熟练度 — **2026-07-21**
- [x] `AddExp` / `CheckBook` 主路径已有
- [x] 功体状态 26–28 — [~] 与其它状态叠加顺序 / 战中失去功体等边界再验
- [~] 战斗内物品 / 医疗 / 用毒 / 暗器数值与 UI 反馈保真（部分已有）

---

## 3. UI / 漫游补齐

- [ ] `FourPets` / `PetStatus` / `PetLearnSkill`
- [ ] `CheckHotkey`（数字键 1–6 快捷选人/菜单）
- [ ] `ShowMap`（大地图界面；`ShowSceneName` 已有）
- [ ] `SetScene` 雾 / 雨 / 雪等氛围完整化
- [ ] `NewMenuTeammate` / 漫游医疗解毒菜单收尾
- [ ] 建角难度 `MenuDifficult`（若资源与剧本需要）
- [x] 虚拟按键确认键（右下 OK→Space）— [~] 长按连移手感调参（可选）

---

## 4. 音频与输入

- [x] 非 Win：WAV BGM 路径（过渡方案）
- [ ] 以 **SDL_mixer / SDL3 等价** 统一播 mid/ogg（减少依赖预转 wav）
- [ ] 音效覆盖与音量路径对齐 `PlaySound*` / 系统菜单音量
- [ ] 将剩余 UI 阻塞 `PollEvent` 收敛到 `InputManager`（可分批：对话 → 菜单 → 战斗）
- [x] Android 虚拟方向 + Esc；BACK 映射

---

## 5. Android 真机闭环（方案 A 未完成部分）

脚手架与虚拟键已就绪；下列为「可安装可玩」门槛。

> **2026-07-21 本机阻塞说明（公司 TSD）**  
> 当前开发机启用了透明加解密（TSD）：NDK sysroot 头文件对未授权进程显示为 `%TSD-Header-###%`，`clang`/`cmake`/`ninja` 不在白名单，**本机无法完成** `assembleDebug` 原生编包（重装 NDK 无效）。  
> **处理**：已清理本机编译垃圾（`.cxx` / `build` / `_cmake_probe*` 等）；请在**无 TSD** 的机器、CI，或经 IT 放行 NDK 工具链后按 [ANDROID.md](../ANDROID.md) 重新编包验收。

- [ ] Android Studio **真机/模拟器** `assembleDebug` 编过并启动到标题（**待新环境**；工程侧 JDK/CMake/NDK/`--target` 已就绪）
- [ ] 外置 `game_data` 拷贝流程验证（外部存储 / PrefPath）
- [ ] 横屏、刘海/安全区、虚拟键与 letterbox 不挡关键 UI
- [x] 触控确认键：右下 OK→Space（方向 + Esc + OK 齐全）— [ ] 菜单点选 / 对话推进真机手感验收
- [ ] 音乐：批量 mid→wav 或接入解码库后验收 BGM
- [ ] 字体：打包 `Chinese.ttf` 进数据或确认系统 CJK 字体回退
- [ ] 存档读写权限与 Android 分区存储适配
- [ ] 性能：低端机帧率 / 内存（大地图、战斗特效）
- [ ] Release 签名与基础混淆策略（可选）

---

## 6. 制作器（持续）

- [x] 基础模块可用
- [ ] 场景地图可视化编辑（SData 格子）深化
- [ ] 事件调试：与引擎同脚本单步 / 断点（中长期）
- [ ] 导出/校验：改档后自动跑 `save_inspector` / 格式校验
- [ ] CI：`editor/tests` 在 PR 中固定跑

---

## 7. 存档与工程质量

- [ ] 存档写盘 + 原版/DOS **互读**验证（`save_inspector` 常态化）
- [ ] 清理大量 `size_t`→`int` / `Uint64`→`uint32_t` 编译警告
- [ ] 收敛硬编码 UI 坐标；统一 Present 路径（已部分 `VirtualControls::present`）
- [ ] 回归清单：新游戏开场 → 前几场战斗 → 存读 → 商店 → 小游戏抽样

---

## 已完成（历史基线，勿回退）

- [x] 物品栏分类 / 使用 / 装备；系统菜单存读档；标题读档竖排；ESC 环形菜单
- [x] 战斗手动操作菜单；大地图入口闭环；`instruct_27` 动画（勿与 `23` 用毒混淆）
- [x] `ShowShop` + 商店指令接入；小游戏主路径；功体 26–28；制作器初版+人物物品详情
- [x] 双端 CMake + PlatformCompat + VirtualControls + 非 Win 文本/WAV

---

# 下一步：完整开发计划

按依赖与「能否推进主线剧情」排序。每阶段有明确验收标准。

## 阶段 P0 — 战斗可过主线（约 1.5–3 周）

| 序号 | 任务 | 验收 |
|------|------|------|
| P0.1 | `CalHurtValue` 等与 `kys_battle.pas` 对照表（含功体/装备修正） | 固定战例伤害与 Pascal 误差在约定阈值内 |
| P0.2 | 范围选择族核对 + 修正 | 点/线/面/十字等与原版选格一致 |
| P0.3 | `.eft` / `PlayMagicAmination` 时序与 AOE | 主线常见武学特效可读、不错位 |
| P0.4 | AI / `SelectAutoMode` 补强 | 自动战斗不卡死、行为接近原版 |
| P0.5 | 战后升级 / 秘籍 / PetEffect 边界 | 胜场后数值与提示正确 |

**出口：** 能稳定打通前中期若干固定战役（自定清单）。

## 阶段 P1 — 事件与剧本安全网（约 1–2 周，可与 P0 并行部分）

| 序号 | 任务 | 验收 |
|------|------|------|
| P1.1 | `instruct_50e` 按 Pascal code 列表打勾验收 | 文档列出已验 code；未知 code 有日志；`test_50e` 覆盖字符串/算术/25–26 |
| P1.2 | 剧本抽样：开场 → 第一次战斗 → 商店 → 一次 `50e` 重逻辑 | 无卡死 / 无静默吞指令 |
| P1.3 | 小游戏保真打磨（按触达频率） | 玩家可完成且结果写入正确 |

**出口：** 事件解释器对当前 `kdef` 无明显 Stub 黑洞。

## 阶段 P2 — Android 可玩原型（约 1–2 周）

| 序号 | 任务 | 验收 |
|------|------|------|
| P2.1 | AS 真机 Debug 包启动到标题 | **须无 TSD 干扰的环境**；日志可见 Data root / 字体；JDK 17/21 + CMake 3.31.6（见 ANDROID.md） |
| P2.2 | 外置 `game_data` + 存档读写 | 新游戏 / 读档 / 存档成功 |
| P2.3 | 虚拟键确认 + 安全区 | 右下 OK→Space 已合入；真机验收漫游/对话/ESC 单手可用 |
| P2.4 | 音乐 wav 或解码方案 | 标题/场景 BGM 可播 |
| P2.5 | 横屏与 UI 点击命中 | 无大面积误触、虚拟键不挡关键按钮 |

**出口：** 手机上能玩开场 + 存档（非完美移植）。

## 阶段 P3 — UI / 漫游体验（约 1–2 周）

| 序号 | 任务 | 验收 |
|------|------|------|
| P3.1 | `ShowMap` | 与 Pascal 信息量接近 |
| P3.2 | 宠物三件套 UI | 可查看 / 学技能主路径 |
| P3.3 | `CheckHotkey` 1–6 | 快捷有效 |
| P3.4 | `SetScene` 氛围 | 雾雨雪至少视觉可用 |
| P3.5 | `MenuDifficult`（可选） | 与资源一致时启用 |

**出口：** 日常漫游 UI 接近原版完整度。

## 阶段 P4 — 音频统一与输入架构（约 1 周+）

| 序号 | 任务 | 验收 |
|------|------|------|
| P4.1 | SDL_mixer（或 SDL3 方案）统一 BGM | Win/Android 同资源格式 |
| P4.2 | 音效与音量全路径 | 系统菜单音量即时生效 |
| P4.3 | UI `PollEvent` → `InputManager` 分批迁移 | 无双键状态分裂；虚拟键仍可用 |

**出口：** 非 Win 不再依赖「必须预转 wav」作为唯一方案（可选保留）。

## 阶段 P5 — 制作器与工程化（持续）

| 序号 | 任务 | 验收 |
|------|------|------|
| P5.1 | 场景格子可视化编辑增强 | 常用改图不靠十六进制 |
| P5.2 | 改档校验 + `save_inspector` 脚本化 | 一键检查损坏布局 |
| P5.3 | 警告清理 / 回归清单文档化 | CI 或本地脚本可跑冒烟 |
| P5.4 | （中长期）事件调试器对接引擎 | 可选 |

## 阶段 P6 — Beta 收敛（视进度）

- 全剧本冒烟；DOS 存档互读矩阵；性能与内存；Android Release；已知问题清单冻结。
- 文档三角同步：`TODO_LIST.md` ↔ `PROJECT_STATUS.md` ↔ `doc/PROGRESS.md`。

---

## 建议执行顺序（一句话）

**P0 战斗保真 → P1 `50e`/剧本 → P2 Android 真机可玩 → P3 UI 补齐 → P4 音频/输入统一 → P5 工具与质量 → P6 Beta。**

并行建议：P0 与 P1 可两人拆；P2 在双端脚手架已就绪后尽早穿插真机验证，避免桌面改完再集中爆雷。

---
*与 [PROJECT_STATUS.md](PROJECT_STATUS.md)、[doc/PROGRESS.md](doc/PROGRESS.md)、[../ANDROID.md](../ANDROID.md) 同步。*
