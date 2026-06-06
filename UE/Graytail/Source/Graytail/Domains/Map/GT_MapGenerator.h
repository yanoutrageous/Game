#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_MapGenerator.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MapGenerationSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration")
	EGT_MapMode MapMode = EGT_MapMode::BasicDebug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration")
	int32 Width = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration")
	int32 Height = 10;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MapGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	bool bSuccess = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	EGT_MapMode MapMode = EGT_MapMode::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	int32 Width = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	int32 Height = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	FGT_TruthMap TruthMap;
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_MapGenerator : public UObject
{
	GENERATED_BODY()

public:
	static bool GenerateMap(const FGT_MapGenerationSpec& Spec, FGT_MapGenerationResult& OutResult);

private:
	static void ApplyBasicDebugLayout(FGT_TruthMap& TruthMap);
};
