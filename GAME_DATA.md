# 前传游戏数据布局（配合本仓库 GitHub 代码）

本仓库**不包含**受版权保护的二进制资源。桌面 / 编辑器运行依赖仓库根目录的 `game_data/`。

## 指定位置

```text
<repo>/game_data/
  resource/   # 必需（含 smp、kdef、talk、mmap、*.002 …）
  save/       # 必需（ranger.*、alldef.grp、allsin.grp）
  fight/      # 必需
  eft/        # 必需
  list/       # 建议
  music/      # 可选
  sound/      # 可选
```

然后把资源接到运行目录：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/link_game_data.ps1 -Config Debug
```

可执行文件：`build/Debug/kys_cpp.exe`。

## 一键整理 / 打包

从本机「前传64位-sdl3」发行目录（或你指定的完整前传目录）同步到 `game_data/`，并生成便携副本与 zip：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/pack_promise_game_data.ps1
```

常用参数：

| 参数 | 含义 |
|------|------|
| `-Source <路径>` | 指定前传根目录（需含 `resource/smp`） |
| `-SkipZip` | 不生成 zip |
| `-SkipLink` | 不同时执行 `link_game_data.ps1` |
| `-Config Release` | 链接到 `build/Release` |

产出：

| 路径 | 说明 |
|------|------|
| `game_data/` | 稳定工作副本（gitignore，构建清理不丢） |
| `dist/kys_promise_game_data/` | 干净便携树（排除 `.bak` / `corrupt_*`） |
| `dist/kys_promise_game_data.zip` | 同上压缩包 |

校验：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify_game_data.ps1
```

## 开场模板注意

`save/alldef.grp` 与 `save/allsin.grp` 是**新游戏模板**。请使用稳定发行版内容；不要把剧情推进后的 D/S 写回这两份文件。引擎 `SaveGame(0)` 也不会再覆盖它们。

## Android

把同样七个目录放到设备 `/sdcard/kys_promise/`，或使用 `scripts/push_game_data_android.ps1`。
