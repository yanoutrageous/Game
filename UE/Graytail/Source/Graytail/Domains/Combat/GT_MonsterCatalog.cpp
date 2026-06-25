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
				A.HpBase = 24;                 // 调高: 更扛揍(早期肉搏壮汉)
				A.MoveSpeed = 0.18f;           // 调高: 逼得更紧
				A.DamageMult = 1.3f;           // 调高: 近战更疼
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
				A.IdealDistance = 0.34f;       // 只在很近时才轻微后撤
				A.KiteStrength = 0.45f;        // 远离权重低 -> 主要随机游走, 易被追打(修"太难打")
				A.WanderWeight = 1.1f;         // 随机游走主导
				A.bChaseWhenFar = false;       // 不追回保持射程: 随机乱窜 + 小小远离
				A.AimDuration = 0.5f;          // 散射前短瞄准
				A.AttackInterval = 1.5f;
				A.SpreadCount = 3;
				A.SpreadHalfAngleDeg = 25.f;
				A.SpritePath = TEXT("/Game/Graytail/Sprites/Monsters/bat_idle_0");
				A.TintColor = FLinearColor::White;   // 真彩贴图自带紫色, 不再占位染色
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
				A.HpBase = 28;                 // 最肉(精英远程, 三怪最高血)
				A.MoveSpeed = 0.10f;           // 慢悬停
				A.DamageMult = 1.5f;           // 最疼: 激光躲失误代价高(三怪最高伤害)
				A.PlayerAttackRange = 0.21f;
				A.IdealDistance = 0.50f;       // kiting 理想距离(远)
				A.AimDuration = 0.8f;          // 蓄力
				A.AttackInterval = 2.5f;
				A.LaserDuration = 1.2f;        // 光束持续
				A.LaserTickInterval = 0.3f;    // 站内扣血间隔
				A.LaserTurnRateDeg = 18.f;     // 发射后跟随(70->32->12->18 回调): 侧移可甩开
				A.AimTurnRateDeg = 0.f;        // 蓄力开始即锁死发射方向(红线固定预警): 蓄力期移开红线 -> 发射必脱靶
				A.KiteStrength = 1.0f;         // 严格保持距离(慢肉怪靠激光压制)
				A.WanderWeight = 0.5f;
				A.SpritePath = TEXT("/Game/Graytail/Sprites/Monsters/drone_idle_0");
				A.TintColor = FLinearColor::White;   // 真彩贴图自带蓝灰, 不再占位染色
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
