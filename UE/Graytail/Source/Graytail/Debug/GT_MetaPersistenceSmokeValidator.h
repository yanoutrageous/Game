#pragma once

#include "CoreMinimal.h"

struct FGT_RuntimeSmokeCheckResult;
class UGT_MetaProgressSubsystem;
class UGT_RunSubsystem;

namespace GT_MetaPersistenceSmokeValidator
{
	void AppendChecks(
		TArray<FGT_RuntimeSmokeCheckResult>& OutResults,
		UGT_MetaProgressSubsystem* MetaProgress,
		UGT_RunSubsystem* RunSubsystem);
}
