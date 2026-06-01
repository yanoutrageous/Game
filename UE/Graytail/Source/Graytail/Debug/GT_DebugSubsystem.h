#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"
#include "GT_DebugSubsystem.generated.h"

UCLASS()
class GRAYTAIL_API UGT_DebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Graytail|Debug")
	FString GetCurrentRunDebugSummary() const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	void GetCurrentMiniMapDebugCells(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const;
};
