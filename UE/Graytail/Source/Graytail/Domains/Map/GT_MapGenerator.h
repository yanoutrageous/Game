#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_MapGenerator.generated.h"

// 手摆固定地图布局(对齐 Lua Tutorial.GetMapConfig 的 manualMap): 出生/雷/特殊房/撤离全部
// 写死坐标(0-based, UE 约定), 用于新手教程这种需要稳定可教学的关卡。bEnabled=false 时走随机生成。
struct FGT_ManualMapLayout
{
	bool bEnabled = false;
	FIntPoint Spawn = FIntPoint::ZeroValue;
	TArray<FIntPoint> Mines;
	TArray<FIntPoint> MonsterRooms;   // Combat
	TArray<FIntPoint> ChestRooms;     // Chest
	TArray<FIntPoint> EventRooms;     // Event
	TArray<FIntPoint> Exits;          // Exit
};

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

	// 宝箱房(Chest)数量占总格数的比例, 2-5 个(对齐 Minefield.lua 正式局: ratio 0.025, min 2, max 5)。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChestRoomRatio = 0.025f;

	// 随机(不可见)撤离房数量, 需探索发现。仅 Standard 模式使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration", meta = (ClampMin = "0"))
	int32 RandomExitCount = 2;

	// 开局把所有撤离信标在情报图上标为可见(对齐 Lua 教程的固定可见撤离点)。
	// 由 RunContext 在初始化时消费; 普通局留 false(撤离点靠探索发现)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|MapGeneration")
	bool bRevealExitsAtStart = false;

	// 手摆固定布局(教程关用)。bEnabled=true 时 Standard 生成走 ApplyManualLayout, 忽略上面的随机参数。
	// 非 UPROPERTY: 仅 C++ 生成期输入, 不进反射/序列化。
	FGT_ManualMapLayout ManualLayout;
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

	// 出生点: BasicDebug 固定 (0,0)(163 测试夹具不动); Standard 全图均匀随机
	// (对齐 Minefield.lua normal 模式 _ChooseRandomSpawn)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|MapGeneration")
	FIntPoint SpawnCoord = FIntPoint::ZeroValue;

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

	// 真随机扫雷布局: 确定性 RNG 随机出生点+布雷, 出生安全区保留。移植自 Minefield.lua。
	static void ApplyStandardLayout(FGT_TruthMap& TruthMap, const FGT_MapGenerationSpec& Spec, FIntPoint& OutSpawnCoord);

	// 手摆固定布局: 严格按 Spec.ManualLayout 的坐标放置, 不做任何随机。出生点取布局指定值。
	static void ApplyManualLayout(FGT_TruthMap& TruthMap, const FGT_MapGenerationSpec& Spec, FIntPoint& OutSpawnCoord);
};
