# Kys Promise 解耦制作器
#
# 与 C++ 引擎完全解耦：只读写 game_data/，不修改、不链接 cpp_reborn。
#
# ## 安装
#
# ```bash
# cd editor
# pip install -r requirements.txt
# python main.py
# ```
#
# ## 数据根目录
#
# 默认使用仓库根下的 `../game_data`。也可在工具栏中选择其它路径。
# 写回文件前会自动生成 `.bak` 备份。
#
# ## 模块
#
# | Tab | 功能 |
# |-----|------|
# | 存档数据 | Ranger/Rn：Header、人物/物品/武功（分组详情）、背包、商店、场景 |
# | 事件 | Kdef 中文指令名 + 参数 tooltip（对话/物品/角色/战斗映射）、talk、DData/SData |
# | 战斗 | War.sta 列表/编辑/追加、阵型网格、warfld 地形、脚本引用检查 |
# | 贴图 | Heads/Items/Begin/.Pic 导入导出替换、fight/eft、smp 砖预览、HeadNum 联动 |
# | 交叉引用 | 战斗/物品 ↔ 脚本反查 |
#
# ### 武功编辑（存档 → 武功）
#
# - **类别** `MagicType`：1拳 2剑 3刀 4奇门 5内功；`HurtType=1` 为吸星（伤内力）
# - **特效** `AmiNum` → 预览 `eft/eftNNN.pic`
# - **威力** `MinHurt/MaxHurt/HurtModulus`，按引擎 `CalNewHurtValue` 推算 1/10 级并显示公式与 1～10 级表
# - **加成模式** `Attack/MP/Speed/Weapon Modulus`（攻击型/内力型/轻功型/兵器型权重）
# - **范围** `AttAreaType`（点/线/面/十字等）+ `MoveDistance`/`AttDistance`
# - **内功** `NeedExp`、`AddHP/MP/Att/Def/Spd`、`BattleState` 等
#
# ### 人物编辑（存档 → 人物）
#
# - 基本信息、生命/内力、战斗属性、技能熟练度
# - 五装备栏、功体/修炼、10 栏武功与等级、随身物品
# - 头像预览（Heads.Pic / HeadNum）
#
# ### 物品定义（存档 → 物品定义）
#
# - 类型（剧情/装备/秘籍/丹药/暗器）、装备部位、价格/事件/关联武功
# - 战斗特效 `BattleEffect`、说明文本、图标预览（Items.Pic）
# - 全部 Add* 加成、Need* 需求、合成材料栏
#
# ## 格式库
#
# `kys_formats/` 可独立于 UI 使用：
#
# ```python
# from kys_formats import RangerArchive, WarArchive, PicArchive
# ```
#
# ## 注意事项
#
# - 背包槽按磁盘 **400** 读写；C++ 引擎若 `MAX_ITEM_AMOUNT=300`，游戏内写档可能截断 301–400。
# - talk/name 为 XOR 0xFF + Big5/GBK；ranger 内定长名多为 GBK。
# - `.Pic` 使用与引擎一致的 **end-offset** 布局。
# - 不要将修改后的 `game_data` 二进制提交进 git。
#
# ## 测试
#
# ```bash
# cd editor
# pytest tests/ -v
# ```
#
# 需本地存在 `game_data/`。
