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
| 事件脚本解释器（核心指令） | 大部分可用；opcode 7 / 61 已对齐 |
| ESC 环形菜单、物品 / 武学 / 状态 / 系统 / 商店 / 地图 / 宠物 | 已实现 |
| 小游戏（作诗/针灸/射雕/拼图/黑白棋/贪吃蛇/推块） | 主路径已迁入 |
| 战斗（移动、武学、用毒医疗、物品、敌我自动） | 主路径可用；伤害边界与特效仍在对照 |
| 存档读写（`R/S/D/G*.grp`） | 已实现；slot0 不覆盖新游戏模板 |
| 音频（音乐 / 音效） | 启动时打开 SDL 音频；Windows 可播 mid/mp3/ogg，安卓需 WAV |
| 制作器 | 大地图批量编辑、入口标注、RLE 缩略图与导入导出 |

对照参考源码仍保留在仓库根目录：`kys_*.pas`、`kys_promise.dpr` 等。

---

## 目录结构

```text
.
├── cpp_reborn/          # C++ 主工程（源码、CMake、文档）
│   ├── include/
│   ├── src/
│   ├── tests/
│   ├── doc/
│   ├── CMakeLists.txt
│   └── CMakePresets.json  # binaryDir → 仓库根 build/
├── build/               # 唯一构建输出（gitignore；勿使用 cpp_reborn/build）
├── game_data/           # 本地原版资源存放点（gitignore；勿只放在 build/ 下）
├── scripts/             # 辅助脚本（如 link_game_data.ps1）
├── kys-promise-androidstudio/  # Android 壳（Gradle/JNI；引擎仍用 cpp_reborn）
├── ANDROID.md           # Android 双端构建与外置资源说明
├── SDL3-3.4.0/          # 预置 SDL3（Windows 开发便利）
├── SDL3_image-3.2.6/
├── SDL3_ttf-3.1.0/
├── SDL2_mixer-2.8.1/    # 可选音乐库
├── kys_*.pas            # 原版 Pascal 参考实现
└── README.md
```

**构建约定：** 源码在 `cpp_reborn/`，产物只进仓库根目录 `build/`。不要再创建或使用 `cpp_reborn/build/`。

**双端（方案 A）：** 一套 `cpp_reborn` 源码。桌面编 `kys_cpp`；Android 经 Studio 编出 `libkys_promise.so`。详见 [ANDROID.md](ANDROID.md)。

---

## 环境要求

- Windows 10/11（当前主要验证平台）
- CMake ≥ 3.15
- Visual Studio 2022（含“使用 C++ 的桌面开发”）
- 本仓库已附带 SDL3 / SDL3_image / SDL3_ttf 预编译包；也可改为系统安装版本并调整 `cpp_reborn/CMakeLists.txt` 中的路径

---

## 编译

在仓库根目录执行（**唯一官方路径**）：

```powershell
cmake -S cpp_reborn -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target kys_cpp
```

也可使用 `cpp_reborn/CMakePresets.json`（产物仍写入根目录 `build/`）：

```powershell
cmake --preset default -S cpp_reborn
cmake --build build --config Debug --target kys_cpp
```

成功后生成：

```text
build/Debug/kys_cpp.exe
```

构建脚本会尝试将 `SDL3.dll`、`SDL3_image.dll`、`SDL3_ttf.dll` 复制到可执行文件目录。

**重要：** 原版 `resource/` / `save/` / `fight/` 等资源请放在仓库根目录 `game_data/`，再用脚本链到运行目录。不要只放在 `build/` 里——清理构建目录会删掉它们。

```powershell
# 推荐：从本机前传发行目录一键同步到 game_data/ 并打包 + 链接
powershell -ExecutionPolicy Bypass -File scripts/pack_promise_game_data.ps1

# 或手动拷贝资源后：
powershell -ExecutionPolicy Bypass -File scripts/link_game_data.ps1 -Config Debug
powershell -ExecutionPolicy Bypass -File scripts/verify_game_data.ps1
```

完整说明见 [GAME_DATA.md](GAME_DATA.md)。

---

## 运行所需资源

引擎会在可执行文件附近自动查找以 `smp` 为标志的 `resource/` 目录。推荐布局：

```text
game_data/                 # 稳定存放（gitignore）
├── resource/
├── save/
├── fight/
├── eft/
├── list/
├── music/
└── sound/

build/Debug/               # 运行目录（junction 指向 game_data）
├── kys_cpp.exe
├── SDL3.dll / SDL3_image.dll / SDL3_ttf.dll
├── resource/  -> game_data/resource
├── save/      -> game_data/save
├── fight/     -> game_data/fight
├── eft/       -> game_data/eft
├── list/      -> game_data/list
├── music/     -> game_data/music
└── sound/     -> game_data/sound
```

也可从合法原版游戏目录整体拷贝上述文件夹到 `game_data/`，再执行 `scripts/link_game_data.ps1`。

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
| `Game.Pic` | 小游戏贴图（贪吃蛇/射雕/针灸/拼图等） |
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
| `Acupuncture.bin` | 针灸小游戏穴位布局（可选；缺省用回退网格） |

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
cd build/Debug
.\kys_cpp.exe
```

请确认上述资源目录已就位，且 DLL 与 exe 同目录。

### Android

资源放在 **SD 卡根目录** `kys_promise/`（不要用 `Android/data/...`）：

```text
/sdcard/kys_promise/
  resource/  save/  fight/  eft/  list/  music/  sound/
```

从 PC 推送：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/push_game_data_android.ps1
```

详见 [ANDROID.md](ANDROID.md)。

---

## 近期更新

- 启动时初始化音频设备。设备不可用时游戏继续运行。Windows 用 MCI 播放 mid/mp3/ogg，安卓与 Linux 循环播放 `music/N.wav`，音量同时作用到 SDL 音频流。
- 武功 `.eft` 帧间延时按原版 `GameSpeed` 计算，播放时处理系统事件，避免窗口无响应。
- 预留 [`cpp_reborn/include/GameHooks.h`](cpp_reborn/include/GameHooks.h)：可替换音乐、音效、武功特效，并在战斗结束时收到编号与胜负。
- 制作器支持大地图贴图/建筑层框选复制粘贴、场景入口标注，以及 mmap/场景砖/战场砖的缩略图和 PNG 导入导出。
- 资源整理脚本：`scripts/pack_promise_game_data.ps1`、`scripts/verify_game_data.ps1`。布局见 [GAME_DATA.md](GAME_DATA.md)。
- 功能对照与优先级：`cpp_reborn/doc/FEATURE_PORT_LIST.md`、`cpp_reborn/doc/PRIORITY_TASK_LIST.md`。

## 开发说明

- 语言标准：C++17（以 `cpp_reborn/CMakeLists.txt` 为准）
- 多媒体：SDL3、SDL3_image、SDL3_ttf；可选 SDL2_mixer（`USE_SDL2_MIXER`）
- 逻辑对照：优先对齐 `kys_battle.pas`、`kys_event.pas`、`kys_engine.pas`、`kys_main.pas`
- **功能对照 list：** `cpp_reborn/doc/FEATURE_PORT_LIST.md`
- **未完成优先级：** `cpp_reborn/doc/PRIORITY_TASK_LIST.md`
- 单元勾选进度：`cpp_reborn/doc/PROGRESS.md`；摘要：`cpp_reborn/PROJECT_STATUS.md`、`cpp_reborn/TODO_LIST.md`
- 资源布局：`GAME_DATA.md`

---

## 声明

本项目仅用于学习、引擎重构与技术交流。游戏剧情、美术、音乐等版权归原作者及权利方所有。请支持正版。
