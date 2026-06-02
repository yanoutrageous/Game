#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/GT_ActorTypes.h"
#include "Domains/Map/GT_MapTypes.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"
#include "GT_QueryFacade.generated.h"

class UGT_RunContext;

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_QueryFacade : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Query")
	void Initialize(UGT_RunContext* InRunContext);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Query")
	void Reset();

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	bool HasValidRunContext() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	FGuid GetRunId() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	int32 GetSeed() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	int32 GetMapWidth() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	int32 GetMapHeight() const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Query")
	void BuildMiniMapViewData(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	FName GetPlayerActorId() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	bool TryGetPlayerPosition(int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Query")
	bool GetActorStates(TArray<FGT_ActorRuntimeState>& OutActors) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Query")
	bool GetIntelCellViewData(int32 X, int32 Y, FGT_MiniMapCellViewData& OutCell) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	bool IsIntelCellExplored(int32 X, int32 Y) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Query")
	bool IsIntelCellVisible(int32 X, int32 Y) const;

	bool GetTruthCellDebugOnly(int32 X, int32 Y, FGT_TruthCell& OutCell) const;
	bool IsTruthMineDebugOnly(int32 X, int32 Y) const;
	bool IsTruthExitDebugOnly(int32 X, int32 Y) const;
	bool GetTruthAdjacentCoords4DebugOnly(int32 X, int32 Y, TArray<FIntPoint>& OutCoords) const;
	bool GetTruthAdjacentCoords8DebugOnly(int32 X, int32 Y, TArray<FIntPoint>& OutCoords) const;
	bool CountAdjacentMinesDebugOnly(int32 X, int32 Y, int32& OutMineCount) const;

private:
	UPROPERTY(Transient)
	UGT_RunContext* RunContext = nullptr;
};
