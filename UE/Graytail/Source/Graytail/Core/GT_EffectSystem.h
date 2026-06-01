#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GT_EffectSystem.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_EffectSystem : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Effect")
	void ApplyEffectById(FName EffectId);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Effect")
	void ClearQueuedEffects();

	UFUNCTION(BlueprintPure, Category = "Graytail|Effect")
	int32 GetQueuedEffectCount() const;

private:
	UPROPERTY(Transient)
	TArray<FName> QueuedEffectIds;
};
