#include "Core/GT_RunSubsystem.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_CommandProcessor.h"
#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EffectSystem.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunContext.h"

void UGT_RunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CommandBus = NewObject<UGT_CommandBus>(this);
	CommandProcessor = NewObject<UGT_CommandProcessor>(this);
	EventBus = NewObject<UGT_EventBus>(this);
	EffectSystem = NewObject<UGT_EffectSystem>(this);
	ContentRegistry = NewObject<UGT_ContentRegistry>(this);
	QueryFacade = NewObject<UGT_QueryFacade>(this);
}

void UGT_RunSubsystem::Deinitialize()
{
	EndCurrentRun();

	CommandBus = nullptr;
	CommandProcessor = nullptr;
	EventBus = nullptr;
	EffectSystem = nullptr;
	ContentRegistry = nullptr;
	QueryFacade = nullptr;

	Super::Deinitialize();
}

UGT_RunContext* UGT_RunSubsystem::StartNewRun(int32 Seed, int32 Width, int32 Height)
{
	CurrentRunContext = NewObject<UGT_RunContext>(this);
	CurrentRunContext->InitializeRun(Seed, Width, Height);

	if (QueryFacade)
	{
		QueryFacade->Initialize(CurrentRunContext);
	}

	if (CommandProcessor)
	{
		CommandProcessor->Initialize(CurrentRunContext, EventBus);
	}

	if (EventBus)
	{
		int32 PlayerX = 0;
		int32 PlayerY = 0;
		CurrentRunContext->TryGetPlayerPosition(PlayerX, PlayerY);

		FGT_GameEvent Event;
		Event.EventType = FName(TEXT("RunStarted"));
		Event.SourceSystem = FName(TEXT("RunSubsystem"));
		Event.SourceActorId = CurrentRunContext->GetPlayerActorId();
		Event.TargetActorId = CurrentRunContext->GetPlayerActorId();
		Event.X = PlayerX;
		Event.Y = PlayerY;
		Event.RoomCoord = FIntPoint(PlayerX, PlayerY);
		Event.bSuccess = true;
		EventBus->PublishEvent(Event);
	}

	return CurrentRunContext;
}

bool UGT_RunSubsystem::SubmitCommand(const FGT_Command& Command)
{
	if (!CommandBus || !CommandProcessor)
	{
		return false;
	}

	CommandBus->SubmitCommand(Command);
	return CommandProcessor->ProcessCommand(Command);
}

UGT_RunContext* UGT_RunSubsystem::GetCurrentRunContext() const
{
	return CurrentRunContext;
}

void UGT_RunSubsystem::EndCurrentRun()
{
	if (CurrentRunContext)
	{
		CurrentRunContext->ResetRun();
		CurrentRunContext = nullptr;
	}

	if (QueryFacade)
	{
		QueryFacade->Reset();
	}

	if (CommandProcessor)
	{
		CommandProcessor->Initialize(nullptr, EventBus);
	}
}

UGT_CommandBus* UGT_RunSubsystem::GetCommandBus() const
{
	return CommandBus;
}

UGT_EventBus* UGT_RunSubsystem::GetEventBus() const
{
	return EventBus;
}

UGT_EffectSystem* UGT_RunSubsystem::GetEffectSystem() const
{
	return EffectSystem;
}

UGT_ContentRegistry* UGT_RunSubsystem::GetContentRegistry() const
{
	return ContentRegistry;
}

UGT_QueryFacade* UGT_RunSubsystem::GetQueryFacade() const
{
	return QueryFacade;
}
