#pragma once

#include "CoreMinimal.h"
#include "Core/GT_ActorTypes.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_SaveTypes.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RunSaveMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	FGuid RunId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 MapWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 MapHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	FDateTime SavedAtUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 SchemaVersion = 1;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MapSaveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 Height = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Save")
	TArray<FGT_IntelCell> PlayerIntelCells;
};
