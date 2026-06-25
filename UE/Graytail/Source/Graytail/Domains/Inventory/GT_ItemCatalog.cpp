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
		// 2026-06-24 新增 8 件异常回收物(Zuhe2 §9)。
		TEXT("/Game/Graytail/Items/Defs/old_gear"),
		TEXT("/Game/Graytail/Items/Defs/broken_terminal"),
		TEXT("/Game/Graytail/Items/Defs/dead_battery"),
		TEXT("/Game/Graytail/Items/Defs/old_gauge"),
		TEXT("/Game/Graytail/Items/Defs/damaged_circuit"),
		TEXT("/Game/Graytail/Items/Defs/data_disk"),
		TEXT("/Game/Graytail/Items/Defs/fluorescent_shard"),
		TEXT("/Game/Graytail/Items/Defs/anomaly_core_shard"),
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

		// S6 幸运硬币: 纯逻辑 RNG 消耗品, 无独立 UGT_ItemDef 资产, 以代码合成条目接入。
		// (UseConsumableAtPlayer 按 Kind==Consumable 放行; 图标走消耗品兜底, 真图标见部署终端映射。)
		{
			FGT_ItemCatalogEntry Coin;
			Coin.ItemId = FName(TEXT("lucky_coin"));
			Coin.DisplayName = TEXT("幸运硬币");
			Coin.Kind = EGT_ItemKind::Consumable;
			Coin.Rarity = FName(TEXT("epic"));
			Coin.Value = 50;
			Coin.EffectText = TEXT("使用: 50% 得 30 金 / 50% 揭示相邻未知房");
			Coin.Description = TEXT("一枚来历不明的硬币, 抛掷决定命运。");
			Entries.Add(Coin);
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

	FName GetQualityItemId(EGT_ItemQuality Quality, int32 Selector)
	{
		// 掉落档位(品质) -> 该档一组回收物, Selector 在组内确定性选一个。
		// 档位↔稀有度 1:1: Low=白 / Common=蓝 / Rare=紫 / Precious=金 / Abnormal=红。
		static const TArray<FName> Low = {
			FName(TEXT("broken_copper_wire")), FName(TEXT("dim_capacitor")),
			FName(TEXT("old_gear")), FName(TEXT("broken_terminal")) };
		static const TArray<FName> Common = {
			FName(TEXT("dead_battery")), FName(TEXT("old_gauge")), FName(TEXT("damaged_circuit")) };
		static const TArray<FName> Rare = {
			FName(TEXT("static_lens")), FName(TEXT("blackbox_tag")), FName(TEXT("data_disk")) };
		static const TArray<FName> Precious = {
			FName(TEXT("whisper_wick")), FName(TEXT("fluorescent_shard")) };
		static const TArray<FName> Abnormal = {
			FName(TEXT("sealed_core_shard")), FName(TEXT("anomaly_core_shard")) };

		const TArray<FName>* Pool = nullptr;
		switch (Quality)
		{
		case EGT_ItemQuality::Low:      Pool = &Low; break;
		case EGT_ItemQuality::Common:   Pool = &Common; break;
		case EGT_ItemQuality::Rare:     Pool = &Rare; break;
		case EGT_ItemQuality::Precious: Pool = &Precious; break;
		case EGT_ItemQuality::Abnormal: Pool = &Abnormal; break;
		default: return NAME_None;
		}
		if (Pool->Num() == 0)
		{
			return NAME_None;
		}
		const int32 Idx = ((Selector % Pool->Num()) + Pool->Num()) % Pool->Num();
		return (*Pool)[Idx];
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
			{ FName(TEXT("old_gear")),           TEXT("/Game/Graytail/Items/Recovered/old_gear") },
			{ FName(TEXT("broken_terminal")),    TEXT("/Game/Graytail/Items/Recovered/broken_terminal") },
			{ FName(TEXT("dead_battery")),       TEXT("/Game/Graytail/Items/Recovered/dead_battery") },
			{ FName(TEXT("old_gauge")),          TEXT("/Game/Graytail/Items/Recovered/old_gauge") },
			{ FName(TEXT("damaged_circuit")),    TEXT("/Game/Graytail/Items/Recovered/damaged_circuit") },
			{ FName(TEXT("data_disk")),          TEXT("/Game/Graytail/Items/Recovered/data_disk") },
			{ FName(TEXT("fluorescent_shard")),  TEXT("/Game/Graytail/Items/Recovered/fluorescent_shard") },
			{ FName(TEXT("anomaly_core_shard")), TEXT("/Game/Graytail/Items/Recovered/anomaly_core_shard") },
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