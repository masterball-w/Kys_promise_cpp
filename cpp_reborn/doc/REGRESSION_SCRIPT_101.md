# 回归说明：事件脚本 101 / 开场卧室

**关联修复**（2026-07-24 ~ 07-28）：`Instruct_40`、`Instruct_ModifyEvent`、`DrawScene`、`InitialScene`、开场模板 `alldef`/`allsin`、`SaveGame(0)` 保护

## 脚本与场景

| 项 | 值 |
|----|-----|
| Kdef 脚本 ID | **101**（新游戏 `BEGIN_EVENT`） |
| 开场场景 | **场景 0**（卧室 → 大厅） |
| 权威模板 | 稳定发布版 `金庸群侠前传1.22.3.21/前传64位-sdl3/save/alldef.grp` + `allsin.grp` |

### 开场模板（稳定版，勿与「脚本跑完后的内存态」混淆）

| 事件 | 贴图 | 坐标 (Y,X) | 说明 |
|------|------|------------|------|
| 0 | **8284** | (38, 40) | 主角躺床（smp 4142）；脚本后改为 **8282** 坐起 |
| 1 | **8268** | (40, 37) | 孔霹雳在**卧室旁**（smp 4134）；`allsin` 层3 `(37,40)=1` |
| 7 | 2606 | (13, 23) | 其它场景物 |

> 曾损坏的仓库模板：事件0 `pic=0`、事件1 被挪到大厅 `(28,14)` 且带脚本 `104/1701`——形态与 **脚本 101 执行后** 一致，属模板被事后状态覆盖，不是读取公式错误。

## 手工验收步骤

1. 确认 `game_data/save/alldef.grp` 事件0=`8284`、事件1=`8268@(40,37)`（制作器或 `pytest`）。
2. 编译运行 `kys_cpp`，**新游戏**进入开场。
3. **卧室镜头** `(Sx,Sy)=(40,38)`：床上躺人 8284；孔霹雳在卧室旁可见。
4. 两句对话 → 淡出 → opcode3 改事件0 为 **8282**（坐起）应可见。
5. 后续平推 / 事件2→8286 等按脚本继续验收。

### 脚本关键 PC（相对脚本起点；控制台绝对 PC≈202+相对）

| 相对 PC | 指令 | 要点 |
|---------|------|------|
| 19 | opcode 3 | 事件0 → 贴图 **8282**（坐起） |
| 100 | opcode 3 | 事件1 坐标写到大厅侧（剧情推进） |
| 114 | opcode 3 | 事件2 → **8286** |
| 130 | opcode 25 | 镜头平推 `[38,40,28,15]` |
| 182 | 50e·43 `-5` | 打开 `ShowMR` |
| 190 | opcode 40 | 面向 **0**（`Sface`） |
| 192 | opcode 19 | 瞬移 `(28,16)` → 引擎 `Sx=16,Sy=28` |

## 引擎改动摘要

1. **`Instruct_40`**：场景内写 `m_subMapFace`（`Sface`）。
2. **`Instruct_ModifyEvent`**：`SData` 条件对齐 Pascal（`X>0` 且 `Y≥0`）；`UpdateSceneGraphic` 占位 + `Instruct_Redraw`。
3. **`DrawScene`**：去重复绘制；主角 `2501+Sface*7+step`；场景内不画云；事件走 `DrawTile`；不透明清屏。
4. **`InitialScene`**：保留 `allsin` 层3；按格 `DData[5]:=DData[7]`（`7≠0`）；层3 空才 `RefreshEventLayer`。
5. **`InitNewGame`**：强制重载模板 `alldef`/`allsin` 再 `InitialScene`；`SetExecutionContext(..., -1)`。
6. **`SaveGame(0)`**：**禁止**写回 `alldef.grp`/`allsin.grp`（只写 `ranger`），保护新游戏模板。
7. **`DrawRLE8`**：热点 `xs/ys := header + 1`（对齐 Pascal）。

## 制作器

- `SceneEventData` / `SceneMapData` 对稳定版文件 **roundtrip 字节级无损**。
- 开场单测：`editor/tests/test_formats.py::test_alldef_scene0_opening_pics`。
- 损坏备份示例：`game_data/save/alldef.grp.corrupt_*.bak`（勿当模板用）。

## 失败时排查

- 用稳定版 MD5 / 制作器核对事件0=`8284`、事件1 坐标 `(40,37)`、层3 `(37,40)=1`。
- 控制台：`[InitialScene]` / `[DrawScene] event=` / `ModEvent:` / `[Instruct_25 done]`。
- 若再次出现「事件0 pic=0、孔霹雳在 (14,28)」：检查是否有旧版引擎把 slot0 写回了模板。
