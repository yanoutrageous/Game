#include "Debug/GT_DebugSubsystem.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Engine/GameInstance.h"

namespace
{
	const FName GTDebugCommandType_Move(TEXT("Move"));
	const FName GTDebugCommandType_Scan(TEXT("Scan"));
	const FName GTDebugCommandType_Extract(TEXT("Extract"));
	const FName GTDebugCommandType_ChooseEventOption(TEXT("ChooseEventOption"));
	const FName GTDebugCommandType_ResolveCombat(TEXT("ResolveCombat"));
	const FName GTDebugActorId_Player(TEXT("Player"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTCombatResult_Success(TEXT("Combat_DebugResult_Success"));
	const FName GTDebugEventType_EventResolved(TEXT("EventResolved"));
	const FName GTDebugEventType_CombatResolved(TEXT("CombatResolved"));
	const FName GTRoomIcon_Exit(TEXT("Exit"));
	const FName GTRoomIcon_Mine(TEXT("Mine"));
	const FName GTRoomIcon_Event(TEXT("Event"));
	const FName GTRoomIcon_Combat(TEXT("Combat"));

	FName GetDebugRoomIcon(EGT_RoomBaseType RoomBaseType)
	{
		switch (RoomBaseType)
		{
		case EGT_RoomBaseType::Exit:
			return GTRoomIcon_Exit;
		case EGT_RoomBaseType::Mine:
			return GTRoomIcon_Mine;
		case EGT_RoomBaseType::Event:
			return GTRoomIcon_Event;
		case EGT_RoomBaseType::Combat:
			return GTRoomIcon_Combat;
		default:
			return NAME_None;
		}
	}

	FString GetRunStateText(EGT_RunState RunState)
	{
		switch (RunState)
		{
		case EGT_RunState::NotStarted:
			return TEXT("NotStarted");
		case EGT_RunState::Running:
			return TEXT("Running");
		case EGT_RunState::Failed:
			return TEXT("Failed");
		case EGT_RunState::Succeeded:
			return TEXT("Succeeded");
		case EGT_RunState::Ended:
			return TEXT("Ended");
		default:
			return TEXT("Unknown");
		}
	}

	FString GetRoomBaseTypeText(EGT_RoomBaseType RoomBaseType)
	{
		switch (RoomBaseType)
		{
		case EGT_RoomBaseType::Unknown:
			return TEXT("Unknown");
		case EGT_RoomBaseType::Normal:
			return TEXT("Normal");
		case EGT_RoomBaseType::Mine:
			return TEXT("Mine");
		case EGT_RoomBaseType::Exit:
			return TEXT("Exit");
		case EGT_RoomBaseType::Event:
			return TEXT("Event");
		case EGT_RoomBaseType::Combat:
			return TEXT("Combat");
		default:
			return TEXT("Unknown");
		}
	}

	FString BuildDebugEventSummaryText(const TArray<FGT_DebugEventSummary>& EventSummary)
	{
		if (EventSummary.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(EventSummary.Num());
		for (const FGT_DebugEventSummary& Summary : EventSummary)
		{
			Parts.Add(FString::Printf(TEXT("%s=%d"), *Summary.EventType.ToString(), Summary.Count));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	FString BuildEventOptionDefsText(const TArray<FGT_EventOptionDef>& Definitions)
	{
		if (Definitions.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(Definitions.Num());
		for (const FGT_EventOptionDef& Definition : Definitions)
		{
			Parts.Add(FString::Printf(
				TEXT("%s(%s)"),
				*Definition.Id.ToString(),
				Definition.DisplayName.IsEmpty() ? TEXT("Unnamed") : *Definition.DisplayName));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	FString BuildCombatResultDefsText(const TArray<FGT_CombatResultDef>& Definitions)
	{
		if (Definitions.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(Definitions.Num());
		for (const FGT_CombatResultDef& Definition : Definitions)
		{
			Parts.Add(FString::Printf(
				TEXT("%s(%s)"),
				*Definition.Id.ToString(),
				Definition.DisplayName.IsEmpty() ? TEXT("Unnamed") : *Definition.DisplayName));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	bool HasDebugEventSummaryType(const TArray<FGT_DebugEventSummary>& EventSummary, FName EventType)
	{
		return EventSummary.ContainsByPredicate([EventType](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == EventType && Summary.Count > 0;
		});
	}
}

void UGT_DebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGT_DebugSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

FString UGT_DebugSubsystem::GetCurrentRunDebugSummary() const
{
	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;

	if (!QueryFacade || !QueryFacade->HasValidRunContext())
	{
		return TEXT("No active run");
	}

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const bool bHasPlayerPosition = QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);

	TArray<FGT_ActorRuntimeState> ActorStates;
	QueryFacade->GetActorStates(ActorStates);

	return FString::Printf(
		TEXT("RunId=%s, Seed=%d, Size=%dx%d, PlayerActorId=%s, PlayerPosition=%s, ActorCount=%d"),
		*QueryFacade->GetRunId().ToString(),
		QueryFacade->GetSeed(),
		QueryFacade->GetMapWidth(),
		QueryFacade->GetMapHeight(),
		*QueryFacade->GetPlayerActorId().ToString(),
		bHasPlayerPosition ? *FString::Printf(TEXT("(%d,%d)"), PlayerX, PlayerY) : TEXT("None"),
		ActorStates.Num());
}

bool UGT_DebugSubsystem::DebugStartNewRun(int32 Seed, int32 Width, int32 Height, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	if (!RunSubsystem)
	{
		OutSnapshot.Summary = TEXT("RunSubsystem is not valid");
		return false;
	}

	const bool bStarted = RunSubsystem->StartNewRun(Seed, Width, Height) != nullptr;
	GetDebugRunSnapshot(OutSnapshot);
	return bStarted;
}

bool UGT_DebugSubsystem::DebugMoveTo(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot)
{
	return SubmitDebugCommand(GTDebugCommandType_Move, X, Y, OutSnapshot);
}

bool UGT_DebugSubsystem::DebugScanCell(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot)
{
	return SubmitDebugCommand(GTDebugCommandType_Scan, X, Y, OutSnapshot);
}

bool UGT_DebugSubsystem::DebugExtract(FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	return SubmitDebugCommand(GTDebugCommandType_Extract, PlayerX, PlayerY, OutSnapshot);
}

bool UGT_DebugSubsystem::DebugChooseEventOption(FName OptionId, FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_ChooseEventOption, PlayerX, PlayerY, OutSnapshot, OptionId);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		OutSnapshot.Summary = FString::Printf(
			TEXT("ChooseEventOption rejected: expected current Event room and a registry-valid Event option. OptionId=%s. %s"),
			OptionId.IsNone() ? TEXT("RegistryDefault") : *OptionId.ToString(),
			*OutSnapshot.Summary);
	}

	return bAccepted;
}

bool UGT_DebugSubsystem::DebugResolveCombat(FName ResultId, FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_ResolveCombat, PlayerX, PlayerY, OutSnapshot, ResultId);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		const TCHAR* Reason = ResultId.IsNone()
			? TEXT("expected Combat room and a valid registry default result")
			: TEXT("expected Combat room and a registry-valid placeholder result");
		OutSnapshot.Summary = FString::Printf(
			TEXT("ResolveCombat rejected: %s. Result=%s. %s"),
			Reason,
			ResultId.IsNone() ? TEXT("RegistryDefault") : *ResultId.ToString(),
			*OutSnapshot.Summary);
	}

	return bAccepted;
}

bool UGT_DebugSubsystem::GetDebugRunSnapshot(FGT_DebugRunSnapshot& OutSnapshot) const
{
	OutSnapshot = FGT_DebugRunSnapshot();

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (!QueryFacade || !QueryFacade->HasValidRunContext())
	{
		OutSnapshot.Summary = TEXT("No active run");
		return false;
	}

	OutSnapshot.RunState = QueryFacade->GetRunState();
	OutSnapshot.MapWidth = QueryFacade->GetMapWidth();
	OutSnapshot.MapHeight = QueryFacade->GetMapHeight();
	QueryFacade->TryGetPlayerPosition(OutSnapshot.PlayerX, OutSnapshot.PlayerY);

	FGT_TruthCell CurrentTruthCell;
	if (QueryFacade->GetTruthCellDebugOnly(OutSnapshot.PlayerX, OutSnapshot.PlayerY, CurrentTruthCell))
	{
		OutSnapshot.CurrentRoomBaseType = CurrentTruthCell.RoomBaseType;
		OutSnapshot.CurrentRoomContentId = CurrentTruthCell.RoomContentId;
		OutSnapshot.CurrentRoomRuleId = CurrentTruthCell.RoomRuleId;
		OutSnapshot.CurrentRoomInstanceId = CurrentTruthCell.RoomInstanceId;
		OutSnapshot.bCurrentRoomTriggered = CurrentTruthCell.bTriggered;
		OutSnapshot.bCurrentRoomResolved = CurrentTruthCell.bResolved;

		const UGT_ContentRegistry* ContentRegistry = RunSubsystem ? RunSubsystem->GetContentRegistry() : nullptr;
		FGT_RoomContentDef ContentDefinition;
		FGT_RoomRuleDef RuleDefinition;
		FString DefinitionFailureReason;
		if (ContentRegistry
			&& ContentRegistry->FindRoomDefinitions(
				CurrentTruthCell.RoomContentId,
				CurrentTruthCell.RoomRuleId,
				CurrentTruthCell.RoomBaseType,
				ContentDefinition,
				RuleDefinition,
				DefinitionFailureReason))
		{
			OutSnapshot.CurrentRoomContentDisplayName = ContentDefinition.DisplayName;
			OutSnapshot.CurrentRoomContentDebugDescription = ContentDefinition.DebugDescription;
			OutSnapshot.CurrentRoomRuleDisplayName = RuleDefinition.DisplayName;
			OutSnapshot.CurrentRoomRuleDebugDescription = RuleDefinition.DebugDescription;
		}

		if (ContentRegistry && CurrentTruthCell.RoomBaseType == EGT_RoomBaseType::Event)
		{
			TArray<FGT_EventOptionDef> EventOptionDefinitions;
			if (ContentRegistry->GetEventOptionDefsForRoom(
				CurrentTruthCell.RoomContentId,
				CurrentTruthCell.RoomRuleId,
				EventOptionDefinitions,
				DefinitionFailureReason))
			{
				OutSnapshot.CurrentRoomAvailableEventOptions = BuildEventOptionDefsText(EventOptionDefinitions);
			}
		}
		else if (ContentRegistry && CurrentTruthCell.RoomBaseType == EGT_RoomBaseType::Combat)
		{
			TArray<FGT_CombatResultDef> CombatResultDefinitions;
			if (ContentRegistry->GetCombatResultDefsForRoom(
				CurrentTruthCell.RoomContentId,
				CurrentTruthCell.RoomRuleId,
				CombatResultDefinitions,
				DefinitionFailureReason))
			{
				OutSnapshot.CurrentRoomAvailableCombatResults = BuildCombatResultDefsText(CombatResultDefinitions);
			}
		}
	}

	const UGT_EventBus* EventBus = RunSubsystem ? RunSubsystem->GetEventBus() : nullptr;
	OutSnapshot.EventCount = EventBus ? EventBus->GetEventCount() : 0;
	OutSnapshot.Summary = FString::Printf(
		TEXT("RunState=%d, Player=(%d,%d), Size=%dx%d, EventCount=%d, RoomBaseType=%d, RoomContentId=%s, RoomRuleId=%s, RoomContentName=%s, RoomRuleName=%s, AvailableEventOptions=%s, AvailableCombatResults=%s, RoomTriggered=%s, RoomResolved=%s"),
		static_cast<int32>(OutSnapshot.RunState),
		OutSnapshot.PlayerX,
		OutSnapshot.PlayerY,
		OutSnapshot.MapWidth,
		OutSnapshot.MapHeight,
		OutSnapshot.EventCount,
		static_cast<int32>(OutSnapshot.CurrentRoomBaseType),
		*OutSnapshot.CurrentRoomContentId.ToString(),
		*OutSnapshot.CurrentRoomRuleId.ToString(),
		*OutSnapshot.CurrentRoomContentDisplayName,
		*OutSnapshot.CurrentRoomRuleDisplayName,
		OutSnapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *OutSnapshot.CurrentRoomAvailableEventOptions,
		OutSnapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *OutSnapshot.CurrentRoomAvailableCombatResults,
		OutSnapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
		OutSnapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"));
	return true;
}

bool UGT_DebugSubsystem::GetDebugMiniMapViewData(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const
{
	OutCells.Reset();
	OutWidth = 0;
	OutHeight = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (!QueryFacade || !QueryFacade->HasValidRunContext())
	{
		return false;
	}

	QueryFacade->BuildMiniMapViewData(OutCells, OutWidth, OutHeight);
	for (FGT_MiniMapCellViewData& Cell : OutCells)
	{
		if (!Cell.bVisible && !Cell.bExplored && !Cell.bScanned)
		{
			continue;
		}

		FGT_TruthCell TruthCell;
		if (QueryFacade->GetTruthCellDebugOnly(Cell.X, Cell.Y, TruthCell))
		{
			Cell.VisibleRoomIcon = GetDebugRoomIcon(TruthCell.RoomBaseType);
		}
	}
	return true;
}

void UGT_DebugSubsystem::GetDebugEventSummary(TArray<FGT_DebugEventSummary>& OutEvents) const
{
	OutEvents.Reset();

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_EventBus* EventBus = RunSubsystem ? RunSubsystem->GetEventBus() : nullptr;
	const UGT_ContentRegistry* ContentRegistry = RunSubsystem ? RunSubsystem->GetContentRegistry() : nullptr;
	if (!EventBus)
	{
		return;
	}

	TMap<FName, FGT_DebugEventSummary> EventSummaries;
	for (const FGT_GameEvent& Event : EventBus->GetEventHistory())
	{
		if (Event.EventType.IsNone())
		{
			continue;
		}

		FGT_DebugEventSummary& Summary = EventSummaries.FindOrAdd(Event.EventType);
		Summary.EventType = Event.EventType;
		++Summary.Count;
		Summary.LastContentId = Event.ContentId;
		Summary.LastRuleId = Event.RuleId;
		Summary.LastSequenceId = Event.SequenceId;
		Summary.bLastSuccess = Event.bSuccess;
		Summary.LastPayloadText = Event.PayloadText;

		if (ContentRegistry)
		{
			FGT_RoomContentDef ContentDefinition;
			if (ContentRegistry->FindRoomContentDef(Event.ContentId, ContentDefinition))
			{
				Summary.LastContentDisplayName = ContentDefinition.DisplayName;
			}

			FGT_RoomRuleDef RuleDefinition;
			if (ContentRegistry->FindRoomRuleDef(Event.RuleId, RuleDefinition))
			{
				Summary.LastRuleDisplayName = RuleDefinition.DisplayName;
			}

			if (!Summary.LastContentDisplayName.IsEmpty() || !Summary.LastRuleDisplayName.IsEmpty())
			{
				const FString ContentDescription = ContentDefinition.DebugDescription.IsEmpty() ? TEXT("None") : ContentDefinition.DebugDescription;
				const FString RuleDescription = RuleDefinition.DebugDescription.IsEmpty() ? TEXT("None") : RuleDefinition.DebugDescription;
				Summary.LastDebugDescription = FString::Printf(TEXT("Content=%s Rule=%s"), *ContentDescription, *RuleDescription);
			}
		}
	}

	TArray<FName> EventTypes;
	EventSummaries.GetKeys(EventTypes);
	EventTypes.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& EventType : EventTypes)
	{
		OutEvents.Add(EventSummaries.FindRef(EventType));
	}
}

void UGT_DebugSubsystem::GetDebugCommandHelpLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();
	OutLines.Add(TEXT("Graytail manual play console commands:"));
	OutLines.Add(TEXT("  gt.Help - Show commands and recommended demo flow."));
	OutLines.Add(TEXT("  gt.Commands - Alias for gt.Help."));
	OutLines.Add(TEXT("  gt.StartRun [Seed] [Width Height] - Start a debug/basic run."));
	OutLines.Add(TEXT("  gt.Status - Show run state, player position, current room, and event counts."));
	OutLines.Add(TEXT("  gt.Room - Show current room details and placeholder action hints."));
	OutLines.Add(TEXT("  gt.Move X Y - Move to an adjacent coordinate through the command path."));
	OutLines.Add(TEXT("  gt.Scan X Y - Scan a cell through the command path."));
	OutLines.Add(TEXT("  gt.Extract - Attempt extraction at the current cell."));
	OutLines.Add(TEXT("  gt.Snapshot - Log the raw debug run snapshot."));
	OutLines.Add(TEXT("  gt.Minimap - Log a text minimap."));
	OutLines.Add(TEXT("  gt.Events - Log event type counts and recent payload fields."));
	OutLines.Add(TEXT("  gt.ChooseEventOption [OptionId] - Resolve the Event placeholder room through registry option data."));
	OutLines.Add(TEXT("    Example: gt.ChooseEventOption Event_DebugOption_Continue"));
	OutLines.Add(TEXT("    Example: gt.ChooseEventOption Event_DebugOption_Scout"));
	OutLines.Add(TEXT("  gt.ResolveCombat [Result] - Resolve the Combat placeholder room through registry result data."));
	OutLines.Add(TEXT("    Example: gt.ResolveCombat Combat_DebugResult_Success"));
	OutLines.Add(TEXT("    Example: gt.ResolveCombat Combat_DebugResult_Retreat"));
	OutLines.Add(TEXT("  gt.RunDemo - Run a fixed Event and Combat placeholder demo path."));
	OutLines.Add(TEXT("Recommended manual flow: gt.StartRun -> gt.Minimap -> gt.Move 1 0 -> gt.Scan 1 1 -> gt.Status -> gt.Room."));
	OutLines.Add(TEXT("Event demo path: gt.StartRun -> gt.Move 1 0 -> gt.Move 2 0 -> gt.Move 3 0 -> gt.Move 4 0 -> gt.Move 4 1 -> gt.ChooseEventOption Event_DebugOption_Continue -> gt.Events."));
	OutLines.Add(TEXT("Combat demo path: gt.StartRun -> gt.Move 0 1 -> gt.Move 0 2 -> gt.Move 0 3 -> gt.Move 0 4 -> gt.Move 1 4 -> gt.ResolveCombat Combat_DebugResult_Success -> gt.Events."));
}

bool UGT_DebugSubsystem::GetDebugStatusText(FString& OutStatus) const
{
	FGT_DebugRunSnapshot Snapshot;
	if (!GetDebugRunSnapshot(Snapshot))
	{
		OutStatus = TEXT("gt.Status: No active run. Start with gt.StartRun.");
		return false;
	}

	TArray<FGT_DebugEventSummary> EventSummary;
	GetDebugEventSummary(EventSummary);
	OutStatus = FString::Printf(
		TEXT("gt.Status: RunState=%s PlayerPosition=(%d,%d) RoomBaseType=%s RoomContentId=%s RoomRuleId=%s ContentName=%s RuleName=%s ContentDescription=%s RuleDescription=%s EventOptions=%s CombatResults=%s Triggered=%s Resolved=%s EventCount=%d Events={%s}"),
		*GetRunStateText(Snapshot.RunState),
		Snapshot.PlayerX,
		Snapshot.PlayerY,
		*GetRoomBaseTypeText(Snapshot.CurrentRoomBaseType),
		*Snapshot.CurrentRoomContentId.ToString(),
		*Snapshot.CurrentRoomRuleId.ToString(),
		*Snapshot.CurrentRoomContentDisplayName,
		*Snapshot.CurrentRoomRuleDisplayName,
		*Snapshot.CurrentRoomContentDebugDescription,
		*Snapshot.CurrentRoomRuleDebugDescription,
		Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions,
		Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults,
		Snapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
		Snapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"),
		Snapshot.EventCount,
		*BuildDebugEventSummaryText(EventSummary));
	return true;
}

bool UGT_DebugSubsystem::GetDebugRoomText(FString& OutRoomText) const
{
	FGT_DebugRunSnapshot Snapshot;
	if (!GetDebugRunSnapshot(Snapshot))
	{
		OutRoomText = TEXT("gt.Room: No active run. Start with gt.StartRun.");
		return false;
	}

	FString Hint = TEXT("No placeholder room action is available here.");
	if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event)
	{
		Hint = FString::Printf(
			TEXT("Event placeholder action: gt.ChooseEventOption [OptionId]. Available=%s."),
			Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions);
	}
	else if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat)
	{
		Hint = FString::Printf(
			TEXT("Combat placeholder action: gt.ResolveCombat [Result]. Available=%s."),
			Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults);
	}

	OutRoomText = FString::Printf(
		TEXT("gt.Room: Position=(%d,%d) RoomBaseType=%s RoomContentId=%s RoomRuleId=%s RoomInstanceId=%s ContentName=%s RuleName=%s ContentDescription=%s RuleDescription=%s EventOptions=%s CombatResults=%s Triggered=%s Resolved=%s Hint=%s"),
		Snapshot.PlayerX,
		Snapshot.PlayerY,
		*GetRoomBaseTypeText(Snapshot.CurrentRoomBaseType),
		*Snapshot.CurrentRoomContentId.ToString(),
		*Snapshot.CurrentRoomRuleId.ToString(),
		*Snapshot.CurrentRoomInstanceId.ToString(),
		*Snapshot.CurrentRoomContentDisplayName,
		*Snapshot.CurrentRoomRuleDisplayName,
		*Snapshot.CurrentRoomContentDebugDescription,
		*Snapshot.CurrentRoomRuleDebugDescription,
		Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions,
		Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults,
		Snapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
		Snapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"),
		*Hint);
	return true;
}

bool UGT_DebugSubsystem::DebugRunDemo(TArray<FString>& OutLogLines, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutLogLines.Reset();
	OutSnapshot = FGT_DebugRunSnapshot();

	auto AppendStep = [&OutLogLines](const FString& StepName, bool bAccepted, const FGT_DebugRunSnapshot& Snapshot)
	{
		OutLogLines.Add(FString::Printf(TEXT("%s %s: %s"), *StepName, bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary));
	};

	auto MovePath = [this, &OutLogLines, &OutSnapshot, &AppendStep](const TCHAR* Label, const TArray<FIntPoint>& Path) -> bool
	{
		for (const FIntPoint& Coord : Path)
		{
			const bool bMoved = DebugMoveTo(Coord.X, Coord.Y, OutSnapshot);
			AppendStep(FString::Printf(TEXT("%s gt.Move %d %d"), Label, Coord.X, Coord.Y), bMoved, OutSnapshot);
			if (!bMoved)
			{
				return false;
			}
		}

		return true;
	};

	bool bDemoOk = DebugStartNewRun(42345, 10, 10, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo StartRun"), bDemoOk, OutSnapshot);
	if (!bDemoOk)
	{
		return false;
	}

	const bool bScanned = DebugScanCell(1, 1, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo Scan 1 1"), bScanned, OutSnapshot);
	bDemoOk = bDemoOk && bScanned;

	const TArray<FIntPoint> EventPath = {
		FIntPoint(1, 0),
		FIntPoint(2, 0),
		FIntPoint(3, 0),
		FIntPoint(4, 0),
		FIntPoint(4, 1)
	};
	bDemoOk = bDemoOk && MovePath(TEXT("gt.RunDemo EventPath"), EventPath);

	const bool bChooseEvent = bDemoOk && DebugChooseEventOption(GTEventOption_DefaultContinue, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo ChooseEventOption Event_DebugOption_Continue"), bChooseEvent, OutSnapshot);
	bDemoOk = bDemoOk && bChooseEvent;

	const TArray<FIntPoint> CombatPath = {
		FIntPoint(3, 1),
		FIntPoint(2, 1),
		FIntPoint(1, 1),
		FIntPoint(1, 2),
		FIntPoint(1, 3),
		FIntPoint(1, 4)
	};
	bDemoOk = bDemoOk && MovePath(TEXT("gt.RunDemo CombatPath"), CombatPath);

	const bool bResolveCombat = bDemoOk && DebugResolveCombat(GTCombatResult_Success, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo ResolveCombat Combat_DebugResult_Success"), bResolveCombat, OutSnapshot);
	bDemoOk = bDemoOk && bResolveCombat;

	TArray<FGT_DebugEventSummary> EventSummary;
	GetDebugEventSummary(EventSummary);
	const bool bHasEventResolved = HasDebugEventSummaryType(EventSummary, GTDebugEventType_EventResolved);
	const bool bHasCombatResolved = HasDebugEventSummaryType(EventSummary, GTDebugEventType_CombatResolved);
	OutLogLines.Add(FString::Printf(
		TEXT("gt.RunDemo Events: %s EventResolved=%s CombatResolved=%s"),
		*BuildDebugEventSummaryText(EventSummary),
		bHasEventResolved ? TEXT("true") : TEXT("false"),
		bHasCombatResolved ? TEXT("true") : TEXT("false")));

	FString StatusText;
	GetDebugStatusText(StatusText);
	OutLogLines.Add(StatusText);
	return bDemoOk && bHasEventResolved && bHasCombatResolved;
}

void UGT_DebugSubsystem::GetCurrentMiniMapDebugCells(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const
{
	GetDebugMiniMapViewData(OutCells, OutWidth, OutHeight);
}

bool UGT_DebugSubsystem::RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults)
{
	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	UGT_RuntimeSmokeValidator* Validator = NewObject<UGT_RuntimeSmokeValidator>(this);
	Validator->Initialize(RunSubsystem);
	Validator->SetDebugSubsystem(this);
	return Validator->RunMinimalMovementSmokeTest(OutResults);
}

UGT_RunSubsystem* UGT_DebugSubsystem::GetRunSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
}

bool UGT_DebugSubsystem::SubmitDebugCommand(FName CommandType, int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot, FName PayloadId)
{
	OutSnapshot = FGT_DebugRunSnapshot();

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	if (!RunSubsystem)
	{
		OutSnapshot.Summary = TEXT("RunSubsystem is not valid");
		return false;
	}

	const UGT_QueryFacade* QueryFacade = RunSubsystem->GetQueryFacade();
	const FName PlayerActorId = QueryFacade && QueryFacade->HasValidRunContext()
		? QueryFacade->GetPlayerActorId()
		: GTDebugActorId_Player;

	FGT_Command Command;
	Command.CommandType = CommandType;
	Command.SourceActorId = PlayerActorId;
	Command.TargetActorId = PlayerActorId;
	Command.TargetX = X;
	Command.TargetY = Y;
	Command.PayloadId = PayloadId;

	const bool bAccepted = RunSubsystem->SubmitCommand(Command);
	GetDebugRunSnapshot(OutSnapshot);
	return bAccepted;
}
