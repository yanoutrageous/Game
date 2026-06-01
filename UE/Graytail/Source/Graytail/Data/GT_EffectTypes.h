#pragma once

#include "CoreMinimal.h"
#include "GT_EffectTypes.generated.h"

UENUM(BlueprintType)
enum class EGT_EffectTiming : uint8
{
	Immediate UMETA(DisplayName = "Immediate"),
	OnEnterRoom UMETA(DisplayName = "On Enter Room"),
	OnResolveRoom UMETA(DisplayName = "On Resolve Room"),
	OnUseItem UMETA(DisplayName = "On Use Item"),
	OnActivateSkill UMETA(DisplayName = "On Activate Skill"),
	Passive UMETA(DisplayName = "Passive")
};

UENUM(BlueprintType)
enum class EGT_ModifierDurationPolicy : uint8
{
	Instant UMETA(DisplayName = "Instant"),
	Turns UMETA(DisplayName = "Turns"),
	Run UMETA(DisplayName = "Run"),
	Permanent UMETA(DisplayName = "Permanent"),
	Conditional UMETA(DisplayName = "Conditional")
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_EffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Effect")
	FName EffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Effect")
	EGT_EffectTiming Timing = EGT_EffectTiming::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Effect")
	FName TargetPolicy = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Effect")
	int32 Magnitude = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Effect")
	TMap<FName, FString> Parameters;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ModifierSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Modifier")
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Modifier")
	EGT_ModifierDurationPolicy DurationPolicy = EGT_ModifierDurationPolicy::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Modifier")
	int32 DurationTurns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Modifier")
	TArray<FGT_EffectSpec> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Modifier")
	TMap<FName, FString> Parameters;
};
