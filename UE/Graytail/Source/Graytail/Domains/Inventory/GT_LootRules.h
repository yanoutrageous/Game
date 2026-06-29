#pragma once

#include "CoreMinimal.h"
#include "Domains/Inventory/GT_InventoryTypes.h"

namespace GT_LootRules
{
	GRAYTAIL_API int32 RollRange(int32 Seed, int32 X, int32 Y, int32 Salt, int32 MinValue, int32 MaxValue);
	GRAYTAIL_API FGT_SearchReward ComputeSearchReward(
		int32 Seed,
		int32 X,
		int32 Y,
		int32 AdjacentMineCount,
		bool bIsChest);

	GRAYTAIL_API int32 GetFleeGoldDropPercent();
	GRAYTAIL_API int32 GetFleeItemDropPercent();
	GRAYTAIL_API int32 GetFleeDropSalt();
}
