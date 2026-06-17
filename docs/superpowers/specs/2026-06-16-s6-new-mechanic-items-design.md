# S6 局外组新机制物品 接入设计

- 日期: 2026-06-16
- 状态: **设计已定、暂缓实现**(因下午演示先去合并战斗模块;本文档为存档,恢复时直接进 writing-plans)
- 前置: S1 状态/存档 · S2 结算 · S3 开局应用 · S4 议价 · S5 部署终端五页(均已在 main 或待合 PR)

## 背景与定位

- 现有局内"回收物"(断裂铜线/暗淡电容/低语灯芯/封存核心碎片/静电透镜/黑匣标签)**只有价值,无任何持有效果**;仅用于搜索掉落 → 估值 → 出售/结算。这套不动。
- 之前队友另起炉灶的 `Source/Graytail/ItemSystem/`(Actor 范式)编译不过、与现有管线重复,已撤除(PR #7)。**不复用、不参考**。
- 本模块的 6 件来自 `局外组员内容/灰尾公司-物品信息表.xlsx`(12 件中筛掉与现有物品/天赋重复的 6 件,保留 6 件;素材在 `assets/item_loadout/`)。
- **定位结论(用户拍板)**: 这些不是局内掉落,而是**局外商店申领的新装备 / 新消耗品**,接进现有 `GT_MetaCatalog` + loadout 体系(装备占装备位 ≤2,消耗品走 loadout 带入)。

## 范围: 5 件(超载零件箱删除)

`overload_parts_box 超载零件箱` 依赖"背包负重上限"系统 —— UE 端**不存在负重系统**,删除(用户确认)。其余 5 件全做:

| id | 名称 | 类型 | 稀有 | xlsx价值 | 机制 | 触发点 | 本局状态 |
|---|---|---|---|---|---|---|---|
| anomaly_fang | 异常体犬牙 | 装备 | 稀有 | 80 | 怪物房战斗胜利后本局攻击力 +2 | 战斗胜利 | 叠加层数 ≤5(+10 封顶) |
| lockdown_crystal | 封锁区结晶 | 装备 | 史诗 | 200 | 协议每升一级回 1 血 | 协议升级(`FGT_ProtocolState::bLevelChanged`) | 回血次数 ≤4(+4 HP) |
| company_badge | 公司工牌 | 装备 | 史诗 | 150 | 撤离成功结算金币 +15% | S2 撤离结算 | 无(结算时算) |
| salvage_magnet | 回收磁石 | 装备 | 稀有 | 90 | 进入宝箱房额外掉 1 件随机低价值回收物 | 进入宝箱房 | 可加"每房一次"去重 |
| lucky_coin | 幸运硬币 | 消耗品 | 罕见 | 50 | 使用后 RNG: 50% 得 30 金 / 50% 揭示 1 个相邻未知房(免协议压力) | UseItem 命令 | 无 |

> 注: "商人价格 -20%"(company_badge 原文另一半)因 UE 旅商**只收购不卖货**、无意义,省略,只保留结算 +15%。
> 注: "异常体"在 UE 无独立怪物子类型 → 用**怪物房战斗胜利**触发。
> 注: 宝箱房**已实现**(`EGT_RoomBaseType::Chest` + 生成 + 金箱开箱演出);`GT_LootRules.h` 里"宝箱房待 TODO"是过时旧注释,实现时顺手清掉。
> 注: 揭示相邻房 = `MarkPlayerIntelCellExplored` 在相邻未知情报格上,小地图显示图标(可加一点点特效)。

## 架构: 数据标签 + 触发派发(贴合现有数据驱动风格)

现有装备效果(`FGT_EquipBonus`)都是**开局静态加成**(HP/战力/减雷/免雷/搜索%/撤离提示),在 `ApplyMetaLoadout` 一次性灌进 RunContext。新物品要的是**触发型效果**,需新增:

1. **触发标签**: 新 UENUM `EGT_ItemTrigger { None, KillPowerStack, ProtocolHeal, SettleGoldBonus, ChestBonusLoot, ... }`(**只往后追加**,守铁律)。`FGT_EquipDef` / `FGT_ConsumableDef` 加 `EGT_ItemTrigger Trigger` 字段(+ 必要的数值参数,如叠加上限/数额)。
2. **RunContext 触发态**: `ApplyMetaLoadout` 时收集"已装备的触发型物品"成一个激活集合 + 本局状态(`AnomalyPowerStacks`、`LockdownHealsLeft` 等),`InitializeFromSpec`/`ResetRun` 重置。
3. **钩子派发**(在**现有**触发点查激活集合并执行,不新开 Command 走被动效果):
   - 战斗胜利路径(`ProcessResolveCombatCommand` / `RoomResolver::ResolveCombatAt` 胜利分支)→ KillPowerStack。
   - 协议压力更新后 `bLevelChanged && 新等级<旧等级` → ProtocolHeal。
   - 进入宝箱房(移动进入 `EGT_RoomBaseType::Chest` 格)→ ChestBonusLoot(走现有掉落 grant)。
   - 撤离成功结算 `GT_MetaSettlement::SettleExtraction`(meta 侧,有 Meta)→ 直接 `Meta->IsEquipped(company_badge)` 时 gold×1.15(无需 RunContext 触发态)。
4. **消耗品效果分派**: 现有 UseConsumable 只回血;扩成按 `FGT_ConsumableDef.Trigger` 分派(幸运硬币 = LuckyCoin: 确定性 RNG(seed,x,y,salt)二选一 → AddSafeGold(30) 或 揭示相邻未知房)。

**BasicDebug 零影响**: BasicDebug 永不应用 loadout → 激活集合恒空 → 163 不受影响(同 S3/S4)。

## 集成点

- `GT_MetaCatalog`: 加 4 件装备 def + 1 件消耗品 def(含 Trigger 标签 + 购买价)。
- 部署终端申领页: 已遍历 catalog 列装备+消耗品 → 新物品**自动出现**(含稀有度筛选,需给装备补 rarity tier 或沿用 EquipTierKey 派生)。
- 图标: `assets/item_loadout/{anomaly_fang,company_badge,lockdown_crystal,lucky_coin,salvage_magnet}.png` → 按 `_import_deploy_icons.py` 流程导入 `Content/Graytail/UI/deploy/`,接进 `IconForEquip`/`IconForConsumable`(注意 headless 新导入图标显示问题,以 PIE 为准)。
- 购买价: xlsx 的"价值"是卖价;商店购买价按稀有度档位另定(可调),实现时给个初值,留待平衡。

## 验证计划

- 编译 + 163 冒烟全过(基础路径零影响)。
- 逐件无头验证: 装备后开 Standard,触发各钩子看效果(击杀叠攻力 / 协议升级回血 / 撤离 +15% / 宝箱房额外掉 / 硬币 RNG 两分支)。
- 申领页 PIE 目检新物品卡片 + 图标。

## 待定/可调

- 各物品**购买价**(初值 + 平衡)。
- 异常体犬牙叠加上限 5 / +10、封锁区结晶回血 ≤4 —— 已按 xlsx「机制详解」表锁定,实现照此。
- 幸运硬币"揭示相邻房"的具体选格规则(随机一个相邻未知 / 最近未知)+ 小地图特效程度。
