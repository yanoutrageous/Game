#pragma once

#include "CoreMinimal.h"
#include "GT_ActorTypes.generated.h"

UENUM(BlueprintType)
enum class EGT_ActorTeam : uint8
{
	Neutral UMETA(DisplayName = "Neutral"),
	Player UMETA(DisplayName = "Player"),
	Enemy UMETA(DisplayName = "Enemy"),
	Ally UMETA(DisplayName = "Ally"),
	Environment UMETA(DisplayName = "Environment")
};

UENUM(BlueprintType)
enum class EGT_ActorFaction : uint8
{
	None UMETA(DisplayName = "None"),
	Graytail UMETA(DisplayName = "Graytail"),
	Reclaimer UMETA(DisplayName = "Reclaimer"),
	Hostile UMETA(DisplayName = "Hostile"),
	Neutral UMETA(DisplayName = "Neutral")
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ActorId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	FName Value = NAME_None;

	bool IsValid() const
	{
		return !Value.IsNone();
	}

	FName ToName() const
	{
		return Value;
	}

	void Reset()
	{
		Value = NAME_None;
	}
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ActorRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	FGT_ActorId ActorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	FName ActorDefId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	EGT_ActorTeam Team = EGT_ActorTeam::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	EGT_ActorFaction Faction = EGT_ActorFaction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	int32 CurrentHealth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	int32 CurrentEnergy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Actor")
	bool bAlive = true;
};
