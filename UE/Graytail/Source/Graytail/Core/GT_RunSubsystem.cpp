#include "Core/GT_RunSubsystem.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EffectSystem.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_RunContext.h"

void UGT_RunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CommandBus = NewObject<UGT_CommandBus>(this);
	EventBus = NewObject<UGT_EventBus>(this);
	EffectSystem = NewObject<UGT_EffectSystem>(this);
	ContentRegistry = NewObject<UGT_ContentRegistry>(this);
}

void UGT_RunSubsystem::Deinitialize()
{
	EndCurrentRun();

	CommandBus = nullptr;
	EventBus = nullptr;
	EffectSystem = nullptr;
	ContentRegistry = nullptr;

	Super::Deinitialize();
}

UGT_RunContext* UGT_RunSubsystem::StartNewRun(int32 Seed, int32 Width, int32 Height)
{
	CurrentRunContext = NewObject<UGT_RunContext>(this);
	CurrentRunContext->InitializeRun(Seed, Width, Height);
	return CurrentRunContext;
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
