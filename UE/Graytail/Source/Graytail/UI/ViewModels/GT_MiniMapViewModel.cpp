#include "UI/ViewModels/GT_MiniMapViewModel.h"

TSet<FIntPoint> GT_NeighborhoodSensingViewModel::FindSafeUnknownNeighbors(
	const TArray<FGT_MiniMapCellViewData>& Cells,
	int32 Width,
	int32 Height,
	const FIntPoint& Center,
	const TSet<FIntPoint>& FlaggedCells)
{
	TSet<FIntPoint> UnknownNeighbors;
	if (Width <= 0
		|| Height <= 0
		|| Cells.Num() != Width * Height
		|| Center.X < 0
		|| Center.Y < 0
		|| Center.X >= Width
		|| Center.Y >= Height)
	{
		return UnknownNeighbors;
	}

	const FGT_MiniMapCellViewData& CenterCell = Cells[Center.Y * Width + Center.X];
	if (!CenterCell.bScanned)
	{
		return UnknownNeighbors;
	}

	int32 KnownMineCount = 0;
	for (int32 DeltaY = -1; DeltaY <= 1; ++DeltaY)
	{
		for (int32 DeltaX = -1; DeltaX <= 1; ++DeltaX)
		{
			if (DeltaX == 0 && DeltaY == 0)
			{
				continue;
			}

			const FIntPoint Neighbor(Center.X + DeltaX, Center.Y + DeltaY);
			if (Neighbor.X < 0 || Neighbor.Y < 0 || Neighbor.X >= Width || Neighbor.Y >= Height)
			{
				continue;
			}

			const FGT_MiniMapCellViewData& Cell = Cells[Neighbor.Y * Width + Neighbor.X];
			const bool bKnown = Cell.bExplored || Cell.bVisible;
			const bool bKnownMine = bKnown && Cell.VisibleRoomIcon == FName(TEXT("Mine"));
			const bool bFlaggedMine = !bKnown && FlaggedCells.Contains(Neighbor);
			if (bKnownMine || bFlaggedMine)
			{
				++KnownMineCount;
			}
			else if (!bKnown)
			{
				UnknownNeighbors.Add(Neighbor);
			}
		}
	}

	if (KnownMineCount != CenterCell.DisplayedNumber)
	{
		UnknownNeighbors.Reset();
	}
	return UnknownNeighbors;
}

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
