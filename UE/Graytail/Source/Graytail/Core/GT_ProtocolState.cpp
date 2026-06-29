#include "Core/GT_ProtocolState.h"

#include "Data/GT_GameDataSubsystem.h"

namespace
{
	const FGT_ProtocolBalanceConfig& GetProtocolConfig()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Protocol rules accessed without valid game data."));
		return Snapshot->Core.Protocol;
	}
}

namespace GT_ProtocolRules
{
	int32 GetMaxPressure()
	{
		return GetProtocolConfig().MaxPressure;
	}

	int32 GetExplorePressure()
	{
		return GetProtocolConfig().ExplorePressure;
	}

	int32 GetMinePressure()
	{
		return GetProtocolConfig().MinePressure;
	}

	int32 GetCombatKillPressure()
	{
		return GetProtocolConfig().CombatKillPressure;
	}

	int32 ComputeLevel(int32 Pressure)
	{
		for (const FGT_ProtocolThresholdConfig& Threshold : GetProtocolConfig().Thresholds)
		{
			if (Pressure >= Threshold.Pressure)
			{
				return Threshold.Level;
			}
		}
		return 0;
	}
}
