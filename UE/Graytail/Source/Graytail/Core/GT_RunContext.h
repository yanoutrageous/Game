#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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

	const FGT_TruthMap& GetTruthMap() const;
	const FGT_IntelMap& GetPlayerIntelMap() const;

private:
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
};
