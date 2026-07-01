#include "Domains/Meta/GT_MetaProgressSubsystem.h"

#include "Domains/Meta/GT_MetaCatalog.h"
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
	Load();
}

void UGT_MetaProgressSubsystem::Save()
{
	UGT_MetaSaveGame* SaveObj = Cast<UGT_MetaSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UGT_MetaSaveGame::StaticClass()));
	if (!SaveObj)
	{
		UE_LOG(LogGraytailMeta, Warning, TEXT("Save: failed to create save object."));
		return;
	}
	SaveObj->State = State;
	if (!UGameplayStatics::SaveGameToSlot(SaveObj, SaveSlotName(), SaveUserIndex))
	{
		UE_LOG(LogGraytailMeta, Warning, TEXT("Save: SaveGameToSlot failed."));
		return;
	}

#if !UE_BUILD_SHIPPING
	TSharedRef<FJsonObject> StateObject = MakeShared<FJsonObject>();
	if (FJsonObjectConverter::UStructToJsonObject(
		FGT_MetaProgressState::StaticStruct(),
		&SaveObj->State,
		StateObject,
		0,
		0))
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("saveVersion"), SaveObj->SaveVersion);
		RootObject->SetObjectField(TEXT("state"), StateObject);

		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
		if (FJsonSerializer::Serialize(RootObject, Writer))
		{
			const FString MirrorDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
			IFileManager::Get().MakeDirectory(*MirrorDirectory, true);
			const FString MirrorPath = FPaths::Combine(MirrorDirectory, TEXT("GraytailMeta.debug.json"));
			if (!FFileHelper::SaveStringToFile(
				Json,
				*MirrorPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOG(LogGraytailMeta, Warning, TEXT("Save: failed to write debug mirror %s."), *MirrorPath);
			}
		}
	}
#endif

	UE_LOG(LogGraytailMeta, Log, TEXT("Saved: gold=%d"), State.Gold);
}

void UGT_MetaProgressSubsystem::Load()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName(), SaveUserIndex))
	{
		State = FGT_MetaProgressState();   // 全新起始
		UE_LOG(LogGraytailMeta, Log, TEXT("Load: no save, starting fresh."));
		return;
	}
	UGT_MetaSaveGame* SaveObj = Cast<UGT_MetaSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName(), SaveUserIndex));
	if (!SaveObj)
	{
		State = FGT_MetaProgressState();
		UE_LOG(LogGraytailMeta, Warning, TEXT("Load: slot exists but load failed; starting fresh."));
		return;
	}
	State = SaveObj->State;
	SanitizeAfterLoad();
	UE_LOG(LogGraytailMeta, Log, TEXT("Loaded: gold=%d"), State.Gold);
}

void UGT_MetaProgressSubsystem::SanitizeAfterLoad()
{
	State.Gold = ClampNonNegative(State.Gold);
	if (!GT_GameData::IsReady())
	{
		return;
	}

	// 已装备项必须确实拥有(对齐 Lua 校验)。
	State.EquippedItems.RemoveAll([this](const FName& Id)
	{
		return !State.OwnedItems.Contains(Id) || !GT_MetaCatalog::FindEquip(Id);
	});
	// 上限保护。
	if (State.EquippedItems.Num() > GT_MetaCatalog::GetMaxEquipped())
	{
		State.EquippedItems.SetNum(GT_MetaCatalog::GetMaxEquipped());
	}
	// loadout 数量夹到库存且不超 maxCarry, 去掉未知/<=0。
	for (auto It = State.LoadoutConsumables.CreateIterator(); It; ++It)
	{
		const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(It.Key());
		const int32 Stock = State.ConsumableStock.FindRef(It.Key());
		int32 Count = FMath::Min(It.Value(), Stock);
		if (Def && Def->MaxCarry > 0)
		{
			Count = FMath::Min(Count, Def->MaxCarry);
		}
		if (!Def || Count <= 0)
		{
			It.RemoveCurrent();
		}
		else
		{
			It.Value() = Count;
		}
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
	Save();
	return true;
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
	Save();
	return true;
}

bool UGT_MetaProgressSubsystem::ToggleEquip(FName ItemId, FName& OutError)
{
	if (!State.OwnedItems.Contains(ItemId)) { OutError = TEXT("not_owned"); return false; }
	const int32 Index = State.EquippedItems.IndexOfByKey(ItemId);
	if (Index != INDEX_NONE)
	{
		State.EquippedItems.RemoveAt(Index);   // 已装备 -> 卸下
		Save();
		return true;
	}
	if (State.EquippedItems.Num() >= GT_MetaCatalog::GetMaxEquipped())
	{
		OutError = TEXT("max_equipped");
		return false;
	}
	State.EquippedItems.Add(ItemId);
	Save();
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
	Save();
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
	Save();
	return true;
}

bool UGT_MetaProgressSubsystem::AddConsumable(FName ItemId, int32 Count)
{
	if (!GT_MetaCatalog::FindConsumable(ItemId)) { return false; }
	Count = ClampNonNegative(Count);
	if (Count <= 0) { return false; }
	State.ConsumableStock.FindOrAdd(ItemId) += Count;
	Save();
	return true;
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
	Save();
	return true;
}

void UGT_MetaProgressSubsystem::SetLoadoutConsumable(FName ItemId, int32 Count)
{
	const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(ItemId);
	if (!Def) { return; }
	Count = ClampNonNegative(Count);
	const int32 Stock = State.ConsumableStock.FindRef(ItemId);
	Count = FMath::Min(Count, Stock);
	if (Def->MaxCarry > 0) { Count = FMath::Min(Count, Def->MaxCarry); }
	if (Count <= 0) { State.LoadoutConsumables.Remove(ItemId); }
	else { State.LoadoutConsumables.Add(ItemId, Count); }
	Save();
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
	Save();
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
