#include "Domains/Meta/GT_MetaSettlement.h"

#include "Core/GT_RunContext.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Meta/GT_MetaCatalog.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"

namespace GT_MetaSettlement
{
	FGT_MetaOperationResult SettleExtraction(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta)
	{
		const FGT_RunInventoryState& Inv = Run.GetRunInventory();

		FGT_ExtractionReward Reward;
		TMap<FName, int32> ReturnedConsumables;
		int32 DirectGold = Inv.PendingGold + Inv.SafeGold;   // 对齐 Lua directGold = pending+safe
		int32 SettleGoldBonusPercent = 0;
		for (const FName& EquippedId : Meta.GetEquippedItems())
		{
			const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(EquippedId);
			if (Def && Def->Trigger == EGT_ItemTrigger::SettleGoldBonus)
			{
				SettleGoldBonusPercent += Def->TriggerAmount;
			}
		}
		if (SettleGoldBonusPercent > 0)
		{
			DirectGold = FMath::RoundToInt(
				static_cast<double>(DirectGold)
				* (100.0 + static_cast<double>(SettleGoldBonusPercent))
				/ 100.0);
		}
		Reward.DirectGold = DirectGold;
		Reward.LoosePartsGold = 0;                            // Lua GetExtractionReward 写死 0(零散零件不折金)
		for (const FGT_ItemStack& Stack : Inv.CarriedItems)
		{
			if (Stack.ItemId.IsNone() || Stack.Count <= 0)
			{
				continue;
			}
			// 带入消耗品(loadout 灌入, 与回收物混在 CarriedItems): 撤离成功退回库存
			// (风险投资 = 活着拿回未用的), 不当回收物结算。失败/放弃不走这里 → 自然全损失。
			const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
			if (Def && Def->Kind == EGT_ItemKind::Consumable)
			{
				ReturnedConsumables.FindOrAdd(Stack.ItemId) += Stack.Count;
				continue;
			}
			FGT_RewardItem Item;
			Item.ItemId = Stack.ItemId;
			Item.Count = Stack.Count;
			Item.Value = GT_ItemCatalog::GetItemValue(Stack.ItemId);
			Reward.CarriedItems.Add(Item);
		}

		return Meta.CommitExtraction(Reward, ReturnedConsumables);
	}

	FGT_MetaOperationResult SettleFailure(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta)
	{
		const FGT_RunInventoryState& Inv = Run.GetRunInventory();
		FGT_FailureSettlement Settlement;
		Settlement.Reason = Run.GetRunEndReason();

		// 抢救最高价值 1 件(对齐 Lua: 遍历 carriedItems 取 itemBaseValue 最大者, count=1)。
		FName BestId = NAME_None;
		int32 BestValue = -1;
		for (const FGT_ItemStack& Stack : Inv.CarriedItems)
		{
			if (Stack.ItemId.IsNone() || Stack.Count <= 0)
			{
				continue;
			}
			// 消耗品(loadout 带入)死亡全损失, 不参与抢救(只抢救回收物, 对齐 Lua carriedItems 语义)。
			const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
			if (Def && Def->Kind == EGT_ItemKind::Consumable)
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
		Settlement.Gold = Inv.SafeGold + Meta.GetTalentEffects().FailureGoldBonus;

		if (!BestId.IsNone())
		{
			FGT_RewardItem Item;
			Item.ItemId = BestId;
			Item.Count = 1;
			Item.Value = FMath::Max(0, BestValue);
			Settlement.SalvagedItems.Add(Item);
		}
		return Meta.CommitFailure(Settlement);
	}
}
