#include "ItemSystem/GT_ItemSystemManager.h"
#include "ItemSystem/GT_ItemBase.h"
#include "ItemSystem/GT_PassiveArtifact.h"
#include "ItemSystem/GT_Consumable.h"
#include "ItemSystem/GT_Equipment.h"
#include "ItemSystem/GT_Inventory.h"
#include "ItemSystem/GT_Vault.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"

void UItemSystemManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeGlobalVault();
}

void UItemSystemManager::Deinitialize()
{
	DestroyGlobalVault();
	Super::Deinitialize();
}

void UItemSystemManager::LoadItemDataTable(UDataTable* InDataTable)
{
	ItemDataTable = InDataTable;

	if (ItemDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Loaded DataTable with %d rows"), ItemDataTable->GetRowMap().Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemSystem] Failed to load ItemDataTable"));
	}
}

UItemBase* UItemSystemManager::CreateItemByID(int32 ItemID, UObject* OuterObject)
{
	FGT_ItemData ItemData;
	if (!TryGetItemData(ItemID, ItemData))
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemSystem] Item with ID %d not found"), ItemID);
		return nullptr;
	}

	TSubclassOf<UItemBase> ItemClass = GetItemClassForType(ItemData.ItemType);
	if (!ItemClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemSystem] No class found for item type %d"), static_cast<int32>(ItemData.ItemType));
		return nullptr;
	}

	UObject* ItemOuter = OuterObject ? OuterObject : GetOuter();
	UItemBase* NewItem = NewObject<UItemBase>(ItemOuter, ItemClass);

	if (NewItem)
	{
		NewItem->InitializeFromData(ItemData);
		return NewItem;
	}

	return nullptr;
}

UItemBase* UItemSystemManager::CreateItemByName(const FString& ItemName, UObject* OuterObject)
{
	if (!ItemDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemSystem] ItemDataTable not loaded"));
		return nullptr;
	}

	for (auto& Row : ItemDataTable->GetRowMap())
	{
		FGT_ItemData* ItemData = reinterpret_cast<FGT_ItemData*>(Row.Value);
		if (ItemData && ItemData->ItemName == ItemName)
		{
			return CreateItemByID(ItemData->ItemID, OuterObject);
		}
	}

	UE_LOG(LogTemp, Error, TEXT("[ItemSystem] Item with name '%s' not found"), *ItemName);
	return nullptr;
}

void UItemSystemManager::CreateItemsByID(int32 ItemID, int32 Count, TArray<UItemBase*>& OutItems, UObject* OuterObject)
{
	OutItems.Empty();

	if (Count <= 0)
	{
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		UItemBase* NewItem = CreateItemByID(ItemID, OuterObject);
		if (NewItem)
		{
			OutItems.Add(NewItem);
		}
	}
}

UInventory* UItemSystemManager::CreateInventory(float MaxWeight, APawn* Owner)
{
	UInventory* NewInventory = NewObject<UInventory>();
	if (NewInventory)
	{
		NewInventory->Initialize(MaxWeight, Owner);
		return NewInventory;
	}
	return nullptr;
}

void UItemSystemManager::DestroyInventory(UInventory* Inventory)
{
	if (Inventory)
	{
		Inventory->ConditionalBeginDestroy();
	}
}

void UItemSystemManager::InitializeGlobalVault()
{
	if (!GlobalVault)
	{
		GlobalVault = NewObject<UVault>();
		if (GlobalVault)
		{
			GlobalVault->Initialize();
			UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Global Vault initialized"));
		}
	}
}

void UItemSystemManager::DestroyGlobalVault()
{
	if (GlobalVault)
	{
		GlobalVault->ConditionalBeginDestroy();
		GlobalVault = nullptr;
	}
}

FGT_ItemData UItemSystemManager::GetItemData(int32 ItemID) const
{
	FGT_ItemData OutData;
	TryGetItemData(ItemID, OutData);
	return OutData;
}

void UItemSystemManager::GetAllItemData(TArray<FGT_ItemData>& OutItems) const
{
	OutItems.Empty();

	if (!ItemDataTable)
	{
		return;
	}

	for (auto& Row : ItemDataTable->GetRowMap())
	{
		FGT_ItemData* ItemData = reinterpret_cast<FGT_ItemData*>(Row.Value);
		if (ItemData)
		{
			OutItems.Add(*ItemData);
		}
	}
}

bool UItemSystemManager::DoesItemExist(int32 ItemID) const
{
	FGT_ItemData DummyData;
	return TryGetItemData(ItemID, DummyData);
}

void UItemSystemManager::BroadcastItemTriggerEvent(EGT_ItemEffectTrigger TriggerType, APawn* Source, const FString& Context)
{
	if (!Source)
	{
		return;
	}

	// 广播给所有注册的装备物品
	for (UItemBase* Item : RegisteredEquippedItems)
	{
		if (Item && Item->IsEquipped())
		{
			Item->TriggerEffect(TriggerType, Source, Context);
		}
	}
}

void UItemSystemManager::RegisterEquippedItem(UItemBase* Item)
{
	if (Item && !RegisteredEquippedItems.Contains(Item))
	{
		RegisteredEquippedItems.Add(Item);
	}
}

void UItemSystemManager::UnregisterEquippedItem(UItemBase* Item)
{
	RegisteredEquippedItems.Remove(Item);
}

TSubclassOf<UItemBase> UItemSystemManager::GetItemClassForType(EGT_ItemType ItemType) const
{
	switch (ItemType)
	{
	case EGT_ItemType::PassiveArtifact:
		return UPassiveArtifact::StaticClass();
	case EGT_ItemType::Consumable:
		return UConsumable::StaticClass();
	case EGT_ItemType::Equipment:
		return UEquipment::StaticClass();
	default:
		return UItemBase::StaticClass();
	}
}

bool UItemSystemManager::TryGetItemData(int32 ItemID, FGT_ItemData& OutData) const
{
	if (!ItemDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemSystem] ItemDataTable not loaded"));
		return false;
	}

	// 根据 ItemID 查找行（ItemID 作为行的 RowName）
	FString RowName = FString::Printf(TEXT("%d"), ItemID);
	FGT_ItemData* ItemData = ItemDataTable->FindRow<FGT_ItemData>(*RowName, TEXT("TryGetItemData"));

	if (ItemData)
	{
		OutData = *ItemData;
		return true;
	}

	// 备选方案：遍历所有行查找匹配的 ItemID 字段
	for (auto& Row : ItemDataTable->GetRowMap())
	{
		FGT_ItemData* Data = reinterpret_cast<FGT_ItemData*>(Row.Value);
		if (Data && Data->ItemID == ItemID)
		{
			OutData = *Data;
			return true;
		}
	}

	return false;
}
