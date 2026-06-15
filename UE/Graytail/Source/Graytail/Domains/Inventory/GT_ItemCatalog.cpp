#include "Domains/Inventory/GT_ItemCatalog.h"

#include "Data/GT_ItemDef.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 物品定义资产(由 Content/Python/create_item_defs.py 生成)。
	// 顺序即表顺序, 对齐 Lua RunInventory.ITEM_DEFS。
	const TCHAR* GTItemDefAssetPaths[] = {
		TEXT("/Game/Graytail/Items/Defs/broken_copper_wire"),
		TEXT("/Game/Graytail/Items/Defs/dim_capacitor"),
		TEXT("/Game/Graytail/Items/Defs/whisper_wick"),
		TEXT("/Game/Graytail/Items/Defs/sealed_core_shard"),
		TEXT("/Game/Graytail/Items/Defs/emergency_bandage"),
		TEXT("/Game/Graytail/Items/Defs/static_lens"),
		TEXT("/Game/Graytail/Items/Defs/blackbox_tag"),
	};

	FGT_ItemCatalogEntry MakeEntryFromAsset(const UGT_ItemDef& Asset)
	{
		FGT_ItemCatalogEntry Entry;
		Entry.ItemId = Asset.ContentId;
		Entry.DisplayName = Asset.DisplayName.ToString();
		Entry.Kind = Asset.Kind;
		Entry.Rarity = Asset.Rarity;
		Entry.Value = Asset.Value;
		Entry.EffectText = Asset.EffectText;
		Entry.Description = Asset.Description.ToString();
		return Entry;
	}

	TArray<FGT_ItemCatalogEntry> LoadItemDefsFromAssets()
	{
		TArray<FGT_ItemCatalogEntry> Entries;
		for (const TCHAR* AssetPath : GTItemDefAssetPaths)
		{
			const FString ObjectPath = FString(AssetPath) + TEXT(".") + FPackageName::GetShortName(AssetPath);
			if (const UGT_ItemDef* Asset = LoadObject<UGT_ItemDef>(nullptr, *ObjectPath))
			{
				Entries.Add(MakeEntryFromAsset(*Asset));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("GT_ItemCatalog: missing item def asset %s (run create_item_defs.py?)"), AssetPath);
			}
		}
		return Entries;
	}
}

namespace GT_ItemCatalog
{
	const TArray<FGT_ItemCatalogEntry>& GetAllItemDefs()
	{
		// 进程内只加载一次; 资产数据随后以值拷贝缓存, 不持有 UObject 引用(GC 安全)。
		static const TArray<FGT_ItemCatalogEntry> ItemDefs = LoadItemDefsFromAssets();
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
		// 对齐 Lua QUALITY_ITEMS 表。掉落档位 -> 物品是平衡性规则, 留在代码里。
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

	FString GetItemIconAssetPath(FName ItemId)
	{
		// 逐物品图标(2026-06-15 接入局外组美术, 写实画风, assets/item_recovered/<id>.png
		// 经 _import_item_icons.py 导成 uasset)。覆盖原"全部回收物共用矿块图"的占位。
		static const TMap<FName, const TCHAR*> IconById = {
			{ FName(TEXT("broken_copper_wire")), TEXT("/Game/Graytail/Items/Recovered/broken_copper_wire") },
			{ FName(TEXT("dim_capacitor")),      TEXT("/Game/Graytail/Items/Recovered/dim_capacitor") },
			{ FName(TEXT("whisper_wick")),       TEXT("/Game/Graytail/Items/Recovered/whisper_wick") },
			{ FName(TEXT("sealed_core_shard")),  TEXT("/Game/Graytail/Items/Recovered/sealed_core_shard") },
			{ FName(TEXT("static_lens")),        TEXT("/Game/Graytail/Items/Recovered/static_lens") },
			{ FName(TEXT("blackbox_tag")),       TEXT("/Game/Graytail/Items/Recovered/blackbox_tag") },
		};
		if (const TCHAR* const* Found = IconById.Find(ItemId))
		{
			return *Found;
		}
		// 止血贴沿用部署绷带图; 未单独配图的物品退回共用占位(消耗品=医疗包 / 其余=矿块)。
		if (ItemId == FName(TEXT("emergency_bandage")))
		{
			return TEXT("/Game/Graytail/UI/deploy/ui_icon_bandage");
		}
		const FGT_ItemCatalogEntry* Def = FindItemDef(ItemId);
		return (Def && Def->Kind == EGT_ItemKind::Consumable)
			? FString(TEXT("/Game/Graytail/Items/Consumable/item_consumable_medkit"))
			: FString(TEXT("/Game/Graytail/Items/Recovered/item_recovered_ore"));
	}
}
