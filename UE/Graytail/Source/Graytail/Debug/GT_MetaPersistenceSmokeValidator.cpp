#include "Debug/GT_MetaPersistenceSmokeValidator.h"

#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Domains/Meta/GT_MetaPersistenceTypes.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "Save/GT_MetaSaveGame.h"

namespace
{
	void AddCheck(
		TArray<FGT_RuntimeSmokeCheckResult>& OutResults,
		const TCHAR* Name,
		bool bPassed,
		const FString& Message)
	{
		FGT_RuntimeSmokeCheckResult Result;
		Result.CheckName = FName(Name);
		Result.bPassed = bPassed;
		Result.Message = Message;
		OutResults.Add(MoveTemp(Result));
	}
}

void GT_MetaPersistenceSmokeValidator::AppendChecks(TArray<FGT_RuntimeSmokeCheckResult>& OutResults)
{
	FGT_MetaProgressState VersionOneState;
	VersionOneState.Gold = 17;
	VersionOneState.ActiveRun.bActive = true;
	VersionOneState.ActiveRun.RunId = FGuid::NewGuid();
	FString Error;
	const bool bVersionOneMigrated = GT_MigrateMetaSave(1, VersionOneState, Error);
	AddCheck(
		OutResults,
		TEXT("MetaSaveVersionOneMigrates"),
		bVersionOneMigrated
			&& VersionOneState.Gold == 17
			&& !VersionOneState.ActiveRun.bActive,
		Error);

	FGT_MetaProgressState VersionTwoState;
	VersionTwoState.Gold = 23;
	VersionTwoState.ActiveRun.bActive = true;
	VersionTwoState.ActiveRun.RunId = FGuid::NewGuid();
	Error.Reset();
	const bool bVersionTwoAccepted = GT_MigrateMetaSave(2, VersionTwoState, Error);
	AddCheck(
		OutResults,
		TEXT("MetaSaveVersionTwoPreserved"),
		bVersionTwoAccepted
			&& VersionTwoState.Gold == 23
			&& VersionTwoState.ActiveRun.bActive,
		Error);

	FGT_MetaProgressState FutureState;
	Error.Reset();
	const bool bFutureRejected = !GT_MigrateMetaSave(
		UGT_MetaSaveGame::CurrentSaveVersion + 1,
		FutureState,
		Error);
	AddCheck(
		OutResults,
		TEXT("MetaSaveFutureVersionRejected"),
		bFutureRejected && !Error.IsEmpty(),
		Error);

	FGT_MetaProgressState DirtyState;
	DirtyState.Gold = -100;
	DirtyState.OwnedItems = {
		FName(TEXT("company_badge")),
		FName(TEXT("company_badge")),
		FName(TEXT("unknown_equipment"))
	};
	DirtyState.EquippedItems = {
		FName(TEXT("company_badge")),
		FName(TEXT("unknown_equipment"))
	};
	DirtyState.UnlockedTalents = {
		FName(TEXT("talent_map")),
		FName(TEXT("talent_map")),
		FName(TEXT("unknown_talent"))
	};
	DirtyState.ConsumableStock.Add(FName(TEXT("emergency_bandage")), -2);
	DirtyState.LoadoutConsumables.Add(FName(TEXT("emergency_bandage")), 99);
	DirtyState.Stats.TotalRuns = -1;
	Error.Reset();
	const bool bDirtySanitized = GT_ValidateAndSanitizeMetaState(DirtyState, Error);
	AddCheck(
		OutResults,
		TEXT("MetaSaveSafeCorruptionSanitized"),
		bDirtySanitized
			&& DirtyState.Gold == 0
			&& DirtyState.OwnedItems == TArray<FName>({
				FName(TEXT("company_badge")),
				FName(TEXT("unknown_equipment"))
			})
			&& DirtyState.EquippedItems == TArray<FName>({ FName(TEXT("company_badge")) })
			&& DirtyState.UnlockedTalents == TArray<FName>({
				FName(TEXT("talent_map")),
				FName(TEXT("unknown_talent"))
			})
			&& !DirtyState.ConsumableStock.Contains(FName(TEXT("emergency_bandage")))
			&& !DirtyState.LoadoutConsumables.Contains(FName(TEXT("emergency_bandage")))
			&& DirtyState.Stats.TotalRuns == 0,
		Error);

	FGT_MetaProgressState InvalidEscrowState;
	InvalidEscrowState.ActiveRun.bActive = true;
	InvalidEscrowState.ActiveRun.RunId.Invalidate();
	Error.Reset();
	const bool bInvalidEscrowRejected = !GT_ValidateAndSanitizeMetaState(InvalidEscrowState, Error);
	AddCheck(
		OutResults,
		TEXT("MetaSaveInvalidActiveEscrowRejected"),
		bInvalidEscrowRejected && Error.Contains(TEXT("RunId")),
		Error);
}
