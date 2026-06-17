#pragma once

#include "CoreMinimal.h"
#include "Domains/Inventory/GT_InventoryTypes.h"

// 搜索/宝箱的数值与掉落规则(对齐 Lua Balance.lua + RunInventory.GetReward)。
// 全部纯函数、确定性: 同一 (seed, x, y) 永远结出同样的奖励, 与调用次数无关。
namespace GT_LootRules
{
	// 普通搜索数值(对齐 Balance.search)。
	namespace SearchBalance
	{
		constexpr int32 BaseMin = 0;
		constexpr int32 BaseMax = 2;
		constexpr int32 AdjacentDivisor = 2;
		constexpr int32 GoldCap = 4;
		// 周围雷数 >= AdjacentAtLeast 时掉落 roll +RareBonus(高危高收益)。
		constexpr int32 HighAdjacentAtLeast = 3;
		constexpr int32 HighAdjacentRareBonus = 10;
	}

	// 宝箱数值(对齐 Balance.chest)。宝箱房已实现(EGT_RoomBaseType::Chest)。
	namespace ChestBalance
	{
		constexpr int32 BaseMin = 3;
		constexpr int32 BaseMax = 7;
		constexpr int32 GoldCap = 11;
		constexpr int32 MinItems = 1;
		constexpr int32 MaxItems = 2;
	}

	// 确定性区间取值(对齐 Balance.RollRange): 由 seed/x/y/salt 哈希出 [Min, Max] 内的整数。
	// Lua 用 double 算大数乘法会丢精度, 这里用 int64, 常规种子下结果与 Lua 一致。
	GRAYTAIL_API int32 RollRange(int32 Seed, int32 X, int32 Y, int32 Salt, int32 MinValue, int32 MaxValue);

	// 结算 (X, Y) 格的搜索奖励(对齐 RunInventory.GetReward): 金币 + 掉落物品。
	// AdjacentMineCount 是该格 8 邻域雷数(影响金币加成与稀有掉率)。
	GRAYTAIL_API FGT_SearchReward ComputeSearchReward(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount, bool bIsChest);
}
