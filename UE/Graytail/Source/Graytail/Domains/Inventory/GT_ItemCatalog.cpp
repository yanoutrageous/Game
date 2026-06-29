#include "Domains/Inventory/GT_ItemCatalog.h"

#include "Data/GT_GameDataSubsystem.h"
#include "Data/GT_ItemDef.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const TCHAR* GTItemDefAssetPaths[] = {
		TEXT("/Game/Graytail/Items/Defs/broken_copper_wire"),
		TEXT("/Game/Graytail/Items/Defs/dim_capacitor"),
		TEXT("/Game/Graytail/Items/Defs/whisper_wick"),
		TEXT("/Game/Graytail/Items/Defs/sealed_core_shard"),
		TEXT("/Game/Graytail/Items/Defs/emergency_bandage"),
		TEXT("/Game/Graytail/Items/Defs/static_lens"),
		TEXT("/Game/Graytail/Items/Defs/blackbox_tag"),
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
		Entry.EffectText = Asset.EffectText;
		Entry.Description = Asset.Description.ToString();
		return Entry;
	}

	TArray<FGT_ItemCatalogEntry> LoadItemDefs()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Item catalog accessed without valid game data."));

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
				UE_LOG(LogTemp, Error, TEXT("GT_ItemCatalog: missing item def asset %s."), AssetPath);
			}
		}

		FGT_ItemCatalogEntry Coin;
		Coin.ItemId = FName(TEXT("lucky_coin"));
		Coin.DisplayName = TEXT("幸运硬币");
		Coin.Kind = EGT_ItemKind::Consumable;
		Coin.Rarity = FName(TEXT("epic"));
		Coin.EffectText = FString::Printf(
			TEXT("使用: %d%% 得 %d 金币 / 其余概率揭示相邻未知房"),
			Snapshot->LootEvents.LuckyCoin.GoldChancePercent,
			Snapshot->LootEvents.LuckyCoin.SafeGoldReward);
		Coin.Description = TEXT("一枚来历不明的硬币，抛掷决定命运。");
		Entries.Add(Coin);

		for (const FGT_ItemBalanceRow& Balance : Snapshot->Items.Items)
		{
			FGT_ItemCatalogEntry* Entry = Entries.FindByPredicate(
				[&Balance](const FGT_ItemCatalogEntry& Candidate)
				{
					return Candidate.ItemId == FName(*Balance.Id);
				});
			checkf(Entry, TEXT("Validated item '%s' has no catalog asset or runtime entry."), *Balance.Id);
			Entry->Value = Balance.Value;
		}
		return Entries;
	}

	FString QualityId(EGT_ItemQuality Quality)
	{
		switch (Quality)
		{
		case EGT_ItemQuality::Low: return TEXT("low");
		case EGT_ItemQuality::Common: return TEXT("common");
		case EGT_ItemQuality::Rare: return TEXT("rare");
		case EGT_ItemQuality::Precious: return TEXT("precious");
		case EGT_ItemQuality::Abnormal: return TEXT("abnormal");
		default: return FString();
		}
	}
}

namespace GT_ItemCatalog
{
	const TArray<FGT_ItemCatalogEntry>& GetAllItemDefs()
	{
		static const TArray<FGT_ItemCatalogEntry> ItemDefs = LoadItemDefs();
		return ItemDefs;
	}

	const FGT_ItemCatalogEntry* FindItemDef(FName ItemId)
	{
		if (ItemId.IsNone())
		{
			return nullptr;
		}
		return GetAllItemDefs().FindByPredicate(
			[ItemId](const FGT_ItemCatalogEntry& Def)
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
		const FString Id = QualityId(Quality);
		if (Id.IsEmpty())
		{
			return NAME_None;
		}

		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Item catalog accessed without valid game data."));
		const FGT_ItemDropPoolConfig* Pool = Snapshot->Items.DropPools.FindByPredicate(
			[&Id](const FGT_ItemDropPoolConfig& Candidate)
			{
				return Candidate.Quality == Id;
			});
		if (!Pool || Pool->ItemIds.IsEmpty())
		{
			return NAME_None;
		}

		const int32 Index = ((Selector % Pool->ItemIds.Num()) + Pool->ItemIds.Num()) % Pool->ItemIds.Num();
		return FName(*Pool->ItemIds[Index]);
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
		static const TMap<FName, const TCHAR*> IconById = {
			{ FName(TEXT("broken_copper_wire")), TEXT("/Game/Graytail/Items/Recovered/broken_copper_wire") },
			{ FName(TEXT("dim_capacitor")), TEXT("/Game/Graytail/Items/Recovered/dim_capacitor") },
			{ FName(TEXT("whisper_wick")), TEXT("/Game/Graytail/Items/Recovered/whisper_wick") },
			{ FName(TEXT("sealed_core_shard")), TEXT("/Game/Graytail/Items/Recovered/sealed_core_shard") },
			{ FName(TEXT("static_lens")), TEXT("/Game/Graytail/Items/Recovered/static_lens") },
			{ FName(TEXT("blackbox_tag")), TEXT("/Game/Graytail/Items/Recovered/blackbox_tag") },
			{ FName(TEXT("old_gear")), TEXT("/Game/Graytail/Items/Recovered/old_gear") },
			{ FName(TEXT("broken_terminal")), TEXT("/Game/Graytail/Items/Recovered/broken_terminal") },
			{ FName(TEXT("dead_battery")), TEXT("/Game/Graytail/Items/Recovered/dead_battery") },
			{ FName(TEXT("old_gauge")), TEXT("/Game/Graytail/Items/Recovered/old_gauge") },
			{ FName(TEXT("damaged_circuit")), TEXT("/Game/Graytail/Items/Recovered/damaged_circuit") },
			{ FName(TEXT("data_disk")), TEXT("/Game/Graytail/Items/Recovered/data_disk") },
			{ FName(TEXT("fluorescent_shard")), TEXT("/Game/Graytail/Items/Recovered/fluorescent_shard") },
			{ FName(TEXT("anomaly_core_shard")), TEXT("/Game/Graytail/Items/Recovered/anomaly_core_shard") },
		};
		if (const TCHAR* const* Found = IconById.Find(ItemId))
		{
			return *Found;
		}
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
