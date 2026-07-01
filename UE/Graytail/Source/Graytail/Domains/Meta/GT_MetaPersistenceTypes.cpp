#include "Domains/Meta/GT_MetaPersistenceTypes.h"

#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Meta/GT_MetaCatalog.h"
#include "Save/GT_MetaSaveGame.h"

namespace
{
	template <typename Predicate>
	void FilterUniqueNames(TArray<FName>& Values, Predicate Keep)
	{
		TSet<FName> Seen;
		Values.RemoveAll([&Seen, &Keep](FName Id)
		{
			if (Id.IsNone() || Seen.Contains(Id) || !Keep(Id))
			{
				return true;
			}
			Seen.Add(Id);
			return false;
		});
	}

	bool IsKnownWarehouseItem(FName ItemId)
	{
		return GT_ItemCatalog::FindItemDef(ItemId)
			|| GT_MetaCatalog::FindEquip(ItemId)
			|| GT_MetaCatalog::FindConsumable(ItemId);
	}
}

FGT_MetaOperationResult FGT_MetaOperationResult::Success()
{
	FGT_MetaOperationResult Result;
	Result.bSuccess = true;
	return Result;
}

FGT_MetaOperationResult FGT_MetaOperationResult::Failure(FName ErrorCode, const FText& Message)
{
	FGT_MetaOperationResult Result;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

bool GT_MigrateMetaSave(
	int32 SourceVersion,
	FGT_MetaProgressState& InOutState,
	FString& OutError)
{
	OutError.Reset();
	if (SourceVersion == 1)
	{
		InOutState.ActiveRun = FGT_ActiveRunEscrow();
		return true;
	}
	if (SourceVersion == UGT_MetaSaveGame::CurrentSaveVersion)
	{
		return true;
	}

	OutError = FString::Printf(
		TEXT("Unsupported save version %d; current version is %d."),
		SourceVersion,
		UGT_MetaSaveGame::CurrentSaveVersion);
	return false;
}

bool GT_ValidateAndSanitizeMetaState(
	FGT_MetaProgressState& InOutState,
	FString& OutError)
{
	OutError.Reset();

	InOutState.Gold = FMath::Max(0, InOutState.Gold);
	InOutState.Stats.TotalRuns = FMath::Max(0, InOutState.Stats.TotalRuns);
	InOutState.Stats.TotalExtractions = FMath::Max(0, InOutState.Stats.TotalExtractions);
	InOutState.Stats.TotalGoldEarned = FMath::Max(0, InOutState.Stats.TotalGoldEarned);
	InOutState.Recovery.TotalItems = FMath::Max(0, InOutState.Recovery.TotalItems);
	InOutState.Recovery.TotalValue = FMath::Max(0, InOutState.Recovery.TotalValue);
	InOutState.Recovery.TotalExtractionsWithItems = FMath::Max(
		0,
		InOutState.Recovery.TotalExtractionsWithItems);

	FilterUniqueNames(InOutState.OwnedItems, [](FName Id)
	{
		return true;
	});
	FilterUniqueNames(InOutState.EquippedItems, [&InOutState](FName Id)
	{
		return InOutState.OwnedItems.Contains(Id) && GT_MetaCatalog::FindEquip(Id);
	});
	if (InOutState.EquippedItems.Num() > GT_MetaCatalog::GetMaxEquipped())
	{
		InOutState.EquippedItems.SetNum(GT_MetaCatalog::GetMaxEquipped());
	}
	FilterUniqueNames(InOutState.UnlockedTalents, [](FName Id)
	{
		return true;
	});

	for (auto It = InOutState.ConsumableStock.CreateIterator(); It; ++It)
	{
		if (!GT_MetaCatalog::FindConsumable(It.Key()) || It.Value() <= 0)
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = InOutState.LoadoutConsumables.CreateIterator(); It; ++It)
	{
		const FGT_ConsumableDef* Definition = GT_MetaCatalog::FindConsumable(It.Key());
		const int32 Stock = InOutState.ConsumableStock.FindRef(It.Key());
		const int32 MaxCarry = Definition ? FMath::Max(0, Definition->MaxCarry) : 0;
		const int32 SanitizedCount = FMath::Min3(It.Value(), Stock, MaxCarry);
		if (!Definition || SanitizedCount <= 0)
		{
			It.RemoveCurrent();
		}
		else
		{
			It.Value() = SanitizedCount;
		}
	}

	TMap<FName, FGT_WarehouseEntry> WarehouseById;
	for (const FGT_WarehouseEntry& Entry : InOutState.Warehouse)
	{
		if (Entry.ItemId.IsNone() || Entry.Count <= 0 || !IsKnownWarehouseItem(Entry.ItemId))
		{
			continue;
		}
		FGT_WarehouseEntry& Merged = WarehouseById.FindOrAdd(Entry.ItemId);
		if (Merged.ItemId.IsNone())
		{
			Merged = Entry;
			Merged.Count = 0;
		}
		Merged.Count = FMath::Max(0, Merged.Count + Entry.Count);
		Merged.Value = FMath::Max(0, Entry.Value);
		Merged.bUnique |= Entry.bUnique;
	}
	InOutState.Warehouse.Reset();
	WarehouseById.GenerateValueArray(InOutState.Warehouse);
	InOutState.Warehouse.Sort([](const FGT_WarehouseEntry& A, const FGT_WarehouseEntry& B)
	{
		return A.ItemId.LexicalLess(B.ItemId);
	});

	FilterUniqueNames(InOutState.Recovery.RecentItemIds, [](FName Id)
	{
		return IsKnownWarehouseItem(Id);
	});
	if (InOutState.Recovery.RecentItemIds.Num() > GT_MetaCatalog::GetRecentRecoveryMax())
	{
		InOutState.Recovery.RecentItemIds.SetNum(GT_MetaCatalog::GetRecentRecoveryMax());
	}

	if (!InOutState.ActiveRun.bActive)
	{
		InOutState.ActiveRun = FGT_ActiveRunEscrow();
		return true;
	}
	if (!InOutState.ActiveRun.RunId.IsValid())
	{
		OutError = TEXT("Active run escrow has an invalid RunId.");
		return false;
	}
	const UEnum* DifficultyEnum = StaticEnum<EGT_Difficulty>();
	if (!DifficultyEnum
		|| !DifficultyEnum->IsValidEnumValue(static_cast<int64>(InOutState.ActiveRun.Difficulty)))
	{
		OutError = TEXT("Active run escrow has an invalid difficulty.");
		return false;
	}
	FilterUniqueNames(InOutState.ActiveRun.EquippedItemIds, [](FName Id)
	{
		return GT_MetaCatalog::FindEquip(Id) != nullptr;
	});
	for (auto It = InOutState.ActiveRun.ConsumedConsumables.CreateIterator(); It; ++It)
	{
		if (!GT_MetaCatalog::FindConsumable(It.Key()) || It.Value() <= 0)
		{
			OutError = FString::Printf(
				TEXT("Active run escrow has invalid consumed item '%s'."),
				*It.Key().ToString());
			return false;
		}
	}
	return true;
}
