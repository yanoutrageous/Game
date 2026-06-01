#include "Debug/GT_RuntimeSmokeValidator.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunSubsystem.h"

namespace
{
	const FName GTCheck_RunSubsystemValid(TEXT("RunSubsystemValid"));
	const FName GTCheck_PlayerExists(TEXT("PlayerExists"));
	const FName GTCheck_InitialPlayerPosition(TEXT("InitialPlayerPosition"));
	const FName GTCheck_InitialIntelCell(TEXT("InitialIntelCell"));
	const FName GTCheck_LegalMoveAccepted(TEXT("LegalMoveAccepted"));
	const FName GTCheck_MovedPlayerPosition(TEXT("MovedPlayerPosition"));
	const FName GTCheck_MovedIntelCell(TEXT("MovedIntelCell"));
	const FName GTCheck_OutOfBoundsMoveRejected(TEXT("OutOfBoundsMoveRejected"));
	const FName GTCheck_RejectedMovePreservesPosition(TEXT("RejectedMovePreservesPosition"));
	const FName GTCheck_EventsRecorded(TEXT("EventsRecorded"));
	const FName GTCheck_QueryFacadePlayerPosition(TEXT("QueryFacadePlayerPosition"));

	const FName GTCommandType_Move(TEXT("Move"));
	const FName GTEventType_ActorMoved(TEXT("ActorMoved"));
	const FName GTEventType_CommandFailed(TEXT("CommandFailed"));
	const FName GTActorId_Player(TEXT("Player"));
}

void UGT_RuntimeSmokeValidator::Initialize(UGT_RunSubsystem* InRunSubsystem)
{
	RunSubsystem = InRunSubsystem;
}

bool UGT_RuntimeSmokeValidator::RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults)
{
	OutResults.Reset();

	if (!IsValid(RunSubsystem))
	{
		AddCheck(OutResults, GTCheck_RunSubsystemValid, false, TEXT("RunSubsystem is not valid."));
		return false;
	}

	AddCheck(OutResults, GTCheck_RunSubsystemValid, true, TEXT("RunSubsystem is valid."));

	RunSubsystem->StartNewRun(12345, 10, 10);

	UGT_QueryFacade* QueryFacade = RunSubsystem->GetQueryFacade();
	UGT_EventBus* EventBus = RunSubsystem->GetEventBus();
	if (EventBus)
	{
		EventBus->ClearEventHistory();
	}

	TArray<FGT_ActorRuntimeState> ActorStates;
	const bool bGotActors = QueryFacade && QueryFacade->GetActorStates(ActorStates);
	const bool bPlayerExists = bGotActors && ActorStates.ContainsByPredicate([](const FGT_ActorRuntimeState& ActorState)
	{
		return ActorState.ActorId.ToName() == GTActorId_Player;
	});
	AddCheck(OutResults, GTCheck_PlayerExists, bPlayerExists, bPlayerExists ? TEXT("Player actor exists.") : TEXT("Player actor was not found."));

	int32 PlayerX = INDEX_NONE;
	int32 PlayerY = INDEX_NONE;
	const bool bGotInitialPosition = QueryFacade && QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bInitialPositionOk = bGotInitialPosition && PlayerX == 0 && PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_InitialPlayerPosition,
		bInitialPositionOk,
		FString::Printf(TEXT("Initial player position is (%d,%d)."), PlayerX, PlayerY));

	const bool bInitialIntelOk = QueryFacade
		&& QueryFacade->IsIntelCellVisible(0, 0)
		&& QueryFacade->IsIntelCellExplored(0, 0);
	AddCheck(OutResults, GTCheck_InitialIntelCell, bInitialIntelOk, bInitialIntelOk ? TEXT("Initial cell is visible and explored.") : TEXT("Initial cell is not visible/explored."));

	FGT_Command MoveCommand;
	MoveCommand.CommandType = GTCommandType_Move;
	MoveCommand.SourceActorId = GTActorId_Player;
	MoveCommand.TargetActorId = GTActorId_Player;
	MoveCommand.TargetX = 1;
	MoveCommand.TargetY = 0;

	const bool bMoveAccepted = RunSubsystem->SubmitCommand(MoveCommand);
	AddCheck(OutResults, GTCheck_LegalMoveAccepted, bMoveAccepted, bMoveAccepted ? TEXT("Legal move to (1,0) accepted.") : TEXT("Legal move to (1,0) was rejected."));

	PlayerX = INDEX_NONE;
	PlayerY = INDEX_NONE;
	const bool bGotMovedPosition = QueryFacade && QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bMovedPositionOk = bGotMovedPosition && PlayerX == 1 && PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_MovedPlayerPosition,
		bMovedPositionOk,
		FString::Printf(TEXT("Player position after legal move is (%d,%d)."), PlayerX, PlayerY));

	const bool bMovedIntelOk = QueryFacade
		&& QueryFacade->IsIntelCellVisible(1, 0)
		&& QueryFacade->IsIntelCellExplored(1, 0);
	AddCheck(OutResults, GTCheck_MovedIntelCell, bMovedIntelOk, bMovedIntelOk ? TEXT("Moved cell is visible and explored.") : TEXT("Moved cell is not visible/explored."));

	FGT_Command OutOfBoundsCommand;
	OutOfBoundsCommand.CommandType = GTCommandType_Move;
	OutOfBoundsCommand.SourceActorId = GTActorId_Player;
	OutOfBoundsCommand.TargetActorId = GTActorId_Player;
	OutOfBoundsCommand.TargetX = -1;
	OutOfBoundsCommand.TargetY = 0;

	const bool bOutOfBoundsAccepted = RunSubsystem->SubmitCommand(OutOfBoundsCommand);
	AddCheck(OutResults, GTCheck_OutOfBoundsMoveRejected, !bOutOfBoundsAccepted, !bOutOfBoundsAccepted ? TEXT("Out-of-bounds move rejected.") : TEXT("Out-of-bounds move was accepted."));

	PlayerX = INDEX_NONE;
	PlayerY = INDEX_NONE;
	const bool bGotRejectedPosition = QueryFacade && QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bRejectedPositionOk = bGotRejectedPosition && PlayerX == 1 && PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_RejectedMovePreservesPosition,
		bRejectedPositionOk,
		FString::Printf(TEXT("Player position after rejected move is (%d,%d)."), PlayerX, PlayerY));

	const bool bEventsRecorded = EventBus
		&& EventBus->HasEventOfType(GTEventType_ActorMoved)
		&& EventBus->HasEventOfType(GTEventType_CommandFailed);
	AddCheck(OutResults, GTCheck_EventsRecorded, bEventsRecorded, bEventsRecorded ? TEXT("ActorMoved and CommandFailed events recorded.") : TEXT("Expected movement events were not recorded."));

	PlayerX = INDEX_NONE;
	PlayerY = INDEX_NONE;
	const bool bQueryPositionOk = QueryFacade
		&& QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY)
		&& PlayerX == 1
		&& PlayerY == 0;
	AddCheck(OutResults, GTCheck_QueryFacadePlayerPosition, bQueryPositionOk, bQueryPositionOk ? TEXT("QueryFacade reads final player position.") : TEXT("QueryFacade failed to read final player position."));

	for (const FGT_RuntimeSmokeCheckResult& Result : OutResults)
	{
		if (!Result.bPassed)
		{
			return false;
		}
	}

	return true;
}

void UGT_RuntimeSmokeValidator::AddCheck(TArray<FGT_RuntimeSmokeCheckResult>& OutResults, FName CheckName, bool bPassed, const FString& Message)
{
	FGT_RuntimeSmokeCheckResult Result;
	Result.bPassed = bPassed;
	Result.CheckName = CheckName;
	Result.Message = Message;
	OutResults.Add(Result);
}
