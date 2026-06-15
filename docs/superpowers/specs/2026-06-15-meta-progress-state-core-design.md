# 局外元进度 · S1 状态内核 + 存档 — 设计文档

- 日期: 2026-06-15
- 分支: `feature/mechanic-items`
- 子项目: 局外系统 S1(共 S1~S6,见末尾路线图)
- 源对照: `scripts/systems/MetaProgress.lua`、`scripts/systems/Balance.lua`

## 1. 目标与范围

把 Lua `MetaProgress.lua` 的**局外持久状态内核**迁到 UE C++:一份跨局存在的元进度数据(结算币 / 已购装备 / 已装备 / 已解锁天赋 / 仓库 / 消耗品库存 / loadout / 统计 / 抢救摘要),加上基于 UE 原生 `SaveGame` 的持久化,以及一组无头可验证的调试命令。

S1 是整条局外循环的**数据地基**,S2~S6 全都读写它。S1 本身不碰 UI、不碰渲染、不改任何局内规则。

### 成功标准

- 编译 0 错;163 冒烟测试不受影响(全过)。
- 无头(`-game -nullrhi`)下用 `gt.Meta*` 命令能:买装备/装备(≤2 上限)/解锁天赋/买消耗品/配 loadout/卖仓库物品/加币/查加成,数值与 Lua 逐项一致。
- 退出再进(或 `gt.MetaSave` 后 `gt.MetaLoad`),状态从磁盘 slot 完整恢复。

### 非目标(明确划界,留给后续子项目)

- **S2 结算闭环**:撤离成功→加币+入库、失败→抢救。S1 只提供 `RecordExtractionReward` / `RecordRun` / `SellWarehouseItem` 等 API,不接 run-end 事件。
- **S3 开局应用**:把加成灌进 `RunContext`。S1 只提供 `GetEquipBonus()` / `GetTalentEffects()` 查询,不改 `RunContext`。
- **S4 商店/经济命令层**:S1 的操作 API 已可被直接调用;S4 负责把它们组织成商店/仓库交互流程(若有额外校验/批处理)。
- **S5 元终端 UI**:Lua 里的显示适配层(`copyDisplayData` / `GetUnifiedItemDisplayData` / `refreshDisplayAdapter` / 各 `displayFrom*`)是 UI 视图模型,**不进 S1**,留给 S5。
- **S6 新机制物品**:6 件组员设计的宝物(anomaly_fang 等)+ 新效果时机。S1 目录只含 Lua 原版的 6 装备 / 5 天赋 / 1 消耗品。

## 2. 架构

```
UGT_MetaProgressSubsystem : UGameInstanceSubsystem     ← 持久状态,跨局存在(UGT_RunSubsystem 的兄弟)
   ├─ FGT_MetaProgressState State                       ← 运行时状态(对齐 Lua 的 data 表)
   ├─ 操作 API(直接公有方法,非局内 CommandBus)
   ├─ 局内查询: GetEquipBonus() / GetTalentEffects()    ← S3 开局读
   └─ Save()/Load()  ──▶  UGT_MetaSaveGame : USaveGame  ← slot "GraytailMeta"

GT_MetaCatalog(命名空间, C++ 静态目录)
   ├─ 6 装备(id/名/价/加成) · 5 天赋(id/名/价/效果) · 1 消耗品(id/名/价/heal/maxCarry)
   └─ 数值照搬 Balance.shop / Balance.talents / MetaProgress.CONSUMABLES
```

### 关键架构决定

- **状态家在 `UGameInstanceSubsystem`**:跨局、跨关卡在一个游戏实例内持续,菜单态也可访问;与现有 `UGT_RunSubsystem` 同范式。职责切分:RunSubsystem = 一局生命周期;MetaProgressSubsystem = 持久元进度。
- **局外操作走子系统公有 API,不走局内 Command 管线**:铁律"改状态的玩家动作走 CommandProcessor"针对的是**局内**动作(依赖一个活着的 `RunContext`)。买/装备/解锁发生在**局外**、无 RunContext,因此由子系统直接暴露方法(对齐 Lua `MetaProgress.BuyItem` 等直接函数)。S5 UI 调这些方法、调用后重读状态刷新。
- **定义目录用 C++ 静态表**(已与豪豪确认):局外配置数值与 `GetEquipBonus`/`GetTalentEffects` 逻辑强耦合,且少需非程序员改;回收物的 `UGT_ItemDef` 资产保持现状不动。
- **每次变更即存**(对齐 Lua:`AddGold`/`SpendGold`/`Buy*`/`Toggle*`/`Unlock*` 末尾都 `Save()`)。S1 数据量小,无性能顾虑。

## 3. 数据模型

新文件 `Domains/Meta/GT_MetaTypes.h`,纯 USTRUCT(对齐 Lua `data` 表)。命名遵守 `FGT_` 铁律。

```cpp
// 仓库里一条物品(回收物/装备/消耗品统一表示, source 区分来源)。
USTRUCT() struct FGT_WarehouseEntry {
    FName ItemId;            // 对齐 Lua warehouse.items 的 key
    int32 Count = 0;
    int32 Value = 0;         // 单件价值(出售用); 入库时从来源拷入
    FName Source = NAME_None; // recovered / equipment / consumable(只有 recovered 可卖)
    bool bUnique = false;     // unique 物品不可卖
};

// 抢救/回收摘要(局外终端展示用的累计统计)。
USTRUCT() struct FGT_RecoverySummary {
    int32 TotalItems = 0;
    int32 TotalValue = 0;
    int32 TotalExtractionsWithItems = 0;
    TArray<FName> RecentItemIds;   // 最近带回, 上限 5(对齐 RECENT_RECOVERY_MAX)
};

USTRUCT() struct FGT_MetaStats {
    int32 TotalRuns = 0;
    int32 TotalExtractions = 0;
    int32 TotalGoldEarned = 0;
};

// 局外持久状态全集(对齐 Lua MetaProgress 的 data 表)。
USTRUCT() struct FGT_MetaProgressState {
    int32 Gold = 0;
    TArray<FName> OwnedItems;       // 已购装备 id(Lua ownedItems set → 这里用数组)
    TArray<FName> EquippedItems;    // 已装备 id, 上限 MAX_EQUIPPED=2(有序)
    TArray<FName> UnlockedTalents;  // 已解锁天赋 id
    TArray<FGT_WarehouseEntry> Warehouse;
    TMap<FName,int32> ConsumableStock;     // 消耗品库存 id→数量(Lua data.consumables)
    TMap<FName,int32> LoadoutConsumables;  // 选定带入 id→数量(Lua data.loadout.consumables)
    FGT_RecoverySummary Recovery;
    FGT_MetaStats Stats;
};
```

同一头文件还定义两个**查询结果**结构(非持久化,`GetEquipBonus`/`GetTalentEffects` 的返回):

```cpp
USTRUCT() struct FGT_EquipBonus {
    int32 BonusHP = 0; int32 BonusPower = 0; bool bMineImmunity = false;
    int32 MineDmgReduce = 0; bool bShowExitHint = false; int32 SearchBonus = 0;
};
USTRUCT() struct FGT_TalentEffects {
    int32 MineDmgReduce = 0; int32 MonsterFleeBonus = 0; int32 FailureGoldBonus = 0;
    int32 TradePrice = 15; bool bMapHighlight = false;
};
```

> 备注:Lua 用 set(`{[id]=true}`)存 owned/equipped/talents;C++ 用 `TArray<FName>` 更直观且 `USaveGame` 友好。`ConsumableStock`/`LoadoutConsumables` 用 `TMap` 对齐 Lua 的 count map。`Warehouse` 用数组(Lua 是 `items[id]=entry` map,数组里靠 ItemId 查找,数量小)。

## 4. 定义目录 `GT_MetaCatalog`

新文件 `Domains/Meta/GT_MetaCatalog.h/.cpp`,命名空间内静态表 + 查询函数。数值逐项照搬。

### 4.1 装备(6 件,`MetaProgress.ITEMS` + `Balance.shop`)

| id | 显示名 | 价 | 效果(GetEquipBonus 字段) |
|----|--------|----|---------------------------|
| armor | 防护背心 | 110 | bonusHP +20 |
| whetstone | 磨刀石 | 90 | bonusPower +2 |
| medkit | 急救包 | 120 | mineImmunity(首次踩雷免疫) |
| insulated_gloves | 绝缘套 | 140 | mineDmgReduce +10 |
| compass | 罗盘 | 160 | showExitHint(撤离方向提示) |
| backpack | 大背包 | 220 | searchBonus +50(%) |

### 4.2 天赋(5 个,`MetaProgress.TALENTS` + `Balance.talents`)

| id | 显示名 | 价 | 效果(GetTalentEffects 字段) |
|----|--------|----|------------------------------|
| talent_map | 邻域感知 | 100 | mapHighlight = true |
| talent_mine | 厚皮 | 120 | mineDmgReduce = 10 |
| talent_monster | 威压 | 120 | monsterFleeBonus = 2 |
| talent_extract | 抢救条款 | 140 | failureGoldBonus = 10 |
| talent_event | 议价 | 140 | tradePrice = 20(默认 15) |

### 4.3 消耗品(1 件,`MetaProgress.CONSUMABLES`)

| id | 显示名 | 价 | 效果 | maxCarry |
|----|--------|----|------|----------|
| emergency_bandage | 应急止血贴 | 12 | heal 25 | 3 |

> 文案(name/desc/description/effectText)照搬 Lua `ITEM_BALANCE_TEXT` / 各定义。装备/天赋的 struct 各含一个"效果"子结构(如 `FGT_EquipDef{ FName Id; FText Name; int32 Price; int32 BonusHP; int32 BonusPower; bool bMineImmunity; int32 MineDmgReduce; bool bShowExitHint; int32 SearchBonus; }`),`GetEquipBonus` 直接累加目录值,避免散落魔数。

## 5. 操作 API(`UGT_MetaProgressSubsystem` 公有方法)

逐一对齐 Lua,语义/返回值/失败原因一致(失败返回 `false` + 原因 FName,成功 `Save()`):

- 币:`GetGold()` / `AddGold(int32)` / `SpendGold(int32)→bool`
- 装备:`OwnsItem(FName)` / `BuyItem(FName)→(bool,FName)`(已拥有/不存在/币不足)/ `ToggleEquip(FName)→(bool,FName)`(未拥有/超上限2)/ `IsEquipped(FName)` / `GetEquippedItems()`
- 天赋:`HasTalent(FName)` / `UnlockTalent(FName)→(bool,FName)`
- 消耗品库存:`GetConsumableCount(FName)` / `BuyConsumable(FName,int32)` / `AddConsumable` / `RemoveConsumable`
- loadout:`SetLoadoutConsumable(FName,int32)`(夹到 stock 与 maxCarry)/ `GetLoadout()` / `ConsumeLoadoutForRun()→runLoadout`(S3 开局调)
- 仓库:`AddWarehouseItems(items,source)` / `GetWarehouseItems(filter)` / `GetWarehouseItemCount(FName)` / `CanSellItem(FName)` / `SellWarehouseItem(FName,count)` / `RemoveWarehouseItem`
- 结算/统计:`RecordRun()` / `RecordExtractionReward(reward,runStats)→receipt`(S2 调)/ `GetStats()` / `GetRecoverySummary()`
- 局内查询(S3 读):`GetEquipBonus()→FGT_EquipBonus` / `GetTalentEffects()→FGT_TalentEffects`
- GM:`GMGrantItem` / `GMGrantTalent` / `GMEquipAll` / `GMUnequipAll` / `GMReset`

> `GetEquipBonus` / `GetTalentEffects` 的返回结构对齐 Lua 同名函数的字段(bonusHP/bonusPower/mineImmunity/mineDmgReduce/showExitHint/searchBonus;mineDmgReduce/monsterFleeBonus/failureGoldBonus/tradePrice/mapHighlight)。

## 6. 持久化 `UGT_MetaSaveGame : USaveGame`

```cpp
UCLASS() class UGT_MetaSaveGame : public USaveGame {
    UPROPERTY() FGT_MetaProgressState State;
    UPROPERTY() int32 SaveVersion = 1;   // 预留迁移
};
```

- slot 名 `"GraytailMeta"`,user index 0,经 `UGameplayStatics::SaveGameToSlot/LoadGameFromSlot`(本项目首次接 slot 持久化,确立范式)。
- `Initialize()` 时 `Load()`:slot 存在则读入 `State`,否则全新起始(gold=0)。读入时做校验(对齐 Lua:已装备的物品必须确实拥有,否则剔除;loadout 数量夹到 stock)。
- 任一变更操作末尾 `Save()`。

## 7. 调试命令(无头验证用)

在 `Debug/GT_EditorManualPlayCommands.cpp`(或 `GT_DebugSubsystem`)加 `gt.Meta*`,结果打到 `LogGraytailManualPlay`(对齐现有 gt.* 范式):

- `gt.Meta` — 打印全状态(币/已购/已装/天赋/仓库/消耗品/loadout/统计)
- `gt.MetaBuy <id>` / `gt.MetaEquip <id>` / `gt.MetaTalent <id>` / `gt.MetaBuyConsumable <id> [n]` / `gt.MetaLoadout <id> <n>` / `gt.MetaSell <id> [n]`
- `gt.MetaBonus` — 打印 `GetEquipBonus` + `GetTalentEffects`(验证 S3 输入)
- `gt.MetaAddGold <n>` / `gt.MetaReset`(GM)
- `gt.MetaSave` / `gt.MetaLoad`(强制存/读,验证持久化往返)

## 8. 文件清单

- 新增 `Source/Graytail/Domains/Meta/GT_MetaTypes.h`(数据结构)
- 新增 `Source/Graytail/Domains/Meta/GT_MetaCatalog.h/.cpp`(定义目录)
- 新增 `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h/.cpp`(子系统 + 操作 API + 加成查询)
- 新增 `Source/Graytail/Save/GT_MetaSaveGame.h/.cpp`(USaveGame 子类)
- 改 `Debug/GT_EditorManualPlayCommands.cpp`(注册 gt.Meta* 命令)
- 不改:`GT_RunContext` / `GT_RunSubsystem` / 任何局内规则 / 163 夹具 / `UGT_ItemDef` 资产

## 9. 验证计划

1. 关编辑器 → `Build.bat GraytailEditor`,退出码 0。
2. 163 冒烟:`-run=GT_RuntimeSmokeRunner` → `Overall=Pass / Pass=163 / Fail=0`(S1 不碰局内,应零影响)。
3. 无头脚本:`-game -nullrhi -ExecCmds="gt.MetaReset, gt.MetaAddGold 500, gt.MetaBuy armor, gt.MetaBuy whetstone, gt.MetaEquip armor, gt.MetaEquip whetstone, gt.MetaTalent talent_mine, gt.MetaBuyConsumable emergency_bandage 3, gt.MetaLoadout emergency_bandage 2, gt.MetaBonus, gt.Meta, gt.MetaSave, quit"` → grep 日志核对:币 500→剩余正确、armor+whetstone 已购已装、talent_mine 已解锁、绷带库存 3/loadout 2、`GetEquipBonus` = {bonusHP 20, bonusPower 2}、`GetTalentEffects` = {mineDmgReduce 10}。
4. 持久化往返:第二次 `-ExecCmds="gt.MetaLoad, gt.Meta, quit"` 确认上轮状态从磁盘恢复。
5. 上限/失败语义抽查:装第 3 件应被拒(超上限 2);币不足买不动;重复解锁天赋被拒。

## 10. 局外系统路线图(上下文)

S1 状态内核+存档(本文档) → S2 结算闭环 → S3 开局应用 loadout → S4 商店/仓库/天赋经济 → S5 元终端 UI → S6 6 件新机制物品 + 新效果触发。每个子项目各自走 spec → plan → 实现 → 验证。
