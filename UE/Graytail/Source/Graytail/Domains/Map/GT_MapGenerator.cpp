#include "Domains/Map/GT_MapGenerator.h"

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

	return false;
}

void UGT_MapGenerator::ApplyBasicDebugLayout(FGT_TruthMap& TruthMap)
{
	TruthMap.SetExit(TruthMap.Width - 1, TruthMap.Height - 1, true);
	TruthMap.SetMine(2, 2, true);
	ConfigureRoomIdentity(TruthMap, GTDebugEventRoomCoord, EGT_RoomBaseType::Event, GTRoomContent_EventDebugChoice01, GTRoomRule_EventPresentOnly, GTRoomInstance_EventDebugChoice01);
	ConfigureRoomIdentity(TruthMap, GTDebugCombatRoomCoord, EGT_RoomBaseType::Combat, GTRoomContent_CombatDebugDummy01, GTRoomRule_CombatStartOnly, GTRoomInstance_CombatDebugDummy01);
}
