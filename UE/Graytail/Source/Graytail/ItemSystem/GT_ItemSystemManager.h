#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GT_ItemTypes.h"
#include "GT_ItemSystemManager.generated.h"

class UItemBase;
class UInventory;
class UVault;
class APawn;
class UDataTable;

/**
 * 物品系统全局管理器
 * 作为 GameInstanceSubsystem 运行
 * 职责：
 * - 加载 12 件物品的 DataTable
 * - 创建物品实例
 * - 管理物品系统事件（如触发器）
 * - 协调 Inventory 和 Vault
 */
UCLASS()
class GRAYTAIL_API UItemSystemManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========== 初始化与加载 ==========

	/**
	 * 加载物品数据表
	 * @param DataTable 包含 FGT_ItemData 行的 DataTable
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void LoadItemDataTable(UDataTable* InDataTable);

	/**
	 * 获取物品数据表
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|ItemSystem")
	UDataTable* GetItemDataTable() const { return ItemDataTable; }

	// ========== 物品创建 ==========

	/**
	 * 根据 ItemID 创建物品实例
	 * @param ItemID 物品ID（1-12）
	 * @param OuterObject 物品的 Outer（通常为背包或仓库）
	 * @return 创建的物品实例，如果失败返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	UItemBase* CreateItemByID(int32 ItemID, UObject* OuterObject = nullptr);

	/**
	 * 根据物品名称创建物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	UItemBase* CreateItemByName(const FString& ItemName, UObject* OuterObject = nullptr);

	/**
	 * 批量创建物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void CreateItemsByID(int32 ItemID, int32 Count, TArray<UItemBase*>& OutItems, UObject* OuterObject = nullptr);

	// ========== 背包管理 ==========

	/**
	 * 创建背包实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	UInventory* CreateInventory(float MaxWeight, APawn* Owner);

	/**
	 * 销毁背包
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void DestroyInventory(UInventory* Inventory);

	// ========== 仓库管理 ==========

	/**
	 * 获取全局仓库实例
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|ItemSystem")
	UVault* GetGlobalVault() const { return GlobalVault; }

	/**
	 * 创建全局仓库（通常在游戏初始化时调用一次）
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void InitializeGlobalVault();

	/**
	 * 销毁全局仓库
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void DestroyGlobalVault();

	// ========== 物品信息查询 ==========

	/**
	 * 根据 ItemID 获取物品数据
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|ItemSystem")
	FGT_ItemData GetItemData(int32 ItemID) const;

	/**
	 * 获取所有物品数据
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|ItemSystem")
	void GetAllItemData(TArray<FGT_ItemData>& OutItems) const;

	/**
	 * 检查物品是否存在
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|ItemSystem")
	bool DoesItemExist(int32 ItemID) const;

	// ========== 物品效果系统 ==========

	/**
	 * 广播物品触发事件
	 * 所有已装备的物品会自动响应
	 * @param TriggerType 触发类型
	 * @param Source 触发源（通常为玩家角色）
	 * @param Context 上下文信息
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void BroadcastItemTriggerEvent(EGT_ItemEffectTrigger TriggerType, APawn* Source, const FString& Context);

	/**
	 * 注册装备的物品，以便接收事件
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void RegisterEquippedItem(UItemBase* Item);

	/**
	 * 取消注册装备的物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem")
	void UnregisterEquippedItem(UItemBase* Item);

	// ========== 12件物品便利方法 ==========

	/**
	 * 创建"异常体犬牙"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Artifacts")
	UItemBase* CreateAnomalousFang(UObject* OuterObject = nullptr) { return CreateItemByID(1, OuterObject); }

	/**
	 * 创建"受损扫描仪"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Artifacts")
	UItemBase* CreateDamagedScanner(UObject* OuterObject = nullptr) { return CreateItemByID(2, OuterObject); }

	/**
	 * 创建"公司标准手套"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Equipment")
	UItemBase* CreateStandardGloves(UObject* OuterObject = nullptr) { return CreateItemByID(3, OuterObject); }

	/**
	 * 创建"幸运硬币"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Consumables")
	UItemBase* CreateLuckyCoin(UObject* OuterObject = nullptr) { return CreateItemByID(4, OuterObject); }

	/**
	 * 创建"应急绷带"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Consumables")
	UItemBase* CreateEmergencyBandage(UObject* OuterObject = nullptr) { return CreateItemByID(5, OuterObject); }

	/**
	 * 创建"封锁区结晶"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Artifacts")
	UItemBase* CreateLockedZoneCrystal(UObject* OuterObject = nullptr) { return CreateItemByID(6, OuterObject); }

	/**
	 * 创建"残缺地图"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Consumables")
	UItemBase* CreateIncompleteMap(UObject* OuterObject = nullptr) { return CreateItemByID(7, OuterObject); }

	/**
	 * 创建"超载零件箱"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Artifacts")
	UItemBase* CreateOverloadedPartBox(UObject* OuterObject = nullptr) { return CreateItemByID(8, OuterObject); }

	/**
	 * 创建"灰尾应急灯"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Consumables")
	UItemBase* CreateEmergencyLight(UObject* OuterObject = nullptr) { return CreateItemByID(9, OuterObject); }

	/**
	 * 创建"公司工牌"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Artifacts")
	UItemBase* CreateCompanyBadge(UObject* OuterObject = nullptr) { return CreateItemByID(10, OuterObject); }

	/**
	 * 创建"回收磁石"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Artifacts")
	UItemBase* CreateRecoveryMagnet(UObject* OuterObject = nullptr) { return CreateItemByID(11, OuterObject); }

	/**
	 * 创建"协议镇定剂"
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|ItemSystem|Consumables")
	UItemBase* CreateProtocolSedative(UObject* OuterObject = nullptr) { return CreateItemByID(12, OuterObject); }

protected:
	// 物品数据表
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|ItemSystem")
	UDataTable* ItemDataTable = nullptr;

	// 全局仓库实例（持久化）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|ItemSystem")
	UVault* GlobalVault = nullptr;

	// 已注册的装备物品（用于事件广播）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|ItemSystem")
	TArray<UItemBase*> RegisteredEquippedItems;

	/**
	 * 获取物品对应的 C++ 类
	 */
	TSubclassOf<UItemBase> GetItemClassForType(EGT_ItemType ItemType) const;

	/**
	 * 从 DataTable 加载 FGT_ItemData
	 */
	bool TryGetItemData(int32 ItemID, FGT_ItemData& OutData) const;
};
