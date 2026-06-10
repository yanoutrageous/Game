#include "Domains/Map/GT_MapGenerator.h"

#include "Domains/Map/GT_Random.h"

namespace
{
	const FIntPoint GTDebugEventRoomCoord(4, 1);
	const FIntPoint GTDebugCombatRoomCoord(1, 4);
	const FName GTRoomContent_EventDebugChoice01(TEXT("Event_DebugChoice_01"));
	const FName GTRoomRule_EventPresentOnly(TEXT("Event_PresentOnly"));
	const FName GTRoomInstance_EventDebugChoice01(TEXT("Event_DebugChoice_01_Instance"));
	const FName GTRoomContent_CombatDebugDummy01(TEXT("Combat_DebugDummy_01"));
	const FName GTRoomRule_CombatStartOnly(TEXT("Combat_StartOnly"));
	const FName GTRoomInstance_CombatDebugDummy01(TEXT("Combat_DebugDummy_01_Instance"));

	void ConfigureRoomIdentity(
		FGT_TruthMap& TruthMap,
		const FIntPoint& Coord,
		EGT_RoomBaseType RoomBaseType,
		FName RoomContentId,
		FName RoomRuleId,
		FName RoomInstanceId)
	{
		if (!TruthMap.SetRoomBaseType(Coord.X, Coord.Y, RoomBaseType))
		{
			return;
		}

		FGT_TruthCell* Cell = TruthMap.GetCell(Coord.X, Coord.Y);
		if (!Cell)
		{
			return;
		}

		Cell->RoomContentId = RoomContentId;
		Cell->RoomRuleId = RoomRuleId;
		Cell->RoomInstanceId = RoomInstanceId;
	}
}

bool UGT_MapGenerator::GenerateMap(const FGT_MapGenerationSpec& Spec, FGT_MapGenerationResult& OutResult)
{
	OutResult = FGT_MapGenerationResult();

	const EGT_MapMode RequestedMode = Spec.MapMode == EGT_MapMode::Unknown
		? EGT_MapMode::BasicDebug
		: Spec.MapMode;
	const int32 ResolvedWidth = Spec.Width > 0 ? Spec.Width : 10;
	const int32 ResolvedHeight = Spec.Height > 0 ? Spec.Height : 10;

	OutResult.MapMode = RequestedMode;
	OutResult.Seed = Spec.Seed;
	OutResult.Width = ResolvedWidth;
	OutResult.Height = ResolvedHeight;
	OutResult.TruthMap.Initialize(ResolvedWidth, ResolvedHeight, Spec.Seed);

	if (RequestedMode == EGT_MapMode::BasicDebug)
	{
		ApplyBasicDebugLayout(OutResult.TruthMap);
		OutResult.bSuccess = true;
		return true;
	}

	if (RequestedMode == EGT_MapMode::Standard)
	{
		ApplyStandardLayout(OutResult.TruthMap, Spec);
		OutResult.bSuccess = true;
		return true;
	}

	return false;
}

void UGT_MapGenerator::ApplyBasicDebugLayout(FGT_TruthMap& TruthMap)
{
	TruthMap.SetExit(TruthMap.Width - 1, TruthMap.Height - 1, true);
	TruthMap.SetMine(2, 2, true);
	ConfigureRoomIdentity(TruthMap, GTDebugEventRoomCoord, EGT_RoomBaseType::Event, GTRoomContent_EventDebugChoice01, GTRoomRule_EventPresentOnly, GTRoomInstance_EventDebugChoice01);
	ConfigureRoomIdentity(TruthMap, GTDebugCombatRoomCoord, EGT_RoomBaseType::Combat, GTRoomContent_CombatDebugDummy01, GTRoomRule_CombatStartOnly, GTRoomInstance_CombatDebugDummy01);
}

void UGT_MapGenerator::ApplyStandardLayout(FGT_TruthMap& TruthMap, const FGT_MapGenerationSpec& Spec)
{
	const int32 MapWidth = TruthMap.Width;
	const int32 MapHeight = TruthMap.Height;
	const int32 TotalCellCount = MapWidth * MapHeight;

	// 出生点固定 (0,0)。撤离点不再固定在角落, 改为全部随机分布(策划: 不可见撤离点)。
	const FIntPoint SpawnCoord(0, 0);

	// 标记出生点安全区(该半径内不布雷)。
	TSet<int32> ReservedIndices;
	const int32 SafeRadius = FMath::Max(0, Spec.SpawnSafeRadius);
	for (int32 OffsetY = -SafeRadius; OffsetY <= SafeRadius; ++OffsetY)
	{
		for (int32 OffsetX = -SafeRadius; OffsetX <= SafeRadius; ++OffsetX)
		{
			const int32 SafeX = SpawnCoord.X + OffsetX;
			const int32 SafeY = SpawnCoord.Y + OffsetY;
			if (TruthMap.IsValidCoord(SafeX, SafeY))
			{
				ReservedIndices.Add(TruthMap.ToIndex(SafeX, SafeY));
			}
		}
	}

	// 收集所有可布雷的候选格 (排除出生安全区)。
	TArray<FIntPoint> Candidates;
	Candidates.Reserve(TotalCellCount);
	for (int32 CellY = 0; CellY < MapHeight; ++CellY)
	{
		for (int32 CellX = 0; CellX < MapWidth; ++CellX)
		{
			if (!ReservedIndices.Contains(TruthMap.ToIndex(CellX, CellY)))
			{
				Candidates.Add(FIntPoint(CellX, CellY));
			}
		}
	}

	// 洗牌后取前 N 个候选格作为雷, N 由雷密度决定。
	FGT_Random Random(Spec.Seed);
	Random.Shuffle(Candidates);

	int32 MineCount = FMath::RoundToInt(TotalCellCount * Spec.MineDensity);
	MineCount = FMath::Clamp(MineCount, 0, Candidates.Num());

	for (int32 Index = 0; Index < MineCount; ++Index)
	{
		TruthMap.SetMine(Candidates[Index].X, Candidates[Index].Y, true);
	}

	// 布雷后 Candidates[MineCount..] 是剩余的已洗牌非雷安全格, 顺序取用即为随机分配。
	// 特殊房数量基准为总格数(对齐 难度判断.md: 怪物/事件各约 10%)。移植自 Minefield.lua:_AssignSpecialRooms。
	int32 NextIndex = MineCount;

	// 怪物房(Combat): 按总格数比例, 至少 2 个。复用调试用的内容/规则 ID。
	// TODO(接入后): 应为每个房间生成唯一 RoomInstanceId。
	const int32 MonsterCount = FMath::Clamp(FMath::RoundToInt(TotalCellCount * Spec.MonsterRoomRatio), 2, FMath::Max(0, Candidates.Num() - NextIndex));
	for (int32 Placed = 0; Placed < MonsterCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		ConfigureRoomIdentity(TruthMap, Candidates[NextIndex], EGT_RoomBaseType::Combat, GTRoomContent_CombatDebugDummy01, GTRoomRule_CombatStartOnly, GTRoomInstance_CombatDebugDummy01);
	}

	// 宝箱房(Chest): 2-5 个, 放置顺序对齐 Minefield.lua(怪物 -> 宝箱 -> 事件 -> 撤离)。
	// 无即时结算内容, 不挂 Content/Rule ID, 开箱走 Search 命令。
	const int32 ChestCount = FMath::Clamp(FMath::RoundToInt(TotalCellCount * Spec.ChestRoomRatio), 2, 5);
	for (int32 Placed = 0; Placed < ChestCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		TruthMap.SetRoomBaseType(Candidates[NextIndex].X, Candidates[NextIndex].Y, EGT_RoomBaseType::Chest);
	}

	// 事件房(Event): 按总格数比例, 至少 1 个。
	const int32 EventCount = FMath::Clamp(FMath::RoundToInt(TotalCellCount * Spec.EventRoomRatio), 1, FMath::Max(0, Candidates.Num() - NextIndex));
	for (int32 Placed = 0; Placed < EventCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		ConfigureRoomIdentity(TruthMap, Candidates[NextIndex], EGT_RoomBaseType::Event, GTRoomContent_EventDebugChoice01, GTRoomRule_EventPresentOnly, GTRoomInstance_EventDebugChoice01);
	}

	// 不可见撤离房(需探索发现), 数量由难度决定。
	for (int32 Placed = 0; Placed < Spec.RandomExitCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		TruthMap.SetExit(Candidates[NextIndex].X, Candidates[NextIndex].Y, true);
	}
}

FGT_MapGenerationSpec UGT_MapGenerator::MakeSpecForDifficulty(EGT_Difficulty Difficulty, int32 Seed)
{
	FGT_MapGenerationSpec Spec;
	Spec.MapMode = EGT_MapMode::Standard;
	Spec.Seed = Seed;
	Spec.SpawnSafeRadius = 1;

	// 三档雷率/特殊房比例统一(对齐 docs/难度判断.md): 雷 ~20%, 怪物/事件各 ~10%, 宝箱 2-5 个。
	Spec.MineDensity = 0.20f;
	Spec.MonsterRoomRatio = 0.10f;
	Spec.EventRoomRatio = 0.10f;
	Spec.ChestRoomRatio = 0.025f;

	// 难度 = 尺寸 × 撤离点数(撤离点越少越难)。
	switch (Difficulty)
	{
	case EGT_Difficulty::Tutorial:
		Spec.Width = 7;  Spec.Height = 7;  Spec.RandomExitCount = 2; break;
	case EGT_Difficulty::Easy:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 3; break;
	case EGT_Difficulty::Standard:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 2; break;
	case EGT_Difficulty::Hard:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 1; break;
	case EGT_Difficulty::Veteran:
		Spec.Width = 13; Spec.Height = 13; Spec.RandomExitCount = 4; break;
	case EGT_Difficulty::Elite:
		Spec.Width = 13; Spec.Height = 13; Spec.RandomExitCount = 2; break;
	case EGT_Difficulty::Nightmare:
		Spec.Width = 13; Spec.Height = 13; Spec.RandomExitCount = 1; break;
	default:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 2; break;
	}

	return Spec;
}
