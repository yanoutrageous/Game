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

	// 出生点固定在 (0,0)(与 RunContext 当前玩家出生位置一致), 撤离点在右下角。
	const FIntPoint SpawnCoord(0, 0);
	const FIntPoint ExitCoord(MapWidth - 1, MapHeight - 1);

	TruthMap.SetExit(ExitCoord.X, ExitCoord.Y, true);

	// 标记不可布雷的保留格: 出生点及其安全半径, 以及撤离点。
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
	ReservedIndices.Add(TruthMap.ToIndex(ExitCoord.X, ExitCoord.Y));

	// 收集所有可布雷的候选格 (排除保留格)。
	TArray<FIntPoint> Candidates;
	Candidates.Reserve(MapWidth * MapHeight);
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

	int32 MineCount = FMath::RoundToInt(MapWidth * MapHeight * Spec.MineDensity);
	MineCount = FMath::Clamp(MineCount, 0, Candidates.Num());

	for (int32 Index = 0; Index < MineCount; ++Index)
	{
		TruthMap.SetMine(Candidates[Index].X, Candidates[Index].Y, true);
	}

	// 布雷后 Candidates[MineCount..] 是剩余的已洗牌非雷安全格,
	// 顺序取用即为随机分配。移植自 Minefield.lua:_AssignSpecialRooms。
	const int32 SafeCellCount = Candidates.Num() - MineCount;
	int32 NextIndex = MineCount;

	// 怪物房(Combat): 按安全格比例, 至少 2 个。复用调试用的内容/规则 ID。
	// TODO(block D): 接入 run 时应为每个房间生成唯一 RoomInstanceId。
	const int32 MonsterCount = FMath::Clamp(FMath::RoundToInt(SafeCellCount * Spec.MonsterRoomRatio), 2, FMath::Max(0, Candidates.Num() - NextIndex));
	for (int32 Placed = 0; Placed < MonsterCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		ConfigureRoomIdentity(TruthMap, Candidates[NextIndex], EGT_RoomBaseType::Combat, GTRoomContent_CombatDebugDummy01, GTRoomRule_CombatStartOnly, GTRoomInstance_CombatDebugDummy01);
	}

	// 事件房(Event): 按安全格比例, 至少 1 个。
	const int32 EventCount = FMath::Clamp(FMath::RoundToInt(SafeCellCount * Spec.EventRoomRatio), 1, FMath::Max(0, Candidates.Num() - NextIndex));
	for (int32 Placed = 0; Placed < EventCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		ConfigureRoomIdentity(TruthMap, Candidates[NextIndex], EGT_RoomBaseType::Event, GTRoomContent_EventDebugChoice01, GTRoomRule_EventPresentOnly, GTRoomInstance_EventDebugChoice01);
	}

	// 额外随机撤离房。
	for (int32 Placed = 0; Placed < Spec.RandomExitCount && NextIndex < Candidates.Num(); ++Placed, ++NextIndex)
	{
		TruthMap.SetExit(Candidates[NextIndex].X, Candidates[NextIndex].Y, true);
	}
}
