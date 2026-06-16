# 局外元进度 · S3 开局应用 loadout — 设计文档(含实现要点)

- 日期: 2026-06-15
- 分支: `feature/meta-progress`
- 子项目: 局外 S3(承接 S1 状态 + S2 结算)
- 源对照: `scripts/main.lua`(StartNewGame 应用 equipBonus/talentEffects)、`MetaProgress.GetEquipBonus/GetTalentEffects`

## 1. 目标与范围

开局把"局外投入"灌进局内 RunContext,让装备/天赋/loadout 在一局里真实生效。替换 `InitializeFromSpec` Standard 分支里写死的"2 个 emergency_bandage"占位。

**范围 = A + B(B 收敛为 3 项)**:
- **A 开局直接设**: 最大生命 += BonusHP(armor 20)、战力 += BonusPower(whetstone 2)、带入 loadout 消耗品(ConsumeLoadoutForRun 的结果,替换占位)。
- **B 钩进规则层**: 雷伤减免(insulated_gloves 10 + talent_mine 10 叠加)、首次踩雷免疫(medkit,一次性)、搜索奖励 +50%(backpack)。

**搁置(本 S3 不做)**:
- **tradePrice 议价(talent_event)**: UE 旅商用 `GetTraderSaleValue = floor(value*0.75)`(按价值百分比收购),与 Lua 的 `tradePrice`(15→20 固定价)模型不同;映射方式是新平衡决策,留 S4 经济/事件阶段定。
- **C 表现/UI 类**: 撤离方向提示(compass showExitHint)、避让窗口 +2s(talent_monster monsterFleeBonus)、邻域高亮(talent_map mapHighlight)→ S5 UI/表现层。
- failureGoldBonus(撤离天赋)S2 已在 SettleFailure 用到, 不在此重复。

## 2. 架构(挂接点)

`RunContext` 不依赖 MetaProgress 子系统;由 **RunSubsystem 当桥**(已在 S2 承担局终结算)在开局把纯数据结构灌进 RunContext。

```
RunSubsystem::StartNewRunStandard:
  NewObject RunContext → InitializeRunStandard(...) → ApplyMetaLoadoutToRun() → FinishStartRun()

ApplyMetaLoadoutToRun()(仅 Standard + Meta 存在):
  Equip = Meta->GetEquipBonus(); Talents = Meta->GetTalentEffects();
  Consumables = Meta->ConsumeLoadoutForRun();   // 扣库存, 返回本局携带
  RunContext->ApplyMetaLoadout(Equip, Talents, Consumables);
```

- `RunContext.h` 仅 `#include "Domains/Meta/GT_MetaTypes.h"`(纯数据结构 FGT_EquipBonus/FGT_TalentEffects,**不依赖子系统**)。
- BasicDebug 路径(StartNewRun)**不调** ApplyMetaLoadoutToRun → 163 夹具零影响。

## 3. 实现要点

### 3.1 RunContext 新增(GT_RunContext.h/.cpp)
- 字段(开局态,InitializeFromSpec/ResetRun 重置为 0/false):
  ```cpp
  int32 LoadoutMineDmgReduce = 0;
  bool  bLoadoutMineImmunityAvailable = false;
  int32 LoadoutSearchBonusPercent = 0;
  ```
- 方法:
  ```cpp
  void ApplyMetaLoadout(const FGT_EquipBonus& Equip, const FGT_TalentEffects& Talents, const TMap<FName,int32>& Consumables);
  ```
  实现: `MaxHp += Equip.BonusHP; Hp = MaxHp;`(开局满血)`Power += Equip.BonusPower;`
  `LoadoutMineDmgReduce = Equip.MineDmgReduce + Talents.MineDmgReduce;`
  `bLoadoutMineImmunityAvailable = Equip.bMineImmunity;`
  `LoadoutSearchBonusPercent = Equip.SearchBonus;`
  遍历 Consumables → `RunInventory.AddCarriedItem(id, count, "loadout")`。

### 3.2 改 ApplyMineHitToPlayer(GT_RunContext.cpp:487)
```cpp
if (bLoadoutMineImmunityAvailable) { bLoadoutMineImmunityAvailable = false; OutDamage = 0; bOutDead = !PlayerCombatState.IsAlive(); return; }
OutDamage = FMath::Max(GT_CombatRules::MineDamageFloor, GT_CombatRules::MineDamage - LoadoutMineDmgReduce);
PlayerCombatState.ApplyDamage(OutDamage); bOutDead = !PlayerCombatState.IsAlive();
```
(免疫 = 0 伤害绕过下限;减免后仍受 MineDamageFloor=5 下限保护,对齐注释"装备减免后最低 5"。)

### 3.3 改 SearchCurrentRoom 金币(GT_RunContext.cpp:653)
```cpp
RunInventory.AddPendingGold((Reward.Gold * (100 + LoadoutSearchBonusPercent)) / 100);
```
(SearchBonus=0 时不变 → 普通局/无背包零影响。)

### 3.4 删占位(GT_RunContext.cpp:84-86)
移除 Standard 分支写死的 `AddCarriedItem("emergency_bandage", 2, "loadout")`;消耗品改由 ApplyMetaLoadout 按真实 loadout 灌入。无 loadout 配置 = 开局 0 消耗品(正确的 loadout 驱动行为)。

### 3.5 RunSubsystem(GT_RunSubsystem.h/.cpp)
- 私有 `void ApplyMetaLoadoutToRun();`(实现见 §2)。
- `StartNewRunStandard` 在 `InitializeRunStandard` 后、`FinishStartRun` 前调用。

## 4. 文件清单
- 改 `Core/GT_RunContext.h/.cpp`、`Core/GT_RunSubsystem.h/.cpp`。
- 不新增文件。不改 MetaProgress(S1 API 够用)、163 夹具、UGT_ItemDef。

## 5. 验证计划
1. **[BUILD]** 0 错。
2. **[SMOKE]** 163 全过(BasicDebug 不应用 loadout + SearchBonus=0/MineDmgReduce=0 默认零影响)。
3. **无头·加成生效**(配置 loadout 后开 Standard 局):
   `gt.MetaReset, gt.MetaAddGold 999, gt.MetaBuy armor, gt.MetaBuy whetstone, gt.MetaEquip armor, gt.MetaEquip whetstone, gt.MetaBuyConsumable emergency_bandage 3, gt.MetaLoadout emergency_bandage 2, gt.MetaTalent talent_mine, gt.StartStd Tutorial, gt.Bag`
   期望 gt.Bag: Hp=120/120(armor +20)、Power=12(whetstone +2)、CarriedItems 含 emergency_bandage x2(loadout,不再是写死的 2)。
4. **无头·雷伤减免**: 装 insulated_gloves(或解锁 talent_mine)后踩雷,伤害 = 30-10=20(而非 30);medkit 首次踩雷伤害 0、第二次正常。
5. **无头·搜索+50%**: 装 backpack 后搜同一格,金币 = 基准 ×1.5(对比未装)。
6. **无头·无 loadout**: 不配置 → 开局 0 消耗品、HP 100/Power 10(占位已移除)。

## 6. 路线图位置
S1 状态(done) → S2 结算(done) → **S3 开局应用 loadout(本文档)** → S4 商店/经济命令层(含 tradePrice 决策) → S5 元终端 UI(含 C 表现类) → S6 新机制物品。
