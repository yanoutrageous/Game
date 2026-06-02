#include "Core/GT_RoomResolver.h"

#include "Core/GT_EventBus.h"
#include "Core/GT_RunContext.h"

namespace
{
	const FName GTEventType_RoomEntered(TEXT("RoomEntered"));
	const FName GTEventType_RoomResolved(TEXT("RoomResolved"));
	const FName GTEventType_MineEncountered(TEXT("MineEncountered"));
	const FName GTEventType_ExitFound(TEXT("ExitFound"));
	const FName GTSourceSystem_RoomResolver(TEXT("RoomResolver"));
}

void UGT_RoomResolver::Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus)
{
	RunContext = InRunContext;
	EventBus = InEventBus;
}

bool UGT_RoomResolver::ResolveRoomAt(int32 X, int32 Y, FGT_RoomResolveResult& OutResult)
{
	OutResult = FGT_RoomResolveResult();

	if (!IsValid(RunContext) || !RunContext->IsValidMapCoord(X, Y))
	{
		return false;
	}

	FGT_TruthCell TruthCell;
	if (!RunContext->GetTruthCellSnapshot(X, Y, TruthCell))
	{
		return false;
	}

	const bool bMarkedEntered = RunContext->MarkTruthCellEntered(X, Y);
	const bool bMarkedResolved = RunContext->MarkTruthCellResolved(X, Y);
	if (!bMarkedEntered || !bMarkedResolved)
	{
		return false;
	}

	FGT_TruthCell UpdatedTruthCell;
	if (!RunContext->GetTruthCellSnapshot(X, Y, UpdatedTruthCell))
	{
		return false;
	}

	OutResult.bSuccess = true;
	OutResult.X = X;
	OutResult.Y = Y;
	OutResult.RoomBaseType = UpdatedTruthCell.RoomBaseType;
	OutResult.bTriggered = UpdatedTruthCell.bTriggered;
	OutResult.bResolved = UpdatedTruthCell.bResolved;

	if (UpdatedTruthCell.bHasMine || UpdatedTruthCell.RoomBaseType == EGT_RoomBaseType::Mine)
	{
		OutResult.Outcome = EGT_RoomResolveOutcome::MineEncountered;
	}
	else if (UpdatedTruthCell.bIsExit || UpdatedTruthCell.RoomBaseType == EGT_RoomBaseType::Exit)
	{
		OutResult.Outcome = EGT_RoomResolveOutcome::ExitFound;
	}
	else if (UpdatedTruthCell.RoomBaseType == EGT_RoomBaseType::Normal)
	{
		OutResult.Outcome = EGT_RoomResolveOutcome::NormalResolved;
	}
	else
	{
		OutResult.Outcome = EGT_RoomResolveOutcome::Unsupported;
	}

	PublishResolverEvent(GTEventType_RoomEntered, X, Y, true);
	PublishResolverEvent(GTEventType_RoomResolved, X, Y, true);

	if (OutResult.Outcome == EGT_RoomResolveOutcome::MineEncountered)
	{
		PublishResolverEvent(GTEventType_MineEncountered, X, Y, true);
	}
	else if (OutResult.Outcome == EGT_RoomResolveOutcome::ExitFound)
	{
		PublishResolverEvent(GTEventType_ExitFound, X, Y, true);
	}

	return true;
}

void UGT_RoomResolver::PublishResolverEvent(FName EventType, int32 X, int32 Y, bool bSuccess) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent Event;
	Event.EventType = EventType;
	Event.SourceSystem = GTSourceSystem_RoomResolver;
	Event.X = X;
	Event.Y = Y;
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}
