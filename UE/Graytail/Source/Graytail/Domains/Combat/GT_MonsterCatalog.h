#pragma once

#include "CoreMinimal.h"
#include "GT_MonsterCatalog.generated.h"

// 怪物类型(行为原型 id)。**只在末尾追加新类型**(对齐 UENUM 铁律)。
// 加新怪 = 这里加一项 + GT_MonsterCatalog.cpp 的表里加一行原型 + 需要时给 PickTypeForCell 加选型权重。
UENUM(BlueprintType)
enum class EGT_MonsterType : uint8
{
	Slime UMETA(DisplayName = "Slime"),   // 第 1 个原型: 缓慢追击 + 近战预警圈(对齐原版)
};

// 移动模式(表现层 RoomView 按此分派)。
UENUM(BlueprintType)
enum class EGT_MonsterMovePattern : uint8
{
	Stationary UMETA(DisplayName = "Stationary"),       // 原地不动(纯原版)
	ChasePlayer UMETA(DisplayName = "Chase Player"),    // 朝玩家缓慢逼近, 进攻击圈即停
};

// 攻击模式(表现层 RoomView 按此分派; 伤害仍走 MonsterHit 命令)。
UENUM(BlueprintType)
enum class EGT_MonsterAttackPattern : uint8
{
	MeleeCircle UMETA(DisplayName = "Melee Circle"),        // 地面预警圈 -> 填充判定圈(对齐原版近战)
	RangedProjectile UMETA(DisplayName = "Ranged Projectile"), // 红线瞄准 -> 飞弹(PR#9 远程, 留给未来远程型)
};

// 怪物行为原型(纯数据, 非 UPROPERTY): 数值 + AI 画像 + 实时相位时长 + 贴图。
// HP/伤害的战力缩放公式在 GT_CombatRules(共享); 这里给每类的基底与画像。
struct FGT_MonsterArchetype
{
	EGT_MonsterType Type = EGT_MonsterType::Slime;
	EGT_MonsterMovePattern MovePattern = EGT_MonsterMovePattern::ChasePlayer;
	EGT_MonsterAttackPattern AttackPattern = EGT_MonsterAttackPattern::MeleeCircle;

	int32 HpBase = 18;            // monsterMaxHP = HpBase + power(对齐 Combat.lua monsterHpBase)
	float MoveSpeed = 0.16f;      // 追击速度(归一化坐标/秒)
	float AttackRadius = 0.20f;   // 判定圈半径(对齐 CONFIG.monsterAttackRadius)
	float PlayerAttackRange = 0.21f; // 玩家挥砍射程(对齐 CONFIG.playerAttackRange)

	// 攻击相位时长(对齐 Combat.lua CONFIG)。
	float IdleDuration = 1.10f;
	float WarningDuration = 0.75f;
	float ActiveDuration = 0.28f;
	float CooldownDuration = 0.55f;

	const TCHAR* SpritePath = TEXT("/Game/Graytail/Sprites/enemy_slime");
};

namespace GT_MonsterCatalog
{
	// 取某类型的原型(未知类型回退到 Slime)。
	GRAYTAIL_API const FGT_MonsterArchetype& GetArchetype(EGT_MonsterType Type);

	// 确定性选型(对齐 MakeEnemyForCell 的 seed+坐标哈希思路): 同 seed 同格稳定。
	// 目前只有 Slime; 加类型后在此按难度/深度/哈希分配权重。
	GRAYTAIL_API EGT_MonsterType PickTypeForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount);
}
