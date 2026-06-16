#include "ItemSystem/GT_Inventory.h"
#include "ItemSystem/GT_ItemBase.h"
#include "ItemSystem/GT_Consumable.h"
#include "ItemSystem/GT_Equipment.h"
#include "GameFramework/Pawn.h"

UInventory::UInventory()
	: MaxWeight(20.f)
{
}

void UInventory::BeginDestroy()
{
	Clear();
	Super::BeginDestroy();
}

void UInventory::Initialize(float InMaxWeight, APawn* InOwner)
{
	MaxWeight = InMaxWeight;
	OwnerPawn = InOwner;
	Items.Empty();
	EquippedItems.Empty();
}

bool UInventory::AddItem(UItemBase* Item, bool bMergeStack)
{
	if (!Item)
	{
		return false;
	}

	// 检查是否可以合并堆叠
	if (bMergeStack && Item->CanStack())
	{
		if (TryMergeStack(Item))
		{
			BroadcastInventoryChanged(Item, 1);
			return true;
		}
	}

	// 检查负重
	float ItemWeight = Item->GetTotalWeight();
	if (GetCurrentWeight() + ItemWeight > MaxWeight)
	{
		return false;
	}

	// 添加到背包
	Items.Add(Item);
	Item->Rename(nullptr, this);
	BroadcastInventoryChanged(Item, 1);
	return true;
}

bool UInventory::RemoveItem(UItemBase* Item)
{
	if (!Item)
	{
		return false;
	}

	int32 Index = Items.Find(Item);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Items.RemoveAt(Index);
	BroadcastInventoryChanged(Item, -1);
	return true;
}

UItemBase* UInventory::RemoveItemAt(int32 Index)
{
	if (!Items.IsValidIndex(Index))
	{
		return nullptr;
	}

	UItemBase* Item = Items[Index];
	Items.RemoveAt(Index);
	BroadcastInventoryChanged(Item, -1);
	return Item;
}

bool UInventory::UseConsumableAt(int32 Index)
{
	if (!Items.IsValidIndex(Index))
	{
		return false;
	}

	UConsumable* Consumable = Cast<UConsumable>(Items[Index]);
	if (!Consumable || !OwnerPawn.IsValid())
	{
		return false;
	}

	Consumable->OnUse(OwnerPawn.Get());

	// 如果堆叠清空则移除
	if (Consumable->GetStackSize() <= 0)
	{
		RemoveItemAt(Index);
	}

	return true;
}

int32 UInventory::SellItemAt(int32 Index)
{
	if (!Items.IsValidIndex(Index))
	{
		return 0;
	}

	UItemBase* Item = Items[Index];
	int32 SaleValue = Item->GetValue() * Item->GetStackSize();

	RemoveItemAt(Index);
	return SaleValue;
}

bool UInventory::EquipItemAt(int32 Index, EGT_EquipmentSlot Slot)
{
	if (!Items.IsValidIndex(Index) || !OwnerPawn.IsValid())
	{
		return false;
	}

	UEquipment* Equipment = Cast<UEquipment>(Items[Index]);
	if (!Equipment || Equipment->GetRequiredSlot() != Slot)
	{
		return false;
	}

	// 卸下已装备的物品
	if (EquippedItems.Contains(Slot))
	{
		UnequipSlot(Slot);
	}

	// 装备新物品
	Equipment->OnEquip(OwnerPawn.Get());
	EquippedItems[Slot] = Equipment;
	return true;
}

bool UInventory::UnequipSlot(EGT_EquipmentSlot Slot)
{
	if (!EquippedItems.Contains(Slot))
	{
		return false;
	}

	UItemBase* Equipment = EquippedItems[Slot];
	if (Equipment)
	{
		Equipment->OnUnequip();
	}

	EquippedItems.Remove(Slot);
	return true;
}

float UInventory::GetCurrentWeight() const
{
	float TotalWeight = 0.f;
	for (const UItemBase* Item : Items)
	{
		if (Item)
		{
			TotalWeight += Item->GetTotalWeight();
		}
	}
	return TotalWeight;
}

float UInventory::GetAvailableCapacity() const
{
	return FMath::Max(0.f, MaxWeight - GetCurrentWeight());
}

bool UInventory::IsFull() const
{
	return GetAvailableCapacity() <= 0.f;
}

int32 UInventory::GetTotalValue() const
{
	int32 TotalValue = 0;
	for (const UItemBase* Item : Items)
	{
		if (Item)
		{
			TotalValue += Item->GetValue() * Item->GetStackSize();
		}
	}
	return TotalValue;
}

UItemBase* UInventory::GetItemAt(int32 Index) const
{
	if (Items.IsValidIndex(Index))
	{
		return Items[Index];
	}
	return nullptr;
}

int32 UInventory::FindItemIndex(UItemBase* Item) const
{
	return Items.Find(Item);
}

UItemBase* UInventory::FindItemByID(int32 ItemID) const
{
	for (UItemBase* Item : Items)
	{
		if (Item && Item->GetItemID() == ItemID)
		{
			return Item;
		}
	}
	return nullptr;
}

int32 UInventory::CountItemByID(int32 ItemID) const
{
	int32 Count = 0;
	for (const UItemBase* Item : Items)
	{
		if (Item && Item->GetItemID() == ItemID)
		{
			Count += Item->GetStackSize();
		}
	}
	return Count;
}

UItemBase* UInventory::GetEquippedItem(EGT_EquipmentSlot Slot) const
{
	if (EquippedItems.Contains(Slot))
	{
		return EquippedItems[Slot];
	}
	return nullptr;
}

void UInventory::SaveToArchive(FArchive& Ar)
{
	Ar << MaxWeight;
	int32 ItemCount = Items.Num();
	Ar << ItemCount;

	for (UItemBase* Item : Items)
	{
		if (Item)
		{
			Item->SaveToArchive(Ar);
		}
	}

	int32 EquippedCount = EquippedItems.Num();
	Ar << EquippedCount;
	for (const auto& Pair : EquippedItems)
	{
		uint8 SlotValue = static_cast<uint8>(Pair.Key);
		Ar << SlotValue;
		if (Pair.Value)
		{
			Pair.Value->SaveToArchive(Ar);
		}
	}
}

void UInventory::LoadFromArchive(FArchive& Ar)
{
	Clear();
	Ar << MaxWeight;

	int32 ItemCount;
	Ar >> ItemCount;
	for (int32 i = 0; i < ItemCount; ++i)
	{
		// 物品加载逻辑将由 ItemSystemManager 处理
		// 这里仅加载结构
	}

	int32 EquippedCount;
	Ar >> EquippedCount;
	for (int32 i = 0; i < EquippedCount; ++i)
	{
		uint8 SlotValue;
		Ar << SlotValue;
		// 装备加载逻辑
	}
}

void UInventory::Clear()
{
	for (UItemBase* Item : Items)
	{
		if (Item)
		{
			Item->ConditionalBeginDestroy();
		}
	}
	Items.Empty();
	EquippedItems.Empty();
}

bool UInventory::TryMergeStack(UItemBase* Item)
{
	if (!Item->CanStack())
	{
		return false;
	}

	for (UItemBase* ExistingItem : Items)
	{
		if (ExistingItem && 
		    ExistingItem->GetItemID() == Item->GetItemID() &&
		    ExistingItem->GetStackSize() < ExistingItem->GetMaxStackSize())
		{
			int32 CanAdd = ExistingItem->GetMaxStackSize() - ExistingItem->GetStackSize();
			int32 ToAdd = FMath::Min(CanAdd, Item->GetStackSize());
			ExistingItem->AddStack(ToAdd);
			Item->RemoveStack(ToAdd);

			if (Item->GetStackSize() <= 0)
			{
				return true;
			}
		}
	}

	return false;
}

void UInventory::BroadcastInventoryChanged(UItemBase* Item, int32 ChangeType)
{
	if (OnInventoryChanged.IsBound())
	{
		OnInventoryChanged.Broadcast(Item, ChangeType);
	}
}
