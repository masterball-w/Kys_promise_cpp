---
name: add-kys-story
description: >-
  Adds or edits 金庸群侠前传 story beats across kdef scripts, talk text, DData/SData
  map hooks, battles, items, and martial arts. Use when the user asks to add
  剧情, 对话, 事件脚本, 场景触发, 战斗奖励, 物品或武功变动.
---

# 添加前传剧情

只改数据文件。不要在 C++ 里为单段剧情写死贴图号、坐标或对白。新游戏模板是 `game_data/save/alldef.grp` 与 `allsin.grp`；进度槽是 `D1`–`D6` / `S1`–`S6`。脚本和对话在 `game_data/resource/`，全槽共用。

## 流程

复制并逐项完成：

```
- [ ] 1. 用 inspect_story.py 读出现有场景事件、脚本和对话
- [ ] 2. 先写 talk，再写 kdef，最后挂 DData/SData
- [ ] 3. 对白符合 voice.md
- [ ] 4. 触发方式与地图格一致（layers.md）
- [ ] 5. 战斗、物品、武功只走对应 opcode（effects.md）
- [ ] 6. 脚本以 -1 结束；跳转用相对偏移，不用手算错的绝对 PC
- [ ] 7. 保存前备份；不要把剧情跑完后的内存态写回 slot0 模板
```

检查（只读）：

```bash
python .cursor/skills/add-kys-story/scripts/inspect_story.py --scene 0 --event 0
```

工作目录为仓库根。数据根默认 `game_data/`。

## 调用链

```
玩家踩格 / 按空格 / 使用物品
  → SData 层3 的事件号
  → DData[场景, 事件, 2|3|4] 脚本号
  → kdef 脚本（resource，1 起编）
  → opcode 1/68 读 talk；opcode 6 进战斗；2/32/33/35 改物品或武功
  → opcode 3 回写 DData（换脚本、换贴图、换坐标）
```

| 层 | 文件 | 谁改 |
|----|------|------|
| 对白 | `resource/talk.idx` + `talk.grp` | `TalkArchive` |
| 脚本 | `resource/kdef.idx` + `kdef.grp` | `KdefArchive.append_script` / `set_script` |
| 挂接 | `save/alldef.grp` 或 `Dn.grp` | `SceneEventData` |
| 事件格 | `save/allsin.grp` 或 `Sn.grp` | `SceneMapData` 层 3 |
| 入口 | `ranger` 场景字 10–13 | 大地图入口，不是室内触发 |

制作器入口：`editor/ui/event_editor.py`。格式库：`editor/kys_formats/`。引擎解释器：`cpp_reborn/src/EventManager.cpp` 的 `ExecuteEvent`。

## 写一条新事件

1. 在目标场景找空的 DData 事件号（0–199），或复用已有号。
2. `TalkArchive` 追加对白，记下 talk 编号。编码 GBK，控制符见 [voice.md](voice.md)。
3. 组装 kdef 字流，末尾必须是 `-1`。参数个数以 `editor/kys_formats/kdef.py` 的 `OPCODE_ARGC` 为准。
4. DData 十一字：`[条件, 备用, 手动脚本, 物品脚本, 踩上脚本, 当前贴图, 结束贴图, 起始贴图, 备用, Y, X]`。
5. 把该事件号写入 SData 层 3 的 `(X, Y)`。层 3 存的是事件号，不是脚本号。`X` 为 DData[10]，`Y` 为 DData[9]。
6. 开场或换幕用 opcode 3 改 DData，不要在引擎里 `SetEventData` 写死贴图。

三种触发只填对应槽，其余脚本槽保持 `<=0`：

| 玩家动作 | DData 字 | 引擎 |
|----------|----------|------|
| 面对 NPC 按空格 | [2] 手动 | 交互键 |
| 走到格子上 | [4] 踩上 | `CheckEvent3` 读层 3 |
| 对格子使用物品 | [3] 物品 | 物品事件 |

条件字 [0] 是触发门闩。沿用同场景相邻事件的非零值；不要把存档槽号写进这里。

## 禁止

- 不要把 `SaveGame(0)` 或跑完脚本后的 D/S 覆盖 `alldef`/`allsin`。
- 不要改入口坐标来“触发室内剧情”。入口只决定大地图进哪个场景。
- opcode 7 是结束当前脚本，不是对白。opcode 61 的参数是相对跳转，不是脚本号。
- 负的 opcode（通常 `-1`）表示脚本结束。

## 延伸阅读

- 地图与触发：[layers.md](layers.md)
- 对白风格：[voice.md](voice.md)
- 战斗、物品、武功：[effects.md](effects.md)
