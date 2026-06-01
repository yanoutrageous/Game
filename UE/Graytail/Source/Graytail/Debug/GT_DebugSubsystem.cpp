#include "Debug/GT_DebugSubsystem.h"

#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunSubsystem.h"
#include "Engine/GameInstance.h"

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
	const UGT_RunSubsystem* RunSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
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

void UGT_DebugSubsystem::GetCurrentMiniMapDebugCells(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const
{
	OutCells.Reset();
	OutWidth = 0;
	OutHeight = 0;

	const UGT_RunSubsystem* RunSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;

	if (QueryFacade)
	{
		QueryFacade->BuildMiniMapViewData(OutCells, OutWidth, OutHeight);
	}
}
