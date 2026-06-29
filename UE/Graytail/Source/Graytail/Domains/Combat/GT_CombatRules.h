#pragma once

#include "CoreMinimal.h"
#include "GT_CombatRules.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_PlayerCombatState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 Hp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 MaxHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 Power = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 MonsterPowerBonus = 0;

	bool IsAlive() const
	{
		return Hp > 0;
	}

	int32 ApplyDamage(int32 Damage)
	{
		Damage = FMath::Max(0, Damage);
		const int32 Before = Hp;
		Hp = FMath::Clamp(Hp - Damage, 0, MaxHp);
		return Before - Hp;
	}

	int32 Heal(int32 Amount)
	{
		Amount = FMath::Max(0, Amount);
		const int32 Before = Hp;
		Hp = FMath::Clamp(Hp + Amount, 0, MaxHp);
		return Hp - Before;
	}
};

namespace GT_CombatRules
{
	GRAYTAIL_API int32 GetMineDamage();
	GRAYTAIL_API int32 GetMineDamageFloor();
	GRAYTAIL_API int32 GetPowerGainPerKill();
	GRAYTAIL_API int32 GetPowerGainCap();
	GRAYTAIL_API int32 MonsterMaxHpForPower(int32 EnemyPower);
	GRAYTAIL_API int32 MonsterDamageForPower(int32 EnemyPower);
	GRAYTAIL_API void MakeEnemyForCell(
		int32 Seed,
		int32 X,
		int32 Y,
		int32 AdjacentMineCount,
		FString& OutName,
		int32& OutPower);
	GRAYTAIL_API int32 KillRewardGold(int32 EnemyPower);
}
