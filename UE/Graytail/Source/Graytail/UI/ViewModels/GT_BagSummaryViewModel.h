#pragma once

#include "CoreMinimal.h"
#include "Domains/Inventory/GT_InventoryTypes.h"

namespace GT_BagSummaryViewModel
{
	inline constexpr int32 VisibleRowCount = 4;
	inline constexpr float RowHeight = 48.f;
	inline constexpr float RowSpacing = 4.f;
	inline constexpr float CardHeight = RowHeight - RowSpacing;
	inline constexpr float ViewportHeight = VisibleRowCount * RowHeight;

	GRAYTAIL_API void SortForDisplay(TArray<FGT_ItemStack>& Stacks);
}
