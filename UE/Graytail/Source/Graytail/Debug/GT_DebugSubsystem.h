#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Debug/GT_DebugTypes.h"
#include "Debug/GT_RuntimeSmokeValidator.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"
#include "GT_DebugSubsystem.generated.h"

class UGT_RunSubsystem;

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
	bool DebugStartNewRun(int32 Seed, int32 Width, int32 Height, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugMoveTo(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugScanCell(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugExtract(FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugChooseEventOption(FName OptionId, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugResolveCombat(FName ResultId, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool GetDebugRunSnapshot(FGT_DebugRunSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool GetDebugMiniMapViewData(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	void GetDebugEventSummary(TArray<FGT_DebugEventSummary>& OutEvents) const;

	void GetDebugCommandHelpLines(TArray<FString>& OutLines) const;
	bool GetDebugStatusText(FString& OutStatus) const;
	bool GetDebugRoomText(FString& OutRoomText) const;
	bool DebugRunDemo(TArray<FString>& OutLogLines, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	void GetCurrentMiniMapDebugCells(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults);

private:
	UGT_RunSubsystem* GetRunSubsystem() const;
	bool SubmitDebugCommand(FName CommandType, int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot, FName PayloadId = NAME_None);
};
