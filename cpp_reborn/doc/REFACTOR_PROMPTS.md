# KYS C++ 重构提示词清单 (精简版)

本文件包含标准化的 Prompt，用于指导 AI 按照 `check.md` 规则进行重构。请按顺序执行。

---

## Phase 1: 数据层基础 (Data Foundation)

### Prompt 1.1: 实现 FileLoader
**参考文件**: 无 (通用逻辑，参考 `kys_main.pas` 中的资源文件名常量)
```text
请实现 `FileLoader` 类用于读取 `.grp` 和 `.idx` 文件。
1. 创建 `include/FileLoader.h` 和 `src/FileLoader.cpp`。
2. 实现 `loadGroupRecord(filename, index)`，返回二进制数据。
3. 确保兼容原版资源路径 (`resource/`)。
4. 集成到 `GameManager`。
```

### Prompt 1.2: 完善 Role 和 Item 类
**参考文件**: `kys_main.pas` (Type Definitions: `TRole`, `TItem`)
```text
请根据 `kys_main.pas` 的类型定义完善 `Role` 和 `Item` 类。
1. 在 `Role.h` / `Item.h` 中补全所有字段的 Getter/Setter。
2. 严格按照 Pascal `Variant Record` 布局计算内存偏移量 (Offset)。
3. 编写测试代码打印加载的数据以验证二进制兼容性。
```

---

## Phase 2: 渲染系统 (Rendering System)

### Prompt 2.1: SDL3 初始化
**参考文件**: `kys_engine.pas` (Initialization, `Run`)
```text
请移植 `kys_engine.pas` 的初始化逻辑。
1. 在 `GameManager::Init` 中初始化 SDL3 (Video, Audio)。
2. 创建窗口和渲染器。
3. 实现主循环 `Run`，处理退出事件。
4. 确保程序能编译运行并显示窗口。
```

### Prompt 2.2: 移植 GraphicsUtils
**参考文件**: `kys_gfx.pas` (RotoZoom), `kys_engine.pas` (DrawPixel, PutPixel)
```text
请移植核心绘图函数到 `GraphicsUtils`。
1. 实现 `DrawPixel`, `GetPixel`。
2. 实现调色板转换 (8-bit index -> 32-bit RGBA)。
3. 实现 RLE 格式图片解码与绘制 (参考 `kys_engine.pas` 中的 RLE 算法)。
4. 实现透明色键 (Color Key) 处理。
```

### Prompt 2.3: 字体渲染
**参考文件**: `kys_engine.pas` (DrawText, DrawBig5Text)
```text
请移植文本渲染逻辑。
1. 集成 SDL_ttf。
2. 实现 `TextManager`，处理 GBK/Big5 到 UTF-8 的转码。
3. 实现 `DrawText` 函数。
4. 测试中文输出。
```

---

## Phase 3: 场景与交互 (Scene & Interaction)

### Prompt 3.1: 场景绘制
**参考文件**: `kys_main.pas` (TScene), `kys_engine.pas` (DrawScene, DrawMMap)
```text
请实现场景地图绘制。
1. 在 `GameManager` 加载 `Scene.grp`。
2. 创建 `SceneManager` 管理 Tilemap。
3. 移植 `DrawScene` 逻辑，正确处理层级遮挡。
4. 显示初始场景。
```

### Prompt 3.2: 移动与碰撞
**参考文件**: `kys_engine.pas` (Walk, CanWalk, GetPositionOnScreen)
```text
请移植角色移动逻辑。
1. 处理输入控制主角移动。
2. 移植 `CanWalk` 进行碰撞检测 (读取场景阻挡层)。
3. 实现摄像机跟随 (Camera Follow)。
```

### Prompt 3.3: 动画与特效
**参考文件**: `kys_engine.pas` (DrawClouds, ChangeCol)
```text
请实现基础动画与特效。
1. 实现 `ChangeCol` (调色板循环动画)，用于水面闪烁等效果。
2. 实现 `DrawClouds` (云层飘动)。
3. 移植角色行走动画 (Frame Cycle)。
```

---

## Phase 4: 逻辑与事件 (Logic & Events)

### Prompt 4.1: 事件系统
**参考文件**: `kys_event.pas` (全文件)
```text
请移植事件系统。
1. 创建 `EventManager`。
2. 移植 `kys_event.pas` 中的核心触发逻辑 (如空格键触发)。
3. 选取一个典型事件 (如对话) 进行移植验证。
```

### Prompt 4.2: UI 系统
**参考文件**: `kys_engine.pas` (ShowMenu, DrawRectangle)
```text
请移植基础 UI。
1. 实现 `UIManager`。
2. 移植 `ShowMenu` (菜单) 和 `DrawRectangle` (边框) 逻辑。
3. 实现状态栏显示。
```

---

## Phase 5: 战斗系统 (Battle System)

### Prompt 5.1: 战斗数据
**参考文件**: `kys_battle.pas` (TBattleRole), `kys_main.pas` (TWarSta)
```text
请搭建战斗系统基础。
1. 创建 `BattleManager`。
2. 定义 `BattleRole` 类 (对应 `TBattleRole`)。
3. 实现战斗地图加载。
```

### Prompt 5.2: 战斗逻辑
**参考文件**: `kys_battle.pas` (AutoBattle, Move, Attack)
```text
请移植战斗核心逻辑。
1. 实现回合制状态机。
2. 实现 BFS 移动范围计算。
3. 移植攻击伤害公式。
4. 移植基础 AI。
```
