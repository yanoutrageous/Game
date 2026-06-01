#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GT_ContentDef.generated.h"

UCLASS(Abstract, BlueprintType)
class GRAYTAIL_API UGT_ContentDef : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Graytail|Content")
	FName GetContentId() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Content")
	FName ContentId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Content")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Content", meta = (MultiLine = "true"))
	FText Description;
};
