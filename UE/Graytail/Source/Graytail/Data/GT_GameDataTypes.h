#pragma once

#include "CoreMinimal.h"
#include "GT_GameDataTypes.generated.h"

USTRUCT()
struct FGT_PlayerBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 BaseHp = 0;
	UPROPERTY() int32 BasePower = 0;
};

USTRUCT()
struct FGT_CombatBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 MineDamage = 0;
	UPROPERTY() int32 MineDamageFloor = 0;
	UPROPERTY() int32 EnemyPowerMin = 0;
	UPROPERTY() int32 EnemyPowerMax = 0;
	UPROPERTY() int32 MonsterGoldMin = 0;
	UPROPERTY() int32 MonsterGoldMax = 0;
	UPROPERTY() int32 PowerGainPerKill = 0;
	UPROPERTY() int32 PowerGainCap = 0;
	UPROPERTY() int32 DefaultMonsterHpBase = 0;
	UPROPERTY() int32 MonsterDamageMin = 0;
};

USTRUCT()
struct FGT_ProtocolThresholdConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 Pressure = 0;
	UPROPERTY() int32 Level = 0;
};

USTRUCT()
struct FGT_ProtocolBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 MaxPressure = 0;
	UPROPERTY() int32 ExplorePressure = 0;
	UPROPERTY() int32 MinePressure = 0;
	UPROPERTY() int32 CombatKillPressure = 0;
	UPROPERTY() TArray<FGT_ProtocolThresholdConfig> Thresholds;
};

USTRUCT()
struct FGT_CoreBalanceFile
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() FGT_PlayerBalanceConfig Player;
	UPROPERTY() FGT_CombatBalanceConfig Combat;
	UPROPERTY() FGT_ProtocolBalanceConfig Protocol;
};

USTRUCT()
struct FGT_DataCoord
{
	GENERATED_BODY()

	UPROPERTY() int32 X = 0;
	UPROPERTY() int32 Y = 0;
};

USTRUCT()
struct FGT_ManualLayoutConfig
{
	GENERATED_BODY()

	UPROPERTY() bool bEnabled = false;
	UPROPERTY() FGT_DataCoord Spawn;
	UPROPERTY() TArray<FGT_DataCoord> Mines;
	UPROPERTY() TArray<FGT_DataCoord> MonsterRooms;
	UPROPERTY() TArray<FGT_DataCoord> ChestRooms;
	UPROPERTY() TArray<FGT_DataCoord> EventRooms;
	UPROPERTY() TArray<FGT_DataCoord> Exits;
};

USTRUCT()
struct FGT_DifficultyBalanceRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 Width = 0;
	UPROPERTY() int32 Height = 0;
	UPROPERTY() float MineDensity = 0.0f;
	UPROPERTY() int32 SpawnSafeRadius = 0;
	UPROPERTY() float MonsterRoomRatio = 0.0f;
	UPROPERTY() float EventRoomRatio = 0.0f;
	UPROPERTY() float ChestRoomRatio = 0.0f;
	UPROPERTY() int32 RandomExitCount = 0;
	UPROPERTY() bool bRevealExitsAtStart = false;
	UPROPERTY() int32 FixedSeed = 0;
	UPROPERTY() FGT_ManualLayoutConfig ManualLayout;
};

USTRUCT()
struct FGT_DifficultiesBalanceFile
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() TArray<FGT_DifficultyBalanceRow> Difficulties;
};

USTRUCT()
struct FGT_MonsterBalanceRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 HpBase = 0;
	UPROPERTY() float MoveSpeed = 0.0f;
	UPROPERTY() float DamageMult = 0.0f;
	UPROPERTY() float AttackRadius = 0.0f;
	UPROPERTY() float PlayerAttackRange = 0.0f;
	UPROPERTY() float IdleDuration = 0.0f;
	UPROPERTY() float WarningDuration = 0.0f;
	UPROPERTY() float ActiveDuration = 0.0f;
	UPROPERTY() float CooldownDuration = 0.0f;
	UPROPERTY() float IdealDistance = 0.0f;
	UPROPERTY() float AimDuration = 0.0f;
	UPROPERTY() float AttackInterval = 0.0f;
	UPROPERTY() int32 SpreadCount = 0;
	UPROPERTY() float SpreadHalfAngleDeg = 0.0f;
	UPROPERTY() float LaserDuration = 0.0f;
	UPROPERTY() float LaserTickInterval = 0.0f;
	UPROPERTY() float LaserTurnRateDeg = 0.0f;
	UPROPERTY() float AimTurnRateDeg = 0.0f;
	UPROPERTY() float KiteStrength = 0.0f;
	UPROPERTY() float WanderWeight = 0.0f;
	UPROPERTY() bool bChaseWhenFar = false;
	UPROPERTY() bool bDashAwayOnHit = false;
	UPROPERTY() int32 SpawnWeight = 0;
	UPROPERTY() float SplitPowerMultiplier = 0.0f;
	UPROPERTY() int32 SplitCount = 0;
};

USTRUCT()
struct FGT_MonstersBalanceFile
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() TArray<FGT_MonsterBalanceRow> Monsters;
};

USTRUCT()
struct FGT_ItemBalanceRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 Value = 0;
	UPROPERTY() FString Quality;
};

USTRUCT()
struct FGT_ItemDropPoolConfig
{
	GENERATED_BODY()

	UPROPERTY() FString Quality;
	UPROPERTY() TArray<FString> ItemIds;
};

USTRUCT()
struct FGT_ItemsBalanceFile
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() TArray<FGT_ItemBalanceRow> Items;
	UPROPERTY() TArray<FGT_ItemDropPoolConfig> DropPools;
};

USTRUCT()
struct FGT_DropThresholdConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 MaxRoll = 0;
	UPROPERTY() FString Quality;
};

USTRUCT()
struct FGT_SearchBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 BaseMin = 0;
	UPROPERTY() int32 BaseMax = 0;
	UPROPERTY() int32 AdjacentDivisor = 0;
	UPROPERTY() int32 GoldCap = 0;
	UPROPERTY() int32 HighAdjacentAtLeast = 0;
	UPROPERTY() int32 HighAdjacentRareBonus = 0;
	UPROPERTY() TArray<FGT_DropThresholdConfig> DropTable;
};

USTRUCT()
struct FGT_ChestBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 BaseMin = 0;
	UPROPERTY() int32 BaseMax = 0;
	UPROPERTY() int32 GoldCap = 0;
	UPROPERTY() int32 MinItems = 0;
	UPROPERTY() int32 MaxItems = 0;
	UPROPERTY() TArray<FGT_DropThresholdConfig> DropTable;
};

USTRUCT()
struct FGT_GamblerBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 Bet = 0;
	UPROPERTY() int32 LoseMaxRoll = 0;
	UPROPERTY() int32 SmallWinNet = 0;
	UPROPERTY() int32 BigWinRoll = 0;
	UPROPERTY() int32 BigWinNet = 0;
};

USTRUCT()
struct FGT_AltarStepConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 HpCost = 0;
	UPROPERTY() int32 RewardGold = 0;
	UPROPERTY() FString RewardQuality;
};

USTRUCT()
struct FGT_TrapBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 PowerRequirement = 0;
	UPROPERTY() int32 SuccessGold = 0;
	UPROPERTY() int32 FailHpLoss = 0;
	UPROPERTY() int32 FailPressure = 0;
};

USTRUCT()
struct FGT_TraderBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 BaseSalePercent = 0;
};

USTRUCT()
struct FGT_FleeBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 GoldDropPercent = 0;
	UPROPERTY() int32 ItemDropPercent = 0;
	UPROPERTY() int32 DropSalt = 0;
};

USTRUCT()
struct FGT_LuckyCoinBalanceConfig
{
	GENERATED_BODY()

	UPROPERTY() int32 GoldChancePercent = 0;
	UPROPERTY() int32 SafeGoldReward = 0;
	UPROPERTY() int32 RevealCount = 0;
};

USTRUCT()
struct FGT_EventWeightConfig
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 Weight = 0;
};

USTRUCT()
struct FGT_LootEventsBalanceFile
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() FGT_SearchBalanceConfig Search;
	UPROPERTY() FGT_ChestBalanceConfig Chest;
	UPROPERTY() FGT_GamblerBalanceConfig Gambler;
	UPROPERTY() TArray<FGT_AltarStepConfig> AltarSteps;
	UPROPERTY() FGT_TrapBalanceConfig Trap;
	UPROPERTY() FGT_TraderBalanceConfig Trader;
	UPROPERTY() FGT_FleeBalanceConfig Flee;
	UPROPERTY() FGT_LuckyCoinBalanceConfig LuckyCoin;
	UPROPERTY() TArray<FGT_EventWeightConfig> EventWeights;
};

USTRUCT()
struct FGT_EquipBalanceRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 Price = 0;
	UPROPERTY() int32 BonusHp = 0;
	UPROPERTY() int32 BonusPower = 0;
	UPROPERTY() bool bMineImmunity = false;
	UPROPERTY() int32 MineDamageReduce = 0;
	UPROPERTY() bool bShowExitHint = false;
	UPROPERTY() int32 SearchBonus = 0;
	UPROPERTY() FString Trigger;
	UPROPERTY() int32 TriggerCap = 0;
	UPROPERTY() int32 TriggerAmount = 0;
};

USTRUCT()
struct FGT_TalentBalanceRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 Price = 0;
	UPROPERTY() int32 MineDamageReduce = 0;
	UPROPERTY() int32 MonsterFleeBonus = 0;
	UPROPERTY() int32 FailureGoldBonus = 0;
	UPROPERTY() int32 TradePrice = 0;
	UPROPERTY() bool bMapHighlight = false;
};

USTRUCT()
struct FGT_ConsumableBalanceRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() int32 Price = 0;
	UPROPERTY() int32 Heal = 0;
	UPROPERTY() int32 MaxCarry = 0;
	UPROPERTY() FString Trigger;
};

USTRUCT()
struct FGT_MetaCatalogBalanceFile
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() int32 MaxEquipped = 0;
	UPROPERTY() int32 RecentRecoveryMax = 0;
	UPROPERTY() TArray<FGT_EquipBalanceRow> Equipment;
	UPROPERTY() TArray<FGT_TalentBalanceRow> Talents;
	UPROPERTY() TArray<FGT_ConsumableBalanceRow> Consumables;
};

struct GRAYTAIL_API FGT_GameDataSnapshot
{
	FGT_CoreBalanceFile Core;
	FGT_DifficultiesBalanceFile Difficulties;
	FGT_MonstersBalanceFile Monsters;
	FGT_ItemsBalanceFile Items;
	FGT_LootEventsBalanceFile LootEvents;
	FGT_MetaCatalogBalanceFile MetaCatalog;
};
