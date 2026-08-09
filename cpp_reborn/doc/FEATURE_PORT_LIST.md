# Pascal → C++ 功能移植清单

**更新日期：** 2026-08-09  
**真源：** `kys_*.pas`  
**进度勾选：** [PROGRESS.md](PROGRESS.md)  
**优先级：** [PRIORITY_TASK_LIST.md](PRIORITY_TASK_LIST.md)

## P0 事件解释器

| 功能 | Pascal | C++ | 状态 |
|------|--------|-----|------|
| opcode 7 break | `CallEvent` | `EventManager::ExecuteEvent` | Done |
| opcode 61 相对跳转 | `CallEvent` | `EventManager::ExecuteEvent` | Done |
| opcode 50 双路径 | `instruct_50` | `ApplyInstruct50Result` | Done |
| 脚本 101 回归 | Event 101 | `REGRESSION_SCRIPT_101.md` | 需目视 |

## P1 战斗

| 功能 | Pascal | C++ | 状态 |
|------|--------|-----|------|
| 敌方 AI | `AutoBattle` | `BattleManager::AutoBattle` | Done |
| 我方自动 | `AutoBattle2` | `BattleManager::AutoBattle2` | Done |
| 伤害/击杀时序 | `CalHurtRole` | `BattleManager` | Partial |
| 宠物战后 | `PetEffect` | `BattleManager::PetEffect` | Done |

## P2 事件扩展

| 功能 | Pascal | C++ | 状态 |
|------|--------|-----|------|
| 50e·41 绘图 | `instruct_50e` | `Instruct_50e` | Done |
| 50e·50 改名 | `InputBox` | `UIManager::ShowInputBox` | Done |
| 50e·51 数量 | `InputAmount` | `UIManager::InputAmount` | Done |
| 50e·60 Lua | 空实现 | no-op 日志 | Done |
| 43 / 50e·43 | `HandleInstruct43Sub` | 共用 | Done |
| 存档互读 | `SaveR/LoadR` | `save_inspector` + 脚本 | 脚本化 |

## P3 UI / 漫游

| 功能 | Pascal | C++ | 状态 |
|------|--------|-----|------|
| `ShowMap` | `kys_engine` | `UIManager::ShowMap` | Done |
| `FourPets` | `kys_main` | `UIManager::FourPets` | Done |
| `CheckHotkey` 1–6 | `kys_main` | `UIManager::CheckHotkey` | Done |
| `MenuDifficult` | `kys_main` | `UIManager::MenuDifficult` | Done |
| `SetScene` 天气 | `Mapmode` | `SceneManager::SetSceneWeather` | Partial |

## P4–P6 平台与工程

| 项 | 说明 |
|----|------|
| Android | `scripts/verify_android_build.ps1` + [ANDROID.md](../../ANDROID.md) |
| 音频 | Win MCI / Android WAV → 统一 SDL3 方案待做 |
| 输入 | `InputManager` 分批收敛阻塞 PollEvent |
| CI | `.github/workflows/cpp_reborn_ci.yml` |
