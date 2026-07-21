# Android 构建说明（方案 A：一套源码双端兼容）

PC 与 Android **共用** `cpp_reborn/` 引擎源码。Android Studio 工程只负责 Gradle / JNI / Activity，通过 `add_subdirectory` 引用同一套 CMake。

```text
cpp_reborn/                         # 唯一游戏引擎（PC + Android）
  CMakeLists.txt                    # Desktop → kys_cpp.exe；Android → libkys_promise.so
kys-promise-androidstudio/
  app/jni/CMakeLists.txt            # FetchContent SDL3* + add_subdirectory(cpp_reborn)
  app/src/main/.../SDLActivity.java # getLibraries: SDL3, SDL3_image, SDL3_ttf, kys_promise
```

## 设计原则

| 端 | 产物 | 入口 |
|----|------|------|
| Windows / 桌面 | `kys_cpp` 可执行文件 | `cpp_reborn` 单独 cmake |
| Android | `libkys_promise.so` | NDK 编进 APK，由 `SDLActivity` 加载 |

- **不要**把游戏逻辑复制进 Android 工程。
- Windows 默认路径与本地预编译 SDL3 包不变；不开 Android 也能单独编跑。
- 平台差异集中在 `PlatformCompat`、以及 `#ifdef _WIN32` / `__ANDROID__` 薄封装（路径、字体、MCI vs WAV、返回键）。

## 环境

- Android Studio Ladybug+ / AGP 与工程内 `build.gradle` 一致
- **JDK 17 或 21**（Gradle 8.12 不支持 Java 25；若 `java -version` 为 25+，请用 Android Studio 自带 JBR，或设置 `JAVA_HOME` 指向 17/21）
- NDK（建议 r26+；工程 pin `27.0.12077973`）、**CMake 3.31.6**（SDK Manager 安装；3.22 在 Windows 上易触发 NDK lld 问题）
- 首次配置需能访问 GitHub（`FetchContent` 拉 SDL3 / SDL3_ttf / SDL3_image）
- 在 `local.properties` 写明 `sdk.dir`（已 gitignore），例如 `sdk.dir=C\:\\Android\\Sdk`

命令行示例（指定 JDK 21）：

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21.0.11"
$env:ANDROID_HOME = "C:\Android\Sdk"
# 新建 local.properties：sdk.dir=C\:\\Android\\Sdk
cd kys-promise-androidstudio
.\gradlew.bat :app:assembleDebug
```

> Windows + NDK：若 CMake 报 `lld: unknown argument: --no-rosegment`，确认 `app/build.gradle` 里已带 `--target=aarch64-linux-android29` 的 `CMAKE_*_FLAGS`（本仓库已默认加入）。可选工具链包装：`app/jni/kys-android.toolchain.cmake`。

### 公司 TSD / 透明加解密（重要）

部分办公机对磁盘文件做透明加解密。若 NDK 头文件内容变成 `%TSD-Header-###%`（或类似占位），说明 **clang 未被授权读解密后的 sysroot**，此时：

- 重装 NDK / 换 CMake 版本 **无效**
- 本机 `assembleDebug` 会在配置或编译阶段失败（大量「无法识别的 token」类错误）
- **解决办法**：换无 TSD 的机器 / CI；或请 IT 将 NDK 的 `clang`、`cmake`、`ninja`、Android Studio 加入解密白名单

编包前请确认 sysroot 头可读，例如：

```powershell
Get-Content "$env:ANDROID_HOME\ndk\27.0.12077973\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\include\stdio.h" -TotalCount 3
```

正常应看到 `#ifndef` / `#include` 等 C 头内容，而不是 `%TSD-...%`。

## 编译 APK

1. 用 Android Studio 打开 `kys-promise-androidstudio/`
2. Sync Gradle，等待 NDK 拉取并编译 SDL3 与 `kys_promise`
3. Run → 生成 `kys-promise-debug.apk`（见 `app/build.gradle` 的 `outputFileName`）

命令行示例（需本机已装 SDK/NDK）：

```powershell
cd kys-promise-androidstudio
.\gradlew.bat :app:assembleDebug
```

## 游戏数据（外置，与旧 Pascal 包类似）

APK **不内置**原版资源。在设备可写目录放入完整 `game_data`（或扁平的 `resource/` + `save/` + …），引擎会按顺序探测：

1. `SDL_GetAndroidExternalStoragePath()` 及其下 `kys_promise/`、`game_data/`
2. 内部存储 / PrefPath 下的 `game_data/`
3. 回退到外部存储根目录

推荐：把 PC 侧同一份 `game_data/` 拷到手机：

```text
<外部存储>/Android/data/org.libsdl.kys_promise/files/game_data/
  resource/
  save/
  fight/
  eft/
  list/
  music/     # Android 需 .wav（见下）
  sound/
```

或：

```text
<外部存储>/kys_promise/   # 内含 resource/、save/ …
```

存档：若数据根下已有 `ranger.grp`，则读写该处 `save/`；否则写入 PrefPath 下的 `save/`。

## 音乐

Windows 仍可用 MCI 播 `.mid` / `.mp3` / `.ogg`。  
Android / Linux 当前走 SDL3 WAV 循环，请预先转换：

```powershell
# 需要本机 ffmpeg；将 music/*.mid|ogg|mp3 → music/*.wav
powershell -ExecutionPolicy Bypass -File scripts/convert_music_to_wav.ps1
```

## 触控与返回键

- **屏幕虚拟按键**（Android 默认开启；桌面设环境变量 `KYS_VIRTUAL_PAD=1`）：
  - 左下角：方向键（↑↓←→）
  - 右上角：Esc（取消 / 系统菜单）
  - 右下角：OK（Space，确认对话 / 菜单）
- SDL 触控不会再合成鼠标点击到虚拟键区域（避免误点 UI）。
- 实体 **BACK** 仍映射为 Escape + 右键（`PlatformCompat`）。

## 与 PC 构建的关系

PC 仍按仓库根 `README.md`：

```powershell
cmake -S cpp_reborn -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target kys_cpp
```

不要用 `kys-promise-androidstudio/app/jni/CMakeLists.txt` 编 PC 版。
