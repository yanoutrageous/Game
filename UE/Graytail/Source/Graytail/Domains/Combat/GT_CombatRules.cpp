#include "Domains/Combat/GT_CombatRules.h"

namespace GT_CombatRules
{
	void MakeEnemyForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount, FString& OutName, int32& OutPower)
	{
		// 对齐 Lua: hash = (x*131 + y*97 + seed*41) % 1000, int64 防溢出并保证非负。
		const int64 RawHash = static_cast<int64>(X) * 131
			+ static_cast<int64>(Y) * 97
			+ static_cast<int64>(Seed) * 41;
		const int32 Hash = static_cast<int32>(((RawHash % 1000) + 1000) % 1000);

		const int32 BasePower = EnemyPowerMin + Hash % (EnemyPowerMax - EnemyPowerMin + 1);
		OutPower = BasePower + AdjacentMineCount * 2;

		// 对齐 Lua 的怪物名表(顺序一致)。
		static const TCHAR* EnemyNames[] = {
			TEXT("滞留工偶"),
			TEXT("空壳巡工"),
			TEXT("失控搬运机"),
			TEXT("头灯哨卫"),
			TEXT("管道清理机"),
		};
		OutName = EnemyNames[Hash % UE_ARRAY_COUNT(EnemyNames)];
	}

	int32 KillRewardGold(int32 EnemyPower)
	{
		const int32 Span = MonsterGoldMax - MonsterGoldMin + 1;
		return MonsterGoldMin + (Span > 0 ? FMath::Max(0, EnemyPower) % Span : 0);
	}
}
