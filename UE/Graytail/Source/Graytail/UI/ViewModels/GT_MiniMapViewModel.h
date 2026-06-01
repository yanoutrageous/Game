#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_MiniMapViewModel.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MiniMapCellViewData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	int32 X = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	int32 Y = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	bool bVisible = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	bool bExplored = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	bool bScanned = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	int32 DisplayedNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	FName MarkerState = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	FName VisibleRoomIcon = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	bool bStale = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MiniMap")
	EGT_IntelReliabilityState ReliabilityState = EGT_IntelReliabilityState::Unknown;
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_MiniMapViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|MiniMap")
	void BuildFromIntelMap(const FGT_IntelMap& IntelMap);

	UFUNCTION(BlueprintCallable, Category = "Graytail|MiniMap")
	void Reset();

	UFUNCTION(BlueprintPure, Category = "Graytail|MiniMap")
	TArray<FGT_MiniMapCellViewData> GetCells() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|MiniMap")
	int32 GetWidth() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|MiniMap")
	int32 GetHeight() const;

private:
	UPROPERTY(Transient)
	TArray<FGT_MiniMapCellViewData> Cells;

	UPROPERTY(Transient)
	int32 Width = 0;

	UPROPERTY(Transient)
	int32 Height = 0;
};
