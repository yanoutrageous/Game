#include "Core/GT_RunContext.h"

#include "Domains/Map/GT_MapGenerator.h"

namespace
{
	const FName GTCombatResult_Success(TEXT("Combat_DebugResult_Success"));
}

void UGT_RunContext::InitializeRun(int32 InSeed, int32 InWidth, int32 InHeight)
{
	FGT_MapGenerationSpec MapSpec;
	MapSpec.MapMode = EGT_MapMode::BasicDebug;
	MapSpec.Seed = InSeed;
	MapSpec.Width = InWidth;
	MapSpec.Height = InHeight;

	FGT_MapGenerationResult MapResult;
	UGT_MapGenerator::GenerateMap(MapSpec, MapResult);

	RunId = FGuid::NewGuid();
	RunState = EGT_RunState::Running;
	RunEndReason = NAME_None;
	Seed = MapResult.Seed;
	MapWidth = MapResult.Width;
	MapHeight = MapResult.Height;
	TruthMap = MapResult.TruthMap;
	PlayerIntelMap.Initialize(MapWidth, MapHeight, FName(TEXT("Player")));
	CombatRuntimeState = FGT_CombatRuntimeState();

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
	RunState = EGT_RunState::NotStarted;
	RunEndReason = NAME_None;
	Seed = 0;
	MapWidth = 0;
	MapHeight = 0;
	TruthMap.Reset();
	PlayerIntelMap.Reset();
	ActorStates.Reset();
	PlayerActorId = NAME_None;
	CombatRuntimeState = FGT_CombatRuntimeState();
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

EGT_RunState UGT_RunContext::GetRunState() const
{
	return RunState;
}

bool UGT_RunContext::IsRunActive() const
{
	return RunState == EGT_RunState::Running;
}

bool UGT_RunContext::IsRunFailed() const
{
	return RunState == EGT_RunState::Failed;
}

bool UGT_RunContext::IsRunSucceeded() const
{
	return RunState == EGT_RunState::Succeeded;
}

bool UGT_RunContext::MarkRunFailed(FName Reason)
{
	if (RunState != EGT_RunState::Running)
	{
		return false;
	}

	RunState = EGT_RunState::Failed;
	RunEndReason = Reason;
	return true;
}

bool UGT_RunContext::MarkRunSucceeded(FName Reason)
{
	if (RunState != EGT_RunState::Running)
	{
		return false;
	}

	RunState = EGT_RunState::Succeeded;
	RunEndReason = Reason;
	return true;
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

bool UGT_RunContext::StartDummyCombat(int32 X, int32 Y, FName RoomContentId, FName RoomRuleId, int32 InitialDummyHp)
{
	if (!IsRunActive() || !IsValidMapCoord(X, Y))
	{
		return false;
	}

	if (CombatRuntimeState.CombatX == X
		&& CombatRuntimeState.CombatY == Y
		&& CombatRuntimeState.bCombatResolved)
	{
		return true;
	}

	CombatRuntimeState.bCombatActive = true;
	CombatRuntimeState.bCombatResolved = false;
	CombatRuntimeState.DummyEnemyHp = FMath::Max(1, InitialDummyHp);
	CombatRuntimeState.CombatX = X;
	CombatRuntimeState.CombatY = Y;
	CombatRuntimeState.RoomContentId = RoomContentId;
	CombatRuntimeState.RoomRuleId = RoomRuleId;
	CombatRuntimeState.LastCombatResultId = NAME_None;
	return true;
}

bool UGT_RunContext::AttackDummyCombat(FGT_CombatRuntimeState& OutState)
{
	OutState = CombatRuntimeState;
	if (!IsRunActive() || !CombatRuntimeState.bCombatActive || CombatRuntimeState.DummyEnemyHp <= 0)
	{
		return false;
	}

	CombatRuntimeState.DummyEnemyHp = FMath::Max(0, CombatRuntimeState.DummyEnemyHp - 1);
	if (CombatRuntimeState.DummyEnemyHp == 0)
	{
		CombatRuntimeState.bCombatActive = false;
		CombatRuntimeState.bCombatResolved = true;
		CombatRuntimeState.LastCombatResultId = GTCombatResult_Success;
	}

	OutState = CombatRuntimeState;
	return true;
}

bool UGT_RunContext::ResolveDummyCombat(FName ResultId, FGT_CombatRuntimeState& OutState)
{
	OutState = CombatRuntimeState;
	if (!IsRunActive())
	{
		return false;
	}

	if (!CombatRuntimeState.bCombatActive && !CombatRuntimeState.bCombatResolved)
	{
		return false;
	}

	CombatRuntimeState.bCombatActive = false;
	CombatRuntimeState.bCombatResolved = true;
	CombatRuntimeState.LastCombatResultId = ResultId;
	if (ResultId == GTCombatResult_Success)
	{
		CombatRuntimeState.DummyEnemyHp = 0;
	}

	OutState = CombatRuntimeState;
	return true;
}

bool UGT_RunContext::GetCombatStateSnapshot(FGT_CombatRuntimeState& OutState) const
{
	OutState = CombatRuntimeState;
	return CombatRuntimeState.bCombatActive
		|| CombatRuntimeState.bCombatResolved
		|| CombatRuntimeState.CombatX != INDEX_NONE
		|| CombatRuntimeState.CombatY != INDEX_NONE;
}
