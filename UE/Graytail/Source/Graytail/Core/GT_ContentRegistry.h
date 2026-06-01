#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GT_ContentRegistry.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_ContentRegistry : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Content")
	void RegisterContentId(FName ContentId);

	UFUNCTION(BlueprintPure, Category = "Graytail|Content")
	bool IsContentRegistered(FName ContentId) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Content")
	void ClearRegistry();

private:
	UPROPERTY(Transient)
	TSet<FName> RegisteredContentIds;
};
