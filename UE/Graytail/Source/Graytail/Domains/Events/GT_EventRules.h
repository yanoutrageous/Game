#pragma once

#include "CoreMinimal.h"
#include "Data/GT_GameDataTypes.h"
#include "Domains/Events/GT_EventTypes.h"

namespace GT_EventRules
{
	GRAYTAIL_API const FGT_GamblerBalanceConfig& GetGambler();
	GRAYTAIL_API const TArray<FGT_AltarStepConfig>& GetAltarSteps();
	GRAYTAIL_API EGT_ItemQuality GetAltarRewardQuality(int32 StepIndex);
	GRAYTAIL_API const FGT_TrapBalanceConfig& GetTrap();
	GRAYTAIL_API const FGT_LuckyCoinBalanceConfig& GetLuckyCoin();

	GRAYTAIL_API EGT_EventKind GetEventKindAt(int32 Seed, int32 X, int32 Y);
	GRAYTAIL_API int32 RollDiceAt(int32 Seed, int32 X, int32 Y, int32 PendingGold);
	GRAYTAIL_API int32 GetTraderSaleValue(int32 BaseValue, int32 BonusPercent = 0);

	GRAYTAIL_API FString GetEventTitle(EGT_EventKind Kind);
	GRAYTAIL_API FString GetEventEnterText(EGT_EventKind Kind);
	GRAYTAIL_API FString GetEventDoneText(EGT_EventKind Kind);
	GRAYTAIL_API FString GetEventHotkeyCaption(EGT_EventKind Kind);
	GRAYTAIL_API FString GetEventShortLabel(EGT_EventKind Kind);
}
