# 局外元进度 · S2 结算闭环 — 设计文档(含实现要点)

- 日期: 2026-06-15
- 分支: `feature/meta-progress`
- 子项目: 局外 S2(承接 S1 状态内核)
- 源对照: `scripts/main.lua`(失败/成功结算)、`scripts/systems/RunInventory.lua`(奖励公式)、`scripts/systems/MetaProgress.lua`

## 1. 目标与范围

把"一局结束 → 局外结算"接通:撤离成功 → 加币 + 回收物入仓库 + 统计;失败 → 抢救(保留 SafeGold + 撤离天赋补币 + 自动抢救最高价值 1 件)。S1 已提供 `RecordExtractionReward` / `AddGold` / `AddWarehouseItems` / `RecordRun` / `HasTalent` / `GetTalentEffects` 等 API,S2 只负责**在局终把它们调起来**,数据从 `RunContext` 读。

### 非目标
- 不改局内规则、不动 163 BasicDebug 夹具(**结算只在 Standard 模式触发**)。
- 不做结算 UI(那是 S5;S2 只改状态,UI 后续读 MetaProgress)。
- loadout 消耗 / 开局加成应用 = S3(本 S2 只在开局做 `RecordRun` 统计)。

## 2. 挂接点(已勘察确认)

EventBus(`UGT_EventBus`,`OnGameEventPublished` 动态多播)是 `UGT_RunSubsystem` 的常驻成员。所有局终路径都已广播事件:
- `RunStarted` — `GT_RunSubsystem::FinishStartRun`(开局)
- `RunSucceeded` — `GT_CommandProcessor` 撤离成功(:376,唯一一次)
- `RunFailed` — 各失败路径(踩雷 :206/:212、战斗死 :474、事件死 :409、满压 :517;均 `if(MarkRunFailed)` 守卫,一次)

**设计:`UGT_RunSubsystem` 在 `Initialize` 里 `OnGameEventPublished.AddDynamic` 订阅自己的 EventBus**,UFUNCTION 处理器按 `EventType` 分发。一处覆盖所有局终路径,不在多个失败点散插。

- 仅当 `CurrentRunContext->GetMapMode() == EGT_MapMode::Standard` 才结算(BasicDebug/163 夹具零影响;对齐 Lua 训练工单 `allowFailureRewards==false` 不结算)。
- **一次性 guard** `bRunSettled`(StartNewRun/StartNewRunStandard 重置,首次结算后置位),防御任何重复事件。

## 3. 结算规则(照搬 Lua,带源码佐证)

### 3.1 开局 RunStarted
`MetaProgress.RecordRun()`(`Stats.TotalRuns++`)。仅 Standard。

### 3.2 撤离成功 RunSucceeded(对齐 main.lua:3180 + RunInventory.GetExtractionReward:788)
从 `RunContext.GetRunInventory()`(`FGT_RunInventoryState`)构造 `FGT_ExtractionReward`:
- `DirectGold = PendingGold + SafeGold`(Lua: `totalGold = pendingGold + safeGold`,`directGold = totalGold`)
- `LoosePartsGold = 0`(**Lua 写死 0** — `GetExtractionReward` 里 `loosePartsGold=0/convertedGold=0`,零散零件不折金;忠实迁移)
- `CarriedItems`:`RunInventory.CarriedItems` 每堆 → `FGT_RewardItem{ ItemId, Count, Value = GT_ItemCatalog::GetItemValue(ItemId) }`

→ `MetaProgress.RecordExtractionReward(Reward)`(S1 已实现:加币 = DirectGold+LoosePartsGold、回收物入仓库、recovery/extractions 统计)。

### 3.3 失败 RunFailed(对齐 main.lua:2586 + RunInventory.GetFailureSalvageOptions:814 / ApplyFailureSalvage:844)
- **抢救最高价值 1 件**:遍历 `RunInventory.CarriedItems`,按 `GT_ItemCatalog::GetItemValue` 取最大者(1 件)。
- **金币** = `SafeGold + GetTalentEffects().FailureGoldBonus`(撤离天赋解锁则 +10;`canSalvagePart=false`,零件不抢救;PendingGold 丢失)。
- 调:`MetaProgress.AddGold(FinalGold)` + 若有抢救件 `MetaProgress.AddWarehouseItems({BestItem×1}, "recovered")` + `MetaProgress.Save()`(AddWarehouseItems 不自存,故末尾补 Save)。

## 4. 文件清单

- 新增 `Source/Graytail/Domains/Meta/GT_MetaSettlement.h/.cpp`:命名空间 `GT_MetaSettlement`,两个自由函数
  - `void SettleExtraction(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta);`
  - `void SettleFailure(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta);`
  - 读 RunContext 背包 + GT_ItemCatalog 价值,构造数据调 S1 API。隔离 RunSubsystem 与结算细节。
- 改 `Source/Graytail/Core/GT_RunSubsystem.h/.cpp`:
  - 加 `UFUNCTION() void HandleRunEvent(FGT_GameEvent Event);`(AddDynamic 要求 UFUNCTION)
  - 加 `bool bRunSettled = false;`
  - `Initialize`:创建 EventBus 后 `EventBus->OnGameEventPublished.AddDynamic(this, &UGT_RunSubsystem::HandleRunEvent);`
  - `StartNewRun`/`StartNewRunStandard`:`bRunSettled = false;`
  - `HandleRunEvent`:按 EventType 分发(RunStarted→RecordRun;RunSucceeded→SettleExtraction;RunFailed→SettleFailure),门控 Standard + bRunSettled。
- 不改:RunContext、CommandProcessor、MetaProgress(S1 的 API 已够用)、163 夹具、UGT_ItemDef。

> MetaProgress 子系统经 `GetGameInstance()->GetSubsystem<UGT_MetaProgressSubsystem>()` 取(RunSubsystem 本身是 GameInstanceSubsystem)。

## 5. 验证计划

1. **[BUILD]** 编译 0 错。
2. **[SMOKE]** 163 全过(结算门控 Standard,BasicDebug 夹具不触发结算 → 零影响,这是关键回归点)。
3. **无头·撤离结算**(用 Tutorial 固定 5×5 图,Standard 模式,坐标确定):
   `gt.MetaReset, gt.StartStd Tutorial, gt.Teleport 1 4(宝箱), gt.Search, gt.Teleport 4 4(撤离), gt.Extract, gt.Meta, quit`
   期望:`gt.Meta` 显示 stats `runs=1 extractions=1`、gold 增加(= 该局 pending+safe)、warehouse 出现搜到的回收物。
4. **无头·失败抢救**:在 Standard 局制造死亡(走入 Tutorial 雷格 / 满压),`gt.Meta` 显示 stats `runs=1 extractions=0`、warehouse 有最高价值 1 件、gold = SafeGold(+10 若解锁撤离天赋)。具体致死手法实现时定(走雷多次 / 协议);若无头难以确定性致死,记录为 PIE 复核项。

## 6. 路线图位置
S1 状态内核(done) → **S2 结算闭环(本文档)** → S3 开局应用 loadout → S4 经济命令层 → S5 元终端 UI → S6 新机制物品。
