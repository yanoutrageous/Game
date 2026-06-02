#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/GT_ActorTypes.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_RunContext.generated.h"

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_RunContext : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	void InitializeRun(int32 InSeed, int32 InWidth = 10, int32 InHeight = 10);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	void ResetRun();

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	FGuid GetRunId() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	int32 GetSeed() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	int32 GetMapWidth() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	int32 GetMapHeight() const;

	const FGT_TruthMap& GetTruthMapForDebugOnly() const;
	const FGT_IntelMap& GetPlayerIntelMap() const;
	const TArray<FGT_ActorRuntimeState>& GetActorStates() const;
	FGT_ActorRuntimeState* FindActorStateMutable(FName ActorId);
	const FGT_ActorRuntimeState* FindActorState(FName ActorId) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	FName GetPlayerActorId() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool TryGetPlayerPosition(int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool IsValidMapCoord(int32 X, int32 Y) const;

	bool MarkPlayerIntelCellExplored(int32 X, int32 Y);
	bool MarkPlayerIntelCellVisible(int32 X, int32 Y, bool bVisible);
	bool CountAdjacentMines8(int32 X, int32 Y, int32& OutMineCount) const;
	bool SetPlayerIntelCellScannedNumber(int32 X, int32 Y, int32 InDisplayedNumber);
	bool GetTruthCellSnapshot(int32 X, int32 Y, FGT_TruthCell& OutCell) const;
	bool MarkTruthCellEntered(int32 X, int32 Y);
	bool MarkTruthCellResolved(int32 X, int32 Y);

private:
	void InitializeBasicMapDebugLayout();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGuid RunId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	int32 MapWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	int32 MapHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGT_TruthMap TruthMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGT_IntelMap PlayerIntelMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	TArray<FGT_ActorRuntimeState> ActorStates;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FName PlayerActorId = NAME_None;
};
