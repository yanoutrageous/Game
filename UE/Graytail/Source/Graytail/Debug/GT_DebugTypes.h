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
};
