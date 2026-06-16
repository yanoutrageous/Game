#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GT_ItemTypes.h"
#include "GT_Inventory.generated.h"

class UItemBase;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryChanged, UItemBase*, Item, int32, ChangeType);

/**
 * 背包系统
 * 管理当前 Run 中玩家携带的物品
 * 占用负重的物品：PassiveArtifact、Equipment、Consumable
 * 不占负重的：金币（通过 CurrencyState 管理）
 */
UCLASS(BlueprintType, Blueprintable)
class GRAYTAIL_API UInventory : public UObject
{
	GENERATED_BODY()

public:
	UInventory();

	virtual void BeginDestroy() override;

	// ========== 初始化 ==========

	/**
	 * 初始化背包
	 * @param InMaxWeight 最大负重
	 * @param InOwner 背包拥有者
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	void Initialize(float InMaxWeight, APawn* InOwner);

	// ========== 物品操作 ==========

	/**
	 * 添加物品到背包
	 * @param Item 要添加的物品
	 * @param bMergeStack 是否尝试合并堆叠
	 * @return 是否成功添加
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	bool AddItem(UItemBase* Item, bool bMergeStack = true);

	/**
	 * 从背包移除物品
	 * @param Item 要移除的物品
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	bool RemoveItem(UItemBase* Item);

	/**
	 * 从背包移除指定索引的物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	UItemBase* RemoveItemAt(int32 Index);

	/**
	 * 使用消耗品
	 * @param Index 背包中的索引
	 * @return 是否成功使用
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	bool UseConsumableAt(int32 Index);

	/**
	 * 出售物品
	 * @param Index 背包中的索引
	 * @return 获得的金币数
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	int32 SellItemAt(int32 Index);

	/**
	 * 装备物品
	 * @param Index 背包中的索引
	 * @param Slot 装备槽位
	 * @return 是否成功装备
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	bool EquipItemAt(int32 Index, EGT_EquipmentSlot Slot);

	/**
	 * 卸下装备
	 * @param Slot 装备槽位
	 * @return 是否成功卸下
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	bool UnequipSlot(EGT_EquipmentSlot Slot);

	// ========== 查询 ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	int32 GetItemCount() const { return Items.Num(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	float GetMaxWeight() const { return MaxWeight; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	float GetAvailableCapacity() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	bool IsFull() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	int32 GetTotalValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	UItemBase* GetItemAt(int32 Index) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	int32 FindItemIndex(UItemBase* Item) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	UItemBase* FindItemByID(int32 ItemID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	int32 CountItemByID(int32 ItemID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	TArray<UItemBase*> GetAllItems() const { return Items; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Inventory")
	UItemBase* GetEquippedItem(EGT_EquipmentSlot Slot) const;

	// ========== 事件 ==========

	UPROPERTY(BlueprintAssignable, Category = "Graytail|Inventory")
	FOnInventoryChanged OnInventoryChanged;

	// ========== 序列化 ==========

	/**
	 * 将背包内容保存到存档
	 */
	virtual void SaveToArchive(FArchive& Ar);

	/**
	 * 从存档加载背包内容
	 */
	virtual void LoadFromArchive(FArchive& Ar);

	/**
	 * 清空背包
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Inventory")
	void Clear();

protected:
	// 物品集合
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	TArray<UItemBase*> Items;

	// 装备槽位（Tool和Accessory）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	TMap<EGT_EquipmentSlot, UItemBase*> EquippedItems;

	// 最大负重
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	float MaxWeight = 20.f;

	// 背包拥有者
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	TWeakObjectPtr<APawn> OwnerPawn;

	/**
	 * 尝试合并到已有堆叠
	 * @return 是否成功合并
	 */
	bool TryMergeStack(UItemBase* Item);

	/**
	 * 广播背包改变事件
	 */
	void BroadcastInventoryChanged(UItemBase* Item, int32 ChangeType);
};
