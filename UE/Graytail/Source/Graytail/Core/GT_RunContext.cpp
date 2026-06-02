#include "Core/GT_RunContext.h"

void UGT_RunContext::InitializeRun(int32 InSeed, int32 InWidth, int32 InHeight)
{
	RunId = FGuid::NewGuid();
	Seed = InSeed;
	MapWidth = InWidth > 0 ? InWidth : 10;
	MapHeight = InHeight > 0 ? InHeight : 10;

	TruthMap.Initialize(MapWidth, MapHeight, Seed);
	InitializeBasicMapDebugLayout();
	PlayerIntelMap.Initialize(MapWidth, MapHeight, FName(TEXT("Player")));

	PlayerActorId = FName(TEXT("Player"));

	FGT_ActorRuntimeState PlayerState;
	PlayerState.ActorId.Value = PlayerActorId;
	PlayerState.ActorDefId = FName(TEXT("PlayerDefault"));
	PlayerState.Team = EGT_ActorTeam::Player;
	PlayerState.Faction = EGT_ActorFaction::Graytail;
	PlayerState.X = 0;
	PlayerState.Y = 0;
	PlayerState.bAlive = true;

	ActorStates.Reset();
	ActorStates.Add(PlayerState);

	MarkPlayerIntelCellExplored(PlayerState.X, PlayerState.Y);
}

void UGT_RunContext::ResetRun()
{
	RunId.Invalidate();
	Seed = 0;
	MapWidth = 0;
	MapHeight = 0;
	TruthMap.Reset();
	PlayerIntelMap.Reset();
	ActorStates.Reset();
	PlayerActorId = NAME_None;
}

FGuid UGT_RunContext::GetRunId() const
{
	return RunId;
}

int32 UGT_RunContext::GetSeed() const
{
	return Seed;
}

int32 UGT_RunContext::GetMapWidth() const
{
	return MapWidth;
}

int32 UGT_RunContext::GetMapHeight() const
{
	return MapHeight;
}

const FGT_TruthMap& UGT_RunContext::GetTruthMapForDebugOnly() const
{
	return TruthMap;
}

const FGT_IntelMap& UGT_RunContext::GetPlayerIntelMap() const
{
	return PlayerIntelMap;
}

const TArray<FGT_ActorRuntimeState>& UGT_RunContext::GetActorStates() const
{
	return ActorStates;
}

FGT_ActorRuntimeState* UGT_RunContext::FindActorStateMutable(FName ActorId)
{
	if (ActorId.IsNone())
	{
		return nullptr;
	}

	return ActorStates.FindByPredicate([ActorId](const FGT_ActorRuntimeState& ActorState)
	{
		return ActorState.ActorId.ToName() == ActorId;
	});
}

const FGT_ActorRuntimeState* UGT_RunContext::FindActorState(FName ActorId) const
{
	if (ActorId.IsNone())
	{
		return nullptr;
	}

	return ActorStates.FindByPredicate([ActorId](const FGT_ActorRuntimeState& ActorState)
	{
		return ActorState.ActorId.ToName() == ActorId;
	});
}

FName UGT_RunContext::GetPlayerActorId() const
{
	return PlayerActorId;
}

bool UGT_RunContext::TryGetPlayerPosition(int32& OutX, int32& OutY) const
{
	const FGT_ActorRuntimeState* PlayerState = FindActorState(PlayerActorId);
	if (!PlayerState)
	{
		OutX = 0;
		OutY = 0;
		return false;
	}

	OutX = PlayerState->X;
	OutY = PlayerState->Y;
	return true;
}

bool UGT_RunContext::IsValidMapCoord(int32 X, int32 Y) const
{
	return TruthMap.IsValidCoord(X, Y);
}

bool UGT_RunContext::MarkPlayerIntelCellExplored(int32 X, int32 Y)
{
	return PlayerIntelMap.MarkExplored(X, Y);
}

bool UGT_RunContext::MarkPlayerIntelCellVisible(int32 X, int32 Y, bool bVisible)
{
	return PlayerIntelMap.MarkVisible(X, Y, bVisible);
}

bool UGT_RunContext::CountAdjacentMines8(int32 X, int32 Y, int32& OutMineCount) const
{
	return TruthMap.CountAdjacentMines8(X, Y, OutMineCount);
}

bool UGT_RunContext::SetPlayerIntelCellScannedNumber(int32 X, int32 Y, int32 InDisplayedNumber)
{
	return PlayerIntelMap.SetScannedNumber(X, Y, InDisplayedNumber);
}

bool UGT_RunContext::GetTruthCellSnapshot(int32 X, int32 Y, FGT_TruthCell& OutCell) const
{
	OutCell = FGT_TruthCell();

	const FGT_TruthCell* TruthCell = TruthMap.GetCellConst(X, Y);
	if (!TruthCell)
	{
		return false;
	}

	OutCell = *TruthCell;
	return true;
}

bool UGT_RunContext::MarkTruthCellEntered(int32 X, int32 Y)
{
	return TruthMap.MarkCellEntered(X, Y);
}

bool UGT_RunContext::MarkTruthCellResolved(int32 X, int32 Y)
{
	return TruthMap.MarkCellResolved(X, Y);
}

void UGT_RunContext::InitializeBasicMapDebugLayout()
{
	TruthMap.SetExit(MapWidth - 1, MapHeight - 1, true);
	TruthMap.SetMine(2, 2, true);
}
