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
