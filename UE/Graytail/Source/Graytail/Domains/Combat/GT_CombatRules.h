#pragma once

#include "CoreMinimal.h"
#include "GT_CombatRules.generated.h"

// 玩家战斗属性(对齐 Combat.lua 的 hp/maxHp/power/monsterPowerBonus)。
// 挂在 UGT_RunContext 上, 一局一份。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_PlayerCombatState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 Hp = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 MaxHp = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 Power = 10;

	// 击杀怪物累计获得的战力(有上限), 对齐 Lua monsterPowerBonus。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 MonsterPowerBonus = 0;

	bool IsAlive() const
	{
		return Hp > 0;
	}

	// 扣血(负数无效), 血量夹在 [0, MaxHp]。返回实际扣除量。
	int32 ApplyDamage(int32 Damage)
	{
		Damage = FMath::Max(0, Damage);
		const int32 Before = Hp;
		Hp = FMath::Clamp(Hp - Damage, 0, MaxHp);
		return Before - Hp;
	}

	// 回血(负数无效), 血量夹在 [0, MaxHp]。返回实际回血量(对齐 Lua 止血贴 applyHpDelta)。
	int32 Heal(int32 Amount)
	{
		Amount = FMath::Max(0, Amount);
		const int32 Before = Hp;
		Hp = FMath::Clamp(Hp + Amount, 0, MaxHp);
		return Hp - Before;
	}
};

// 战斗规则层数值与纯函数(对齐 Combat.lua + Balance.lua)。
// 注意: Combat.lua 的实时动作层(攻击前摇/判定圈/无敌帧)属于表现层, 待场景阶段再迁。
namespace GT_CombatRules
{
	constexpr int32 MineDamage = 30;        // Balance.mineDamage
	constexpr int32 MineDamageFloor = 5;    // 雷伤减免后的最低伤害
	constexpr int32 EnemyPowerMin = 5;
	constexpr int32 EnemyPowerMax = 20;
	constexpr int32 MonsterGoldMin = 0;     // Balance.monster.goldMin/goldMax
	constexpr int32 MonsterGoldMax = 3;
	constexpr int32 PowerGainPerKill = 1;   // Balance.monster.powerGain
	constexpr int32 PowerGainCap = 5;       // Balance.monster.powerGainCap

	// 实时战斗怪物数值(对齐 Combat.lua CONFIG / ensureMonsterState)。
	constexpr int32 MonsterHpBase = 18;     // CONFIG.monsterHpBase, monsterMaxHP = base + power
	constexpr int32 MonsterDamageMin = 4;   // CONFIG.monsterDamageMin, monsterDamage = max(min, power/3)

	// 怪物最大血量(对齐 ensureMonsterState: monsterMaxHP = monsterHpBase + power)。
	FORCEINLINE int32 MonsterMaxHpForPower(int32 EnemyPower)
	{
		return MonsterHpBase + FMath::Max(0, EnemyPower);
	}

	// 怪物单次攻击伤害(对齐 ensureMonsterState: max(monsterDamageMin, floor(power/3)))。
	FORCEINLINE int32 MonsterDamageForPower(int32 EnemyPower)
	{
		return FMath::Max(MonsterDamageMin, FMath::Max(0, EnemyPower) / 3);
	}

	// 确定性生成怪物(对齐 Combat.TrySpawnEnemy): 战力由 seed+坐标哈希决定, 周围雷数 ×2 加成。
	GRAYTAIL_API void MakeEnemyForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount, FString& OutName, int32& OutPower);

	// 击杀奖励金币(对齐 Combat.makeReward): goldMin + 战力取模区间宽度。
	GRAYTAIL_API int32 KillRewardGold(int32 EnemyPower);
}
