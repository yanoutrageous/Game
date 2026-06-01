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

	return FString::Printf(
		TEXT("RunId=%s, Seed=%d, Size=%dx%d"),
		*QueryFacade->GetRunId().ToString(),
		QueryFacade->GetSeed(),
		QueryFacade->GetMapWidth(),
		QueryFacade->GetMapHeight());
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
