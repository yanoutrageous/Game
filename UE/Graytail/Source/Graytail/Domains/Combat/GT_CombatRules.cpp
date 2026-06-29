#include "Domains/Combat/GT_CombatRules.h"

#include "Data/GT_GameDataSubsystem.h"

namespace
{
	const FGT_CombatBalanceConfig& GetCombatConfig()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Combat rules accessed without valid game data."));
		return Snapshot->Core.Combat;
	}
}

namespace GT_CombatRules
{
	int32 GetMineDamage()
	{
		return GetCombatConfig().MineDamage;
	}

	int32 GetMineDamageFloor()
	{
		return GetCombatConfig().MineDamageFloor;
	}

	int32 GetPowerGainPerKill()
	{
		return GetCombatConfig().PowerGainPerKill;
	}

	int32 GetPowerGainCap()
	{
		return GetCombatConfig().PowerGainCap;
	}

	int32 MonsterMaxHpForPower(int32 EnemyPower)
	{
		return GetCombatConfig().DefaultMonsterHpBase + FMath::Max(0, EnemyPower);
	}

	int32 MonsterDamageForPower(int32 EnemyPower)
	{
		return FMath::Max(GetCombatConfig().MonsterDamageMin, FMath::Max(0, EnemyPower) / 3);
	}

	void MakeEnemyForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount, FString& OutName, int32& OutPower)
	{
		// 对齐 Lua: hash = (x*131 + y*97 + seed*41) % 1000, int64 防溢出并保证非负。
		const int64 RawHash = static_cast<int64>(X) * 131
			+ static_cast<int64>(Y) * 97
			+ static_cast<int64>(Seed) * 41;
		const int32 Hash = static_cast<int32>(((RawHash % 1000) + 1000) % 1000);

		const FGT_CombatBalanceConfig& Config = GetCombatConfig();
		const int32 BasePower = Config.EnemyPowerMin + Hash % (Config.EnemyPowerMax - Config.EnemyPowerMin + 1);
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
		const FGT_CombatBalanceConfig& Config = GetCombatConfig();
		const int32 Span = Config.MonsterGoldMax - Config.MonsterGoldMin + 1;
		return Config.MonsterGoldMin + (Span > 0 ? FMath::Max(0, EnemyPower) % Span : 0);
	}
}
