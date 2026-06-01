#include "UI/ViewModels/GT_MiniMapViewModel.h"

void UGT_MiniMapViewModel::BuildFromIntelMap(const FGT_IntelMap& IntelMap)
{
	Width = IntelMap.Width;
	Height = IntelMap.Height;

	Cells.Reset();
	Cells.Reserve(IntelMap.Cells.Num());

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

		Cells.Add(ViewData);
	}
}

void UGT_MiniMapViewModel::Reset()
{
	Cells.Reset();
	Width = 0;
	Height = 0;
}

TArray<FGT_MiniMapCellViewData> UGT_MiniMapViewModel::GetCells() const
{
	return Cells;
}

int32 UGT_MiniMapViewModel::GetWidth() const
{
	return Width;
}

int32 UGT_MiniMapViewModel::GetHeight() const
{
	return Height;
}
