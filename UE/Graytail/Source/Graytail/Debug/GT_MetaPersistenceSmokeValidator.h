#pragma once

#include "CoreMinimal.h"

struct FGT_RuntimeSmokeCheckResult;

namespace GT_MetaPersistenceSmokeValidator
{
	void AppendChecks(TArray<FGT_RuntimeSmokeCheckResult>& OutResults);
}
