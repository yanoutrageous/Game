#include "Debug/GT_DebugSubsystem.h"

#include "Core/GT_CommandBus.h"
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
	const FName GTDebugActorId_Player(TEXT("Player"));
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

	const UGT_EventBus* EventBus = RunSubsystem ? RunSubsystem->GetEventBus() : nullptr;
	OutSnapshot.EventCount = EventBus ? EventBus->GetEventCount() : 0;
	OutSnapshot.Summary = FString::Printf(
		TEXT("RunState=%d, Player=(%d,%d), Size=%dx%d, EventCount=%d"),
		static_cast<int32>(OutSnapshot.RunState),
		OutSnapshot.PlayerX,
		OutSnapshot.PlayerY,
		OutSnapshot.MapWidth,
		OutSnapshot.MapHeight,
		OutSnapshot.EventCount);
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
	return true;
}

void UGT_DebugSubsystem::GetDebugEventSummary(TArray<FGT_DebugEventSummary>& OutEvents) const
{
	OutEvents.Reset();

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_EventBus* EventBus = RunSubsystem ? RunSubsystem->GetEventBus() : nullptr;
	if (!EventBus)
	{
		return;
	}

	TMap<FName, int32> EventTypeCounts;
	EventBus->GetEventTypeCounts(EventTypeCounts);

	TArray<FName> EventTypes;
	EventTypeCounts.GetKeys(EventTypes);
	EventTypes.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& EventType : EventTypes)
	{
		FGT_DebugEventSummary Summary;
		Summary.EventType = EventType;
		Summary.Count = EventTypeCounts.FindRef(EventType);
		OutEvents.Add(Summary);
	}
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

bool UGT_DebugSubsystem::SubmitDebugCommand(FName CommandType, int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot)
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

	const bool bAccepted = RunSubsystem->SubmitCommand(Command);
	GetDebugRunSnapshot(OutSnapshot);
	return bAccepted;
}
