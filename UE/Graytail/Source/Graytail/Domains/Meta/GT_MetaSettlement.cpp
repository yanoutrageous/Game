#include "Domains/Meta/GT_MetaSettlement.h"

#include "Core/GT_RunContext.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"

namespace GT_MetaSettlement
{
	void SettleExtraction(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta)
	{
		const FGT_RunInventoryState& Inv = Run.GetRunInventory();

		FGT_ExtractionReward Reward;
		Reward.DirectGold = Inv.PendingGold + Inv.SafeGold;   // 对齐 Lua directGold = pending+safe
		Reward.LoosePartsGold = 0;                            // Lua GetExtractionReward 写死 0(零散零件不折金)
		for (const FGT_ItemStack& Stack : Inv.CarriedItems)
		{
			if (Stack.ItemId.IsNone() || Stack.Count <= 0)
			{
				continue;
			}
			FGT_RewardItem Item;
			Item.ItemId = Stack.ItemId;
			Item.Count = Stack.Count;
			Item.Value = GT_ItemCatalog::GetItemValue(Stack.ItemId);
			Reward.CarriedItems.Add(Item);
		}

		Meta.RecordExtractionReward(Reward);
	}

	void SettleFailure(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta)
	{
		const FGT_RunInventoryState& Inv = Run.GetRunInventory();

		// 抢救最高价值 1 件(对齐 Lua: 遍历 carriedItems 取 itemBaseValue 最大者, count=1)。
		FName BestId = NAME_None;
		int32 BestValue = -1;
		for (const FGT_ItemStack& Stack : Inv.CarriedItems)
		{
			if (Stack.ItemId.IsNone() || Stack.Count <= 0)
			{
				continue;
			}
			const int32 Value = GT_ItemCatalog::GetItemValue(Stack.ItemId);
			if (Value > BestValue)
			{
				BestValue = Value;
				BestId = Stack.ItemId;
			}
		}

		// 金币 = SafeGold + 撤离天赋补币(PendingGold 丢失; canSalvagePart=false 零件不抢救)。
		const int32 FinalGold = Inv.SafeGold + Meta.GetTalentEffects().FailureGoldBonus;
		if (FinalGold > 0)
		{
			Meta.AddGold(FinalGold);
		}

		if (!BestId.IsNone())
		{
			FGT_RewardItem Item;
			Item.ItemId = BestId;
			Item.Count = 1;
			Item.Value = FMath::Max(0, BestValue);
			TArray<FGT_RewardItem> Salvaged;
			Salvaged.Add(Item);
			Meta.AddWarehouseItems(Salvaged, FName(TEXT("recovered")));
			Meta.Save();   // AddWarehouseItems 不自存, 末尾补一次落盘
		}
	}
}
