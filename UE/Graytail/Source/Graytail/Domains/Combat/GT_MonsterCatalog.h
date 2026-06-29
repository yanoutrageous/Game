#pragma once

#include "CoreMinimal.h"
#include "GT_MonsterCatalog.generated.h"

UENUM(BlueprintType)
enum class EGT_MonsterType : uint8
{
	Slime UMETA(DisplayName = "Slime"),
	Bat UMETA(DisplayName = "Bat"),
	Drone UMETA(DisplayName = "Drone"),
	Slimeling UMETA(DisplayName = "Slimeling"),
};

UENUM(BlueprintType)
enum class EGT_MonsterMovePattern : uint8
{
	Stationary UMETA(DisplayName = "Stationary"),
	ChasePlayer UMETA(DisplayName = "Chase Player"),
	KeepDistance UMETA(DisplayName = "Keep Distance"),
	RandomRoam UMETA(DisplayName = "Random Roam"),
};

UENUM(BlueprintType)
enum class EGT_MonsterAttackPattern : uint8
{
	MeleeCircle UMETA(DisplayName = "Melee Circle"),
	RangedProjectile UMETA(DisplayName = "Ranged Projectile"),
	RangedSpread UMETA(DisplayName = "Ranged Spread"),
	RangedLaser UMETA(DisplayName = "Ranged Laser"),
};

struct FGT_MonsterArchetype
{
	EGT_MonsterType Type = EGT_MonsterType::Slime;
	EGT_MonsterMovePattern MovePattern = EGT_MonsterMovePattern::ChasePlayer;
	EGT_MonsterAttackPattern AttackPattern = EGT_MonsterAttackPattern::MeleeCircle;

	int32 HpBase = 0;
	float MoveSpeed = 0.0f;
	float AttackRadius = 0.0f;
	float PlayerAttackRange = 0.0f;
	float IdleDuration = 0.0f;
	float WarningDuration = 0.0f;
	float ActiveDuration = 0.0f;
	float CooldownDuration = 0.0f;
	float IdealDistance = 0.0f;
	float AimDuration = 0.0f;
	float AttackInterval = 0.0f;
	int32 SpreadCount = 0;
	float SpreadHalfAngleDeg = 0.0f;
	float LaserDuration = 0.0f;
	float LaserTickInterval = 0.0f;
	float LaserTurnRateDeg = 0.0f;
	float AimTurnRateDeg = 0.0f;
	float KiteStrength = 0.0f;
	float WanderWeight = 0.0f;
	bool bChaseWhenFar = false;
	float DamageMult = 0.0f;
	bool bDashAwayOnHit = false;
	int32 SplitCount = 0;
	float SplitPowerMultiplier = 0.0f;

	const TCHAR* SpritePath = TEXT("");
	FLinearColor TintColor = FLinearColor::White;
};

namespace GT_MonsterCatalog
{
	GRAYTAIL_API const FGT_MonsterArchetype& GetArchetype(EGT_MonsterType Type);
	GRAYTAIL_API EGT_MonsterType PickTypeForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount);
}
