#pragma once

#include "CoreMinimal.h"
#include "Core/GT_RunContext.h"
#include "GT_DebugTypes.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_DebugRunSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	EGT_RunState RunState = EGT_RunState::NotStarted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 PlayerX = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 PlayerY = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 MapWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 MapHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 EventCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	EGT_RoomBaseType CurrentRoomBaseType = EGT_RoomBaseType::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName CurrentRoomContentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName CurrentRoomRuleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName CurrentRoomInstanceId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString CurrentRoomContentDisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString CurrentRoomContentDebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString CurrentRoomRuleDisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString CurrentRoomRuleDebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString CurrentRoomAvailableEventOptions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString CurrentRoomAvailableCombatResults;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bCombatActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bCombatResolved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 DummyEnemyHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName LastCombatResultId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bRunSummaryAvailable = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName RunSummaryOutcome = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bRunSummaryExtracted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 RunSummaryFinalPlayerX = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 RunSummaryFinalPlayerY = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 RunSummaryTotalEventCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 RunSummarySeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 RunSummaryMapWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 RunSummaryMapHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bRunSummaryCombatActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bRunSummaryCombatResolved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName RunSummaryLastCombatResultId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bCurrentRoomTriggered = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bCurrentRoomResolved = false;

	// 玩家战斗状态(供 UI: 血量条 / 踩雷红闪门控 / 战斗表现)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 PlayerHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 PlayerMaxHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_DebugEventSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName EventType = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 Count = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName LastContentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FName LastRuleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString LastContentDisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString LastRuleDisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString LastDebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	int32 LastSequenceId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	bool bLastSuccess = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Debug")
	FString LastPayloadText;
};
