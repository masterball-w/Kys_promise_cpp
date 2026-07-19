# Kys Promise C++（《金庸群侠前传》C++ 重构）

将 Pascal/Delphi 版《金庸群侠前传》引擎迁移到 C++17 + SDL3，目标是在兼容原版二进制资源与存档的前提下，便于维护与扩展。

> **本仓库不包含原版游戏资源与存档数据。** 运行前需自行准备合法取得的资源文件（见下文「运行所需资源」）。

仓库地址：<https://github.com/masterball-w/Kys_promise_cpp>

---

## 功能概览

| 模块 | 状态 |
|------|------|
| SDL3 渲染 / `.pic` `.grp` `.idx` 解析 | 已实现 |
| 大地图 / 场景漫游、碰撞与事件触发 | 已实现 |
| 事件脚本解释器（核心指令） | 大部分可用 |
| ESC 环形菜单、物品 / 武学 / 状态 / 系统等 UI | 已实现 |
| 战斗（移动、武学、用毒医疗、物品、AI 基础） | Alpha，持续对照原版修正 |
| 存档读写（`R/S/D/G*.grp`） | 已实现 |
| 音频（音乐 / 音效） | 部分可用（Windows 侧重） |

对照参考源码仍保留在仓库根目录：`kys_*.pas`、`kys_promise.dpr` 等。

---

## 目录结构

```text
.
├── cpp_reborn/          # C++ 主工程（源码、CMake、文档）
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── doc/
├── SDL3-3.4.0/          # 预置 SDL3（Windows 开发便利）
├── SDL3_image-3.2.6/
├── SDL3_ttf-3.1.0/
├── SDL2_mixer-2.8.1/    # 可选音乐库
├── kys_*.pas            # 原版 Pascal 参考实现
└── README.md
```

---

## 环境要求

- Windows 10/11（当前主要验证平台）
- CMake ≥ 3.15
- Visual Studio 2022（含“使用 C++ 的桌面开发”）
- 本仓库已附带 SDL3 / SDL3_image / SDL3_ttf 预编译包；也可改为系统安装版本并调整 `cpp_reborn/CMakeLists.txt` 中的路径

---

## 编译

在仓库根目录执行：

```powershell
cmake -S cpp_reborn -B cpp_reborn/build
cmake --build cpp_reborn/build --config Debug --target kys_cpp
```

成功后生成：

```text
cpp_reborn/build/Debug/kys_cpp.exe
```

构建脚本会尝试将 `SDL3.dll`、`SDL3_image.dll`、`SDL3_ttf.dll` 复制到可执行文件目录。

---

## 运行所需资源

引擎会在可执行文件附近自动查找以 `smp` 为标志的 `resource/` 目录。推荐将资源放在：

```text
cpp_reborn/build/Debug/
├── kys_cpp.exe
├── SDL3.dll
├── SDL3_image.dll
├── SDL3_ttf.dll
├── resource/          # 必需：原版 resource 目录
├── save/              # 必需：初始数据与存档
├── fight/             # 必需：战斗人物动画
├── eft/               # 必需：武功特效
├── list/              # 建议：升级/套装等表
├── music/             # 可选：背景音乐
└── sound/             # 可选：音效
```

也可从合法原版游戏目录整体拷贝上述文件夹到运行目录。

### 1. `resource/`（核心，缺少则无法正常启动）

| 文件 | 用途 |
|------|------|
| `smp` / `sdx` | 场景地块图（`smp` 也用于定位资源根目录） |
| `wmp` / `wdx` | 战斗地块图 |
| `mmap.grp` / `mmap.idx` | 大地图精灵 |
| `EARTH.002`（或 `earth.002`）、`surface.002`、`building.002`、`buildx.002`、`buildy.002` | 480×480 大地图层数据 |
| `Scene.Pic` | 场景动态精灵 |
| `cloud.grp` / `cloud.idx` | 云层 |
| `MMAP.COL` | 调色板 |
| `kdef.idx` / `kdef.grp` | 事件脚本（**启动硬依赖**） |
| `talk.idx` / `talk.grp` | 对话文本（**启动硬依赖**） |
| `Background.Pic`、`Begin.Pic`、`Heads.Pic`、`Items.Pic`、`Skill.pic` | UI / 开场 / 头像 / 物品 / 技能图标 |
| `War.sta`、`warfld.idx`、`warfld.grp` | 战斗配置与战场 |
| `name.idx` / `name.grp` | 可选：人物姓名 |
| `Chinese.ttf` / `simkai.ttf` / `font.ttf`、`English.ttf` | 可选字体；缺省时 Windows 会尝试系统字体 |

### 2. `save/`（初始数据与存档）

| 文件 | 用途 |
|------|------|
| `ranger.idx` + `ranger.grp`（或 `Ranger.grp`） | 新游戏基础数据（角色 / 物品 / 武功 / 场景等） |
| `allsin.grp` | 场景地图数据 |
| `alldef.grp` | 场景事件数据 |
| `R1.grp`…`Rn.grp`、`S*.grp`、`D*.grp`、`G*.grp` | 各槽位存档（运行中读写） |

### 3. `fight/` 与 `eft/`（战斗画面）

```text
fight/<三位角色编号>/<两位动作编号>.pic
  例：fight/000/00.pic
eft/eft<三位特效编号>.pic
  例：eft/eft001.pic
```

### 4. `list/`（建议提供）

| 文件 | 用途 |
|------|------|
| `levelup.bin` | 升级经验表 |
| `Set.bin` | 套装等配置 |

当前实现会依次尝试 `save/list/` 与运行目录下的 `list/`。

### 5. `music/` 与 `sound/`（可选）

```text
music/<编号>.mid|.mp3|.ogg|.wav
sound/e<三位编号>.wav
```

### 资源来源说明

请从你拥有的《金庸群侠前传》原版或官方/授权发行包中复制。本仓库**不会**、也**不应**分发这些二进制资源。若资源缺失，程序可能在启动阶段因找不到 `kdef` / `talk` / `smp` 等文件而失败。

---

## 运行

```powershell
cd cpp_reborn/build/Debug
.\kys_cpp.exe
```

请确认上述资源目录已就位，且 DLL 与 exe 同目录。

---

## 开发说明

- 语言标准：C++17（以 `cpp_reborn/CMakeLists.txt` 为准）
- 多媒体：SDL3、SDL3_image、SDL3_ttf；可选 SDL2_mixer（`USE_SDL2_MIXER`）
- 逻辑对照：优先对齐 `kys_battle.pas`、`kys_event.pas`、`kys_engine.pas`、`kys_main.pas`
- 更多进度见 `cpp_reborn/PROJECT_STATUS.md` 与 `cpp_reborn/doc/`

---

## 声明

本项目仅用于学习、引擎重构与技术交流。游戏剧情、美术、音乐等版权归原作者及权利方所有。请支持正版。
