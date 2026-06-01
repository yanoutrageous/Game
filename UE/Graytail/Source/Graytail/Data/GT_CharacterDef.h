#pragma once

#include "CoreMinimal.h"
#include "Data/GT_ContentDef.h"
#include "Data/GT_EffectTypes.h"
#include "GT_CharacterDef.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_CharacterDef : public UGT_ContentDef
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Character")
	int32 MaxHealth = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Character")
	int32 MaxEnergy = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Character")
	TArray<FName> StartingItemIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Character")
	TArray<FName> StartingSkillIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Character")
	TArray<FGT_ModifierSpec> StartingModifiers;
};
