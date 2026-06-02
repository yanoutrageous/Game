#include "Debug/GT_RuntimeSmokeValidator.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"

namespace
{
	const FName GTCheck_RunSubsystemValid(TEXT("RunSubsystemValid"));
	const FName GTCheck_PlayerExists(TEXT("PlayerExists"));
	const FName GTCheck_InitialPlayerPosition(TEXT("InitialPlayerPosition"));
	const FName GTCheck_InitialIntelCell(TEXT("InitialIntelCell"));
	const FName GTCheck_LegalMoveAccepted(TEXT("LegalMoveAccepted"));
	const FName GTCheck_MovedPlayerPosition(TEXT("MovedPlayerPosition"));
	const FName GTCheck_MovedIntelCell(TEXT("MovedIntelCell"));
	const FName GTCheck_RoomNotResolvedBeforeMove(TEXT("RoomNotResolvedBeforeMove"));
	const FName GTCheck_MoveResolvesTargetRoom(TEXT("MoveResolvesTargetRoom"));
	const FName GTCheck_MoveTriggersTargetRoom(TEXT("MoveTriggersTargetRoom"));
	const FName GTCheck_RoomEnteredEvent(TEXT("RoomEnteredEvent"));
	const FName GTCheck_RoomResolvedEvent(TEXT("RoomResolvedEvent"));
	const FName GTCheck_InvalidMoveDoesNotResolveRoom(TEXT("InvalidMoveDoesNotResolveRoom"));
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
	const FName GTCheck_ScanDoesNotResolveRoom(TEXT("ScanDoesNotResolveRoom"));
	const FName GTCheck_InvalidScanRejected(TEXT("InvalidScanRejected"));
	const FName GTCheck_InvalidScanDoesNotWriteIntel(TEXT("InvalidScanDoesNotWriteIntel"));
	const FName GTCheck_InvalidScanCommandFailedEvent(TEXT("InvalidScanCommandFailedEvent"));
	const FName GTCheck_MiniMapViewModelBuild(TEXT("MiniMapViewModelBuild"));
	const FName GTCheck_MiniMapViewModelSize(TEXT("MiniMapViewModelSize"));
	const FName GTCheck_MiniMapViewModelScannedCell(TEXT("MiniMapViewModelScannedCell"));
	const FName GTCheck_MiniMapViewModelDisplayedNumber(TEXT("MiniMapViewModelDisplayedNumber"));
	const FName GTCheck_MiniMapViewModelCellVisibleExplored(TEXT("MiniMapViewModelCellVisibleExplored"));
	const FName GTCheck_MiniMapViewModelReliability(TEXT("MiniMapViewModelReliability"));
	const FName GTCheck_NormalRoomResolveOutcome(TEXT("NormalRoomResolveOutcome"));
	const FName GTCheck_NormalRoomEvents(TEXT("NormalRoomEvents"));
	const FName GTCheck_MineRoomResolveOutcome(TEXT("MineRoomResolveOutcome"));
	const FName GTCheck_MineEncounteredEvent(TEXT("MineEncounteredEvent"));
	const FName GTCheck_MineDoesNotFailRunYet(TEXT("MineDoesNotFailRunYet"));
	const FName GTCheck_ExitRoomResolveOutcome(TEXT("ExitRoomResolveOutcome"));
	const FName GTCheck_ExitFoundEvent(TEXT("ExitFoundEvent"));
	const FName GTCheck_ExitDoesNotWinRunYet(TEXT("ExitDoesNotWinRunYet"));
	const FName GTCheck_ScanDoesNotTriggerRoomResolver(TEXT("ScanDoesNotTriggerRoomResolver"));

	const FName GTCommandType_Move(TEXT("Move"));
	const FName GTCommandType_Scan(TEXT("Scan"));
	const FName GTEventType_ActorMoved(TEXT("ActorMoved"));
	const FName GTEventType_RoomEntered(TEXT("RoomEntered"));
	const FName GTEventType_RoomResolved(TEXT("RoomResolved"));
	const FName GTEventType_MineEncountered(TEXT("MineEncountered"));
	const FName GTEventType_ExitFound(TEXT("ExitFound"));
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

	const int32 RoomEnteredCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
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

	const int32 RoomEnteredCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const bool bScanDoesNotTriggerRoomResolverOk = RoomEnteredCountAfterScan == RoomEnteredCountBeforeScan
		&& RoomResolvedCountAfterScan == RoomResolvedCountBeforeScan
		&& MineEncounteredCountAfterScan == MineEncounteredCountBeforeScan
		&& ExitFoundCountAfterScan == ExitFoundCountBeforeScan;
	AddCheck(
		OutResults,
		GTCheck_ScanDoesNotTriggerRoomResolver,
		bScanDoesNotTriggerRoomResolverOk,
		FString::Printf(TEXT("Resolver event counts after scan: entered %d->%d, resolved %d->%d, mine %d->%d, exit %d->%d."),
			RoomEnteredCountBeforeScan,
			RoomEnteredCountAfterScan,
			RoomResolvedCountBeforeScan,
			RoomResolvedCountAfterScan,
			MineEncounteredCountBeforeScan,
			MineEncounteredCountAfterScan,
			ExitFoundCountBeforeScan,
			ExitFoundCountAfterScan));

	FGT_TruthCell ScannedTruthCell;
	const bool bGotScannedTruthCell = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 1, ScannedTruthCell);
	const bool bScanDoesNotResolveRoomOk = bGotScannedTruthCell
		&& !ScannedTruthCell.bResolved
		&& !ScannedTruthCell.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_ScanDoesNotResolveRoom,
		bScanDoesNotResolveRoomOk,
		FString::Printf(TEXT("Scanned truth cell (1,1) resolved=%s triggered=%s."),
			ScannedTruthCell.bResolved ? TEXT("true") : TEXT("false"),
			ScannedTruthCell.bTriggered ? TEXT("true") : TEXT("false")));

	UGT_MiniMapViewModel* MiniMapViewModel = NewObject<UGT_MiniMapViewModel>(this);
	const bool bMiniMapViewModelBuildOk = MiniMapViewModel && RunContext;
	if (bMiniMapViewModelBuildOk)
	{
		MiniMapViewModel->BuildFromIntelMap(RunContext->GetPlayerIntelMap());
	}
	AddCheck(OutResults, GTCheck_MiniMapViewModelBuild, bMiniMapViewModelBuildOk, bMiniMapViewModelBuildOk ? TEXT("MiniMapViewModel built from IntelMap.") : TEXT("MiniMapViewModel could not be built."));

	TArray<FGT_MiniMapCellViewData> MiniMapCells;
	int32 MiniMapWidth = 0;
	int32 MiniMapHeight = 0;
	if (MiniMapViewModel)
	{
		MiniMapCells = MiniMapViewModel->GetCells();
		MiniMapWidth = MiniMapViewModel->GetWidth();
		MiniMapHeight = MiniMapViewModel->GetHeight();
	}

	const bool bMiniMapViewModelSizeOk = MiniMapWidth == 10
		&& MiniMapHeight == 10
		&& MiniMapCells.Num() == 100;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelSize,
		bMiniMapViewModelSizeOk,
		FString::Printf(TEXT("MiniMapViewModel size is %dx%d with %d cells."), MiniMapWidth, MiniMapHeight, MiniMapCells.Num()));

	FGT_MiniMapCellViewData MiniMapScannedCell;
	bool bFoundMiniMapScannedCell = false;
	for (const FGT_MiniMapCellViewData& Cell : MiniMapCells)
	{
		if (Cell.X == 1 && Cell.Y == 1)
		{
			MiniMapScannedCell = Cell;
			bFoundMiniMapScannedCell = true;
			break;
		}
	}

	const bool bMiniMapViewModelScannedCellOk = bFoundMiniMapScannedCell && MiniMapScannedCell.bScanned;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelScannedCell,
		bMiniMapViewModelScannedCellOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) scanned=%s."), MiniMapScannedCell.bScanned ? TEXT("true") : TEXT("false")));

	const bool bMiniMapViewModelDisplayedNumberOk = bFoundMiniMapScannedCell && MiniMapScannedCell.DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelDisplayedNumber,
		bMiniMapViewModelDisplayedNumberOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) displayed number is %d."), MiniMapScannedCell.DisplayedNumber));

	const bool bMiniMapViewModelCellVisibleExploredOk = bFoundMiniMapScannedCell
		&& MiniMapScannedCell.bVisible
		&& MiniMapScannedCell.bExplored;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelCellVisibleExplored,
		bMiniMapViewModelCellVisibleExploredOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) visible=%s explored=%s."),
			MiniMapScannedCell.bVisible ? TEXT("true") : TEXT("false"),
			MiniMapScannedCell.bExplored ? TEXT("true") : TEXT("false")));

	const bool bMiniMapViewModelReliabilityOk = bFoundMiniMapScannedCell
		&& MiniMapScannedCell.ReliabilityState == EGT_IntelReliabilityState::Accurate;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelReliability,
		bMiniMapViewModelReliabilityOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) reliability is %d."), static_cast<int32>(MiniMapScannedCell.ReliabilityState)));

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

	FGT_TruthCell MoveTargetTruthCellBeforeMove;
	const bool bGotMoveTargetBeforeMove = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 0, MoveTargetTruthCellBeforeMove);
	const bool bRoomNotResolvedBeforeMoveOk = bGotMoveTargetBeforeMove
		&& !MoveTargetTruthCellBeforeMove.bResolved
		&& !MoveTargetTruthCellBeforeMove.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_RoomNotResolvedBeforeMove,
		bRoomNotResolvedBeforeMoveOk,
		FString::Printf(TEXT("Move target room (1,0) before move resolved=%s triggered=%s."),
			MoveTargetTruthCellBeforeMove.bResolved ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellBeforeMove.bTriggered ? TEXT("true") : TEXT("false")));

	const int32 RoomEnteredCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;

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

	FGT_TruthCell MoveTargetTruthCellAfterMove;
	const bool bGotMoveTargetAfterMove = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 0, MoveTargetTruthCellAfterMove);
	const bool bMoveResolvesTargetRoomOk = bGotMoveTargetAfterMove && MoveTargetTruthCellAfterMove.bResolved;
	AddCheck(
		OutResults,
		GTCheck_MoveResolvesTargetRoom,
		bMoveResolvesTargetRoomOk,
		FString::Printf(TEXT("Move target room (1,0) resolved=%s."),
			MoveTargetTruthCellAfterMove.bResolved ? TEXT("true") : TEXT("false")));

	const bool bMoveTriggersTargetRoomOk = bGotMoveTargetAfterMove && MoveTargetTruthCellAfterMove.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_MoveTriggersTargetRoom,
		bMoveTriggersTargetRoomOk,
		FString::Printf(TEXT("Move target room (1,0) triggered=%s."),
			MoveTargetTruthCellAfterMove.bTriggered ? TEXT("true") : TEXT("false")));

	const bool bRoomEnteredEventOk = EventBus && EventBus->HasEventOfType(GTEventType_RoomEntered);
	AddCheck(OutResults, GTCheck_RoomEnteredEvent, bRoomEnteredEventOk, bRoomEnteredEventOk ? TEXT("RoomEntered event recorded.") : TEXT("RoomEntered event was not recorded."));

	const bool bRoomResolvedEventOk = EventBus && EventBus->HasEventOfType(GTEventType_RoomResolved);
	AddCheck(OutResults, GTCheck_RoomResolvedEvent, bRoomResolvedEventOk, bRoomResolvedEventOk ? TEXT("RoomResolved event recorded.") : TEXT("RoomResolved event was not recorded."));

	const int32 RoomEnteredCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const bool bNormalRoomResolveOutcomeOk = bGotMoveTargetAfterMove
		&& MoveTargetTruthCellAfterMove.RoomBaseType == EGT_RoomBaseType::Normal
		&& !MoveTargetTruthCellAfterMove.bHasMine
		&& !MoveTargetTruthCellAfterMove.bIsExit
		&& RoomResolvedCountAfterNormalMove == RoomResolvedCountBeforeNormalMove + 1
		&& MineEncounteredCountAfterNormalMove == MineEncounteredCountBeforeNormalMove
		&& ExitFoundCountAfterNormalMove == ExitFoundCountBeforeNormalMove;
	AddCheck(
		OutResults,
		GTCheck_NormalRoomResolveOutcome,
		bNormalRoomResolveOutcomeOk,
		FString::Printf(TEXT("Normal room (1,0) type=%d resolved events %d->%d, mine %d->%d, exit %d->%d."),
			static_cast<int32>(MoveTargetTruthCellAfterMove.RoomBaseType),
			RoomResolvedCountBeforeNormalMove,
			RoomResolvedCountAfterNormalMove,
			MineEncounteredCountBeforeNormalMove,
			MineEncounteredCountAfterNormalMove,
			ExitFoundCountBeforeNormalMove,
			ExitFoundCountAfterNormalMove));

	const bool bNormalRoomEventsOk = RoomEnteredCountAfterNormalMove == RoomEnteredCountBeforeNormalMove + 1
		&& RoomResolvedCountAfterNormalMove == RoomResolvedCountBeforeNormalMove + 1;
	AddCheck(
		OutResults,
		GTCheck_NormalRoomEvents,
		bNormalRoomEventsOk,
		FString::Printf(TEXT("Normal room events entered %d->%d, resolved %d->%d."),
			RoomEnteredCountBeforeNormalMove,
			RoomEnteredCountAfterNormalMove,
			RoomResolvedCountBeforeNormalMove,
			RoomResolvedCountAfterNormalMove));

	FGT_Command OutOfBoundsCommand;
	OutOfBoundsCommand.CommandType = GTCommandType_Move;
	OutOfBoundsCommand.SourceActorId = GTActorId_Player;
	OutOfBoundsCommand.TargetActorId = GTActorId_Player;
	OutOfBoundsCommand.TargetX = -1;
	OutOfBoundsCommand.TargetY = 0;

	const bool bOutOfBoundsAccepted = RunSubsystem->SubmitCommand(OutOfBoundsCommand);
	AddCheck(OutResults, GTCheck_OutOfBoundsMoveRejected, !bOutOfBoundsAccepted, !bOutOfBoundsAccepted ? TEXT("Out-of-bounds move rejected.") : TEXT("Out-of-bounds move was accepted."));

	FGT_TruthCell InvalidMoveTargetTruthCell;
	const bool bInvalidMoveTargetExists = QueryFacade && QueryFacade->GetTruthCellDebugOnly(-1, 0, InvalidMoveTargetTruthCell);
	FGT_TruthCell MoveTargetTruthCellAfterInvalidMove;
	const bool bGotMoveTargetAfterInvalidMove = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 0, MoveTargetTruthCellAfterInvalidMove);
	const bool bInvalidMoveDoesNotResolveRoomOk = !bInvalidMoveTargetExists
		&& bGotMoveTargetAfterInvalidMove
		&& bGotMoveTargetAfterMove
		&& MoveTargetTruthCellAfterInvalidMove.bResolved == MoveTargetTruthCellAfterMove.bResolved
		&& MoveTargetTruthCellAfterInvalidMove.bTriggered == MoveTargetTruthCellAfterMove.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_InvalidMoveDoesNotResolveRoom,
		bInvalidMoveDoesNotResolveRoomOk,
		FString::Printf(TEXT("Invalid move target exists=%s; current room resolved %s->%s triggered %s->%s."),
			bInvalidMoveTargetExists ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterMove.bResolved ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterInvalidMove.bResolved ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterMove.bTriggered ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterInvalidMove.bTriggered ? TEXT("true") : TEXT("false")));

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

	UGT_RunSubsystem* ActiveRunSubsystem = RunSubsystem;
	auto SubmitPlayerMoveTo = [ActiveRunSubsystem](int32 TargetX, int32 TargetY) -> bool
	{
		if (!ActiveRunSubsystem)
		{
			return false;
		}

		FGT_Command Command;
		Command.CommandType = GTCommandType_Move;
		Command.SourceActorId = GTActorId_Player;
		Command.TargetActorId = GTActorId_Player;
		Command.TargetX = TargetX;
		Command.TargetY = TargetY;
		return ActiveRunSubsystem->SubmitCommand(Command);
	};

	bool bPathToMineOk = SubmitPlayerMoveTo(2, 0);
	bPathToMineOk = bPathToMineOk && SubmitPlayerMoveTo(2, 1);
	const int32 MineEncounteredCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const bool bMineMoveAccepted = bPathToMineOk && SubmitPlayerMoveTo(2, 2);
	const int32 MineEncounteredCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;

	FGT_TruthCell MineTruthCell;
	const bool bGotMineTruthCell = QueryFacade && QueryFacade->GetTruthCellDebugOnly(2, 2, MineTruthCell);
	const bool bMineRoomResolveOutcomeOk = bPathToMineOk
		&& bMineMoveAccepted
		&& bGotMineTruthCell
		&& (MineTruthCell.bHasMine || MineTruthCell.RoomBaseType == EGT_RoomBaseType::Mine)
		&& MineTruthCell.bResolved
		&& MineTruthCell.bTriggered
		&& MineEncounteredCountAfterMineMove == MineEncounteredCountBeforeMineMove + 1
		&& ExitFoundCountAfterMineMove == ExitFoundCountBeforeMineMove;
	AddCheck(
		OutResults,
		GTCheck_MineRoomResolveOutcome,
		bMineRoomResolveOutcomeOk,
		FString::Printf(TEXT("Mine room (2,2) path=%s accepted=%s mine=%s resolved=%s triggered=%s mine events %d->%d."),
			bPathToMineOk ? TEXT("true") : TEXT("false"),
			bMineMoveAccepted ? TEXT("true") : TEXT("false"),
			MineTruthCell.bHasMine ? TEXT("true") : TEXT("false"),
			MineTruthCell.bResolved ? TEXT("true") : TEXT("false"),
			MineTruthCell.bTriggered ? TEXT("true") : TEXT("false"),
			MineEncounteredCountBeforeMineMove,
			MineEncounteredCountAfterMineMove));

	const bool bMineEncounteredEventOk = MineEncounteredCountAfterMineMove == MineEncounteredCountBeforeMineMove + 1;
	AddCheck(
		OutResults,
		GTCheck_MineEncounteredEvent,
		bMineEncounteredEventOk,
		FString::Printf(TEXT("MineEncountered events %d->%d."), MineEncounteredCountBeforeMineMove, MineEncounteredCountAfterMineMove));

	int32 MinePositionX = INDEX_NONE;
	int32 MinePositionY = INDEX_NONE;
	const bool bMinePositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(MinePositionX, MinePositionY);
	const bool bMineDoesNotFailRunYetOk = RunSubsystem->GetCurrentRunContext() != nullptr
		&& bMinePositionReadable
		&& MinePositionX == 2
		&& MinePositionY == 2;
	AddCheck(
		OutResults,
		GTCheck_MineDoesNotFailRunYet,
		bMineDoesNotFailRunYetOk,
		FString::Printf(TEXT("Run context after mine is %s; player position is (%d,%d)."),
			RunSubsystem->GetCurrentRunContext() ? TEXT("valid") : TEXT("invalid"),
			MinePositionX,
			MinePositionY));

	const FIntPoint ExitApproachPath[] = {
		FIntPoint(3, 2),
		FIntPoint(4, 2),
		FIntPoint(5, 2),
		FIntPoint(6, 2),
		FIntPoint(7, 2),
		FIntPoint(8, 2),
		FIntPoint(9, 2),
		FIntPoint(9, 3),
		FIntPoint(9, 4),
		FIntPoint(9, 5),
		FIntPoint(9, 6),
		FIntPoint(9, 7),
		FIntPoint(9, 8)
	};

	bool bPathToExitOk = true;
	for (const FIntPoint& Coord : ExitApproachPath)
	{
		if (!SubmitPlayerMoveTo(Coord.X, Coord.Y))
		{
			bPathToExitOk = false;
			break;
		}
	}

	const int32 ExitFoundCountBeforeExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const int32 MineEncounteredCountBeforeExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const bool bExitMoveAccepted = bPathToExitOk && SubmitPlayerMoveTo(9, 9);
	const int32 ExitFoundCountAfterExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const int32 MineEncounteredCountAfterExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;

	FGT_TruthCell ExitTruthCell;
	const bool bGotExitTruthCell = QueryFacade && QueryFacade->GetTruthCellDebugOnly(9, 9, ExitTruthCell);
	const bool bExitRoomResolveOutcomeOk = bPathToExitOk
		&& bExitMoveAccepted
		&& bGotExitTruthCell
		&& (ExitTruthCell.bIsExit || ExitTruthCell.RoomBaseType == EGT_RoomBaseType::Exit)
		&& ExitTruthCell.bResolved
		&& ExitTruthCell.bTriggered
		&& ExitFoundCountAfterExitMove == ExitFoundCountBeforeExitMove + 1
		&& MineEncounteredCountAfterExitMove == MineEncounteredCountBeforeExitMove;
	AddCheck(
		OutResults,
		GTCheck_ExitRoomResolveOutcome,
		bExitRoomResolveOutcomeOk,
		FString::Printf(TEXT("Exit room (9,9) path=%s accepted=%s exit=%s resolved=%s triggered=%s exit events %d->%d."),
			bPathToExitOk ? TEXT("true") : TEXT("false"),
			bExitMoveAccepted ? TEXT("true") : TEXT("false"),
			ExitTruthCell.bIsExit ? TEXT("true") : TEXT("false"),
			ExitTruthCell.bResolved ? TEXT("true") : TEXT("false"),
			ExitTruthCell.bTriggered ? TEXT("true") : TEXT("false"),
			ExitFoundCountBeforeExitMove,
			ExitFoundCountAfterExitMove));

	const bool bExitFoundEventOk = ExitFoundCountAfterExitMove == ExitFoundCountBeforeExitMove + 1;
	AddCheck(
		OutResults,
		GTCheck_ExitFoundEvent,
		bExitFoundEventOk,
		FString::Printf(TEXT("ExitFound events %d->%d."), ExitFoundCountBeforeExitMove, ExitFoundCountAfterExitMove));

	int32 ExitPositionX = INDEX_NONE;
	int32 ExitPositionY = INDEX_NONE;
	const bool bExitPositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(ExitPositionX, ExitPositionY);
	const bool bExitDoesNotWinRunYetOk = RunSubsystem->GetCurrentRunContext() != nullptr
		&& bExitPositionReadable
		&& ExitPositionX == 9
		&& ExitPositionY == 9;
	AddCheck(
		OutResults,
		GTCheck_ExitDoesNotWinRunYet,
		bExitDoesNotWinRunYetOk,
		FString::Printf(TEXT("Run context after exit is %s; player position is (%d,%d)."),
			RunSubsystem->GetCurrentRunContext() ? TEXT("valid") : TEXT("invalid"),
			ExitPositionX,
			ExitPositionY));

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
