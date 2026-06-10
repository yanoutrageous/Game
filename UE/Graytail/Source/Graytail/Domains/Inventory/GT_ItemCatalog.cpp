#include "Domains/Inventory/GT_ItemCatalog.h"

namespace
{
	FGT_ItemCatalogEntry MakeItemDef(const TCHAR* ItemId, const TCHAR* DisplayName, EGT_ItemKind Kind, const TCHAR* Rarity, int32 Value, const TCHAR* EffectText, const TCHAR* Description)
	{
		FGT_ItemCatalogEntry Def;
		Def.ItemId = FName(ItemId);
		Def.DisplayName = DisplayName;
		Def.Kind = Kind;
		Def.Rarity = FName(Rarity);
		Def.Value = Value;
		Def.EffectText = EffectText;
		Def.Description = Description;
		return Def;
	}
}

namespace GT_ItemCatalog
{
	const TArray<FGT_ItemCatalogEntry>& GetAllItemDefs()
	{
		// 对齐 Lua RunInventory.ITEM_DEFS(顺序一致, 文案一致)。
		static const TArray<FGT_ItemCatalogEntry> ItemDefs = {
			MakeItemDef(TEXT("broken_copper_wire"), TEXT("断裂铜线"), EGT_ItemKind::Relic, TEXT("common"), 8,
				TEXT(""), TEXT("仍然能卖钱，这已经很难得了。")),
			MakeItemDef(TEXT("dim_capacitor"), TEXT("暗淡电容"), EGT_ItemKind::Relic, TEXT("common"), 10,
				TEXT(""), TEXT("拆下来时它轻轻响了一声，像是在叹气。")),
			MakeItemDef(TEXT("whisper_wick"), TEXT("低语灯芯"), EGT_ItemKind::Relic, TEXT("rare"), 45,
				TEXT(""), TEXT("它在没有电源的情况下发光，并且偶尔像在催你下班。")),
			MakeItemDef(TEXT("sealed_core_shard"), TEXT("封存核心碎片"), EGT_ItemKind::Relic, TEXT("rare"), 45,
				TEXT(""), TEXT("被封条压住的裂片仍在缓慢发热。")),
			MakeItemDef(TEXT("emergency_bandage"), TEXT("应急止血贴"), EGT_ItemKind::Consumable, TEXT("common"), 10,
				TEXT("恢复少量生命。"), TEXT("后勤部称它经过消毒。包装上的日期不建议细看。")),
			MakeItemDef(TEXT("static_lens"), TEXT("静电透镜"), EGT_ItemKind::Tool, TEXT("uncommon"), 16,
				TEXT("可作为后续扫描设备材料。"), TEXT("透过它看灯光时，会看见不存在的边界线。")),
			MakeItemDef(TEXT("blackbox_tag"), TEXT("黑匣标签"), EGT_ItemKind::Record, TEXT("uncommon"), 18,
				TEXT(""), TEXT("标签上的编号被刮掉了，只剩下回收部门的旧印章。")),
		};
		return ItemDefs;
	}

	const FGT_ItemCatalogEntry* FindItemDef(FName ItemId)
	{
		if (ItemId.IsNone())
		{
			return nullptr;
		}

		return GetAllItemDefs().FindByPredicate([ItemId](const FGT_ItemCatalogEntry& Def)
		{
			return Def.ItemId == ItemId;
		});
	}

	int32 GetItemValue(FName ItemId)
	{
		const FGT_ItemCatalogEntry* Def = FindItemDef(ItemId);
		return Def ? Def->Value : 0;
	}

	FName GetQualityItemId(EGT_ItemQuality Quality)
	{
		// 对齐 Lua QUALITY_ITEMS 表。
		switch (Quality)
		{
		case EGT_ItemQuality::Low:
			return FName(TEXT("broken_copper_wire"));
		case EGT_ItemQuality::Common:
			return FName(TEXT("dim_capacitor"));
		case EGT_ItemQuality::Rare:
			return FName(TEXT("static_lens"));
		case EGT_ItemQuality::Precious:
			return FName(TEXT("whisper_wick"));
		case EGT_ItemQuality::Abnormal:
			return FName(TEXT("sealed_core_shard"));
		default:
			return NAME_None;
		}
	}

	int32 GetCarriedItemsValue(const TArray<FGT_ItemStack>& Stacks)
	{
		int32 TotalValue = 0;
		for (const FGT_ItemStack& Stack : Stacks)
		{
			TotalValue += GetItemValue(Stack.ItemId) * Stack.Count;
		}
		return TotalValue;
	}
}
