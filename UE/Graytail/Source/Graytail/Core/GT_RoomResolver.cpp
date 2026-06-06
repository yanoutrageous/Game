#include "Core/GT_RoomResolver.h"

#include "Core/GT_ContentRegistry.h"
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
	const FName GTEventType_RoomDefinitionMissing(TEXT("RoomDefinitionMissing"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTCombatResult_Success(TEXT("Success"));
	const FName GTCombatResult_Fail(TEXT("Fail"));
	const FName GTSourceSystem_RoomResolver(TEXT("RoomResolver"));
}

void UGT_RoomResolver::Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus, UGT_ContentRegistry* InContentRegistry)
{
	RunContext = InRunContext;
	EventBus = InEventBus;
	ContentRegistry = InContentRegistry;
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

	if (OutResult.RoomBaseType != EGT_RoomBaseType::Event)
	{
		const FName RejectedOptionId = OptionId.IsNone() ? GTEventOption_DefaultContinue : OptionId;
		PublishInteractionEvent(
			GTEventType_EventOptionChooseFailed,
			OutResult,
			RejectedOptionId,
			false,
			FString::Printf(TEXT("ChooseEventOption rejected: expected Event room, current RoomBaseType=%d."), static_cast<int32>(OutResult.RoomBaseType)));
		return false;
	}

	FString DefinitionFailureReason;
	if (!TryGetRoomDefinitions(OutResult, EGT_RoomBaseType::Event, DefinitionFailureReason))
	{
		PublishInteractionEvent(
			GTEventType_EventOptionChooseFailed,
			OutResult,
			OptionId,
			false,
			FString::Printf(TEXT("ChooseEventOption rejected: %s"), *DefinitionFailureReason));
		return false;
	}

	const FName ResolvedOptionId = OptionId.IsNone() ? GetDefaultEventOptionId(OutResult) : OptionId;
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
		BuildRoomDefinitionPayloadText(OutResult, FString::Printf(TEXT("OptionId=%s"), *ResolvedOptionId.ToString())));
	PublishInteractionEvent(
		GTEventType_EventResolved,
		OutResult,
		ResolvedOptionId,
		true,
		BuildRoomDefinitionPayloadText(OutResult, FString::Printf(TEXT("OptionId=%s"), *ResolvedOptionId.ToString())));
	return true;
}

bool UGT_RoomResolver::ResolveCombatAt(int32 X, int32 Y, FName ResultId, FGT_RoomResolveResult& OutResult)
{
	if (!BuildResultFromTruthCell(X, Y, OutResult))
	{
		return false;
	}

	if (OutResult.RoomBaseType != EGT_RoomBaseType::Combat)
	{
		const FName RejectedResultId = ResultId.IsNone() ? GTCombatResult_Success : ResultId;
		PublishInteractionEvent(
			GTEventType_CombatResolveFailed,
			OutResult,
			RejectedResultId,
			false,
			FString::Printf(TEXT("ResolveCombat rejected: expected Combat room, current RoomBaseType=%d."), static_cast<int32>(OutResult.RoomBaseType)));
		return false;
	}

	FString DefinitionFailureReason;
	if (!TryGetRoomDefinitions(OutResult, EGT_RoomBaseType::Combat, DefinitionFailureReason))
	{
		PublishInteractionEvent(
			GTEventType_CombatResolveFailed,
			OutResult,
			ResultId,
			false,
			FString::Printf(TEXT("ResolveCombat rejected: %s"), *DefinitionFailureReason));
		return false;
	}

	const FName ResolvedResultId = ResultId.IsNone() ? GetDefaultCombatResultId(OutResult) : ResultId;
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
		BuildRoomDefinitionPayloadText(OutResult, FString::Printf(TEXT("Result=%s"), *ResolvedResultId.ToString())));
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
	FString DefinitionFailureReason;
	if (!TryGetRoomDefinitions(OutResult, EGT_RoomBaseType::Event, DefinitionFailureReason))
	{
		OutResult.Outcome = EGT_RoomResolveOutcome::Unsupported;
		PublishDefinitionFailureEvent(OutResult, DefinitionFailureReason);
		return false;
	}

	OutResult.Outcome = EGT_RoomResolveOutcome::EventPresented;
	return true;
}

bool UGT_RoomResolver::ResolveCombatRoomPlaceholder(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	FString DefinitionFailureReason;
	if (!TryGetRoomDefinitions(OutResult, EGT_RoomBaseType::Combat, DefinitionFailureReason))
	{
		OutResult.Outcome = EGT_RoomResolveOutcome::Unsupported;
		PublishDefinitionFailureEvent(OutResult, DefinitionFailureReason);
		return false;
	}

	OutResult.Outcome = EGT_RoomResolveOutcome::CombatStarted;
	return true;
}

bool UGT_RoomResolver::ResolveUnsupportedRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const
{
	OutResult.RoomBaseType = TruthCell.RoomBaseType;
	OutResult.Outcome = EGT_RoomResolveOutcome::Unsupported;
	return true;
}

bool UGT_RoomResolver::TryGetRoomDefinitions(
	const FGT_RoomResolveResult& Result,
	EGT_RoomBaseType ExpectedRoomBaseType,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!IsValid(ContentRegistry))
	{
		OutFailureReason = TEXT("ContentRegistry is not valid.");
		return false;
	}

	FGT_RoomContentDef ContentDefinition;
	FGT_RoomRuleDef RuleDefinition;
	return ContentRegistry->FindRoomDefinitions(
		Result.RoomContentId,
		Result.RoomRuleId,
		ExpectedRoomBaseType,
		ContentDefinition,
		RuleDefinition,
		OutFailureReason);
}

FName UGT_RoomResolver::GetDefaultEventOptionId(const FGT_RoomResolveResult& Result) const
{
	if (IsValid(ContentRegistry))
	{
		FGT_RoomContentDef ContentDefinition;
		FGT_RoomRuleDef RuleDefinition;
		FString FailureReason;
		if (ContentRegistry->FindRoomDefinitions(Result.RoomContentId, Result.RoomRuleId, EGT_RoomBaseType::Event, ContentDefinition, RuleDefinition, FailureReason))
		{
			if (!RuleDefinition.DefaultOptionId.IsNone())
			{
				return RuleDefinition.DefaultOptionId;
			}

			if (!ContentDefinition.DefaultOptionId.IsNone())
			{
				return ContentDefinition.DefaultOptionId;
			}
		}
	}

	return GTEventOption_DefaultContinue;
}

FName UGT_RoomResolver::GetDefaultCombatResultId(const FGT_RoomResolveResult& Result) const
{
	if (IsValid(ContentRegistry))
	{
		FGT_RoomContentDef ContentDefinition;
		FGT_RoomRuleDef RuleDefinition;
		FString FailureReason;
		if (ContentRegistry->FindRoomDefinitions(Result.RoomContentId, Result.RoomRuleId, EGT_RoomBaseType::Combat, ContentDefinition, RuleDefinition, FailureReason))
		{
			if (!RuleDefinition.DefaultResultId.IsNone())
			{
				return RuleDefinition.DefaultResultId;
			}

			if (!ContentDefinition.DefaultResultId.IsNone())
			{
				return ContentDefinition.DefaultResultId;
			}
		}
	}

	return GTCombatResult_Success;
}

FString UGT_RoomResolver::BuildRoomDefinitionPayloadText(const FGT_RoomResolveResult& Result, const FString& Prefix) const
{
	const FString BaseText = FString::Printf(
		TEXT("%s RoomContentId=%s RoomRuleId=%s"),
		*Prefix,
		*Result.RoomContentId.ToString(),
		*Result.RoomRuleId.ToString());

	if ((Result.RoomContentId.IsNone() && Result.RoomRuleId.IsNone())
		|| (Result.RoomBaseType != EGT_RoomBaseType::Event && Result.RoomBaseType != EGT_RoomBaseType::Combat))
	{
		return BaseText;
	}

	if (!IsValid(ContentRegistry))
	{
		return FString::Printf(TEXT("%s DefinitionLookup=false Reason=ContentRegistry is not valid."), *BaseText);
	}

	FGT_RoomContentDef ContentDefinition;
	FGT_RoomRuleDef RuleDefinition;
	FString FailureReason;
	if (!ContentRegistry->FindRoomDefinitions(Result.RoomContentId, Result.RoomRuleId, Result.RoomBaseType, ContentDefinition, RuleDefinition, FailureReason))
	{
		return FString::Printf(TEXT("%s DefinitionLookup=false Reason=%s"), *BaseText, *FailureReason);
	}

	return FString::Printf(
		TEXT("%s ContentDisplayName=%s RuleDisplayName=%s ContentDescription=%s RuleDescription=%s"),
		*BaseText,
		*ContentDefinition.DisplayName,
		*RuleDefinition.DisplayName,
		*ContentDefinition.DebugDescription,
		*RuleDefinition.DebugDescription);
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
	Event.PayloadText = BuildRoomDefinitionPayloadText(Result, TEXT("RoomResolved"));
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}

void UGT_RoomResolver::PublishDefinitionFailureEvent(const FGT_RoomResolveResult& Result, const FString& FailureReason) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent Event;
	Event.EventType = GTEventType_RoomDefinitionMissing;
	Event.SourceSystem = GTSourceSystem_RoomResolver;
	Event.SourceActorId = IsValid(RunContext) ? RunContext->GetPlayerActorId() : NAME_None;
	Event.TargetActorId = Event.SourceActorId;
	Event.X = Result.X;
	Event.Y = Result.Y;
	Event.RoomCoord = FIntPoint(Result.X, Result.Y);
	Event.ContentId = Result.RoomContentId;
	Event.RuleId = Result.RoomRuleId;
	Event.NumericValue = static_cast<int32>(Result.Outcome);
	Event.PayloadText = FString::Printf(TEXT("Room definition lookup failed: %s"), *FailureReason);
	Event.bSuccess = false;
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
		? BuildRoomDefinitionPayloadText(Result, FString::Printf(TEXT("PayloadId=%s"), *PayloadId.ToString()))
		: PayloadText;
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}
