#include "Domains/Meta/GT_MetaProgressSubsystem.h"

#include "Domains/Meta/GT_MetaCatalog.h"
#include "Domains/Meta/GT_MetaPersistenceTypes.h"
#include "Data/GT_GameDataSubsystem.h"
#include "Save/GT_MetaSaveGame.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailMeta, Log, All);

FString UGT_MetaProgressSubsystem::SaveSlotName()
{
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("GraytailSaveSlot="), Override)
		&& !Override.IsEmpty())
	{
		return Override;
	}
	return TEXT("GraytailMeta");
}

// ============================================================================
// 生命周期 / 存读档
// ============================================================================

void UGT_MetaProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SaveRepository = FGT_MetaSaveRepository::CreateEngine(SaveSlotName(), SaveUserIndex);
	const FGT_MetaLoadResult LoadResult = Load();
	if (LoadResult.IsUsable() && State.ActiveRun.bActive)
	{
		const FGT_MetaOperationResult Recovery = RecoverInterruptedRun();
		if (Recovery.bSuccess)
		{
			StartupNotice = FText::FromString(
				TEXT("上一局异常中断，已按主动放弃处理装备和消耗品。"));
		}
	}
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::Save()
{
	if (!SaveRepository)
	{
		SaveRepository = FGT_MetaSaveRepository::CreateEngine(SaveSlotName(), SaveUserIndex);
	}
	FGT_MetaProgressState Candidate = State;
	FString ValidationError;
	if (!GT_ValidateAndSanitizeMetaState(Candidate, ValidationError))
	{
		State = LastCommittedState;
		PersistenceStatus = EGT_MetaPersistenceStatus::WriteFailed;
		LastPersistenceResult = FGT_MetaOperationResult::Failure(
			FName(TEXT("save_validation_failed")),
			FText::FromString(ValidationError));
		return LastPersistenceResult;
	}

	LastPersistenceResult = SaveRepository->Commit(Candidate);
	if (!LastPersistenceResult.bSuccess)
	{
		State = LastCommittedState;
		PersistenceStatus = EGT_MetaPersistenceStatus::WriteFailed;
		UE_LOG(
			LogGraytailMeta,
			Warning,
			TEXT("Save failed: %s"),
			*LastPersistenceResult.Message.ToString());
		return LastPersistenceResult;
	}

	State = MoveTemp(Candidate);
	LastCommittedState = State;
	PersistenceStatus = EGT_MetaPersistenceStatus::Ready;
	UE_LOG(LogGraytailMeta, Log, TEXT("Saved: gold=%d"), State.Gold);
	return LastPersistenceResult;
}

FGT_MetaLoadResult UGT_MetaProgressSubsystem::Load()
{
	if (!SaveRepository)
	{
		SaveRepository = FGT_MetaSaveRepository::CreateEngine(SaveSlotName(), SaveUserIndex);
	}
	FGT_MetaLoadResult Result = SaveRepository->Load();
	PersistenceStatus = Result.Status;
	if (!Result.IsUsable())
	{
		LastPersistenceResult = FGT_MetaOperationResult::Failure(
			FName(TEXT("save_load_failed")),
			Result.Message);
		UE_LOG(LogGraytailMeta, Error, TEXT("Load failed: %s"), *Result.Message.ToString());
		return Result;
	}

	State = Result.State;
	LastCommittedState = State;
	LastPersistenceResult = FGT_MetaOperationResult::Success();
	UE_LOG(LogGraytailMeta, Log, TEXT("Loaded: gold=%d"), State.Gold);
	return Result;
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::CommitCandidate(
	FGT_MetaProgressState Candidate)
{
	if (PersistenceStatus == EGT_MetaPersistenceStatus::Corrupt
		|| PersistenceStatus == EGT_MetaPersistenceStatus::UnsupportedVersion
		|| PersistenceStatus == EGT_MetaPersistenceStatus::RecoveryWritePending)
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("persistence_blocked")),
			FText::FromString(TEXT("Progress is blocked until the save error is resolved.")));
	}
	if (!SaveRepository)
	{
		SaveRepository = FGT_MetaSaveRepository::CreateEngine(SaveSlotName(), SaveUserIndex);
	}

	FString ValidationError;
	if (!GT_ValidateAndSanitizeMetaState(Candidate, ValidationError))
	{
		LastPersistenceResult = FGT_MetaOperationResult::Failure(
			FName(TEXT("save_validation_failed")),
			FText::FromString(ValidationError));
		return LastPersistenceResult;
	}

	LastPersistenceResult = SaveRepository->Commit(Candidate);
	if (!LastPersistenceResult.bSuccess)
	{
		PersistenceStatus = EGT_MetaPersistenceStatus::WriteFailed;
		return LastPersistenceResult;
	}

	State = MoveTemp(Candidate);
	LastCommittedState = State;
	PersistenceStatus = EGT_MetaPersistenceStatus::Ready;
	return LastPersistenceResult;
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::BeginRunEscrow(
	int32 Seed,
	EGT_Difficulty Difficulty,
	FGuid& OutRunId,
	TMap<FName, int32>& OutConsumedConsumables)
{
	OutRunId.Invalidate();
	OutConsumedConsumables.Reset();
	if (State.ActiveRun.bActive)
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("active_run_exists")),
			FText::FromString(TEXT("An active run escrow already exists.")));
	}

	FGT_MetaProgressState Candidate = State;
	TMap<FName, int32> Consumed;
	for (const TPair<FName, int32>& Pair : Candidate.LoadoutConsumables)
	{
		const int32 Stock = Candidate.ConsumableStock.FindRef(Pair.Key);
		const int32 Take = FMath::Min(Pair.Value, Stock);
		if (Take <= 0)
		{
			continue;
		}
		Consumed.Add(Pair.Key, Take);
		const int32 Remaining = Stock - Take;
		if (Remaining > 0)
		{
			Candidate.ConsumableStock.Add(Pair.Key, Remaining);
		}
		else
		{
			Candidate.ConsumableStock.Remove(Pair.Key);
		}
	}

	FGT_ActiveRunEscrow Escrow;
	Escrow.bActive = true;
	Escrow.RunId = FGuid::NewGuid();
	Escrow.Seed = Seed;
	Escrow.Difficulty = Difficulty;
	Escrow.EquippedItemIds = Candidate.EquippedItems;
	Escrow.ConsumedConsumables = Consumed;
	Escrow.StartedAtUtc = FDateTime::UtcNow();
	Candidate.ActiveRun = Escrow;
	Candidate.Stats.TotalRuns += 1;

	const FGT_MetaOperationResult Result = CommitCandidate(MoveTemp(Candidate));
	if (Result.bSuccess)
	{
		OutRunId = Escrow.RunId;
		OutConsumedConsumables = MoveTemp(Consumed);
	}
	return Result;
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::CommitExtraction(
	const FGT_ExtractionReward& Reward,
	const TMap<FName, int32>& ReturnedConsumables)
{
	if (!State.ActiveRun.bActive)
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("no_active_run")),
			FText::FromString(TEXT("Extraction has no active run escrow.")));
	}

	FGT_MetaProgressState Candidate = State;
	const int32 GoldAdded =
		ClampNonNegative(Reward.DirectGold)
		+ ClampNonNegative(Reward.LoosePartsGold);
	Candidate.Gold += GoldAdded;
	Candidate.Stats.TotalGoldEarned += GoldAdded;
	Candidate.Stats.TotalExtractions += 1;

	for (const TPair<FName, int32>& Pair : ReturnedConsumables)
	{
		if (GT_MetaCatalog::FindConsumable(Pair.Key) && Pair.Value > 0)
		{
			Candidate.ConsumableStock.FindOrAdd(Pair.Key) += Pair.Value;
		}
	}

	int32 ItemCount = 0;
	int32 ItemValue = 0;
	for (const FGT_RewardItem& Item : Reward.CarriedItems)
	{
		const int32 Count = ClampNonNegative(Item.Count);
		if (Item.ItemId.IsNone() || Count <= 0)
		{
			continue;
		}
		ItemCount += Count;
		ItemValue += FMath::Max(0, Item.Value) * Count;
		FGT_WarehouseEntry* Entry = Candidate.Warehouse.FindByPredicate(
			[&Item](const FGT_WarehouseEntry& Existing)
			{
				return Existing.ItemId == Item.ItemId;
			});
		if (!Entry)
		{
			Entry = &Candidate.Warehouse.AddDefaulted_GetRef();
			Entry->ItemId = Item.ItemId;
			Entry->Value = FMath::Max(0, Item.Value);
			Entry->Source = FName(TEXT("recovered"));
		}
		Entry->Count += Count;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Candidate.Recovery.RecentItemIds.Insert(Item.ItemId, 0);
		}
	}
	if (ItemCount > 0)
	{
		Candidate.Recovery.TotalItems += ItemCount;
		Candidate.Recovery.TotalValue += ItemValue;
		Candidate.Recovery.TotalExtractionsWithItems += 1;
	}
	if (Candidate.Recovery.RecentItemIds.Num() > GT_MetaCatalog::GetRecentRecoveryMax())
	{
		Candidate.Recovery.RecentItemIds.SetNum(GT_MetaCatalog::GetRecentRecoveryMax());
	}
	Candidate.ActiveRun = FGT_ActiveRunEscrow();
	return CommitCandidate(MoveTemp(Candidate));
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::CommitFailure(
	const FGT_FailureSettlement& Settlement)
{
	if (!State.ActiveRun.bActive)
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("no_active_run")),
			FText::FromString(TEXT("Failure settlement has no active run escrow.")));
	}

	FGT_MetaProgressState Candidate = State;
	const TArray<FName> LostEquipment = Candidate.ActiveRun.EquippedItemIds;
	bool bRecoveredAnyItem = false;
	for (FName ItemId : LostEquipment)
	{
		Candidate.OwnedItems.Remove(ItemId);
		Candidate.EquippedItems.Remove(ItemId);
	}
	const int32 GoldAdded = ClampNonNegative(Settlement.Gold);
	Candidate.Gold += GoldAdded;
	Candidate.Stats.TotalGoldEarned += GoldAdded;
	for (const FGT_RewardItem& Item : Settlement.SalvagedItems)
	{
		const int32 Count = ClampNonNegative(Item.Count);
		if (Item.ItemId.IsNone() || Count <= 0)
		{
			continue;
		}
		FGT_WarehouseEntry* Entry = Candidate.Warehouse.FindByPredicate(
			[&Item](const FGT_WarehouseEntry& Existing)
			{
				return Existing.ItemId == Item.ItemId;
			});
		if (!Entry)
		{
			Entry = &Candidate.Warehouse.AddDefaulted_GetRef();
			Entry->ItemId = Item.ItemId;
			Entry->Value = FMath::Max(0, Item.Value);
			Entry->Source = FName(TEXT("recovered"));
		}
		Entry->Count += Count;
		Candidate.Recovery.TotalItems += Count;
		Candidate.Recovery.TotalValue += FMath::Max(0, Item.Value) * Count;
		bRecoveredAnyItem = true;
		Candidate.Recovery.RecentItemIds.Insert(Item.ItemId, 0);
	}
	if (bRecoveredAnyItem)
	{
		Candidate.Recovery.TotalExtractionsWithItems += 1;
	}
	Candidate.ActiveRun = FGT_ActiveRunEscrow();
	const FGT_MetaOperationResult Result = CommitCandidate(MoveTemp(Candidate));
	if (Result.bSuccess)
	{
		LastFailureLostEquips = LostEquipment;
	}
	return Result;
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::RecoverInterruptedRun()
{
	if (!State.ActiveRun.bActive)
	{
		return FGT_MetaOperationResult::Success();
	}
	FGT_FailureSettlement Settlement;
	Settlement.Reason = FName(TEXT("Interrupted"));
	return CommitFailure(Settlement);
}

FText UGT_MetaProgressSubsystem::ConsumeStartupNotice()
{
	FText Notice = StartupNotice;
	StartupNotice = FText::GetEmpty();
	return Notice;
}

bool UGT_MetaProgressSubsystem::CanMutateProgress() const
{
	return PersistenceStatus == EGT_MetaPersistenceStatus::Ready
		|| PersistenceStatus == EGT_MetaPersistenceStatus::Fresh
		|| PersistenceStatus == EGT_MetaPersistenceStatus::RecoveredBackup
		|| PersistenceStatus == EGT_MetaPersistenceStatus::WriteFailed;
}

FGT_MetaOperationResult UGT_MetaProgressSubsystem::ResetCorruptSaveAndCreateFresh()
{
	if (PersistenceStatus != EGT_MetaPersistenceStatus::Corrupt
		&& PersistenceStatus != EGT_MetaPersistenceStatus::UnsupportedVersion)
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("reset_not_allowed")),
			FText::FromString(TEXT("The current save status does not allow reset.")));
	}
	if (!SaveRepository)
	{
		SaveRepository = FGT_MetaSaveRepository::CreateEngine(SaveSlotName(), SaveUserIndex);
	}
	LastPersistenceResult = SaveRepository->ResetWithFreshState();
	if (LastPersistenceResult.bSuccess)
	{
		State = FGT_MetaProgressState();
		LastCommittedState = State;
		PersistenceStatus = EGT_MetaPersistenceStatus::Ready;
	}
	return LastPersistenceResult;
}

#if !UE_BUILD_SHIPPING
void UGT_MetaProgressSubsystem::SetRepositoryForTests(
	TUniquePtr<FGT_MetaSaveRepository> InRepository)
{
	SaveRepository = MoveTemp(InRepository);
	LastCommittedState = State;
	LastPersistenceResult = FGT_MetaOperationResult::Success();
	PersistenceStatus = EGT_MetaPersistenceStatus::Ready;
}

void UGT_MetaProgressSubsystem::RestoreEngineRepositoryForTests()
{
	SaveRepository = FGT_MetaSaveRepository::CreateEngine(SaveSlotName(), SaveUserIndex);
	Load();
}
#endif

void UGT_MetaProgressSubsystem::SanitizeAfterLoad()
{
	if (!GT_GameData::IsReady())
	{
		return;
	}

	FString ValidationError;
	if (!GT_ValidateAndSanitizeMetaState(State, ValidationError))
	{
		UE_LOG(LogGraytailMeta, Error, TEXT("SanitizeAfterLoad: %s"), *ValidationError);
	}
}

// ============================================================================
// 币
// ============================================================================

void UGT_MetaProgressSubsystem::AddGold(int32 Amount)
{
	Amount = ClampNonNegative(Amount);
	if (Amount <= 0) { return; }
	State.Gold += Amount;
	State.Stats.TotalGoldEarned += Amount;
	Save();
}

bool UGT_MetaProgressSubsystem::SpendGold(int32 Amount)
{
	Amount = ClampNonNegative(Amount);
	if (Amount <= 0) { return false; }
	if (State.Gold < Amount) { return false; }
	State.Gold -= Amount;
	return Save().bSuccess;
}

// ============================================================================
// 装备 / 天赋
// ============================================================================

bool UGT_MetaProgressSubsystem::BuyItem(FName ItemId, FName& OutError)
{
	if (State.OwnedItems.Contains(ItemId)) { OutError = TEXT("already_owned"); return false; }
	const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(ItemId);
	if (!Def) { OutError = TEXT("not_found"); return false; }
	if (State.Gold < Def->Price) { OutError = TEXT("not_enough_gold"); return false; }
	State.Gold -= Def->Price;
	State.OwnedItems.AddUnique(ItemId);
	if (!Save().bSuccess) { OutError = TEXT("save_write_failed"); return false; }
	return true;
}

bool UGT_MetaProgressSubsystem::ToggleEquip(FName ItemId, FName& OutError)
{
	if (!State.OwnedItems.Contains(ItemId)) { OutError = TEXT("not_owned"); return false; }
	const int32 Index = State.EquippedItems.IndexOfByKey(ItemId);
	if (Index != INDEX_NONE)
	{
		State.EquippedItems.RemoveAt(Index);   // 已装备 -> 卸下
		if (!Save().bSuccess) { OutError = TEXT("save_write_failed"); return false; }
		return true;
	}
	if (State.EquippedItems.Num() >= GT_MetaCatalog::GetMaxEquipped())
	{
		OutError = TEXT("max_equipped");
		return false;
	}
	State.EquippedItems.Add(ItemId);
	if (!Save().bSuccess) { OutError = TEXT("save_write_failed"); return false; }
	return true;
}

TArray<FName> UGT_MetaProgressSubsystem::LoseEquippedItemsOnFailure()
{
	// 带入(已装备)的装备撤离失败损失; 拥有但未装备的保留(轻装规避=自然缓解)。
	LastFailureLostEquips = State.EquippedItems;
	for (const FName& Id : LastFailureLostEquips)
	{
		State.OwnedItems.Remove(Id);
	}
	State.EquippedItems.Reset();
	if (LastFailureLostEquips.Num() > 0) { Save(); }
	return LastFailureLostEquips;
}

bool UGT_MetaProgressSubsystem::UnlockTalent(FName TalentId, FName& OutError)
{
	if (State.UnlockedTalents.Contains(TalentId)) { OutError = TEXT("already_unlocked"); return false; }
	const FGT_TalentDef* Def = GT_MetaCatalog::FindTalent(TalentId);
	if (!Def) { OutError = TEXT("not_found"); return false; }
	if (State.Gold < Def->Price) { OutError = TEXT("not_enough_gold"); return false; }
	State.Gold -= Def->Price;
	State.UnlockedTalents.AddUnique(TalentId);
	if (!Save().bSuccess) { OutError = TEXT("save_write_failed"); return false; }
	return true;
}

// ============================================================================
// 消耗品库存 / loadout
// ============================================================================

bool UGT_MetaProgressSubsystem::BuyConsumable(FName ItemId, int32 Count, FName& OutError)
{
	const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(ItemId);
	if (!Def) { OutError = TEXT("not_found"); return false; }
	Count = ClampNonNegative(Count);
	if (Count <= 0) { OutError = TEXT("invalid_count"); return false; }
	const int32 Price = Def->Price * Count;
	if (State.Gold < Price) { OutError = TEXT("not_enough_gold"); return false; }
	State.Gold -= Price;
	State.ConsumableStock.FindOrAdd(ItemId) += Count;
	if (!Save().bSuccess) { OutError = TEXT("save_write_failed"); return false; }
	return true;
}

bool UGT_MetaProgressSubsystem::AddConsumable(FName ItemId, int32 Count)
{
	if (!GT_MetaCatalog::FindConsumable(ItemId)) { return false; }
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	State.ConsumableStock.FindOrAdd(ItemId) += Count;
	return Save().bSuccess;
}

bool UGT_MetaProgressSubsystem::RemoveConsumable(FName ItemId, int32 Count)
{
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	int32* Stock = State.ConsumableStock.Find(ItemId);
	if (!Stock || *Stock < Count) { return false; }
	*Stock -= Count;
	if (*Stock <= 0) { State.ConsumableStock.Remove(ItemId); }
	// loadout 不能超过剩余库存
	if (int32* Loadout = State.LoadoutConsumables.Find(ItemId))
	{
		const int32 Remaining = State.ConsumableStock.FindRef(ItemId);
		if (*Loadout > Remaining) { *Loadout = Remaining; }
		if (*Loadout <= 0) { State.LoadoutConsumables.Remove(ItemId); }
	}
	return Save().bSuccess;
}

bool UGT_MetaProgressSubsystem::SetLoadoutConsumable(FName ItemId, int32 Count)
{
	const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(ItemId);
	if (!Def) { return false; }
	Count = ClampNonNegative(Count);
	const int32 Stock = State.ConsumableStock.FindRef(ItemId);
	Count = FMath::Min(Count, Stock);
	if (Def->MaxCarry > 0) { Count = FMath::Min(Count, Def->MaxCarry); }
	if (Count <= 0) { State.LoadoutConsumables.Remove(ItemId); }
	else { State.LoadoutConsumables.Add(ItemId, Count); }
	return Save().bSuccess;
}

TMap<FName, int32> UGT_MetaProgressSubsystem::ConsumeLoadoutForRun()
{
	TMap<FName, int32> RunLoadout;
	for (const TPair<FName, int32>& Pair : State.LoadoutConsumables)
	{
		const int32 Stock = State.ConsumableStock.FindRef(Pair.Key);
		const int32 Take = FMath::Min(Pair.Value, Stock);
		if (Take > 0)
		{
			RunLoadout.Add(Pair.Key, Take);
			int32& S = State.ConsumableStock.FindOrAdd(Pair.Key);
			S -= Take;
			if (S <= 0) { State.ConsumableStock.Remove(Pair.Key); }
		}
	}
	// 库存变了, 重新夹 loadout。
	SanitizeAfterLoad();
	Save();
	return RunLoadout;
}

// ============================================================================
// 仓库 / 统计 / 结算
// ============================================================================

FGT_WarehouseEntry* UGT_MetaProgressSubsystem::FindWarehouse(FName ItemId)
{
	return State.Warehouse.FindByPredicate([ItemId](const FGT_WarehouseEntry& E) { return E.ItemId == ItemId; });
}

const FGT_WarehouseEntry* UGT_MetaProgressSubsystem::FindWarehouse(FName ItemId) const
{
	return State.Warehouse.FindByPredicate([ItemId](const FGT_WarehouseEntry& E) { return E.ItemId == ItemId; });
}

void UGT_MetaProgressSubsystem::AddWarehouseItems(const TArray<FGT_RewardItem>& Items, FName Source)
{
	for (const FGT_RewardItem& Item : Items)
	{
		const int32 Count = ClampNonNegative(Item.Count);
		if (Item.ItemId.IsNone() || Count <= 0) { continue; }
		FGT_WarehouseEntry* Entry = FindWarehouse(Item.ItemId);
		if (!Entry)
		{
			Entry = &State.Warehouse.AddDefaulted_GetRef();
			Entry->ItemId = Item.ItemId;
			Entry->Value = Item.Value;
			Entry->Source = Source.IsNone() ? FName(TEXT("recovered")) : Source;
		}
		Entry->Count += Count;
	}
	// 调用方(RecordExtractionReward)负责 Save, 避免重复落盘。
}

int32 UGT_MetaProgressSubsystem::GetWarehouseItemCount(FName ItemId) const
{
	const FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	return Entry ? Entry->Count : 0;
}

bool UGT_MetaProgressSubsystem::CanSellItem(FName ItemId, FName& OutReason) const
{
	const FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	if (!Entry || Entry->Count <= 0) { OutReason = TEXT("not_owned"); return false; }
	if (Entry->bUnique) { OutReason = TEXT("unique"); return false; }
	if (Entry->Source != FName(TEXT("recovered"))) { OutReason = TEXT("not_sellable"); return false; }
	if (IsEquipped(ItemId)) { OutReason = TEXT("equipped"); return false; }
	if (Entry->Value <= 0) { OutReason = TEXT("no_value"); return false; }
	return true;
}

bool UGT_MetaProgressSubsystem::RemoveWarehouseItem(FName ItemId, int32 Count)
{
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	if (!Entry || Entry->Count < Count) { return false; }
	Entry->Count -= Count;
	if (Entry->Count <= 0)
	{
		State.Warehouse.RemoveAll([ItemId](const FGT_WarehouseEntry& E) { return E.ItemId == ItemId; });
	}
	return true;
}

bool UGT_MetaProgressSubsystem::SellWarehouseItem(FName ItemId, int32 Count, int32& OutGold, FName& OutError)
{
	Count = ClampNonNegative(Count);
	if (Count <= 0) { OutError = TEXT("invalid_count"); return false; }
	if (!CanSellItem(ItemId, OutError)) { return false; }
	const FGT_WarehouseEntry* Entry = FindWarehouse(ItemId);
	if (!Entry || Entry->Count < Count) { OutError = TEXT("not_enough"); return false; }
	OutGold = Entry->Value * Count;
	if (!RemoveWarehouseItem(ItemId, Count)) { OutError = TEXT("remove_failed"); return false; }
	State.Gold += OutGold;
	State.Stats.TotalGoldEarned += OutGold;
	if (!Save().bSuccess) { OutError = TEXT("save_write_failed"); OutGold = 0; return false; }
	return true;
}

void UGT_MetaProgressSubsystem::RecordRun()
{
	State.Stats.TotalRuns += 1;
	Save();
}

void UGT_MetaProgressSubsystem::RecordExtractionReward(const FGT_ExtractionReward& Reward)
{
	const int32 GoldAdded = ClampNonNegative(Reward.DirectGold) + ClampNonNegative(Reward.LoosePartsGold);
	int32 ItemCount = 0;
	int32 ItemValue = 0;
	for (const FGT_RewardItem& Item : Reward.CarriedItems)
	{
		const int32 C = ClampNonNegative(Item.Count);
		ItemCount += C;
		ItemValue += Item.Value * C;
	}

	State.Gold += GoldAdded;
	State.Stats.TotalGoldEarned += GoldAdded;
	State.Stats.TotalExtractions += 1;

	if (ItemCount > 0 || ItemValue > 0)
	{
		State.Recovery.TotalItems += ItemCount;
		State.Recovery.TotalValue += ItemValue;
		State.Recovery.TotalExtractionsWithItems += 1;
		// 最近带回(最新在前, 上限 RecentRecoveryMax)。
		for (const FGT_RewardItem& Item : Reward.CarriedItems)
		{
			for (int32 i = 0; i < ClampNonNegative(Item.Count); ++i)
			{
				State.Recovery.RecentItemIds.Insert(Item.ItemId, 0);
			}
		}
		if (State.Recovery.RecentItemIds.Num() > GT_MetaCatalog::GetRecentRecoveryMax())
		{
			State.Recovery.RecentItemIds.SetNum(GT_MetaCatalog::GetRecentRecoveryMax());
		}
		AddWarehouseItems(Reward.CarriedItems, FName(TEXT("recovered")));
	}
	Save();
}

// ============================================================================
// 局内加成查询
// ============================================================================

FGT_EquipBonus UGT_MetaProgressSubsystem::GetEquipBonus() const
{
	FGT_EquipBonus Bonus;
	for (const FName& ItemId : State.EquippedItems)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(ItemId);
		if (!Def) { continue; }
		Bonus.BonusHP += Def->BonusHP;
		Bonus.BonusPower += Def->BonusPower;
		Bonus.bMineImmunity |= Def->bMineImmunity;
		Bonus.MineDmgReduce += Def->MineDmgReduce;
		Bonus.bShowExitHint |= Def->bShowExitHint;
		Bonus.SearchBonus += Def->SearchBonus;
	}
	return Bonus;
}

FGT_TalentEffects UGT_MetaProgressSubsystem::GetTalentEffects() const
{
	FGT_TalentEffects Effects;
	for (const FName& TalentId : State.UnlockedTalents)
	{
		const FGT_TalentDef* Def = GT_MetaCatalog::FindTalent(TalentId);
		if (!Def) { continue; }
		Effects.MineDmgReduce += Def->MineDmgReduce;
		Effects.MonsterFleeBonus += Def->MonsterFleeBonus;
		Effects.FailureGoldBonus += Def->FailureGoldBonus;
		if (Def->TradePrice > 0) { Effects.TradePrice = Def->TradePrice; } // 议价 → 收购价加成 %
		Effects.bMapHighlight |= Def->bMapHighlight;
	}
	return Effects;
}

// ============================================================================
// GM 调试
// ============================================================================

#if !UE_BUILD_SHIPPING
void UGT_MetaProgressSubsystem::GMGrantItem(FName ItemId)
{
	State.OwnedItems.AddUnique(ItemId);
	Save();
}

void UGT_MetaProgressSubsystem::GMGrantTalent(FName TalentId)
{
	State.UnlockedTalents.AddUnique(TalentId);
	Save();
}

void UGT_MetaProgressSubsystem::GMEquipAll()
{
	State.EquippedItems.Reset();
	for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
	{
		if (State.OwnedItems.Contains(Def.Id))
		{
			State.EquippedItems.Add(Def.Id);
		}
	}
	Save();
}

void UGT_MetaProgressSubsystem::GMUnequipAll()
{
	State.EquippedItems.Reset();
	Save();
}

void UGT_MetaProgressSubsystem::GMReset()
{
	State = FGT_MetaProgressState();
	Save();
}
#endif
