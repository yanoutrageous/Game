#include "Core/GT_CommandProcessor.h"

#include "Core/GT_EventBus.h"
#include "Core/GT_RunContext.h"

namespace
{
	const FName GTCommandType_Move(TEXT("Move"));
	const FName GTCommandType_Scan(TEXT("Scan"));
	const FName GTEventType_ActorMoved(TEXT("ActorMoved"));
	const FName GTEventType_CellScanned(TEXT("CellScanned"));
	const FName GTEventType_CommandFailed(TEXT("CommandFailed"));
	const FName GTSourceSystem_CommandProcessor(TEXT("CommandProcessor"));
}

void UGT_CommandProcessor::Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus)
{
	RunContext = InRunContext;
	EventBus = InEventBus;

	if (!RoomResolver)
	{
		RoomResolver = NewObject<UGT_RoomResolver>(this);
	}

	if (RoomResolver)
	{
		RoomResolver->Initialize(RunContext, EventBus);
	}
}

bool UGT_CommandProcessor::ProcessCommand(const FGT_Command& Command)
{
	if (Command.CommandType == GTCommandType_Move)
	{
		return ProcessMoveCommand(Command);
	}

	if (Command.CommandType == GTCommandType_Scan)
	{
		return ProcessScanCommand(Command);
	}

	PublishCommandEvent(GTEventType_CommandFailed, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
	return false;
}

bool UGT_CommandProcessor::ProcessMoveCommand(const FGT_Command& Command)
{
	if (!IsValid(RunContext) || Command.TargetActorId.IsNone())
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	if (!RunContext->IsValidMapCoord(Command.TargetX, Command.TargetY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	FGT_ActorRuntimeState* ActorState = RunContext->FindActorStateMutable(Command.TargetActorId);
	if (!ActorState)
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	if (FMath::Abs(Command.TargetX - ActorState->X) + FMath::Abs(Command.TargetY - ActorState->Y) != 1)
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	ActorState->X = Command.TargetX;
	ActorState->Y = Command.TargetY;

	if (Command.TargetActorId == RunContext->GetPlayerActorId())
	{
		RunContext->MarkPlayerIntelCellExplored(Command.TargetX, Command.TargetY);
	}

	PublishCommandEvent(GTEventType_ActorMoved, Command.TargetActorId, Command.TargetX, Command.TargetY, true);
	if (Command.TargetActorId == RunContext->GetPlayerActorId() && IsValid(RoomResolver))
	{
		FGT_RoomResolveResult RoomResolveResult;
		RoomResolver->ResolveRoomAt(Command.TargetX, Command.TargetY, RoomResolveResult);
	}

	return true;
}

bool UGT_CommandProcessor::ProcessScanCommand(const FGT_Command& Command)
{
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;

	if (!IsValid(RunContext) || !RunContext->IsValidMapCoord(Command.TargetX, Command.TargetY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, EventTargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	int32 AdjacentMineCount = 0;
	if (!RunContext->CountAdjacentMines8(Command.TargetX, Command.TargetY, AdjacentMineCount)
		|| !RunContext->SetPlayerIntelCellScannedNumber(Command.TargetX, Command.TargetY, AdjacentMineCount))
	{
		PublishCommandEvent(GTEventType_CommandFailed, EventTargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	PublishCommandEvent(GTEventType_CellScanned, EventTargetActorId, Command.TargetX, Command.TargetY, true);
	return true;
}

void UGT_CommandProcessor::PublishCommandEvent(FName EventType, FName TargetActorId, int32 X, int32 Y, bool bSuccess) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent Event;
	Event.EventType = EventType;
	Event.SourceSystem = GTSourceSystem_CommandProcessor;
	Event.TargetActorId = TargetActorId;
	Event.X = X;
	Event.Y = Y;
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}
