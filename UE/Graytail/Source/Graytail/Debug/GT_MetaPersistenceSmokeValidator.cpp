#include "Debug/GT_MetaPersistenceSmokeValidator.h"

#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Domains/Meta/GT_MetaPersistenceTypes.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaSaveRepository.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "Save/GT_MetaSaveGame.h"
#include "Save/GT_SaveGame.h"

namespace
{
	class FGT_FakeMetaSaveBackend final : public IGT_MetaSaveBackend
	{
	public:
		virtual bool Exists(const FString& Slot, int32 UserIndex) const override
		{
			return Slots.Contains(SlotKey(Slot, UserIndex));
		}

		virtual USaveGame* Load(const FString& Slot, int32 UserIndex) const override
		{
			const TStrongObjectPtr<USaveGame>* Found = Slots.Find(SlotKey(Slot, UserIndex));
			return Found ? Found->Get() : nullptr;
		}

		virtual bool Save(USaveGame& Object, const FString& Slot, int32 UserIndex) override
		{
			const FString Key = SlotKey(Slot, UserIndex);
			if (FailWrites.Contains(Key))
			{
				return false;
			}
			Slots.Add(Key, TStrongObjectPtr<USaveGame>(&Object));
			WriteCounts.FindOrAdd(Key) += 1;
			return true;
		}

		void Put(const FString& Slot, USaveGame* Object)
		{
			Slots.Add(SlotKey(Slot, 0), TStrongObjectPtr<USaveGame>(Object));
		}

		void FailWrite(const FString& Slot)
		{
			FailWrites.Add(SlotKey(Slot, 0));
		}

		int32 WriteCount(const FString& Slot) const
		{
			return WriteCounts.FindRef(SlotKey(Slot, 0));
		}

	private:
		static FString SlotKey(const FString& Slot, int32 UserIndex)
		{
			return FString::Printf(TEXT("%s:%d"), *Slot, UserIndex);
		}

		TMap<FString, TStrongObjectPtr<USaveGame>> Slots;
		TSet<FString> FailWrites;
		TMap<FString, int32> WriteCounts;
	};

	UGT_MetaSaveGame* MakeSave(int32 Gold)
	{
		UGT_MetaSaveGame* Save = NewObject<UGT_MetaSaveGame>();
		Save->SaveVersion = UGT_MetaSaveGame::CurrentSaveVersion;
		Save->State.Gold = Gold;
		return Save;
	}

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

void GT_MetaPersistenceSmokeValidator::AppendChecks(
	TArray<FGT_RuntimeSmokeCheckResult>& OutResults,
	UGT_MetaProgressSubsystem* MetaProgress)
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

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		const FGT_MetaLoadResult Result = Repository.Load();
		AddCheck(
			OutResults,
			TEXT("MetaSaveMissingSlotsCreateFreshState"),
			Result.Status == EGT_MetaPersistenceStatus::Fresh
				&& Result.State.Gold == 0,
			Result.Message.ToString());
	}

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		UGT_MetaSaveGame* VersionOneSave = MakeSave(12);
		VersionOneSave->SaveVersion = 1;
		VersionOneSave->State.ActiveRun.bActive = true;
		VersionOneSave->State.ActiveRun.RunId = FGuid::NewGuid();
		Backend->Put(TEXT("TestMain"), VersionOneSave);
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		const FGT_MetaLoadResult Result = Repository.Load();
		const UGT_MetaSaveGame* Rewritten = Cast<UGT_MetaSaveGame>(
			Backend->Load(TEXT("TestMain"), 0));
		AddCheck(
			OutResults,
			TEXT("MetaSaveVersionOneLoadRewritesVersionTwo"),
			Result.Status == EGT_MetaPersistenceStatus::Ready
				&& Result.State.Gold == 12
				&& !Result.State.ActiveRun.bActive
				&& Rewritten
				&& Rewritten->SaveVersion == UGT_MetaSaveGame::CurrentSaveVersion,
			Result.Message.ToString());
	}

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->Put(TEXT("TestMain"), MakeSave(10));
		Backend->FailWrite(TEXT("TestMainBackup"));
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		FGT_MetaProgressState Candidate;
		Candidate.Gold = 20;
		const FGT_MetaOperationResult Result = Repository.Commit(Candidate);
		const UGT_MetaSaveGame* Main = Cast<UGT_MetaSaveGame>(
			Backend->Load(TEXT("TestMain"), 0));
		AddCheck(
			OutResults,
			TEXT("MetaSaveBackupFailurePreventsMainWrite"),
			!Result.bSuccess
				&& Main
				&& Main->State.Gold == 10
				&& Backend->WriteCount(TEXT("TestMain")) == 0,
			Result.Message.ToString());
	}

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->Put(TEXT("TestMain"), MakeSave(10));
		Backend->FailWrite(TEXT("TestMain"));
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		FGT_MetaProgressState Candidate;
		Candidate.Gold = 20;
		const FGT_MetaOperationResult Result = Repository.Commit(Candidate);
		const UGT_MetaSaveGame* Main = Cast<UGT_MetaSaveGame>(
			Backend->Load(TEXT("TestMain"), 0));
		const UGT_MetaSaveGame* Backup = Cast<UGT_MetaSaveGame>(
			Backend->Load(TEXT("TestMainBackup"), 0));
		AddCheck(
			OutResults,
			TEXT("MetaSaveMainFailurePreservesPreviousState"),
			!Result.bSuccess
				&& Main
				&& Main->State.Gold == 10
				&& Backup
				&& Backup->State.Gold == 10,
			Result.Message.ToString());
	}

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->Put(TEXT("TestMain"), NewObject<UGT_SaveGame>());
		Backend->Put(TEXT("TestMainBackup"), MakeSave(31));
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		const FGT_MetaLoadResult Result = Repository.Load();
		AddCheck(
			OutResults,
			TEXT("MetaSaveCorruptMainRecoversBackup"),
			Result.Status == EGT_MetaPersistenceStatus::RecoveredBackup
				&& Result.State.Gold == 31,
			Result.Message.ToString());
	}

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->Put(TEXT("TestMain"), NewObject<UGT_SaveGame>());
		Backend->Put(TEXT("TestMainBackup"), NewObject<UGT_SaveGame>());
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		const FGT_MetaLoadResult Result = Repository.Load();
		AddCheck(
			OutResults,
			TEXT("MetaSaveDualCorruptionBlocksLoad"),
			Result.Status == EGT_MetaPersistenceStatus::Corrupt,
			Result.Message.ToString());
	}

	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->Put(TEXT("TestMain"), NewObject<UGT_SaveGame>());
		Backend->Put(TEXT("TestMainBackup"), MakeSave(44));
		Backend->FailWrite(TEXT("TestMain"));
		FGT_MetaSaveRepository Repository(Backend, TEXT("TestMain"), 0);
		const FGT_MetaLoadResult Result = Repository.Load();
		AddCheck(
			OutResults,
			TEXT("MetaSaveBackupRecoveryWriteFailureBlocksMutation"),
			Result.Status == EGT_MetaPersistenceStatus::RecoveryWritePending
				&& Result.State.Gold == 44,
			Result.Message.ToString());
	}

#if !UE_BUILD_SHIPPING
	if (MetaProgress)
	{
		const int32 GoldBeforeFailure = MetaProgress->GetGold();
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->FailWrite(TEXT("TransactionalMain"));
		MetaProgress->SetRepositoryForTests(
			MakeUnique<FGT_MetaSaveRepository>(Backend, TEXT("TransactionalMain"), 0));
		MetaProgress->AddGold(50);
		AddCheck(
			OutResults,
			TEXT("MetaSaveWriteFailureRollsBackMemory"),
			MetaProgress->GetGold() == GoldBeforeFailure
				&& !MetaProgress->GetLastPersistenceResult().bSuccess,
			MetaProgress->GetLastPersistenceResult().Message.ToString());
		MetaProgress->RestoreEngineRepositoryForTests();
	}
#endif
}
