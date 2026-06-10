#include "Domains/Inventory/GT_LootRules.h"

#include "Domains/Inventory/GT_ItemCatalog.h"

namespace
{
	// 掉落表一行: roll(1-100) <= MaxRoll 时结出该品质。
	struct FGT_DropEntry
	{
		int32 MaxRoll = 0;
		EGT_ItemQuality Quality = EGT_ItemQuality::None;
	};

	// 对齐 Balance.search.dropTable: 35% 无掉落 / 45% 低质 / 15% 一般 / 5% 稀有。
	const FGT_DropEntry SearchDropTable[] = {
		{ 35, EGT_ItemQuality::None },
		{ 80, EGT_ItemQuality::Low },
		{ 95, EGT_ItemQuality::Common },
		{ 100, EGT_ItemQuality::Rare },
	};

	// 对齐 Balance.chest.dropTable: 宝箱必掉, 且品质整体高一档。
	const FGT_DropEntry ChestDropTable[] = {
		{ 45, EGT_ItemQuality::Common },
		{ 80, EGT_ItemQuality::Rare },
		{ 97, EGT_ItemQuality::Precious },
		{ 100, EGT_ItemQuality::Abnormal },
	};

	// 对齐 Lua pickQualityFromTable: 高雷数房间 roll 上浮, 更容易命中表尾的稀有段。
	EGT_ItemQuality PickQualityFromTable(const FGT_DropEntry* Table, int32 TableNum, int32 Roll, int32 AdjacentMineCount)
	{
		if (AdjacentMineCount >= GT_LootRules::SearchBalance::HighAdjacentAtLeast)
		{
			Roll = FMath::Min(100, Roll + GT_LootRules::SearchBalance::HighAdjacentRareBonus);
		}

		for (int32 Index = 0; Index < TableNum; ++Index)
		{
			if (Roll <= Table[Index].MaxRoll)
			{
				return Table[Index].Quality;
			}
		}
		return EGT_ItemQuality::None;
	}

	// 对齐 Lua chooseItemId: 同一格的第 i 件物品用 index*17 扰动 roll, 避免整箱同品质。
	FName ChooseItemId(int32 BaseRoll, bool bIsChest, int32 ItemIndex, int32 AdjacentMineCount)
	{
		const int32 Roll = (BaseRoll + ItemIndex * 17) % 100 + 1;
		const EGT_ItemQuality Quality = bIsChest
			? PickQualityFromTable(ChestDropTable, UE_ARRAY_COUNT(ChestDropTable), Roll, AdjacentMineCount)
			: PickQualityFromTable(SearchDropTable, UE_ARRAY_COUNT(SearchDropTable), Roll, AdjacentMineCount);
		return GT_ItemCatalog::GetQualityItemId(Quality);
	}

	// 对齐 Lua buildRewardItems: 决定件数, 逐件 roll 品质, 同物品合并堆叠。
	TArray<FGT_ItemStack> BuildRewardItems(int32 BaseRoll, bool bIsChest, int32 AdjacentMineCount)
	{
		int32 ItemCount = 0;
		if (bIsChest)
		{
			ItemCount = GT_LootRules::RollRange(BaseRoll, AdjacentMineCount, 0, 5,
				GT_LootRules::ChestBalance::MinItems, GT_LootRules::ChestBalance::MaxItems);
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

			FGT_ItemStack* Found = Items.FindByPredicate([ItemId](const FGT_ItemStack& Stack)
			{
				return Stack.ItemId == ItemId;
			});
			if (Found)
			{
				++Found->Count;
			}
			else
			{
				FGT_ItemStack& NewStack = Items.AddDefaulted_GetRef();
				NewStack.ItemId = ItemId;
				NewStack.Count = 1;
				NewStack.Source = Source;
			}
		}
		return Items;
	}
}

namespace GT_LootRules
{
	int32 RollRange(int32 Seed, int32 X, int32 Y, int32 Salt, int32 MinValue, int32 MaxValue)
	{
		if (MaxValue < MinValue)
		{
			MaxValue = MinValue;
		}
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
		FGT_SearchReward Reward;
		Reward.bIsChest = bIsChest;

		// 物品掉落用的基础 roll(对齐 Lua: (x*37 + y*53 + seed*7) % 100, 用 int64 防溢出)。
		const int64 RollHash = static_cast<int64>(X) * 37
			+ static_cast<int64>(Y) * 53
			+ static_cast<int64>(Seed) * 7;
		const int32 BaseRoll = static_cast<int32>(((RollHash % 100) + 100) % 100);

		if (bIsChest)
		{
			int32 Gold = RollRange(Seed, X, Y, 11, ChestBalance::BaseMin, ChestBalance::BaseMax) + AdjacentMineCount;
			Reward.Gold = FMath::Min(ChestBalance::GoldCap, Gold);
		}
		else
		{
			int32 Gold = RollRange(Seed, X, Y, 7, SearchBalance::BaseMin, SearchBalance::BaseMax)
				+ AdjacentMineCount / SearchBalance::AdjacentDivisor;
			Reward.Gold = FMath::Min(SearchBalance::GoldCap, Gold);
		}

		// TODO(装备阶段): Lua 的 searchBonus(搜索奖励加成百分比)依赖装备效果, 局外系统迁入后补。

		Reward.Items = BuildRewardItems(BaseRoll, bIsChest, AdjacentMineCount);
		for (const FGT_ItemStack& Stack : Reward.Items)
		{
			Reward.Parts += Stack.Count;
		}
		return Reward;
	}
}
