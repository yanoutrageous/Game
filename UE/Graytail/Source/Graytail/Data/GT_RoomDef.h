#pragma once

#include "CoreMinimal.h"
#include "Data/GT_ContentDef.h"
#include "Data/GT_EffectTypes.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_RoomDef.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_RoomDef : public UGT_ContentDef
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Room")
	EGT_RoomBaseType RoomBaseType = EGT_RoomBaseType::Unknown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Room")
	bool bCanContainMine = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Room")
	bool bCanBeExit = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Room")
	TArray<FGT_EffectSpec> EnterEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Room")
	TArray<FGT_EffectSpec> ResolveEffects;
};
