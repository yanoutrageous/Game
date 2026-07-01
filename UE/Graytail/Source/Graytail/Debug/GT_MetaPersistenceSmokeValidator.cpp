#include "Debug/GT_MetaPersistenceSmokeValidator.h"

#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Core/GT_RunSubsystem.h"
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

		void AllowWrite(const FString& Slot)
		{
			FailWrites.Remove(SlotKey(Slot, 0));
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
	UGT_MetaProgressSubsystem* MetaProgress,
	UGT_RunSubsystem* RunSubsystem)
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

#if !UE_BUILD_SHIPPING
	if (MetaProgress)
	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		UGT_MetaProgressSubsystem* EscrowMeta = MetaProgress;
		EscrowMeta->SetRepositoryForTests(
			MakeUnique<FGT_MetaSaveRepository>(Backend, TEXT("EscrowMain"), 0));
		EscrowMeta->GMGrantItem(FName(TEXT("armor")));
		FName EquipError;
		EscrowMeta->ToggleEquip(FName(TEXT("armor")), EquipError);
		EscrowMeta->AddConsumable(FName(TEXT("emergency_bandage")), 2);
		EscrowMeta->SetLoadoutConsumable(FName(TEXT("emergency_bandage")), 1);

		FGuid RunId;
		TMap<FName, int32> Consumed;
		const FGT_MetaOperationResult BeginResult = EscrowMeta->BeginRunEscrow(
			1234,
			EGT_Difficulty::Standard,
			RunId,
			Consumed);
		AddCheck(
			OutResults,
			TEXT("MetaRunEscrowCommitsBeforeRun"),
			BeginResult.bSuccess
				&& RunId.IsValid()
				&& EscrowMeta->GetState().ActiveRun.bActive
				&& EscrowMeta->GetState().ActiveRun.RunId == RunId
				&& Consumed.FindRef(FName(TEXT("emergency_bandage"))) == 1
				&& EscrowMeta->GetConsumableCount(FName(TEXT("emergency_bandage"))) == 1
				&& EscrowMeta->GetStats().TotalRuns == 1,
			BeginResult.Message.ToString());

		const FGT_MetaOperationResult RecoveryResult = EscrowMeta->RecoverInterruptedRun();
		AddCheck(
			OutResults,
			TEXT("MetaInterruptedRunRecoversAsAbandonment"),
			RecoveryResult.bSuccess
				&& !EscrowMeta->GetState().ActiveRun.bActive
				&& !EscrowMeta->OwnsItem(FName(TEXT("armor")))
				&& EscrowMeta->GetConsumableCount(FName(TEXT("emergency_bandage"))) == 1,
			RecoveryResult.Message.ToString());
		EscrowMeta->RestoreEngineRepositoryForTests();
	}

	if (MetaProgress)
	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		UGT_MetaProgressSubsystem* EscrowMeta = MetaProgress;
		EscrowMeta->SetRepositoryForTests(
			MakeUnique<FGT_MetaSaveRepository>(Backend, TEXT("SettlementRetryMain"), 0));

		const int32 InitialGold = EscrowMeta->GetGold();
		const int32 InitialExtractions = EscrowMeta->GetStats().TotalExtractions;
		FGuid RunId;
		TMap<FName, int32> Consumed;
		const FGT_MetaOperationResult BeginResult = EscrowMeta->BeginRunEscrow(
			2468,
			EGT_Difficulty::Standard,
			RunId,
			Consumed);

		Backend->FailWrite(TEXT("SettlementRetryMain"));
		FGT_ExtractionReward Reward;
		Reward.DirectGold = 77;
		const FGT_MetaOperationResult FailedSettlement =
			EscrowMeta->CommitExtraction(Reward, TMap<FName, int32>());
		const bool bFailureWasAtomic =
			BeginResult.bSuccess
			&& !FailedSettlement.bSuccess
			&& EscrowMeta->GetState().ActiveRun.bActive
			&& EscrowMeta->GetGold() == InitialGold
			&& EscrowMeta->GetStats().TotalExtractions == InitialExtractions;

		Backend->AllowWrite(TEXT("SettlementRetryMain"));
		const FGT_MetaOperationResult RetriedSettlement =
			EscrowMeta->CommitExtraction(Reward, TMap<FName, int32>());
		const FGT_MetaOperationResult DuplicateSettlement =
			EscrowMeta->CommitExtraction(Reward, TMap<FName, int32>());
		AddCheck(
			OutResults,
			TEXT("MetaExtractionSettlementRetriesExactlyOnce"),
			bFailureWasAtomic
				&& RetriedSettlement.bSuccess
				&& !DuplicateSettlement.bSuccess
				&& !EscrowMeta->GetState().ActiveRun.bActive
				&& EscrowMeta->GetGold() == InitialGold + 77
				&& EscrowMeta->GetStats().TotalExtractions == InitialExtractions + 1,
			RetriedSettlement.Message.ToString());
		EscrowMeta->RestoreEngineRepositoryForTests();
	}

	if (MetaProgress)
	{
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->FailWrite(TEXT("EscrowFailMain"));
		UGT_MetaProgressSubsystem* EscrowMeta = MetaProgress;
		EscrowMeta->SetRepositoryForTests(
			MakeUnique<FGT_MetaSaveRepository>(Backend, TEXT("EscrowFailMain"), 0));
		FGuid RunId;
		TMap<FName, int32> Consumed;
		const FGT_MetaOperationResult BeginResult = EscrowMeta->BeginRunEscrow(
			5678,
			EGT_Difficulty::Hard,
			RunId,
			Consumed);
		AddCheck(
			OutResults,
			TEXT("MetaRunEscrowWriteFailureLeavesStateUnchanged"),
			!BeginResult.bSuccess
				&& !RunId.IsValid()
				&& Consumed.IsEmpty()
				&& !EscrowMeta->GetState().ActiveRun.bActive
				&& EscrowMeta->GetStats().TotalRuns == 0,
			BeginResult.Message.ToString());
		EscrowMeta->RestoreEngineRepositoryForTests();
	}

	if (MetaProgress && RunSubsystem)
	{
		RunSubsystem->EndCurrentRun();
		const TSharedRef<FGT_FakeMetaSaveBackend> Backend =
			MakeShared<FGT_FakeMetaSaveBackend>();
		Backend->FailWrite(TEXT("RunStartFailMain"));
		MetaProgress->SetRepositoryForTests(
			MakeUnique<FGT_MetaSaveRepository>(Backend, TEXT("RunStartFailMain"), 0));
		UGT_RunContext* FailedRun = RunSubsystem->StartNewRunStandard(
			9012,
			EGT_Difficulty::Standard);
		AddCheck(
			OutResults,
			TEXT("RunStartWaitsForEscrowCommit"),
			FailedRun == nullptr
				&& RunSubsystem->GetCurrentRunContext() == nullptr
				&& !MetaProgress->GetState().ActiveRun.bActive,
			MetaProgress->GetLastPersistenceResult().Message.ToString());
		RunSubsystem->EndCurrentRun();
		MetaProgress->RestoreEngineRepositoryForTests();
	}
#endif
}
