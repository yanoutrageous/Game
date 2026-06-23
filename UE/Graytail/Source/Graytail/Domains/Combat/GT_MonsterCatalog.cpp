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

		const FGT_MonsterArchetype& BatArchetype()
		{
			static const FGT_MonsterArchetype Bat = []()
			{
				FGT_MonsterArchetype A;
				A.Type = EGT_MonsterType::Bat;
				A.MovePattern = EGT_MonsterMovePattern::KeepDistance;
				A.AttackPattern = EGT_MonsterAttackPattern::RangedSpread;
				A.HpBase = 12;                 // 脆
				A.MoveSpeed = 0.28f;           // 快(slime 0.16)
				A.PlayerAttackRange = 0.21f;   // 玩家砍它的射程
				A.IdealDistance = 0.40f;       // kiting 理想距离(中)
				A.AimDuration = 0.5f;          // 散射前短瞄准
				A.AttackInterval = 1.5f;
				A.SpreadCount = 3;
				A.SpreadHalfAngleDeg = 25.f;
				A.SpritePath = TEXT("/Game/Graytail/Sprites/enemy_slime");   // 占位
				A.TintColor = FLinearColor(0.72f, 0.40f, 1.0f);             // 紫
				return A;
			}();
			return Bat;
		}

		const FGT_MonsterArchetype& DroneArchetype()
		{
			static const FGT_MonsterArchetype Drone = []()
			{
				FGT_MonsterArchetype A;
				A.Type = EGT_MonsterType::Drone;
				A.MovePattern = EGT_MonsterMovePattern::KeepDistance;
				A.AttackPattern = EGT_MonsterAttackPattern::RangedLaser;
				A.HpBase = 22;                 // 肉
				A.MoveSpeed = 0.10f;           // 慢悬停
				A.PlayerAttackRange = 0.21f;
				A.IdealDistance = 0.50f;       // kiting 理想距离(远)
				A.AimDuration = 0.8f;          // 蓄力
				A.AttackInterval = 2.5f;
				A.LaserDuration = 1.2f;        // 光束持续
				A.LaserTickInterval = 0.3f;    // 站内扣血间隔
				A.SpritePath = TEXT("/Game/Graytail/Sprites/enemy_slime");   // 占位
				A.TintColor = FLinearColor(0.40f, 0.62f, 0.92f);            // 蓝灰
				return A;
			}();
			return Drone;
		}
	}

	const FGT_MonsterArchetype& GetArchetype(EGT_MonsterType Type)
	{
		switch (Type)
		{
		case EGT_MonsterType::Bat:
			return BatArchetype();
		case EGT_MonsterType::Drone:
			return DroneArchetype();
		case EGT_MonsterType::Slime:
		default:
			return SlimeArchetype();
		}
	}

	EGT_MonsterType PickTypeForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount)
	{
		// 三种怪等概率(slime/bat/drone): (Seed,X,Y) 确定性空间哈希取模 3, 同 seed 同格稳定。
		const uint32 Hash = (uint32(Seed) * 2654435761u) ^ (uint32(X) * 73856093u) ^ (uint32(Y) * 19349663u);
		switch (Hash % 3u)
		{
		case 1u: return EGT_MonsterType::Bat;
		case 2u: return EGT_MonsterType::Drone;
		default: return EGT_MonsterType::Slime;
		}
	}
}
