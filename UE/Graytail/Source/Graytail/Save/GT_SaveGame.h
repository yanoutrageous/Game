#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/GT_SaveTypes.h"
#include "GT_SaveGame.generated.h"

class UGT_RunContext;

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Save")
	void Reset();

	UFUNCTION(BlueprintCallable, Category = "Graytail|Save")
	void SetFromRunContext(const UGT_RunContext* RunContext);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	FGT_RunSaveMetadata Metadata;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	FGT_MapSaveSnapshot MapSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	TArray<FGT_ActorRuntimeState> ActorSnapshots;
};
