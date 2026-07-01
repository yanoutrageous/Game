#include "UI/ViewModels/GT_BagSummaryViewModel.h"

#include "Domains/Inventory/GT_ItemCatalog.h"

namespace
{
	int32 GetRarityRank(FName Rarity)
	{
		if (Rarity == FName(TEXT("mythic"))) { return 4; }
		if (Rarity == FName(TEXT("legendary"))) { return 3; }
		if (Rarity == FName(TEXT("epic"))) { return 2; }
		if (Rarity == FName(TEXT("rare"))) { return 1; }
		return 0;
	}
}

namespace GT_BagSummaryViewModel
{
	void SortForDisplay(TArray<FGT_ItemStack>& Stacks)
	{
		Stacks.Sort([](const FGT_ItemStack& Left, const FGT_ItemStack& Right)
		{
			const FGT_ItemCatalogEntry* LeftDef = GT_ItemCatalog::FindItemDef(Left.ItemId);
			const FGT_ItemCatalogEntry* RightDef = GT_ItemCatalog::FindItemDef(Right.ItemId);
			const int32 LeftRank = GetRarityRank(LeftDef ? LeftDef->Rarity : NAME_None);
			const int32 RightRank = GetRarityRank(RightDef ? RightDef->Rarity : NAME_None);
			if (LeftRank != RightRank)
			{
				return LeftRank > RightRank;
			}

			const int32 LeftValue = LeftDef ? LeftDef->Value : 0;
			const int32 RightValue = RightDef ? RightDef->Value : 0;
			if (LeftValue != RightValue)
			{
				return LeftValue > RightValue;
			}
			return Left.ItemId.LexicalLess(Right.ItemId);
		});
	}
}
