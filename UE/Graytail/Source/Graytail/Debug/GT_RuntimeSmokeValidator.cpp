#include "Debug/GT_RuntimeSmokeValidator.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunContext.h"
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
	const FName GTCheck_TruthMapSize(TEXT("TruthMapSize"));
	const FName GTCheck_TruthMapCellCount(TEXT("TruthMapCellCount"));
	const FName GTCheck_TruthCellOrigin(TEXT("TruthCellOrigin"));
	const FName GTCheck_TruthCellCorner(TEXT("TruthCellCorner"));
	const FName GTCheck_Neighbors4Corner(TEXT("Neighbors4Corner"));
	const FName GTCheck_Neighbors4Center(TEXT("Neighbors4Center"));
	const FName GTCheck_Neighbors8Corner(TEXT("Neighbors8Corner"));
	const FName GTCheck_Neighbors8Center(TEXT("Neighbors8Center"));
	const FName GTCheck_ExitCellDebug(TEXT("ExitCellDebug"));
	const FName GTCheck_MineCellDebug(TEXT("MineCellDebug"));
	const FName GTCheck_AdjacentMineCountNearMine(TEXT("AdjacentMineCountNearMine"));
	const FName GTCheck_AdjacentMineCountFarFromMine(TEXT("AdjacentMineCountFarFromMine"));
	const FName GTCheck_AdjacentMineCountMineCellSelfExcluded(TEXT("AdjacentMineCountMineCellSelfExcluded"));
	const FName GTCheck_AdjacentMineCountInvalidCoord(TEXT("AdjacentMineCountInvalidCoord"));
	const FName GTCheck_ScanCommandAccepted(TEXT("ScanCommandAccepted"));
	const FName GTCheck_ScannedIntelCellMarked(TEXT("ScannedIntelCellMarked"));
	const FName GTCheck_ScannedDisplayedNumber(TEXT("ScannedDisplayedNumber"));
	const FName GTCheck_CellScannedEvent(TEXT("CellScannedEvent"));
	const FName GTCheck_InvalidScanRejected(TEXT("InvalidScanRejected"));
	const FName GTCheck_InvalidScanDoesNotWriteIntel(TEXT("InvalidScanDoesNotWriteIntel"));
	const FName GTCheck_InvalidScanCommandFailedEvent(TEXT("InvalidScanCommandFailedEvent"));

	const FName GTCommandType_Move(TEXT("Move"));
	const FName GTCommandType_Scan(TEXT("Scan"));
	const FName GTEventType_ActorMoved(TEXT("ActorMoved"));
	const FName GTEventType_CellScanned(TEXT("CellScanned"));
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
	const UGT_RunContext* RunContext = RunSubsystem->GetCurrentRunContext();
	const FGT_TruthMap* TruthMap = RunContext ? &RunContext->GetTruthMapForDebugOnly() : nullptr;
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

	const bool bTruthMapSizeOk = TruthMap && TruthMap->Width == 10 && TruthMap->Height == 10;
	AddCheck(
		OutResults,
		GTCheck_TruthMapSize,
		bTruthMapSizeOk,
		FString::Printf(TEXT("TruthMap size is %dx%d."), TruthMap ? TruthMap->Width : 0, TruthMap ? TruthMap->Height : 0));

	const int32 TruthCellCount = TruthMap ? TruthMap->Cells.Num() : 0;
	const bool bTruthMapCellCountOk = TruthCellCount == 100;
	AddCheck(
		OutResults,
		GTCheck_TruthMapCellCount,
		bTruthMapCellCountOk,
		FString::Printf(TEXT("TruthMap cell count is %d."), TruthCellCount));

	FGT_TruthCell TruthCell;
	const bool bTruthCellOriginOk = QueryFacade
		&& QueryFacade->GetTruthCellDebugOnly(0, 0, TruthCell)
		&& TruthCell.X == 0
		&& TruthCell.Y == 0;
	AddCheck(
		OutResults,
		GTCheck_TruthCellOrigin,
		bTruthCellOriginOk,
		FString::Printf(TEXT("Truth origin cell is (%d,%d)."), TruthCell.X, TruthCell.Y));

	TruthCell = FGT_TruthCell();
	const bool bTruthCellCornerOk = QueryFacade
		&& QueryFacade->GetTruthCellDebugOnly(9, 9, TruthCell)
		&& TruthCell.X == 9
		&& TruthCell.Y == 9;
	AddCheck(
		OutResults,
		GTCheck_TruthCellCorner,
		bTruthCellCornerOk,
		FString::Printf(TEXT("Truth corner cell is (%d,%d)."), TruthCell.X, TruthCell.Y));

	TArray<FIntPoint> AdjacentCoords;
	const bool bNeighbors4CornerOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords4DebugOnly(0, 0, AdjacentCoords)
		&& AdjacentCoords.Num() == 2;
	AddCheck(
		OutResults,
		GTCheck_Neighbors4Corner,
		bNeighbors4CornerOk,
		FString::Printf(TEXT("4-neighbor count at (0,0) is %d."), AdjacentCoords.Num()));

	AdjacentCoords.Reset();
	const bool bNeighbors4CenterOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords4DebugOnly(1, 1, AdjacentCoords)
		&& AdjacentCoords.Num() == 4;
	AddCheck(
		OutResults,
		GTCheck_Neighbors4Center,
		bNeighbors4CenterOk,
		FString::Printf(TEXT("4-neighbor count at (1,1) is %d."), AdjacentCoords.Num()));

	AdjacentCoords.Reset();
	const bool bNeighbors8CornerOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords8DebugOnly(0, 0, AdjacentCoords)
		&& AdjacentCoords.Num() == 3;
	AddCheck(
		OutResults,
		GTCheck_Neighbors8Corner,
		bNeighbors8CornerOk,
		FString::Printf(TEXT("8-neighbor count at (0,0) is %d."), AdjacentCoords.Num()));

	AdjacentCoords.Reset();
	const bool bNeighbors8CenterOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords8DebugOnly(1, 1, AdjacentCoords)
		&& AdjacentCoords.Num() == 8;
	AddCheck(
		OutResults,
		GTCheck_Neighbors8Center,
		bNeighbors8CenterOk,
		FString::Printf(TEXT("8-neighbor count at (1,1) is %d."), AdjacentCoords.Num()));

	const bool bExitCellDebugOk = QueryFacade && QueryFacade->IsTruthExitDebugOnly(9, 9);
	AddCheck(OutResults, GTCheck_ExitCellDebug, bExitCellDebugOk, bExitCellDebugOk ? TEXT("Truth cell (9,9) is exit.") : TEXT("Truth cell (9,9) is not exit."));

	const bool bMineCellDebugOk = QueryFacade && QueryFacade->IsTruthMineDebugOnly(2, 2);
	AddCheck(OutResults, GTCheck_MineCellDebug, bMineCellDebugOk, bMineCellDebugOk ? TEXT("Truth cell (2,2) is mine.") : TEXT("Truth cell (2,2) is not mine."));

	int32 AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountNearMineReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(1, 1, AdjacentMineCount);
	const bool bAdjacentMineCountNearMineOk = bAdjacentMineCountNearMineReturned && AdjacentMineCount == 1;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountNearMine,
		bAdjacentMineCountNearMineOk,
		FString::Printf(TEXT("Adjacent mine count at (1,1) is %d."), AdjacentMineCount));

	AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountFarReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(0, 0, AdjacentMineCount);
	const bool bAdjacentMineCountFarOk = bAdjacentMineCountFarReturned && AdjacentMineCount == 0;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountFarFromMine,
		bAdjacentMineCountFarOk,
		FString::Printf(TEXT("Adjacent mine count at (0,0) is %d."), AdjacentMineCount));

	AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountSelfReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(2, 2, AdjacentMineCount);
	const bool bAdjacentMineCountSelfOk = bAdjacentMineCountSelfReturned && AdjacentMineCount == 0;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountMineCellSelfExcluded,
		bAdjacentMineCountSelfOk,
		FString::Printf(TEXT("Adjacent mine count at mine cell (2,2) is %d."), AdjacentMineCount));

	AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountInvalidReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(-1, 0, AdjacentMineCount);
	const bool bAdjacentMineCountInvalidOk = !bAdjacentMineCountInvalidReturned && AdjacentMineCount == 0;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountInvalidCoord,
		bAdjacentMineCountInvalidOk,
		FString::Printf(TEXT("Invalid adjacent mine count query returned %s with count %d."), bAdjacentMineCountInvalidReturned ? TEXT("true") : TEXT("false"), AdjacentMineCount));

	FGT_Command ScanCommand;
	ScanCommand.CommandType = GTCommandType_Scan;
	ScanCommand.SourceActorId = GTActorId_Player;
	ScanCommand.TargetActorId = GTActorId_Player;
	ScanCommand.TargetX = 1;
	ScanCommand.TargetY = 1;

	const bool bScanAccepted = RunSubsystem->SubmitCommand(ScanCommand);
	AddCheck(OutResults, GTCheck_ScanCommandAccepted, bScanAccepted, bScanAccepted ? TEXT("Scan command at (1,1) accepted.") : TEXT("Scan command at (1,1) was rejected."));

	FGT_MiniMapCellViewData ScannedCell;
	const bool bGotScannedCell = QueryFacade && QueryFacade->GetIntelCellViewData(1, 1, ScannedCell);
	const bool bScannedIntelCellMarked = bGotScannedCell
		&& ScannedCell.bScanned
		&& ScannedCell.bVisible
		&& ScannedCell.bExplored;
	AddCheck(
		OutResults,
		GTCheck_ScannedIntelCellMarked,
		bScannedIntelCellMarked,
		FString::Printf(TEXT("Scanned intel cell flags are scanned=%s visible=%s explored=%s."),
			ScannedCell.bScanned ? TEXT("true") : TEXT("false"),
			ScannedCell.bVisible ? TEXT("true") : TEXT("false"),
			ScannedCell.bExplored ? TEXT("true") : TEXT("false")));

	const bool bScannedDisplayedNumberOk = bGotScannedCell && ScannedCell.DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_ScannedDisplayedNumber,
		bScannedDisplayedNumberOk,
		FString::Printf(TEXT("Scanned displayed number at (1,1) is %d."), ScannedCell.DisplayedNumber));

	const bool bCellScannedEventOk = EventBus && EventBus->HasEventOfType(GTEventType_CellScanned);
	AddCheck(OutResults, GTCheck_CellScannedEvent, bCellScannedEventOk, bCellScannedEventOk ? TEXT("CellScanned event recorded.") : TEXT("CellScanned event was not recorded."));

	const int32 CommandFailedCountBeforeInvalidScan = EventBus ? EventBus->CountEventsOfType(GTEventType_CommandFailed) : 0;

	FGT_Command InvalidScanCommand;
	InvalidScanCommand.CommandType = GTCommandType_Scan;
	InvalidScanCommand.SourceActorId = GTActorId_Player;
	InvalidScanCommand.TargetActorId = GTActorId_Player;
	InvalidScanCommand.TargetX = -1;
	InvalidScanCommand.TargetY = 0;

	const bool bInvalidScanAccepted = RunSubsystem->SubmitCommand(InvalidScanCommand);
	AddCheck(OutResults, GTCheck_InvalidScanRejected, !bInvalidScanAccepted, !bInvalidScanAccepted ? TEXT("Invalid scan rejected.") : TEXT("Invalid scan was accepted."));

	FGT_MiniMapCellViewData UntouchedCell;
	const bool bGotUntouchedCell = QueryFacade && QueryFacade->GetIntelCellViewData(0, 1, UntouchedCell);
	const bool bInvalidScanDoesNotWriteIntel = bGotUntouchedCell
		&& !UntouchedCell.bScanned
		&& UntouchedCell.DisplayedNumber == 0;
	AddCheck(
		OutResults,
		GTCheck_InvalidScanDoesNotWriteIntel,
		bInvalidScanDoesNotWriteIntel,
		FString::Printf(TEXT("Untouched intel cell (0,1) scanned=%s displayed=%d."),
			UntouchedCell.bScanned ? TEXT("true") : TEXT("false"),
			UntouchedCell.DisplayedNumber));

	const int32 CommandFailedCountAfterInvalidScan = EventBus ? EventBus->CountEventsOfType(GTEventType_CommandFailed) : 0;
	const bool bInvalidScanCommandFailedEventOk = CommandFailedCountAfterInvalidScan == CommandFailedCountBeforeInvalidScan + 1;
	AddCheck(
		OutResults,
		GTCheck_InvalidScanCommandFailedEvent,
		bInvalidScanCommandFailedEventOk,
		FString::Printf(TEXT("CommandFailed count before invalid scan was %d, after was %d."),
			CommandFailedCountBeforeInvalidScan,
			CommandFailedCountAfterInvalidScan));

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
