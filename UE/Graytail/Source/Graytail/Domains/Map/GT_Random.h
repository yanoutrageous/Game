#pragma once

#include "CoreMinimal.h"

// 确定性伪随机数生成器 (Park-Miller MINSTD 线性同余)。
//
// 数值行为与 Lua 原型 scripts/systems/Minefield.lua 中的 RNG 完全一致:
//   seed = (seed * 48271) % 2147483647
// 因此相同 seed 在 Lua 与 C++ 会产生相同序列, 可用于交叉验证地图生成结果。
//
// 这是一个纯 C++ 工具结构, 不暴露给蓝图; 地图生成等规则层内部使用。
struct FGT_Random
{
	explicit FGT_Random(int32 InSeed)
	{
		// 模拟 Lua 的非负取模, 并避免 seed 落在 0 (LCG 在 0 处会卡死)。
		int64 NormalizedSeed = static_cast<int64>(InSeed) % 2147483647;
		if (NormalizedSeed < 0)
		{
			NormalizedSeed += 2147483647;
		}
		if (NormalizedSeed == 0)
		{
			NormalizedSeed = 2147483646;
		}
		Seed = NormalizedSeed;
	}

	// 推进一步并返回 [0, 1) 的浮点数 (对应 Lua RNG:Next)。
	// 用 int64 承载乘法中间结果以避免溢出 (48271 * 2147483646 远超 int32)。
	double NextFloat()
	{
		Seed = (Seed * 48271) % 2147483647;
		return static_cast<double>(Seed) / 2147483647.0;
	}

	// 返回闭区间 [MinValue, MaxValue] 内的整数 (对应 Lua RNG:Int)。
	int32 RangeInt(int32 MinValue, int32 MaxValue)
	{
		if (MaxValue <= MinValue)
		{
			return MinValue;
		}
		return MinValue + static_cast<int32>(NextFloat() * (MaxValue - MinValue + 1));
	}

	// Fisher-Yates 洗牌 (0-based)。调用序列与 Lua RNG:Shuffle 一致,
	// 因此相同 seed 下洗牌结果与 Lua 原型相同。
	template <typename ElementType>
	void Shuffle(TArray<ElementType>& Items)
	{
		for (int32 HighIndex = Items.Num() - 1; HighIndex >= 1; --HighIndex)
		{
			const int32 SwapIndex = RangeInt(0, HighIndex);
			Items.Swap(HighIndex, SwapIndex);
		}
	}

private:
	int64 Seed = 1;
};
