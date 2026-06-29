#include "Domains/Inventory/GT_LootRules.h"

#include "Data/GT_GameDataSubsystem.h"
#include "Domains/Inventory/GT_ItemCatalog.h"

namespace
{
	const FGT_LootEventsBalanceFile& GetBalance()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Loot rules accessed without valid game data."));
		return Snapshot->LootEvents;
	}

	EGT_ItemQuality ParseQuality(const FString& Quality)
	{
		if (Quality == TEXT("low")) return EGT_ItemQuality::Low;
		if (Quality == TEXT("common")) return EGT_ItemQuality::Common;
		if (Quality == TEXT("rare")) return EGT_ItemQuality::Rare;
		if (Quality == TEXT("precious")) return EGT_ItemQuality::Precious;
		if (Quality == TEXT("abnormal")) return EGT_ItemQuality::Abnormal;
		return EGT_ItemQuality::None;
	}

	EGT_ItemQuality PickQuality(
		const TArray<FGT_DropThresholdConfig>& Table,
		int32 Roll,
		int32 AdjacentMineCount)
	{
		const FGT_SearchBalanceConfig& Search = GetBalance().Search;
		if (AdjacentMineCount >= Search.HighAdjacentAtLeast)
		{
			Roll = FMath::Min(100, Roll + Search.HighAdjacentRareBonus);
		}
		for (const FGT_DropThresholdConfig& Entry : Table)
		{
			if (Roll <= Entry.MaxRoll)
			{
				return ParseQuality(Entry.Quality);
			}
		}
		return EGT_ItemQuality::None;
	}

	FName ChooseItemId(int32 BaseRoll, bool bIsChest, int32 ItemIndex, int32 AdjacentMineCount)
	{
		const int32 Roll = (BaseRoll + ItemIndex * 17) % 100 + 1;
		const FGT_LootEventsBalanceFile& Balance = GetBalance();
		const EGT_ItemQuality Quality = PickQuality(
			bIsChest ? Balance.Chest.DropTable : Balance.Search.DropTable,
			Roll,
			AdjacentMineCount);
		return GT_ItemCatalog::GetQualityItemId(Quality, Roll);
	}

	TArray<FGT_ItemStack> BuildRewardItems(int32 BaseRoll, bool bIsChest, int32 AdjacentMineCount)
	{
		const FGT_LootEventsBalanceFile& Balance = GetBalance();
		int32 ItemCount = 0;
		if (bIsChest)
		{
			ItemCount = GT_LootRules::RollRange(
				BaseRoll,
				AdjacentMineCount,
				0,
				5,
				Balance.Chest.MinItems,
				Balance.Chest.MaxItems);
		}
		else if (!ChooseItemId(BaseRoll, false, 1, AdjacentMineCount).IsNone())
		{
			ItemCount = 1;
		}

		const FName Source = bIsChest ? FName(TEXT("chest")) : FName(TEXT("search"));
		TArray<FGT_ItemStack> Items;
		for (int32 ItemIndex = 1; ItemIndex <= ItemCount; ++ItemIndex)
		{
			const FName ItemId = ChooseItemId(BaseRoll, bIsChest, ItemIndex, AdjacentMineCount);
			if (ItemId.IsNone())
			{
				break;
			}
			if (FGT_ItemStack* Found = Items.FindByPredicate(
				[ItemId](const FGT_ItemStack& Stack) { return Stack.ItemId == ItemId; }))
			{
				++Found->Count;
			}
			else
			{
				FGT_ItemStack& Stack = Items.AddDefaulted_GetRef();
				Stack.ItemId = ItemId;
				Stack.Count = 1;
				Stack.Source = Source;
			}
		}
		return Items;
	}
}

namespace GT_LootRules
{
	int32 RollRange(int32 Seed, int32 X, int32 Y, int32 Salt, int32 MinValue, int32 MaxValue)
	{
		MaxValue = FMath::Max(MaxValue, MinValue);
		const int64 Span = static_cast<int64>(MaxValue) - MinValue + 1;
		int64 Hash = static_cast<int64>(Seed) * 1103515245LL
			+ static_cast<int64>(X) * 928371LL
			+ static_cast<int64>(Y) * 364479LL
			+ static_cast<int64>(Salt) * 7919LL;
		Hash = FMath::Abs(Hash) % 2147483647LL;
		return MinValue + static_cast<int32>(Hash % Span);
	}

	FGT_SearchReward ComputeSearchReward(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount, bool bIsChest)
	{
		const FGT_LootEventsBalanceFile& Balance = GetBalance();
		FGT_SearchReward Reward;
		Reward.bIsChest = bIsChest;

		const int64 RollHash = static_cast<int64>(X) * 37
			+ static_cast<int64>(Y) * 53
			+ static_cast<int64>(Seed) * 7;
		const int32 BaseRoll = static_cast<int32>(((RollHash % 100) + 100) % 100);

		if (bIsChest)
		{
			const int32 Gold = RollRange(Seed, X, Y, 11, Balance.Chest.BaseMin, Balance.Chest.BaseMax)
				+ AdjacentMineCount;
			Reward.Gold = FMath::Min(Balance.Chest.GoldCap, Gold);
		}
		else
		{
			const int32 Gold = RollRange(Seed, X, Y, 7, Balance.Search.BaseMin, Balance.Search.BaseMax)
				+ AdjacentMineCount / Balance.Search.AdjacentDivisor;
			Reward.Gold = FMath::Min(Balance.Search.GoldCap, Gold);
		}

		Reward.Items = BuildRewardItems(BaseRoll, bIsChest, AdjacentMineCount);
		for (const FGT_ItemStack& Stack : Reward.Items)
		{
			Reward.Parts += Stack.Count;
		}
		return Reward;
	}

	int32 GetFleeGoldDropPercent()
	{
		return GetBalance().Flee.GoldDropPercent;
	}

	int32 GetFleeItemDropPercent()
	{
		return GetBalance().Flee.ItemDropPercent;
	}

	int32 GetFleeDropSalt()
	{
		return GetBalance().Flee.DropSalt;
	}
}
