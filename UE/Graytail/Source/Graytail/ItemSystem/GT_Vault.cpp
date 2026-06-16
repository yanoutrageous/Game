#include "ItemSystem/GT_Vault.h"
#include "ItemSystem/GT_ItemBase.h"

UVault::UVault()
{
}

void UVault::Initialize()
{
	StoredItems.Empty();
	ItemCounts.Empty();
}

bool UVault::StoreItem(UItemBase* Item, int32 Count)
{
	if (!Item || Item->GetStackSize() <= 0)
	{
		return false;
	}

	int32 StoreCount = Count == -1 ? Item->GetStackSize() : FMath::Min(Count, Item->GetStackSize());
	if (StoreCount <= 0)
	{
		return false;
	}

	// 创建副本存储
	UItemBase* StoredItem = Item->Duplicate(StoreCount, this);
	if (!StoredItem)
	{
		return false;
	}

	StoredItems.Add(StoredItem);
	UpdateCountCache();
	BroadcastVaultChanged(StoredItem->GetItemID(), StoreCount);
	return true;
}

UItemBase* UVault::WithdrawItem(int32 ItemID, int32 Count)
{
	if (Count <= 0)
	{
		return nullptr;
	}

	// 查找对应的物品
	for (int32 i = 0; i < StoredItems.Num(); ++i)
	{
		UItemBase* Item = StoredItems[i];
		if (Item && Item->GetItemID() == ItemID)
		{
			int32 WithdrawCount = FMath::Min(Count, Item->GetStackSize());
			Item->RemoveStack(WithdrawCount);

			UItemBase* WithdrawnItem = Item->Duplicate(WithdrawCount);

			// 如果堆叠清空则移除
			if (Item->GetStackSize() <= 0)
			{
				StoredItems.RemoveAt(i);
			}

			UpdateCountCache();
			BroadcastVaultChanged(ItemID, -WithdrawCount);
			return WithdrawnItem;
		}
	}

	return nullptr;
}

void UVault::WithdrawItems(int32 ItemID, int32 Count, TArray<UItemBase*>& OutItems)
{
	OutItems.Empty();
	int32 RemainingCount = Count;

	while (RemainingCount > 0)
	{
		UItemBase* WithdrawnItem = WithdrawItem(ItemID, RemainingCount);
		if (!WithdrawnItem)
		{
			break;
		}

		OutItems.Add(WithdrawnItem);
		RemainingCount -= WithdrawnItem->GetStackSize();
	}
}

void UVault::DestroyVaultItem(int32 ItemID, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}

	int32 DestroyCount = Count;
	for (int32 i = StoredItems.Num() - 1; i >= 0 && DestroyCount > 0; --i)
	{
		UItemBase* Item = StoredItems[i];
		if (Item && Item->GetItemID() == ItemID)
		{
			int32 ActualDestroy = FMath::Min(DestroyCount, Item->GetStackSize());
			Item->RemoveStack(ActualDestroy);
			DestroyCount -= ActualDestroy;

			if (Item->GetStackSize() <= 0)
			{
				StoredItems.RemoveAt(i);
			}

			BroadcastVaultChanged(ItemID, -ActualDestroy);
		}
	}

	UpdateCountCache();
}

int32 UVault::GetStoredItemCount(int32 ItemID) const
{
	if (const int32* CountPtr = ItemCounts.Find(ItemID))
	{
		return *CountPtr;
	}
	return 0;
}

TArray<int32> UVault::GetAllStoredItemIDs() const
{
	TArray<int32> ItemIDs;
	ItemCounts.GenerateKeyArray(ItemIDs);
	return ItemIDs;
}

void UVault::SaveToArchive(FArchive& Ar)
{
	int32 ItemCount = StoredItems.Num();
	Ar << ItemCount;

	for (UItemBase* Item : StoredItems)
	{
		if (Item)
		{
			Item->SaveToArchive(Ar);
		}
	}

	int32 CountMapSize = ItemCounts.Num();
	Ar << CountMapSize;
	for (const auto& Pair : ItemCounts)
	{
		int32 ItemID = Pair.Key;
		int32 Count = Pair.Value;
		Ar << ItemID << Count;
	}
}

void UVault::LoadFromArchive(FArchive& Ar)
{
	Clear();

	int32 ItemCount;
	Ar >> ItemCount;
	for (int32 i = 0; i < ItemCount; ++i)
	{
		// 物品加载逻辑将由 ItemSystemManager 处理
	}

	int32 CountMapSize;
	Ar >> CountMapSize;
	for (int32 i = 0; i < CountMapSize; ++i)
	{
		int32 ItemID, Count;
		Ar >> ItemID >> Count;
		ItemCounts.Add(ItemID, Count);
	}
}

void UVault::Clear()
{
	for (UItemBase* Item : StoredItems)
	{
		if (Item)
		{
			Item->ConditionalBeginDestroy();
		}
	}
	StoredItems.Empty();
	ItemCounts.Empty();
}

void UVault::UpdateCountCache()
{
	ItemCounts.Empty();
	for (const UItemBase* Item : StoredItems)
	{
		if (Item)
		{
			ItemCounts.FindOrAdd(Item->GetItemID()) += Item->GetStackSize();
		}
	}
}

void UVault::BroadcastVaultChanged(int32 ItemID, int32 CountChange)
{
	if (OnVaultChanged.IsBound())
	{
		OnVaultChanged.Broadcast(ItemID, CountChange);
	}
}
