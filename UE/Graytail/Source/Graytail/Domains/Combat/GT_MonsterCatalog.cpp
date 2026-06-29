#include "Domains/Combat/GT_MonsterCatalog.h"

#include "Data/GT_GameDataSubsystem.h"

namespace
{
	const FGT_MonsterBalanceRow& FindBalance(const TCHAR* Id)
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Monster catalog accessed without valid game data."));
		const FGT_MonsterBalanceRow* Row = Snapshot->Monsters.Monsters.FindByPredicate(
			[Id](const FGT_MonsterBalanceRow& Candidate)
			{
				return Candidate.Id == Id;
			});
		checkf(Row, TEXT("Validated monster '%s' is missing."), Id);
		return *Row;
	}

	void ApplyBalance(FGT_MonsterArchetype& Archetype, const FGT_MonsterBalanceRow& Row)
	{
		Archetype.HpBase = Row.HpBase;
		Archetype.MoveSpeed = Row.MoveSpeed;
		Archetype.DamageMult = Row.DamageMult;
		Archetype.AttackRadius = Row.AttackRadius;
		Archetype.PlayerAttackRange = Row.PlayerAttackRange;
		Archetype.IdleDuration = Row.IdleDuration;
		Archetype.WarningDuration = Row.WarningDuration;
		Archetype.ActiveDuration = Row.ActiveDuration;
		Archetype.CooldownDuration = Row.CooldownDuration;
		Archetype.IdealDistance = Row.IdealDistance;
		Archetype.AimDuration = Row.AimDuration;
		Archetype.AttackInterval = Row.AttackInterval;
		Archetype.SpreadCount = Row.SpreadCount;
		Archetype.SpreadHalfAngleDeg = Row.SpreadHalfAngleDeg;
		Archetype.LaserDuration = Row.LaserDuration;
		Archetype.LaserTickInterval = Row.LaserTickInterval;
		Archetype.LaserTurnRateDeg = Row.LaserTurnRateDeg;
		Archetype.AimTurnRateDeg = Row.AimTurnRateDeg;
		Archetype.KiteStrength = Row.KiteStrength;
		Archetype.WanderWeight = Row.WanderWeight;
		Archetype.bChaseWhenFar = Row.bChaseWhenFar;
		Archetype.bDashAwayOnHit = Row.bDashAwayOnHit;
		Archetype.SplitCount = Row.SplitCount;
		Archetype.SplitPowerMultiplier = Row.SplitPowerMultiplier;
	}

	FGT_MonsterArchetype BuildSlime()
	{
		FGT_MonsterArchetype Result;
		Result.Type = EGT_MonsterType::Slime;
		Result.MovePattern = EGT_MonsterMovePattern::ChasePlayer;
		Result.AttackPattern = EGT_MonsterAttackPattern::MeleeCircle;
		Result.SpritePath = TEXT("/Game/Graytail/Sprites/enemy_slime");
		ApplyBalance(Result, FindBalance(TEXT("slime")));
		return Result;
	}

	FGT_MonsterArchetype BuildBat()
	{
		FGT_MonsterArchetype Result;
		Result.Type = EGT_MonsterType::Bat;
		Result.MovePattern = EGT_MonsterMovePattern::KeepDistance;
		Result.AttackPattern = EGT_MonsterAttackPattern::RangedSpread;
		Result.SpritePath = TEXT("/Game/Graytail/Sprites/Monsters/bat_idle_0");
		ApplyBalance(Result, FindBalance(TEXT("bat")));
		return Result;
	}

	FGT_MonsterArchetype BuildDrone()
	{
		FGT_MonsterArchetype Result;
		Result.Type = EGT_MonsterType::Drone;
		Result.MovePattern = EGT_MonsterMovePattern::KeepDistance;
		Result.AttackPattern = EGT_MonsterAttackPattern::RangedLaser;
		Result.SpritePath = TEXT("/Game/Graytail/Sprites/Monsters/drone_idle_0");
		ApplyBalance(Result, FindBalance(TEXT("drone")));
		return Result;
	}

	FGT_MonsterArchetype BuildSlimeling()
	{
		FGT_MonsterArchetype Result;
		Result.Type = EGT_MonsterType::Slimeling;
		Result.MovePattern = EGT_MonsterMovePattern::RandomRoam;
		Result.AttackPattern = EGT_MonsterAttackPattern::MeleeCircle;
		Result.SpritePath = TEXT("/Game/Graytail/Sprites/Monsters/enemy_slimeling");
		ApplyBalance(Result, FindBalance(TEXT("slimeling")));
		return Result;
	}
}

namespace GT_MonsterCatalog
{
	const FGT_MonsterArchetype& GetArchetype(EGT_MonsterType Type)
	{
		static const FGT_MonsterArchetype Slime = BuildSlime();
		static const FGT_MonsterArchetype Bat = BuildBat();
		static const FGT_MonsterArchetype Drone = BuildDrone();
		static const FGT_MonsterArchetype Slimeling = BuildSlimeling();
		switch (Type)
		{
		case EGT_MonsterType::Bat: return Bat;
		case EGT_MonsterType::Drone: return Drone;
		case EGT_MonsterType::Slimeling: return Slimeling;
		case EGT_MonsterType::Slime:
		default: return Slime;
		}
	}

	EGT_MonsterType PickTypeForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount)
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Monster catalog accessed without valid game data."));
		int32 TotalWeight = 0;
		for (const FGT_MonsterBalanceRow& Row : Snapshot->Monsters.Monsters)
		{
			TotalWeight += Row.SpawnWeight;
		}
		checkf(TotalWeight > 0, TEXT("Validated monster spawn weights are empty."));

		const uint32 Hash = (uint32(Seed) * 2654435761u)
			^ (uint32(X) * 73856093u)
			^ (uint32(Y) * 19349663u);
		int32 Selector = static_cast<int32>(Hash % static_cast<uint32>(TotalWeight));
		for (const FGT_MonsterBalanceRow& Row : Snapshot->Monsters.Monsters)
		{
			if (Row.SpawnWeight <= 0)
			{
				continue;
			}
			if (Selector < Row.SpawnWeight)
			{
				if (Row.Id == TEXT("bat")) return EGT_MonsterType::Bat;
				if (Row.Id == TEXT("drone")) return EGT_MonsterType::Drone;
				return EGT_MonsterType::Slime;
			}
			Selector -= Row.SpawnWeight;
		}
		return EGT_MonsterType::Slime;
	}
}
