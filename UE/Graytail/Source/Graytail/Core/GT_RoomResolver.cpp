#include "Core/GT_RoomResolver.h"

#include "Core/GT_EventBus.h"
#include "Core/GT_RunContext.h"

namespace
{
	const FName GTEventType_RoomEntered(TEXT("RoomEntered"));
	const FName GTEventType_RoomResolved(TEXT("RoomResolved"));
	const FName GTEventType_MineEncountered(TEXT("MineEncountered"));
	const FName GTEventType_ExitFound(TEXT("ExitFound"));
	const FName GTEventType_EventRoomEntered(TEXT("EventRoomEntered"));
	const FName GTEventType_EventPresented(TEXT("EventPresented"));
	const FName GTEventType_CombatRoomEntered(TEXT("CombatRoomEntered"));
	const FName GTEventType_CombatStarted(TEXT("CombatStarted"));
	const FName GTEventType_EventOptionChosen(TEXT("EventOptionChosen"));
	const FName GTEventType_EventResolved(TEXT("EventResolved"));
	const FName GTEventType_EventOptionChooseFailed(TEXT("EventOptionChooseFailed"));
	const FName GTEventType_CombatResolved(TEXT("CombatResolved"));
	const FName GTEventType_CombatResolveFailed(TEXT("CombatResolveFailed"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTCombatResult_Success(TEXT("Success"));
	const FName GTCombatResult_Fail(TEXT("Fail"));
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
	OutResult.RoomDefId = UpdatedTruthCell.RoomDefId;
	OutResult.RoomContentId = UpdatedTruthCell.RoomContentId;
	OutResult.RoomRuleId = UpdatedTruthCell.RoomRuleId;
	OutResult.RoomInstanceId = UpdatedTruthCell.RoomInstanceId;

	if (!ResolveRoomByHandler(UpdatedTruthCell, OutResult))
	{
		return false;
	}

	PublishResolverEvent(GTEventType_RoomEntered, OutResult, true);
	PublishResolverEvent(GTEventType_RoomResolved, OutResult, true);

	if (OutResult.Outcome == EGT_RoomResolveOutcome::MineEncountered)
	{
		PublishResolverEvent(GTEventType_MineEncountered, OutResult, true);
	}
	else if (OutResult.Outcome == EGT_RoomResolveOutcome::ExitFound)
	{
		PublishResolverEvent(GTEventType_ExitFound, OutResult, true);
	}
	else if (OutResult.Outcome == EGT_RoomResolveOutcome::EventPresented)
	{
		PublishResolverEvent(GTEventType_EventRoomEntered, OutResult, true);
		PublishResolverEvent(GTEventType_EventPresented, OutResult, true);
	}
	else if (OutResult.Outcome == EGT_RoomResolveOutcome::CombatStarted)
	{
		PublishResolverEvent(GTEventType_CombatRoomEntered, OutResult, true);
		PublishResolverEvent(GTEventType_CombatStarted, OutResult, true);
	}

	return true;
}

bool UGT_RoomResolver::ChooseEventOptionAt(int32 X, int32 Y, FName OptionId, FGT_RoomResolveResult& OutResult)
{
	if (!BuildResultFromTruthCell(X, Y, OutResult))
	{
		return false;
	}

	const FName ResolvedOptionId = OptionId.IsNone() ? GTEventOption_DefaultContinue : OptionId;
	if (OutResult.RoomBaseType != EGT_RoomBaseType::Event)
	{
		PublishInteractionEvent(
			GTEventType_EventOptionChooseFailed,
			OutResult,
			ResolvedOptionId,
			false,
			FString::Printf(TEXT("ChooseEventOption rejected: expected Event room, current RoomBaseType=%d."), static_cast<int32>(OutResult.RoomBaseType)));
		return false;
	}

	if (!RunContext->MarkTruthCellResolved(X, Y) || !BuildResultFromTruthCell(X, Y, OutResult))
	{
		PublishInteractionEvent(
			GTEventType_EventOptionChooseFailed,
			OutResult,
			ResolvedOptionId,
			false,
			TEXT("ChooseEventOption rejected: failed to mark current Event room resolved."));
		return false;
	}

	OutResult.Outcome = EGT_RoomResolveOutcome::EventPresented;
	PublishInteractionEvent(
		GTEventType_EventOptionChosen,
		OutResult,
		ResolvedOptionId,
		true,
		FString::Printf(TEXT("OptionId=%s"), *ResolvedOptionId.ToString()));
	PublishInteractionEvent(
		GTEventType_EventResolved,
		OutResult,
		ResolvedOptionId,
		true,
		FString::Printf(TEXT("OptionId=%s"), *ResolvedOptionId.ToString()));
	return true;
}

bool UGT_RoomResolver::ResolveCombatAt(int32 X, int32 Y, FName ResultId, FGT_RoomResolveResult& OutResult)
{
	if (!BuildResultFromTruthCell(X, Y, OutResult))
	{
		return false;
	}

	const FName ResolvedResultId = ResultId.IsNone() ? GTCombatResult_Success : ResultId;
	if (OutResult.RoomBaseType != EGT_RoomBaseType::Combat)
	{
		PublishInteractionEvent(
			GTEventType_CombatResolveFailed,
			OutResult,
			ResolvedResultId,
			false,
			FString::Printf(TEXT("ResolveCombat rejected: expected Combat room, current RoomBaseType=%d."), static_cast<int32>(OutResult.RoomBaseType)));
		return false;
	}

	if (ResolvedResultId == GTCombatResult_Fail)
	{
		PublishInteractionEvent(
			GTEventType_CombatResolveFailed,
			OutResult,
			ResolvedResultId,
			false,
			TEXT("ResolveCombat rejected: Fail result is not implemented for placeholder combat."));
		return false;
	}

	if (ResolvedResultId != GTCombatResult_Success)
	{
		PublishInteractionEvent(
			GTEventType_CombatResolveFailed,
			OutResult,
			ResolvedResultId,
			false,
			FString::Printf(TEXT("ResolveCombat rejected: unsupported Result=%s."), *ResolvedResultId.ToString()));
		return false;
	}

	if (!RunContext->MarkTruthCellResolved(X, Y) || !BuildResultFromTruthCell(X, Y, OutResult))
	{
		PublishInteractionEvent(
			GTEventType_CombatResolveFailed,
			OutResult,
			ResolvedResultId,
			false,
			TEXT("ResolveCombat rejected: failed to mark current Combat room resolved."));
		return false;
	}

	OutResult.Outcome = EGT_RoomResolveOutcome::CombatStarted;
	PublishInteractionEvent(
		GTEventType_CombatResolved,
		OutResult,
		ResolvedResultId,
		true,
		FString::Printf(TEXT("Result=%s"), *ResolvedResultId.ToString()));
	return true;
}

bool UGT_RoomResolver::BuildResultFromTruthCell(int32 X, int32 Y, FGT_RoomResolveResult& OutResult) const
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

	OutResult.bSuccess = true;
	OutResult.X = X;
	OutResult.Y = Y;
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.bTriggered = TruthCell.bTriggered;
	OutResult.bResolved = TruthCell.bResolved;
	OutResult.RoomDefId = TruthCell.RoomDefId;
	OutResult.RoomContentId = TruthCell.RoomContentId;
	OutResult.RoomRuleId = TruthCell.RoomRuleId;
	OutResult.RoomInstanceId = TruthCell.RoomInstanceId;
	return true;
}

bool UGT_RoomResolver::ResolveRoomByHandler(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	if (TruthCell.bHasMine || TruthCell.RoomBaseType == EGT_RoomBaseType::Mine)
	{
		return ResolveMineRoom(TruthCell, OutResult);
	}

	if (TruthCell.bIsExit || TruthCell.RoomBaseType == EGT_RoomBaseType::Exit)
	{
		return ResolveExitRoom(TruthCell, OutResult);
	}

	if (TruthCell.RoomBaseType == EGT_RoomBaseType::Normal)
	{
		return ResolveNormalRoom(TruthCell, OutResult);
	}

	if (TruthCell.RoomBaseType == EGT_RoomBaseType::Event)
	{
		return ResolveEventRoomPlaceholder(TruthCell, OutResult);
	}

	if (TruthCell.RoomBaseType == EGT_RoomBaseType::Combat)
	{
		return ResolveCombatRoomPlaceholder(TruthCell, OutResult);
	}

	return ResolveUnsupportedRoom(TruthCell, OutResult);
}

bool UGT_RoomResolver::ResolveNormalRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::NormalResolved;
	return true;
}

bool UGT_RoomResolver::ResolveMineRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::MineEncountered;
	return true;
}

bool UGT_RoomResolver::ResolveExitRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::ExitFound;
	return true;
}

bool UGT_RoomResolver::ResolveEventRoomPlaceholder(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::EventPresented;
	return true;
}

bool UGT_RoomResolver::ResolveCombatRoomPlaceholder(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::CombatStarted;
	return true;
}

bool UGT_RoomResolver::ResolveUnsupportedRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::Unsupported;
	return true;
}

void UGT_RoomResolver::PublishResolverEvent(FName EventType, const FGT_RoomResolveResult& Result, bool bSuccess) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent Event;
	Event.EventType = EventType;
	Event.SourceSystem = GTSourceSystem_RoomResolver;
	Event.SourceActorId = IsValid(RunContext) ? RunContext->GetPlayerActorId() : NAME_None;
	Event.TargetActorId = Event.SourceActorId;
	Event.X = Result.X;
	Event.Y = Result.Y;
	Event.RoomCoord = FIntPoint(Result.X, Result.Y);
	Event.ContentId = Result.RoomContentId;
	Event.RuleId = Result.RoomRuleId;
	Event.NumericValue = static_cast<int32>(Result.Outcome);
	Event.PayloadText = FString::Printf(TEXT("RoomContentId=%s RoomRuleId=%s"), *Result.RoomContentId.ToString(), *Result.RoomRuleId.ToString());
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}

void UGT_RoomResolver::PublishInteractionEvent(FName EventType, const FGT_RoomResolveResult& Result, FName PayloadId, bool bSuccess, const FString& PayloadText) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent Event;
	Event.EventType = EventType;
	Event.SourceSystem = GTSourceSystem_RoomResolver;
	Event.SourceActorId = IsValid(RunContext) ? RunContext->GetPlayerActorId() : NAME_None;
	Event.TargetActorId = Event.SourceActorId;
	Event.X = Result.X;
	Event.Y = Result.Y;
	Event.RoomCoord = FIntPoint(Result.X, Result.Y);
	Event.ContentId = Result.RoomContentId;
	Event.RuleId = Result.RoomRuleId;
	Event.NumericValue = static_cast<int32>(Result.Outcome);
	Event.PayloadText = PayloadText.IsEmpty()
		? FString::Printf(TEXT("PayloadId=%s RoomContentId=%s RoomRuleId=%s"), *PayloadId.ToString(), *Result.RoomContentId.ToString(), *Result.RoomRuleId.ToString())
		: PayloadText;
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}
