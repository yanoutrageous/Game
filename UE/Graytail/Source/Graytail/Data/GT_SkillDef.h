#pragma once

#include "CoreMinimal.h"
#include "Data/GT_ContentDef.h"
#include "Data/GT_EffectTypes.h"
#include "GT_SkillDef.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_SkillDef : public UGT_ContentDef
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Skill")
	TArray<FGT_EffectSpec> ActivateEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Skill")
	int32 CooldownTurns = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Skill")
	int32 EnergyCost = 0;
};
