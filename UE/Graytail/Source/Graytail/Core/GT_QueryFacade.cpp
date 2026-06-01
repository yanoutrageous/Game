#include "Core/GT_QueryFacade.h"

#include "Core/GT_RunContext.h"

void UGT_QueryFacade::Initialize(UGT_RunContext* InRunContext)
{
	RunContext = InRunContext;
}

void UGT_QueryFacade::Reset()
{
	RunContext = nullptr;
}

bool UGT_QueryFacade::HasValidRunContext() const
{
	return IsValid(RunContext);
}

FGuid UGT_QueryFacade::GetRunId() const
{
	return HasValidRunContext() ? RunContext->GetRunId() : FGuid();
}

int32 UGT_QueryFacade::GetSeed() const
{
	return HasValidRunContext() ? RunContext->GetSeed() : 0;
}

int32 UGT_QueryFacade::GetMapWidth() const
{
	return HasValidRunContext() ? RunContext->GetMapWidth() : 0;
}

int32 UGT_QueryFacade::GetMapHeight() const
{
	return HasValidRunContext() ? RunContext->GetMapHeight() : 0;
}

void UGT_QueryFacade::BuildMiniMapViewData(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const
{
	OutCells.Reset();
	OutWidth = 0;
	OutHeight = 0;

	if (!HasValidRunContext())
	{
		return;
	}

	const FGT_IntelMap& IntelMap = RunContext->GetPlayerIntelMap();
	OutWidth = IntelMap.Width;
	OutHeight = IntelMap.Height;
	OutCells.Reserve(IntelMap.Cells.Num());

	for (const FGT_IntelCell& IntelCell : IntelMap.Cells)
	{
		FGT_MiniMapCellViewData ViewData;
		ViewData.X = IntelCell.X;
		ViewData.Y = IntelCell.Y;
		ViewData.bVisible = IntelCell.bVisible;
		ViewData.bExplored = IntelCell.bExplored;
		ViewData.bScanned = IntelCell.bScanned;
		ViewData.DisplayedNumber = IntelCell.DisplayedNumber;
		ViewData.MarkerState = IntelCell.MarkerState;
		ViewData.VisibleRoomIcon = IntelCell.VisibleRoomIcon;
		ViewData.bStale = IntelCell.bStale;
		ViewData.ReliabilityState = IntelCell.ReliabilityState;

		OutCells.Add(ViewData);
	}
}
