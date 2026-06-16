#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GT_ItemTypes.h"
#include "GT_Vault.generated.h"

class UItemBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVaultChanged, int32, ItemID, int32, CountChange);

/**
 * 仓库系统
 * 局外物品永久存储
 * 与存档系统绑定，游戏重启保留数据
 */
UCLASS(BlueprintType, Blueprintable)
class GRAYTAIL_API UVault : public UObject
{
	GENERATED_BODY()

public:
	UVault();

	// ========== 初始化 ==========

	UFUNCTION(BlueprintCallable, Category = "Graytail|Vault")
	void Initialize();

	// ========== 物品操作 ==========

	/**
	 * 存入物品到仓库
	 * @param Item 要存入的物品
	 * @param Count 堆叠数（如果为-1则存入全部堆叠）
	 * @return 是否成功存入
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Vault")
	bool StoreItem(UItemBase* Item, int32 Count = -1);

	/**
	 * 从仓库取出物品
	 * @param ItemID 物品ID
	 * @param Count 要取出的数量
	 * @return 取出的物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Vault")
	UItemBase* WithdrawItem(int32 ItemID, int32 Count = 1);

	/**
	 * 批量取出物品
	 * @param ItemID 物品ID
	 * @param Count 要取出的数量
	 * @param OutItems 输出物品数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Vault")
	void WithdrawItems(int32 ItemID, int32 Count, TArray<UItemBase*>& OutItems);

	/**
	 * 销毁仓库中的物品
	 * @param ItemID 物品ID
	 * @param Count 销毁数量（-1为全部）
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Vault")
	void DestroyVaultItem(int32 ItemID, int32 Count = -1);

	// ========== 查询 ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Vault")
	int32 GetStoredItemCount(int32 ItemID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Vault")
	bool HasItem(int32 ItemID) const { return GetStoredItemCount(ItemID) > 0; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Vault")
	TArray<int32> GetAllStoredItemIDs() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Vault")
	int32 GetUniqueItemCount() const { return ItemCounts.Num(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Vault")
	TArray<UItemBase*> GetAllStoredItems() const { return StoredItems; }

	/**
	 * 列出仓库内容摘要
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Vault")
	void GetVaultSummary(TMap<int32, int32>& OutItemCounts) const { OutItemCounts = ItemCounts; }

	// ========== 事件 ==========

	UPROPERTY(BlueprintAssignable, Category = "Graytail|Vault")
	FOnVaultChanged OnVaultChanged;

	// ========== 序列化 ==========

	/**
	 * 将仓库内容保存到存档
	 */
	virtual void SaveToArchive(FArchive& Ar);

	/**
	 * 从存档加载仓库内容
	 */
	virtual void LoadFromArchive(FArchive& Ar);

	/**
	 * 清空仓库
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Vault")
	void Clear();

protected:
	// 仓库中的物品集合
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Vault")
	TArray<UItemBase*> StoredItems;

	// 物品ID -> 数量映射表（用于快速查询）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Vault")
	TMap<int32, int32> ItemCounts;

	/**
	 * 更新计数缓存
	 */
	void UpdateCountCache();

	/**
	 * 广播仓库改变事件
	 */
	void BroadcastVaultChanged(int32 ItemID, int32 CountChange);
};
