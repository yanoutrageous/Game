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
		// 手摆固定布局(教程关)优先; 否则走真随机扫雷布局。
		if (Spec.ManualLayout.bEnabled)
		{
			ApplyManualLayout(OutResult.TruthMap, Spec, OutResult.SpawnCoord);
		}
		else
		{
			ApplyStandardLayout(OutResult.TruthMap, Spec, OutResult.SpawnCoord);
		}
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

void UGT_MapGenerator::ApplyStandardLayout(FGT_TruthMap& TruthMap, const FGT_MapGenerationSpec& Spec, FIntPoint& OutSpawnCoord)
{
	const int32 MapWidth = TruthMap.Width;
	const int32 MapHeight = TruthMap.Height;
	const int32 TotalCellCount = MapWidth * MapHeight;

	// 出生点全图均匀随机(对齐 Minefield.lua normal 模式 _ChooseRandomSpawn:
	// 同一个 RNG 先取 x 再取 y, 然后才布雷, 调用顺序与 Lua 一致)。
	// 撤离点不固定在角落, 全部随机分布(策划: 不可见撤离点)。
	// 注: 策划案还想要"玩家自选出生点"(未拍板的设计决策②), 接入点就在这里。
	FGT_Random Random(Spec.Seed);
	const FIntPoint SpawnCoord(Random.RangeInt(0, MapWidth - 1), Random.RangeInt(0, MapHeight - 1));
	OutSpawnCoord = SpawnCoord;

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

	// 洗牌后取前 N 个候选格作为雷, N 由雷密度决定(沿用上面定出生点的同一 RNG, 序列连续)。
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

void UGT_MapGenerator::ApplyManualLayout(FGT_TruthMap& TruthMap, const FGT_MapGenerationSpec& Spec, FIntPoint& OutSpawnCoord)
{
	const FGT_ManualMapLayout& Layout = Spec.ManualLayout;
	OutSpawnCoord = Layout.Spawn;

	// 严格按坐标放置, 不做任何随机。各类房间互不重叠(布局自己保证)。
	for (const FIntPoint& Coord : Layout.Mines)
	{
		TruthMap.SetMine(Coord.X, Coord.Y, true);
	}
	// 怪物房/事件房复用调试用的内容/规则 ID(与 ApplyStandardLayout 一致)。
	for (const FIntPoint& Coord : Layout.MonsterRooms)
	{
		ConfigureRoomIdentity(TruthMap, Coord, EGT_RoomBaseType::Combat, GTRoomContent_CombatDebugDummy01, GTRoomRule_CombatStartOnly, GTRoomInstance_CombatDebugDummy01);
	}
	for (const FIntPoint& Coord : Layout.ChestRooms)
	{
		TruthMap.SetRoomBaseType(Coord.X, Coord.Y, EGT_RoomBaseType::Chest);
	}
	for (const FIntPoint& Coord : Layout.EventRooms)
	{
		ConfigureRoomIdentity(TruthMap, Coord, EGT_RoomBaseType::Event, GTRoomContent_EventDebugChoice01, GTRoomRule_EventPresentOnly, GTRoomInstance_EventDebugChoice01);
	}
	for (const FIntPoint& Coord : Layout.Exits)
	{
		TruthMap.SetExit(Coord.X, Coord.Y, true);
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
		// 新手教程 = 固定手摆 5×5 安全图(对齐 Lua Tutorial.GetMapConfig 的 manualMap, 坐标 1-based→0-based)。
		// 沿反对角线: 出生→数字格→雷险→事件→怪物→宝箱→撤离, 配合教学弹窗逐格教学。
		// seed 固定 777(对齐 Lua), 让怪物战力/掉落/事件可复现, 每次教程体验一致。
		// 撤离点开局可见(bRevealExitsAtStart), 让新手一开始就知道目标在哪。
		Spec.Width = 5; Spec.Height = 5;
		Spec.Seed = 777;
		Spec.RandomExitCount = 0;
		Spec.bRevealExitsAtStart = true;
		Spec.ManualLayout.bEnabled = true;
		Spec.ManualLayout.Spawn = FIntPoint(0, 0);
		Spec.ManualLayout.Mines = { {0,2}, {1,1}, {2,0}, {3,3} };
		Spec.ManualLayout.EventRooms = { {0,3}, {1,2}, {2,1}, {3,0} };
		Spec.ManualLayout.MonsterRooms = { {0,4}, {1,3}, {2,2}, {3,1}, {4,0} };
		Spec.ManualLayout.ChestRooms = { {1,4}, {2,3}, {3,2}, {4,1} };
		Spec.ManualLayout.Exits = { {4,4} };
		break;
	case EGT_Difficulty::Easy:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 3; break;
	case EGT_Difficulty::Standard:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 2; break;
	case EGT_Difficulty::Hard:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 1;
		Spec.MonsterRoomRatio = 0.15f; break;   // 困难: 怪物房加密(基准 0.10)
	case EGT_Difficulty::Veteran:
		Spec.Width = 13; Spec.Height = 13; Spec.RandomExitCount = 4; break;
	case EGT_Difficulty::Elite:
		Spec.Width = 13; Spec.Height = 13; Spec.RandomExitCount = 2;
		Spec.MonsterRoomRatio = 0.15f; break;   // 困难(大图): 怪物房加密
	case EGT_Difficulty::Nightmare:
		Spec.Width = 13; Spec.Height = 13; Spec.RandomExitCount = 1;
		Spec.MonsterRoomRatio = 0.18f; break;   // 地狱: 怪物房最密
	default:
		Spec.Width = 10; Spec.Height = 10; Spec.RandomExitCount = 2; break;
	}

	return Spec;
}
