# 局外元进度 S1(状态内核 + 存档)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 Lua `MetaProgress.lua` 的局外持久状态(币/装备/天赋/仓库/消耗品/loadout/统计/抢救)迁成一个 UE `UGameInstanceSubsystem`,用原生 `SaveGame` 持久化,并提供 `gt.Meta*` 无头验证命令。

**Architecture:** `UGT_MetaProgressSubsystem`(`UGameInstanceSubsystem`)持有 `FGT_MetaProgressState`,局外操作走子系统公有 C++ 方法(不走局内 Command 管线,因无 RunContext);定义数值在 C++ 静态目录 `GT_MetaCatalog`;持久化经 `UGT_MetaSaveGame : USaveGame` 的 slot `"GraytailMeta"`,每次变更即存。

**Tech Stack:** UE 5.7 C++,`UGameInstanceSubsystem`、`USaveGame` + `UGameplayStatics::SaveGameToSlot/LoadGameFromSlot`、`FAutoConsoleCommandWithWorldAndArgs`。

**验证范式说明(重要):** 本项目没有 pytest 式单元测试;验证手段 = ① `Build.bat` 编译(退出码 0)② 163 冒烟(`GT_RuntimeSmokeRunner`,`Overall=Pass`)③ 无头 `gt.*` 命令跑场景 → grep `UE/Graytail/Saved/Logs/Graytail.log` 的 `LogGraytailManualPlay`。下文每个任务的"验证"步骤即采用这套(对齐 `docs` 与现有 gt.* 命令范式),不写 pytest。每个任务自成一个可编译单元,末尾 build + commit。

**前置环境(每次 build/headless 前):**
- 完全关闭 UE 编辑器(Live Coding 占锁);若 build 退出码 6,先杀残留进程:
  `Get-Process | Where {$_.ProcessName -like 'Unreal*' -or $_.ProcessName -like 'CrashReport*' -or $_.ProcessName -eq 'LiveCodingConsole'} | Stop-Process -Force`
- 引擎:`G:\Epic\UE_5.7`;工程:`D:\CODE\NetEase Training\Game\UE\Graytail\Graytail.uproject`
- build/headless 的 PowerShell 调用需 `dangerouslyDisableSandbox=true`(要写 Binaries/Intermediate)
- 编译命令(本计划统一引用为 **[BUILD]**):
  ```
  & "G:\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat" GraytailEditor Win64 Development "-Project=D:\CODE\NetEase Training\Game\UE\Graytail\Graytail.uproject" -WaitMutex 2>&1 | Tee-Object D:\CODE\.build_out.txt | Select-Object -Last 6
  ```
  期望:末尾出现 `Build succeeded`,无 `error C####` / `error LNK####`。
- 163 冒烟(本计划统一引用为 **[SMOKE]**):
  ```
  & "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\CODE\NetEase Training\Game\UE\Graytail\Graytail.uproject" -run=GT_RuntimeSmokeRunner -unattended -nopause -nosplash
  ```
  期望日志含 `Overall=Pass`、`Pass=163`、`Fail=0`。

---

## File Structure

| 文件 | 责任 | 任务 |
|------|------|------|
| `Source/Graytail/Domains/Meta/GT_MetaTypes.h` | 局外持久状态 + 查询结果 USTRUCT | T1 |
| `Source/Graytail/Domains/Meta/GT_MetaCatalog.h` | 装备/天赋/消耗品定义(声明 + 查询) | T2 |
| `Source/Graytail/Domains/Meta/GT_MetaCatalog.cpp` | 定义数值静态表(照搬 Balance) | T2 |
| `Source/Graytail/Save/GT_MetaSaveGame.h/.cpp` | `USaveGame` 数据容器 | T3 |
| `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h` | 子系统类声明(随任务增长) | T4-T8 |
| `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp` | 子系统实现(随任务增长) | T4-T8 |
| `Source/Graytail/Debug/GT_MetaDebugCommands.cpp` | `gt.Meta*` 控制台命令(新文件,不挤进 950 行的 GT_EditorManualPlayCommands.cpp) | T9 |

> **对 spec 的细化:** spec §8 写"改 GT_EditorManualPlayCommands.cpp"。该文件已近千行,本计划改为**新建** `GT_MetaDebugCommands.cpp`(命令自带 `FindMetaSubsystem` 帮手,不依赖该文件的 run-snapshot 机制),职责更聚焦。子系统方法在 S1 一律用**普通 C++ 方法**(非 UFUNCTION),避免 UHT exec 桩对"声明未定义"报链接错;S5 需要时再加 UFUNCTION。

---

## Task 1: 局外状态数据类型

**Files:**
- Create: `Source/Graytail/Domains/Meta/GT_MetaTypes.h`

- [ ] **Step 1: 创建 GT_MetaTypes.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GT_MetaTypes.generated.h"

// 仓库里一条物品(回收物/装备/消耗品统一表示, Source 区分来源)。对齐 Lua warehouse.items[id]。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_WarehouseEntry
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId = NAME_None;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() int32 Value = 0;          // 单件价值(出售用)
	UPROPERTY() FName Source = NAME_None; // recovered / equipment / consumable(仅 recovered 可卖)
	UPROPERTY() bool bUnique = false;
};

// 抢救/回收累计摘要(对齐 Lua data.recovery)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RecoverySummary
{
	GENERATED_BODY()

	UPROPERTY() int32 TotalItems = 0;
	UPROPERTY() int32 TotalValue = 0;
	UPROPERTY() int32 TotalExtractionsWithItems = 0;
	UPROPERTY() TArray<FName> RecentItemIds;   // 最近带回, 上限 5
};

// 统计(对齐 Lua data.stats)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MetaStats
{
	GENERATED_BODY()

	UPROPERTY() int32 TotalRuns = 0;
	UPROPERTY() int32 TotalExtractions = 0;
	UPROPERTY() int32 TotalGoldEarned = 0;
};

// 局外持久状态全集(对齐 Lua MetaProgress 的 data 表)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MetaProgressState
{
	GENERATED_BODY()

	UPROPERTY() int32 Gold = 0;
	UPROPERTY() TArray<FName> OwnedItems;          // 已购装备 id
	UPROPERTY() TArray<FName> EquippedItems;       // 已装备 id, 上限 2, 有序
	UPROPERTY() TArray<FName> UnlockedTalents;     // 已解锁天赋 id
	UPROPERTY() TArray<FGT_WarehouseEntry> Warehouse;
	UPROPERTY() TMap<FName, int32> ConsumableStock;     // 消耗品库存 id->数量
	UPROPERTY() TMap<FName, int32> LoadoutConsumables;  // 选定带入 id->数量
	UPROPERTY() FGT_RecoverySummary Recovery;
	UPROPERTY() FGT_MetaStats Stats;
};

// 一件回收物入账(结算 RecordExtractionReward 的元素, 由 S2 从局内背包构造)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RewardItem
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId = NAME_None;
	UPROPERTY() int32 Count = 1;
	UPROPERTY() int32 Value = 0;
	UPROPERTY() FName DisplayName = NAME_None;  // 可空, 仅展示用
};

// 撤离结算输入(对齐 Lua RecordExtractionReward 的 reward 参数关键字段)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ExtractionReward
{
	GENERATED_BODY()

	UPROPERTY() int32 DirectGold = 0;       // 已结算/安全金币
	UPROPERTY() int32 LoosePartsGold = 0;   // 零散零件折算金币
	UPROPERTY() TArray<FGT_RewardItem> CarriedItems;
};

// GetEquipBonus 返回(对齐 Lua 同名函数字段)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_EquipBonus
{
	GENERATED_BODY()

	UPROPERTY() int32 BonusHP = 0;
	UPROPERTY() int32 BonusPower = 0;
	UPROPERTY() bool bMineImmunity = false;
	UPROPERTY() int32 MineDmgReduce = 0;
	UPROPERTY() bool bShowExitHint = false;
	UPROPERTY() int32 SearchBonus = 0;      // 百分比
};

// GetTalentEffects 返回(对齐 Lua 同名函数字段)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_TalentEffects
{
	GENERATED_BODY()

	UPROPERTY() int32 MineDmgReduce = 0;
	UPROPERTY() int32 MonsterFleeBonus = 0;
	UPROPERTY() int32 FailureGoldBonus = 0;
	UPROPERTY() int32 TradePrice = 15;      // 默认 NPC 交易价
	UPROPERTY() bool bMapHighlight = false;
};
```

- [ ] **Step 2: [BUILD]** — UHT 处理新头并编译 `.gen.cpp`。期望 `Build succeeded`,无 error。

- [ ] **Step 3: Commit**

```bash
git add UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaTypes.h
git commit -F <utf8-msg>   # 见“提交约定”
```
提交正文:`feat: 局外元进度 S1 数据类型(FGT_MetaProgressState 等)`

---

## Task 2: 定义目录 GT_MetaCatalog

**Files:**
- Create: `Source/Graytail/Domains/Meta/GT_MetaCatalog.h`
- Create: `Source/Graytail/Domains/Meta/GT_MetaCatalog.cpp`

- [ ] **Step 1: 创建 GT_MetaCatalog.h**

```cpp
#pragma once

#include "CoreMinimal.h"

// 局外定义目录(C++ 静态表)。数值照搬 Lua Balance.shop / Balance.talents /
// MetaProgress.ITEMS/TALENTS/CONSUMABLES。纯 C++ 结构(不反射), 仅内部使用。
// 回收物的 UGT_ItemDef 资产体系与此无关, 不受影响。

struct FGT_EquipDef
{
	FName Id;
	const TCHAR* DisplayName;
	int32 Price;
	// 加成(累加进 FGT_EquipBonus)
	int32 BonusHP = 0;
	int32 BonusPower = 0;
	bool bMineImmunity = false;
	int32 MineDmgReduce = 0;
	bool bShowExitHint = false;
	int32 SearchBonus = 0;
};

struct FGT_TalentDef
{
	FName Id;
	const TCHAR* DisplayName;
	int32 Price;
	// 效果(写进 FGT_TalentEffects)
	int32 MineDmgReduce = 0;
	int32 MonsterFleeBonus = 0;
	int32 FailureGoldBonus = 0;
	int32 TradePrice = 0;       // 0 = 不改默认 15
	bool bMapHighlight = false;
};

struct FGT_ConsumableDef
{
	FName Id;
	const TCHAR* DisplayName;
	int32 Price;
	int32 Heal = 0;
	int32 MaxCarry = 1;
};

namespace GT_MetaCatalog
{
	const TArray<FGT_EquipDef>& GetEquipDefs();
	const TArray<FGT_TalentDef>& GetTalentDefs();
	const TArray<FGT_ConsumableDef>& GetConsumableDefs();

	const FGT_EquipDef* FindEquip(FName Id);
	const FGT_TalentDef* FindTalent(FName Id);
	const FGT_ConsumableDef* FindConsumable(FName Id);

	// 上限常量(对齐 Lua MAX_EQUIPPED / RECENT_RECOVERY_MAX)。
	constexpr int32 MaxEquipped = 2;
	constexpr int32 RecentRecoveryMax = 5;
}
```

- [ ] **Step 2: 创建 GT_MetaCatalog.cpp**

```cpp
#include "Domains/Meta/GT_MetaCatalog.h"

namespace GT_MetaCatalog
{
	const TArray<FGT_EquipDef>& GetEquipDefs()
	{
		// 数值: Balance.shop(price/bonusHP/bonusPower/mineDmgReduce) + GetEquipBonus 语义。
		static const TArray<FGT_EquipDef> Defs = {
			// Id,                                DisplayName,  Price, HP, Pow, Imm,  MineRed, ExitHint, Search
			{ FName(TEXT("armor")),            TEXT("防护背心"), 110,  20,  0, false,  0, false,  0 },
			{ FName(TEXT("whetstone")),        TEXT("磨刀石"),    90,   0,  2, false,  0, false,  0 },
			{ FName(TEXT("medkit")),           TEXT("急救包"),   120,   0,  0, true,   0, false,  0 },
			{ FName(TEXT("insulated_gloves")), TEXT("绝缘套"),   140,   0,  0, false, 10, false,  0 },
			{ FName(TEXT("compass")),          TEXT("罗盘"),     160,   0,  0, false,  0, true,   0 },
			{ FName(TEXT("backpack")),         TEXT("大背包"),   220,   0,  0, false,  0, false, 50 },
		};
		return Defs;
	}

	const TArray<FGT_TalentDef>& GetTalentDefs()
	{
		// 数值: Balance.talents(price) + GetTalentEffects 语义。
		static const TArray<FGT_TalentDef> Defs = {
			// Id,                          DisplayName,  Price, MineRed, Flee, FailGold, Trade, MapHi
			{ FName(TEXT("talent_map")),     TEXT("邻域感知"), 100,  0, 0,  0,  0, true  },
			{ FName(TEXT("talent_mine")),    TEXT("厚皮"),     120, 10, 0,  0,  0, false },
			{ FName(TEXT("talent_monster")), TEXT("威压"),     120,  0, 2,  0,  0, false },
			{ FName(TEXT("talent_extract")), TEXT("抢救条款"), 140,  0, 0, 10,  0, false },
			{ FName(TEXT("talent_event")),   TEXT("议价"),     140,  0, 0,  0, 20, false },
		};
		return Defs;
	}

	const TArray<FGT_ConsumableDef>& GetConsumableDefs()
	{
		static const TArray<FGT_ConsumableDef> Defs = {
			{ FName(TEXT("emergency_bandage")), TEXT("应急止血贴"), 12, 25, 3 },
		};
		return Defs;
	}

	const FGT_EquipDef* FindEquip(FName Id)
	{
		return GetEquipDefs().FindByPredicate([Id](const FGT_EquipDef& D) { return D.Id == Id; });
	}

	const FGT_TalentDef* FindTalent(FName Id)
	{
		return GetTalentDefs().FindByPredicate([Id](const FGT_TalentDef& D) { return D.Id == Id; });
	}

	const FGT_ConsumableDef* FindConsumable(FName Id)
	{
		return GetConsumableDefs().FindByPredicate([Id](const FGT_ConsumableDef& D) { return D.Id == Id; });
	}
}
```

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: Commit** — 正文:`feat: 局外元进度 S1 定义目录 GT_MetaCatalog(6装备/5天赋/1消耗品)`

---

## Task 3: 存档容器 UGT_MetaSaveGame

**Files:**
- Create: `Source/Graytail/Save/GT_MetaSaveGame.h`
- Create: `Source/Graytail/Save/GT_MetaSaveGame.cpp`

- [ ] **Step 1: 创建 GT_MetaSaveGame.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "GT_MetaSaveGame.generated.h"

// 局外元进度存档容器。slot "GraytailMeta", user index 0。
UCLASS(BlueprintType)
class GRAYTAIL_API UGT_MetaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FGT_MetaProgressState State;
	UPROPERTY() int32 SaveVersion = 1;   // 预留迁移
};
```

- [ ] **Step 2: 创建 GT_MetaSaveGame.cpp**

```cpp
#include "Save/GT_MetaSaveGame.h"
```

> （纯数据容器, 无方法体;.cpp 保留以与项目其它类一致并确保 .gen 链接。）

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: Commit** — 正文:`feat: 局外元进度 S1 存档容器 UGT_MetaSaveGame`

---

## Task 4: 子系统骨架 + 状态 + 存读档 + 币 + GM

**Files:**
- Create: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Create: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`

- [ ] **Step 1: 创建 GT_MetaProgressSubsystem.h(本任务版本)**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "GT_MetaProgressSubsystem.generated.h"

// 局外元进度子系统:持久状态 + 操作 API + 局内加成查询。
// 操作走公有 C++ 方法(不走局内 Command 管线, 因无 RunContext)。
UCLASS()
class GRAYTAIL_API UGT_MetaProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- 存读档 ---
	void Save();
	void Load();

	// --- 币(对齐 Lua GetGold/AddGold/SpendGold) ---
	int32 GetGold() const { return State.Gold; }
	void AddGold(int32 Amount);
	bool SpendGold(int32 Amount);

	// --- GM 调试(免费, 不扣币) ---
	void GMGrantItem(FName ItemId);
	void GMGrantTalent(FName TalentId);
	void GMEquipAll();
	void GMUnequipAll();
	void GMReset();

	// 只读访问(调试命令打印用)
	const FGT_MetaProgressState& GetState() const { return State; }

private:
	FGT_MetaProgressState State;

	static const TCHAR* SaveSlotName() { return TEXT("GraytailMeta"); }
	static constexpr int32 SaveUserIndex = 0;

	// 读档后清洗(对齐 Lua Load 的校验): 已装备项必须确实拥有; loadout 数量夹到库存。
	void SanitizeAfterLoad();

	static int32 ClampNonNegative(int32 V) { return FMath::Max(0, V); }
};
```

- [ ] **Step 2: 创建 GT_MetaProgressSubsystem.cpp(本任务版本)**

```cpp
#include "Domains/Meta/GT_MetaProgressSubsystem.h"

#include "Domains/Meta/GT_MetaCatalog.h"
#include "Save/GT_MetaSaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailMeta, Log, All);

void UGT_MetaProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

void UGT_MetaProgressSubsystem::Save()
{
	UGT_MetaSaveGame* SaveObj = Cast<UGT_MetaSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UGT_MetaSaveGame::StaticClass()));
	if (!SaveObj)
	{
		UE_LOG(LogGraytailMeta, Warning, TEXT("Save: failed to create save object."));
		return;
	}
	SaveObj->State = State;
	if (!UGameplayStatics::SaveGameToSlot(SaveObj, SaveSlotName(), SaveUserIndex))
	{
		UE_LOG(LogGraytailMeta, Warning, TEXT("Save: SaveGameToSlot failed."));
		return;
	}
	UE_LOG(LogGraytailMeta, Log, TEXT("Saved: gold=%d"), State.Gold);
}

void UGT_MetaProgressSubsystem::Load()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName(), SaveUserIndex))
	{
		State = FGT_MetaProgressState();   // 全新起始
		UE_LOG(LogGraytailMeta, Log, TEXT("Load: no save, starting fresh."));
		return;
	}
	UGT_MetaSaveGame* SaveObj = Cast<UGT_MetaSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName(), SaveUserIndex));
	if (!SaveObj)
	{
		State = FGT_MetaProgressState();
		UE_LOG(LogGraytailMeta, Warning, TEXT("Load: slot exists but load failed; starting fresh."));
		return;
	}
	State = SaveObj->State;
	SanitizeAfterLoad();
	UE_LOG(LogGraytailMeta, Log, TEXT("Loaded: gold=%d"), State.Gold);
}

void UGT_MetaProgressSubsystem::SanitizeAfterLoad()
{
	State.Gold = ClampNonNegative(State.Gold);

	// 已装备项必须确实拥有(对齐 Lua 校验)。
	State.EquippedItems.RemoveAll([this](const FName& Id)
	{
		return !State.OwnedItems.Contains(Id);
	});
	// 上限保护。
	if (State.EquippedItems.Num() > GT_MetaCatalog::MaxEquipped)
	{
		State.EquippedItems.SetNum(GT_MetaCatalog::MaxEquipped);
	}
	// loadout 数量夹到库存且不超 maxCarry, 去掉未知/<=0。
	for (auto It = State.LoadoutConsumables.CreateIterator(); It; ++It)
	{
		const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(It.Key());
		const int32 Stock = State.ConsumableStock.FindRef(It.Key());
		int32 Count = FMath::Min(It.Value(), Stock);
		if (Def && Def->MaxCarry > 0)
		{
			Count = FMath::Min(Count, Def->MaxCarry);
		}
		if (!Def || Count <= 0)
		{
			It.RemoveCurrent();
		}
		else
		{
			It.Value() = Count;
		}
	}
}

void UGT_MetaProgressSubsystem::AddGold(int32 Amount)
{
	Amount = ClampNonNegative(Amount);
	if (Amount <= 0) { return; }
	State.Gold += Amount;
	State.Stats.TotalGoldEarned += Amount;
	Save();
}

bool UGT_MetaProgressSubsystem::SpendGold(int32 Amount)
{
	Amount = ClampNonNegative(Amount);
	if (Amount <= 0) { return false; }
	if (State.Gold < Amount) { return false; }
	State.Gold -= Amount;
	Save();
	return true;
}

void UGT_MetaProgressSubsystem::GMGrantItem(FName ItemId)
{
	State.OwnedItems.AddUnique(ItemId);
	Save();
}

void UGT_MetaProgressSubsystem::GMGrantTalent(FName TalentId)
{
	State.UnlockedTalents.AddUnique(TalentId);
	Save();
}

void UGT_MetaProgressSubsystem::GMEquipAll()
{
	State.EquippedItems.Reset();
	for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
	{
		if (State.OwnedItems.Contains(Def.Id))
		{
			State.EquippedItems.Add(Def.Id);
		}
	}
	Save();
}

void UGT_MetaProgressSubsystem::GMUnequipAll()
{
	State.EquippedItems.Reset();
	Save();
}

void UGT_MetaProgressSubsystem::GMReset()
{
	State = FGT_MetaProgressState();
	Save();
}
```

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: [SMOKE]** — 期望 `Overall=Pass / Pass=163 / Fail=0`(S1 不碰局内,应零影响)。

- [ ] **Step 5: Commit** — 正文:`feat: 局外元进度 S1 子系统骨架(状态/存读档/币/GM)`

---

## Task 5: 装备 + 天赋操作

**Files:**
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`(在 `// --- 币` 段后插入)
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`(追加方法体)

- [ ] **Step 1: 头文件追加声明(放在 GM 段之前)**

```cpp
	// --- 装备(对齐 Lua OwnsItem/BuyItem/ToggleEquip/IsEquipped/GetEquippedItems) ---
	bool OwnsItem(FName ItemId) const { return State.OwnedItems.Contains(ItemId); }
	// 返回 false 时 OutError = already_owned / not_found / not_enough_gold
	bool BuyItem(FName ItemId, FName& OutError);
	// 返回 false 时 OutError = not_owned / max_equipped
	bool ToggleEquip(FName ItemId, FName& OutError);
	bool IsEquipped(FName ItemId) const { return State.EquippedItems.Contains(ItemId); }
	const TArray<FName>& GetEquippedItems() const { return State.EquippedItems; }

	// --- 天赋(对齐 Lua HasTalent/UnlockTalent) ---
	bool HasTalent(FName TalentId) const { return State.UnlockedTalents.Contains(TalentId); }
	// 返回 false 时 OutError = already_unlocked / not_found / not_enough_gold
	bool UnlockTalent(FName TalentId, FName& OutError);
```

- [ ] **Step 2: cpp 追加方法体**

```cpp
bool UGT_MetaProgressSubsystem::BuyItem(FName ItemId, FName& OutError)
{
	if (State.OwnedItems.Contains(ItemId)) { OutError = TEXT("already_owned"); return false; }
	const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(ItemId);
	if (!Def) { OutError = TEXT("not_found"); return false; }
	if (State.Gold < Def->Price) { OutError = TEXT("not_enough_gold"); return false; }
	State.Gold -= Def->Price;
	State.OwnedItems.AddUnique(ItemId);
	Save();
	return true;
}

bool UGT_MetaProgressSubsystem::ToggleEquip(FName ItemId, FName& OutError)
{
	if (!State.OwnedItems.Contains(ItemId)) { OutError = TEXT("not_owned"); return false; }
	const int32 Index = State.EquippedItems.IndexOfByKey(ItemId);
	if (Index != INDEX_NONE)
	{
		State.EquippedItems.RemoveAt(Index);   // 已装备 -> 卸下
		Save();
		return true;
	}
	if (State.EquippedItems.Num() >= GT_MetaCatalog::MaxEquipped)
	{
		OutError = TEXT("max_equipped");
		return false;
	}
	State.EquippedItems.Add(ItemId);
	Save();
	return true;
}

bool UGT_MetaProgressSubsystem::UnlockTalent(FName TalentId, FName& OutError)
{
	if (State.UnlockedTalents.Contains(TalentId)) { OutError = TEXT("already_unlocked"); return false; }
	const FGT_TalentDef* Def = GT_MetaCatalog::FindTalent(TalentId);
	if (!Def) { OutError = TEXT("not_found"); return false; }
	if (State.Gold < Def->Price) { OutError = TEXT("not_enough_gold"); return false; }
	State.Gold -= Def->Price;
	State.UnlockedTalents.AddUnique(TalentId);
	Save();
	return true;
}
```

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: Commit** — 正文:`feat: 局外元进度 S1 装备/天赋买卖装备解锁操作`

---

## Task 6: 消耗品库存 + loadout 操作

**Files:**
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`

- [ ] **Step 1: 头文件追加声明**

```cpp
	// --- 消耗品库存(对齐 Lua GetConsumableCount/Buy/Add/Remove) ---
	int32 GetConsumableCount(FName ItemId) const { return State.ConsumableStock.FindRef(ItemId); }
	bool BuyConsumable(FName ItemId, int32 Count, FName& OutError);  // not_found/invalid_count/not_enough_gold
	bool AddConsumable(FName ItemId, int32 Count);
	bool RemoveConsumable(FName ItemId, int32 Count);               // false = not_enough

	// --- loadout(对齐 Lua SetLoadoutConsumable/GetLoadout/ConsumeLoadoutForRun) ---
	// 把 Count 夹到 [0, min(stock, maxCarry)]; 0 表示移除。
	void SetLoadoutConsumable(FName ItemId, int32 Count);
	const TMap<FName, int32>& GetLoadout() const { return State.LoadoutConsumables; }
	// 开局调:从库存扣掉 loadout 选定量, 返回本局携带的 id->数量(S3 用)。
	TMap<FName, int32> ConsumeLoadoutForRun();
```

- [ ] **Step 2: cpp 追加方法体**

```cpp
bool UGT_MetaProgressSubsystem::BuyConsumable(FName ItemId, int32 Count, FName& OutError)
{
	const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(ItemId);
	if (!Def) { OutError = TEXT("not_found"); return false; }
	Count = ClampNonNegative(Count);
	if (Count <= 0) { OutError = TEXT("invalid_count"); return false; }
	const int32 Price = Def->Price * Count;
	if (State.Gold < Price) { OutError = TEXT("not_enough_gold"); return false; }
	State.Gold -= Price;
	State.ConsumableStock.FindOrAdd(ItemId) += Count;
	Save();
	return true;
}

bool UGT_MetaProgressSubsystem::AddConsumable(FName ItemId, int32 Count)
{
	if (!GT_MetaCatalog::FindConsumable(ItemId)) { return false; }
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	State.ConsumableStock.FindOrAdd(ItemId) += Count;
	Save();
	return true;
}

bool UGT_MetaProgressSubsystem::RemoveConsumable(FName ItemId, int32 Count)
{
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	int32* Stock = State.ConsumableStock.Find(ItemId);
	if (!Stock || *Stock < Count) { return false; }
	*Stock -= Count;
	if (*Stock <= 0) { State.ConsumableStock.Remove(ItemId); }
	// loadout 不能超过剩余库存
	if (int32* Loadout = State.LoadoutConsumables.Find(ItemId))
	{
		const int32 Remaining = State.ConsumableStock.FindRef(ItemId);
		if (*Loadout > Remaining) { *Loadout = Remaining; }
		if (*Loadout <= 0) { State.LoadoutConsumables.Remove(ItemId); }
	}
	Save();
	return true;
}

void UGT_MetaProgressSubsystem::SetLoadoutConsumable(FName ItemId, int32 Count)
{
	const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(ItemId);
	if (!Def) { return; }
	Count = ClampNonNegative(Count);
	const int32 Stock = State.ConsumableStock.FindRef(ItemId);
	Count = FMath::Min(Count, Stock);
	if (Def->MaxCarry > 0) { Count = FMath::Min(Count, Def->MaxCarry); }
	if (Count <= 0) { State.LoadoutConsumables.Remove(ItemId); }
	else { State.LoadoutConsumables.Add(ItemId, Count); }
	Save();
}

TMap<FName, int32> UGT_MetaProgressSubsystem::ConsumeLoadoutForRun()
{
	TMap<FName, int32> RunLoadout;
	for (const TPair<FName, int32>& Pair : State.LoadoutConsumables)
	{
		const int32 Stock = State.ConsumableStock.FindRef(Pair.Key);
		const int32 Take = FMath::Min(Pair.Value, Stock);
		if (Take > 0)
		{
			RunLoadout.Add(Pair.Key, Take);
			int32& S = State.ConsumableStock.FindOrAdd(Pair.Key);
			S -= Take;
			if (S <= 0) { State.ConsumableStock.Remove(Pair.Key); }
		}
	}
	// 库存变了, 重新夹 loadout。
	SanitizeAfterLoad();
	Save();
	return RunLoadout;
}
```

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: Commit** — 正文:`feat: 局外元进度 S1 消耗品库存与 loadout 操作`

---

## Task 7: 仓库 + 统计 + 结算 API

**Files:**
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`

- [ ] **Step 1: 头文件追加声明**

```cpp
	// --- 仓库(对齐 Lua AddWarehouseItems/CanSell/Sell/Remove/Count) ---
	void AddWarehouseItems(const TArray<FGT_RewardItem>& Items, FName Source);
	int32 GetWarehouseItemCount(FName ItemId) const;
	bool CanSellItem(FName ItemId, FName& OutReason) const; // not_owned/unique/not_sellable/equipped/no_value
	bool SellWarehouseItem(FName ItemId, int32 Count, int32& OutGold, FName& OutError);
	bool RemoveWarehouseItem(FName ItemId, int32 Count);
	const TArray<FGT_WarehouseEntry>& GetWarehouse() const { return State.Warehouse; }

	// --- 统计 / 结算(RecordExtractionReward 由 S2 调) ---
	void RecordRun();
	void RecordExtractionReward(const FGT_ExtractionReward& Reward);
	const FGT_MetaStats& GetStats() const { return State.Stats; }
	const FGT_RecoverySummary& GetRecoverySummary() const { return State.Recovery; }

private:
	FGT_WarehouseEntry* FindWarehouse(FName ItemId);
	const FGT_WarehouseEntry* FindWarehouse(FName ItemId) const;
```

> 注意:上面末尾把 `private:` 段重开了。实现时把这些私有帮手放进已有 private 区即可(别重复 `private:` 关键字两次造成困惑——把声明并入 Task 4 的 private 块)。

- [ ] **Step 2: cpp 追加方法体**

```cpp
FGT_WarehouseEntry* UGT_MetaProgressSubsystem::FindWarehouse(FName ItemId)
{
	return State.Warehouse.FindByPredicate([ItemId](const FGT_WarehouseEntry& E) { return E.ItemId == ItemId; });
}

const FGT_WarehouseEntry* UGT_MetaProgressSubsystem::FindWarehouse(FName ItemId) const
{
	return State.Warehouse.FindByPredicate([ItemId](const FGT_WarehouseEntry& E) { return E.ItemId == ItemId; });
}

void UGT_MetaProgressSubsystem::AddWarehouseItems(const TArray<FGT_RewardItem>& Items, FName Source)
{
	for (const FGT_RewardItem& Item : Items)
	{
		const int32 Count = ClampNonNegative(Item.Count);
		if (Item.ItemId.IsNone() || Count <= 0) { continue; }
		FGT_WarehouseEntry* Entry = FindWarehouse(Item.ItemId);
		if (!Entry)
		{
			Entry = &State.Warehouse.AddDefaulted_GetRef();
			Entry->ItemId = Item.ItemId;
			Entry->Value = Item.Value;
			Entry->Source = Source.IsNone() ? FName(TEXT("recovered")) : Source;
		}
		Entry->Count += Count;
	}
	// 调用方(RecordExtractionReward)负责 Save, 避免重复落盘。
}

int32 UGT_MetaProgressSubsystem::GetWarehouseItemCount(FName ItemId) const
{
	const FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	return Entry ? Entry->Count : 0;
}

bool UGT_MetaProgressSubsystem::CanSellItem(FName ItemId, FName& OutReason) const
{
	const FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	if (!Entry || Entry->Count <= 0) { OutReason = TEXT("not_owned"); return false; }
	if (Entry->bUnique) { OutReason = TEXT("unique"); return false; }
	if (Entry->Source != FName(TEXT("recovered"))) { OutReason = TEXT("not_sellable"); return false; }
	if (IsEquipped(ItemId)) { OutReason = TEXT("equipped"); return false; }
	if (Entry->Value <= 0) { OutReason = TEXT("no_value"); return false; }
	return true;
}

bool UGT_MetaProgressSubsystem::RemoveWarehouseItem(FName ItemId, int32 Count)
{
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	if (!Entry || Entry->Count < Count) { return false; }
	Entry->Count -= Count;
	if (Entry->Count <= 0)
	{
		State.Warehouse.RemoveAll([ItemId](const FGT_WarehouseEntry& E) { return E.ItemId == ItemId; });
	}
	return true;
}

bool UGT_MetaProgressSubsystem::SellWarehouseItem(FName ItemId, int32 Count, int32& OutGold, FName& OutError)
{
	Count = ClampNonNegative(Count);
	if (Count <= 0) { OutError = TEXT("invalid_count"); return false; }
	if (!CanSellItem(ItemId, OutError)) { return false; }
	const FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	if (!Entry || Entry->Count < Count) { OutError = TEXT("not_enough"); return false; }
	OutGold = Entry->Value * Count;
	if (!RemoveWarehouseItem(ItemId, Count)) { OutError = TEXT("remove_failed"); return false; }
	State.Gold += OutGold;
	State.Stats.TotalGoldEarned += OutGold;
	Save();
	return true;
}

void UGT_MetaProgressSubsystem::RecordRun()
{
	State.Stats.TotalRuns += 1;
	Save();
}

void UGT_MetaProgressSubsystem::RecordExtractionReward(const FGT_ExtractionReward& Reward)
{
	const int32 GoldAdded = ClampNonNegative(Reward.DirectGold) + ClampNonNegative(Reward.LoosePartsGold);
	int32 ItemCount = 0;
	int32 ItemValue = 0;
	for (const FGT_RewardItem& Item : Reward.CarriedItems)
	{
		const int32 C = ClampNonNegative(Item.Count);
		ItemCount += C;
		ItemValue += Item.Value * C;
	}

	State.Gold += GoldAdded;
	State.Stats.TotalGoldEarned += GoldAdded;
	State.Stats.TotalExtractions += 1;

	if (ItemCount > 0 || ItemValue > 0)
	{
		State.Recovery.TotalItems += ItemCount;
		State.Recovery.TotalValue += ItemValue;
		State.Recovery.TotalExtractionsWithItems += 1;
		// 最近带回(最新在前, 上限 RecentRecoveryMax)。
		for (const FGT_RewardItem& Item : Reward.CarriedItems)
		{
			for (int32 i = 0; i < ClampNonNegative(Item.Count); ++i)
			{
				State.Recovery.RecentItemIds.Insert(Item.ItemId, 0);
			}
		}
		if (State.Recovery.RecentItemIds.Num() > GT_MetaCatalog::RecentRecoveryMax)
		{
			State.Recovery.RecentItemIds.SetNum(GT_MetaCatalog::RecentRecoveryMax);
		}
		AddWarehouseItems(Reward.CarriedItems, FName(TEXT("recovered")));
	}
	Save();
}
```

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: Commit** — 正文:`feat: 局外元进度 S1 仓库/统计/结算 API`

---

## Task 8: 局内加成查询 GetEquipBonus / GetTalentEffects

**Files:**
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Modify: `Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`

- [ ] **Step 1: 头文件追加声明**

```cpp
	// --- 局内加成查询(S3 开局读) ---
	FGT_EquipBonus GetEquipBonus() const;       // 对齐 Lua GetEquipBonus
	FGT_TalentEffects GetTalentEffects() const; // 对齐 Lua GetTalentEffects
```

- [ ] **Step 2: cpp 追加方法体**

```cpp
FGT_EquipBonus UGT_MetaProgressSubsystem::GetEquipBonus() const
{
	FGT_EquipBonus Bonus;
	for (const FName& ItemId : State.EquippedItems)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(ItemId);
		if (!Def) { continue; }
		Bonus.BonusHP += Def->BonusHP;
		Bonus.BonusPower += Def->BonusPower;
		Bonus.bMineImmunity |= Def->bMineImmunity;
		Bonus.MineDmgReduce += Def->MineDmgReduce;
		Bonus.bShowExitHint |= Def->bShowExitHint;
		Bonus.SearchBonus += Def->SearchBonus;
	}
	return Bonus;
}

FGT_TalentEffects UGT_MetaProgressSubsystem::GetTalentEffects() const
{
	FGT_TalentEffects Effects;   // TradePrice 默认 15
	for (const FName& TalentId : State.UnlockedTalents)
	{
		const FGT_TalentDef* Def = GT_MetaCatalog::FindTalent(TalentId);
		if (!Def) { continue; }
		Effects.MineDmgReduce += Def->MineDmgReduce;
		Effects.MonsterFleeBonus += Def->MonsterFleeBonus;
		Effects.FailureGoldBonus += Def->FailureGoldBonus;
		if (Def->TradePrice > 0) { Effects.TradePrice = Def->TradePrice; }
		Effects.bMapHighlight |= Def->bMapHighlight;
	}
	return Effects;
}
```

- [ ] **Step 3: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 4: Commit** — 正文:`feat: 局外元进度 S1 局内加成查询 GetEquipBonus/GetTalentEffects`

---

## Task 9: gt.Meta* 调试命令

**Files:**
- Create: `Source/Graytail/Debug/GT_MetaDebugCommands.cpp`

- [ ] **Step 1: 创建 GT_MetaDebugCommands.cpp**

```cpp
#include "CoreMinimal.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailMetaCmd, Log, All);

namespace
{
	// 取 Meta 子系统(带 GEngine 兜底, 适配无头 -game)。仿 GT_EditorManualPlayCommands 的 FindDebugSubsystem。
	UGT_MetaProgressSubsystem* FindMetaSubsystem(UWorld* World)
	{
		if (World)
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UGT_MetaProgressSubsystem* S = GI->GetSubsystem<UGT_MetaProgressSubsystem>())
				{
					return S;
				}
			}
		}
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.OwningGameInstance)
				{
					if (UGT_MetaProgressSubsystem* S = Ctx.OwningGameInstance->GetSubsystem<UGT_MetaProgressSubsystem>())
					{
						return S;
					}
				}
			}
		}
		return nullptr;
	}

	bool ParseInt(const TArray<FString>& Args, int32 Index, int32& Out)
	{
		return Args.IsValidIndex(Index) && LexTryParseString(Out, *Args[Index]);
	}

	FName ArgName(const TArray<FString>& Args, int32 Index)
	{
		return Args.IsValidIndex(Index) ? FName(*Args[Index]) : NAME_None;
	}

	void PrintState(UGT_MetaProgressSubsystem* S)
	{
		const FGT_MetaProgressState& St = S->GetState();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta: gold=%d owned=[%s] equipped=[%s] talents=[%s]"),
			St.Gold,
			*FString::JoinBy(St.OwnedItems, TEXT(","), [](const FName& N) { return N.ToString(); }),
			*FString::JoinBy(St.EquippedItems, TEXT(","), [](const FName& N) { return N.ToString(); }),
			*FString::JoinBy(St.UnlockedTalents, TEXT(","), [](const FName& N) { return N.ToString(); }));
		for (const FGT_WarehouseEntry& E : St.Warehouse)
		{
			UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta warehouse: %s x%d value=%d src=%s"),
				*E.ItemId.ToString(), E.Count, E.Value, *E.Source.ToString());
		}
		for (const TPair<FName, int32>& P : St.ConsumableStock)
		{
			UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta consumable: %s x%d (loadout=%d)"),
				*P.Key.ToString(), P.Value, St.LoadoutConsumables.FindRef(P.Key));
		}
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta stats: runs=%d extractions=%d goldEarned=%d recoveryItems=%d"),
			St.Stats.TotalRuns, St.Stats.TotalExtractions, St.Stats.TotalGoldEarned, St.Recovery.TotalItems);
	}

	void HandleMeta(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.Meta failed: no Meta subsystem (need game world).")); return; }
		PrintState(S);
	}

	void HandleMetaBuy(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaBuy failed: no Meta subsystem.")); return; }
		FName Err;
		const bool bOk = S->BuyItem(ArgName(Args, 0), Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBuy %s: %s%s"),
			*ArgName(Args, 0).ToString(), bOk ? TEXT("ok") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaEquip(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaEquip failed: no Meta subsystem.")); return; }
		FName Err;
		const bool bOk = S->ToggleEquip(ArgName(Args, 0), Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaEquip %s: %s%s"),
			*ArgName(Args, 0).ToString(), bOk ? TEXT("toggled") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaTalent(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaTalent failed: no Meta subsystem.")); return; }
		FName Err;
		const bool bOk = S->UnlockTalent(ArgName(Args, 0), Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaTalent %s: %s%s"),
			*ArgName(Args, 0).ToString(), bOk ? TEXT("unlocked") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaBuyConsumable(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaBuyConsumable failed: no Meta subsystem.")); return; }
		int32 Count = 1; ParseInt(Args, 1, Count);
		FName Err;
		const bool bOk = S->BuyConsumable(ArgName(Args, 0), Count, Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBuyConsumable %s x%d: %s%s"),
			*ArgName(Args, 0).ToString(), Count, bOk ? TEXT("ok") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaLoadout(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaLoadout failed: no Meta subsystem.")); return; }
		int32 Count = 0; ParseInt(Args, 1, Count);
		S->SetLoadoutConsumable(ArgName(Args, 0), Count);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaLoadout %s -> %d"), *ArgName(Args, 0).ToString(),
			S->GetLoadout().FindRef(ArgName(Args, 0)));
		PrintState(S);
	}

	void HandleMetaSell(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaSell failed: no Meta subsystem.")); return; }
		int32 Count = 1; ParseInt(Args, 1, Count);
		int32 Gold = 0; FName Err;
		const bool bOk = S->SellWarehouseItem(ArgName(Args, 0), Count, Gold, Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaSell %s x%d: %s%s gold+=%d"),
			*ArgName(Args, 0).ToString(), Count, bOk ? TEXT("ok") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString(), bOk ? Gold : 0);
		PrintState(S);
	}

	void HandleMetaBonus(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaBonus failed: no Meta subsystem.")); return; }
		const FGT_EquipBonus B = S->GetEquipBonus();
		const FGT_TalentEffects T = S->GetTalentEffects();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBonus equip: HP+%d Power+%d MineImmunity=%d MineDmgReduce=%d ExitHint=%d Search+%d%%"),
			B.BonusHP, B.BonusPower, B.bMineImmunity ? 1 : 0, B.MineDmgReduce, B.bShowExitHint ? 1 : 0, B.SearchBonus);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBonus talent: MineDmgReduce=%d Flee+%d FailGold+%d TradePrice=%d MapHighlight=%d"),
			T.MineDmgReduce, T.MonsterFleeBonus, T.FailureGoldBonus, T.TradePrice, T.bMapHighlight ? 1 : 0);
	}

	void HandleMetaAddGold(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaAddGold failed: no Meta subsystem.")); return; }
		int32 Amount = 0; ParseInt(Args, 0, Amount);
		S->AddGold(Amount);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaAddGold %d -> gold=%d"), Amount, S->GetGold());
	}

	void HandleMetaReset(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaReset failed: no Meta subsystem.")); return; }
		S->GMReset();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaReset: cleared."));
	}

	void HandleMetaSave(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaSave failed: no Meta subsystem.")); return; }
		S->Save();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaSave: written."));
	}

	void HandleMetaLoad(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaLoad failed: no Meta subsystem.")); return; }
		S->Load();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaLoad: reloaded."));
		PrintState(S);
	}

	FAutoConsoleCommandWithWorldAndArgs GTMetaCmd(TEXT("gt.Meta"), TEXT("打印局外元进度全状态"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMeta));
	FAutoConsoleCommandWithWorldAndArgs GTMetaBuyCmd(TEXT("gt.MetaBuy"), TEXT("买装备: gt.MetaBuy <id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaBuy));
	FAutoConsoleCommandWithWorldAndArgs GTMetaEquipCmd(TEXT("gt.MetaEquip"), TEXT("装备/卸下: gt.MetaEquip <id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaEquip));
	FAutoConsoleCommandWithWorldAndArgs GTMetaTalentCmd(TEXT("gt.MetaTalent"), TEXT("解锁天赋: gt.MetaTalent <id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaTalent));
	FAutoConsoleCommandWithWorldAndArgs GTMetaBuyConsumableCmd(TEXT("gt.MetaBuyConsumable"), TEXT("买消耗品: gt.MetaBuyConsumable <id> [n]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaBuyConsumable));
	FAutoConsoleCommandWithWorldAndArgs GTMetaLoadoutCmd(TEXT("gt.MetaLoadout"), TEXT("配 loadout: gt.MetaLoadout <id> <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaLoadout));
	FAutoConsoleCommandWithWorldAndArgs GTMetaSellCmd(TEXT("gt.MetaSell"), TEXT("卖仓库物品: gt.MetaSell <id> [n]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaSell));
	FAutoConsoleCommandWithWorldAndArgs GTMetaBonusCmd(TEXT("gt.MetaBonus"), TEXT("打印装备/天赋加成"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaBonus));
	FAutoConsoleCommandWithWorldAndArgs GTMetaAddGoldCmd(TEXT("gt.MetaAddGold"), TEXT("GM加币: gt.MetaAddGold <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaAddGold));
	FAutoConsoleCommandWithWorldAndArgs GTMetaResetCmd(TEXT("gt.MetaReset"), TEXT("GM清空存档"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaReset));
	FAutoConsoleCommandWithWorldAndArgs GTMetaSaveCmd(TEXT("gt.MetaSave"), TEXT("强制存档"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaSave));
	FAutoConsoleCommandWithWorldAndArgs GTMetaLoadCmd(TEXT("gt.MetaLoad"), TEXT("强制读档"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaLoad));
}
```

- [ ] **Step 2: [BUILD]** — 期望 `Build succeeded`,无 error。

- [ ] **Step 3: Commit** — 正文:`feat: 局外元进度 S1 gt.Meta* 调试命令`

---

## Task 10: 集成验证(全场景无头跑 + 持久化往返)

**Files:** 无代码改动(出问题则回到对应任务修)。

- [ ] **Step 1: [BUILD]** — 确认全量编译通过。

- [ ] **Step 2: [SMOKE]** — `Overall=Pass / Pass=163 / Fail=0`。

- [ ] **Step 3: 主场景无头跑(重置→加币→买装备→装备→天赋→买消耗品→配loadout→查加成→存档)**

先删旧档保证干净起点(slot 在工程 Saved/SaveGames 下):
```
Remove-Item "D:\CODE\NetEase Training\Game\UE\Graytail\Saved\SaveGames\GraytailMeta.sav" -ErrorAction SilentlyContinue
```
（注:若 Remove-Item 被路径保护拦,用 `gt.MetaReset` 作为起点即可。)

```
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\CODE\NetEase Training\Game\UE\Graytail\Graytail.uproject" -game -nullrhi -unattended -nopause -nosplash -ExecCmds="gt.MetaReset, gt.MetaAddGold 600, gt.MetaBuy armor, gt.MetaBuy whetstone, gt.MetaEquip armor, gt.MetaEquip whetstone, gt.MetaTalent talent_mine, gt.MetaBuyConsumable emergency_bandage 3, gt.MetaLoadout emergency_bandage 2, gt.MetaBonus, gt.Meta, gt.MetaSave, quit"
```
grep `UE/Graytail/Saved/Logs/Graytail.log` 的 `LogGraytailMetaCmd`,期望:
- `gt.MetaAddGold 600 -> gold=600`
- 买 armor(110)→ gold=490;买 whetstone(90)→ gold=400
- equipped 含 `armor,whetstone`;talents 含 `talent_mine`
- consumable: `emergency_bandage x3 (loadout=2)`
- `gt.MetaBonus equip: HP+20 Power+2 ... `(armor+whetstone)
- `gt.MetaBonus talent: MineDmgReduce=10 ...`(talent_mine)

- [ ] **Step 4: 持久化往返(第二次进程读档)**

```
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\CODE\NetEase Training\Game\UE\Graytail\Graytail.uproject" -game -nullrhi -unattended -nopause -nosplash -ExecCmds="gt.Meta, gt.MetaBonus, quit"
```
期望:首行 `gt.Meta` 即显示上一进程结束时的状态(gold=400、armor/whetstone 已购已装、talent_mine、绷带 x3/loadout 2),证明 `Initialize` 自动 `Load` 生效。

- [ ] **Step 5: 失败/上限语义抽查**

```
& "...UnrealEditor-Cmd.exe" "...Graytail.uproject" -game -nullrhi -unattended -nopause -nosplash -ExecCmds="gt.MetaReset, gt.MetaAddGold 1000, gt.MetaBuy armor, gt.MetaBuy whetstone, gt.MetaBuy medkit, gt.MetaEquip armor, gt.MetaEquip whetstone, gt.MetaEquip medkit, gt.MetaBuy armor, gt.MetaTalent talent_mine, gt.MetaTalent talent_mine, quit"
```
期望:第 3 件 `gt.MetaEquip medkit` → `failed max_equipped`;重复 `gt.MetaBuy armor` → `failed already_owned`;重复 `gt.MetaTalent talent_mine` → `failed already_unlocked`。

- [ ] **Step 6: 若全过,无需额外提交**(代码在各任务已提交)。如有修复,提交:`fix: 局外元进度 S1 验证修复 <简述>`。

---

## 提交约定

- 用 `git commit -F <utf8-file>`(中文正文,勿用 `-m`,GBK 终端会乱码);`git config i18n.commitEncoding utf-8` 已设。
- 作者 `Anmiciusby <165156125+...>`(勿 gmail);footer `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。
- 永不提交自动改写的 `Config/*.ini` / `.uproject` / `.vsconfig`;提交前 `git status` 核对仅本任务文件。
- 验证后才提交(每任务 build 绿;Task 4/10 还要 163 过)。

---

## Self-Review(已对照 spec)

- **覆盖:** spec §3 数据模型→T1;§4 目录→T2;§6 存档→T3;§2/§5 子系统+操作 API→T4-T8(币/GM=T4,装备天赋=T5,消耗品 loadout=T6,仓库统计结算=T7,加成查询=T8);§7 调试命令→T9;§9 验证计划→T10。无遗漏。
- **占位:** 无 TBD/TODO,所有代码步骤含完整代码。
- **类型一致:** `FGT_MetaProgressState` 字段名在 T4-T8 各方法中一致;`GT_MetaCatalog::MaxEquipped`/`FindEquip`/`FindTalent`/`FindConsumable` 跨任务一致;`ToggleEquip`/`BuyItem`/`SellWarehouseItem` 等签名(含 `FName& OutError`)在 T5/T7 声明与 T9 调用一致。
- **注意:** T7 头文件片段末尾的 `private:` 私有帮手声明,实现时并入 T4 已有的 private 块(别重复关键字)。
