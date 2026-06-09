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

	// 雷密度: 雷数 = 四舍五入(Width * Height * MineDensity)。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MineDensity = 0.20f;

	// 出生点安全半径: 出生点周围该半径内不布雷。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0"))
	int32 SpawnSafeRadius = 1;

	// 怪物房(Combat)数量占总格数(Width*Height)的比例, 至少 2 个。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MonsterRoomRatio = 0.10f;

	// 事件房(Event)数量占总格数(Width*Height)的比例, 至少 1 个。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EventRoomRatio = 0.10f;

	// 随机(不可见)撤离房数量, 需探索发现。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0"))
	int32 RandomExitCount = 2;
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

	// 按难度档位生成对应的地图参数(尺寸/雷率/特殊房比例/撤离点数)。对齐 docs/难度判断.md。
	static FGT_MapGenerationSpec MakeSpecForDifficulty(EGT_Difficulty Difficulty, int32 Seed);

private:
	// 固定的调试布局: 写死的雷/出口/事件/战斗房, 供既有冒烟测试与 gt.RunDemo 使用。
	static void ApplyBasicDebugLayout(FGT_TruthMap& TruthMap);

	// 真随机扫雷布局: 确定性 RNG 布雷, 出生点安全区保留。移植自 Minefield.lua。
	static void ApplyStandardLayout(FGT_TruthMap& TruthMap, const FGT_MapGenerationSpec& Spec);
};
