#pragma once

#include "CoreMinimal.h"
#include "Data/GT_ContentDef.h"
#include "Data/GT_EffectTypes.h"
#include "GT_ItemDef.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_ItemDef : public UGT_ContentDef
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	TArray<FGT_EffectSpec> UseEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	int32 MaxStack = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	bool bConsumable = false;
};
