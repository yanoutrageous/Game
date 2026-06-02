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

FName UGT_QueryFacade::GetPlayerActorId() const
{
	return HasValidRunContext() ? RunContext->GetPlayerActorId() : NAME_None;
}

bool UGT_QueryFacade::TryGetPlayerPosition(int32& OutX, int32& OutY) const
{
	if (!HasValidRunContext())
	{
		OutX = 0;
		OutY = 0;
		return false;
	}

	return RunContext->TryGetPlayerPosition(OutX, OutY);
}

bool UGT_QueryFacade::GetActorStates(TArray<FGT_ActorRuntimeState>& OutActors) const
{
	OutActors.Reset();

	if (!HasValidRunContext())
	{
		return false;
	}

	OutActors = RunContext->GetActorStates();
	return true;
}

bool UGT_QueryFacade::GetIntelCellViewData(int32 X, int32 Y, FGT_MiniMapCellViewData& OutCell) const
{
	OutCell = FGT_MiniMapCellViewData();

	if (!HasValidRunContext())
	{
		return false;
	}

	const FGT_IntelMap& IntelMap = RunContext->GetPlayerIntelMap();
	const FGT_IntelCell* IntelCell = IntelMap.GetCellConst(X, Y);
	if (!IntelCell)
	{
		return false;
	}

	OutCell.X = IntelCell->X;
	OutCell.Y = IntelCell->Y;
	OutCell.bVisible = IntelCell->bVisible;
	OutCell.bExplored = IntelCell->bExplored;
	OutCell.bScanned = IntelCell->bScanned;
	OutCell.DisplayedNumber = IntelCell->DisplayedNumber;
	OutCell.MarkerState = IntelCell->MarkerState;
	OutCell.VisibleRoomIcon = IntelCell->VisibleRoomIcon;
	OutCell.bStale = IntelCell->bStale;
	OutCell.ReliabilityState = IntelCell->ReliabilityState;
	return true;
}

bool UGT_QueryFacade::IsIntelCellExplored(int32 X, int32 Y) const
{
	FGT_MiniMapCellViewData Cell;
	return GetIntelCellViewData(X, Y, Cell) && Cell.bExplored;
}

bool UGT_QueryFacade::IsIntelCellVisible(int32 X, int32 Y) const
{
	FGT_MiniMapCellViewData Cell;
	return GetIntelCellViewData(X, Y, Cell) && Cell.bVisible;
}

bool UGT_QueryFacade::GetTruthCellDebugOnly(int32 X, int32 Y, FGT_TruthCell& OutCell) const
{
	OutCell = FGT_TruthCell();

	if (!HasValidRunContext())
	{
		return false;
	}

	const FGT_TruthMap& TruthMap = RunContext->GetTruthMapForDebugOnly();
	const FGT_TruthCell* TruthCell = TruthMap.GetCellConst(X, Y);
	if (!TruthCell)
	{
		return false;
	}

	OutCell = *TruthCell;
	return true;
}

bool UGT_QueryFacade::IsTruthMineDebugOnly(int32 X, int32 Y) const
{
	if (!HasValidRunContext())
	{
		return false;
	}

	return RunContext->GetTruthMapForDebugOnly().IsMine(X, Y);
}

bool UGT_QueryFacade::IsTruthExitDebugOnly(int32 X, int32 Y) const
{
	if (!HasValidRunContext())
	{
		return false;
	}

	return RunContext->GetTruthMapForDebugOnly().IsExit(X, Y);
}

bool UGT_QueryFacade::GetTruthAdjacentCoords4DebugOnly(int32 X, int32 Y, TArray<FIntPoint>& OutCoords) const
{
	OutCoords.Reset();

	if (!HasValidRunContext())
	{
		return false;
	}

	return RunContext->GetTruthMapForDebugOnly().GetAdjacentCoords4(X, Y, OutCoords);
}

bool UGT_QueryFacade::GetTruthAdjacentCoords8DebugOnly(int32 X, int32 Y, TArray<FIntPoint>& OutCoords) const
{
	OutCoords.Reset();

	if (!HasValidRunContext())
	{
		return false;
	}

	return RunContext->GetTruthMapForDebugOnly().GetAdjacentCoords8(X, Y, OutCoords);
}
