#include "Domains/Combat/GT_MonsterCatalog.h"

namespace GT_MonsterCatalog
{
	namespace
	{
		// 原型表: 加新怪在此追加一行(并在 .h 的 EGT_MonsterType 末尾加项)。
		const FGT_MonsterArchetype& SlimeArchetype()
		{
			static const FGT_MonsterArchetype Slime = []()
			{
				FGT_MonsterArchetype A;
				A.Type = EGT_MonsterType::Slime;
				A.MovePattern = EGT_MonsterMovePattern::ChasePlayer;
				A.AttackPattern = EGT_MonsterAttackPattern::MeleeCircle;
				A.HpBase = 18;
				A.MoveSpeed = 0.16f;
				A.AttackRadius = 0.20f;
				A.PlayerAttackRange = 0.21f;
				A.IdleDuration = 1.10f;
				A.WarningDuration = 0.75f;
				A.ActiveDuration = 0.28f;
				A.CooldownDuration = 0.55f;
				A.SpritePath = TEXT("/Game/Graytail/Sprites/enemy_slime");
				return A;
			}();
			return Slime;
		}
	}

	const FGT_MonsterArchetype& GetArchetype(EGT_MonsterType Type)
	{
		switch (Type)
		{
		case EGT_MonsterType::Slime:
		default:
			return SlimeArchetype();
		}
	}

	EGT_MonsterType PickTypeForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount)
	{
		// 目前只有 Slime。加类型后在此用 (Seed,X,Y) 哈希 + 难度/深度门控分配。
		return EGT_MonsterType::Slime;
	}
}
