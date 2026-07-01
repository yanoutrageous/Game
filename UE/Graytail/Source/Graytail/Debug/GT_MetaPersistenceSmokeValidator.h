#pragma once

#include "CoreMinimal.h"

struct FGT_RuntimeSmokeCheckResult;
class UGT_MetaProgressSubsystem;

namespace GT_MetaPersistenceSmokeValidator
{
	void AppendChecks(
		TArray<FGT_RuntimeSmokeCheckResult>& OutResults,
		UGT_MetaProgressSubsystem* MetaProgress);
}
