#pragma once

/**
 * Run 生命周期与物品系统集成框架
 * 
 * 这是一个参考实现框架，展示如何在 Run 系统中集成物品系统
 * 实现以下生命周期回调：
 * - OnRunStart()     - 冒险开始时初始化背包和金币
 * - OnLootPickup()   - 拾取战利品时添加到背包
 * - OnItemEffect()   - 物品效果触发事件
 * - OnExtractionSuccess() - 撤离成功时转移物品和金币
 * - OnRunFailed()    - 冒险失败时应用可抢救规则
 * - OnReturnToHub()  - 返回大厅时处理金币转换
 * 
 * 使用步骤：
 * 1. 在你的 RunSubsystem 中包含此头文件
 * 2. 在相应的生命周期方法中调用下方定义的方法
 * 3. 根据具体游戏逻辑调整参数和条件
 */

#include "CoreMinimal.h"
#include "ItemSystem/GraytailItemSystem.h"

// ============================================================================
// 第3阶段集成代码框架
// ============================================================================

/**
 * [第3阶段] Run 生命周期与物品系统集成的核心方法
 * 
 * 使用示例：在 UGT_RunSubsystem 或类似模块中集成以下代码
 */
namespace GT_ItemSystemIntegration
{
	/**
	 * 初始化冒险背包系统
	 * 调用时机：OnRunStart()
	 * 
	 * @param ItemMgr 物品系统管理器
	 * @param Player 玩家角色
	 * @param OutInventory 输出背包实例
	 * @param OutCurrency 输出金币状态
	 * @param BaseMaxWeight 基础负重容量
	 */
	inline void InitializeRunInventory(
		UItemSystemManager* ItemMgr,
		APawn* Player,
		UInventory*& OutInventory,
		FGT_CurrencyState& OutCurrency,
		float BaseMaxWeight = 20.f)
	{
		if (!ItemMgr)
		{
			UE_LOG(LogTemp, Error, TEXT("[ItemSystem] ItemMgr is null in InitializeRunInventory"));
			return;
		}

		// 创建冒险背包
		OutInventory = ItemMgr->CreateInventory(BaseMaxWeight, Player);
		if (!OutInventory)
		{
			UE_LOG(LogTemp, Error, TEXT("[ItemSystem] Failed to create inventory"));
			return;
		}

		// 初始化金币状态
		OutCurrency.Reset();
		// 可从仓库加载上一次保留的安全结算币
		// OutCurrency.SecuredGold = LoadedSecuredGold;

		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Run inventory initialized with capacity: %.1f"), BaseMaxWeight);
	}

	/**
	 * 处理战利品拾取
	 * 调用时机：OnLootPickup() 或类似事件
	 * 
	 * @param ItemMgr 物品系统管理器
	 * @param Inventory 玩家背包
	 * @param ItemID 掉落的物品ID
	 * @param ItemCount 物品数量
	 * @param RoomType 房间类型（如"Treasure"）
	 * @return 是否成功添加
	 */
	inline bool HandleLootPickup(
		UItemSystemManager* ItemMgr,
		UInventory* Inventory,
		int32 ItemID,
		int32 ItemCount,
		const FString& RoomType = TEXT(""))
	{
		if (!ItemMgr || !Inventory)
		{
			return false;
		}

		// 创建战利品物品
		TArray<UItemBase*> LootedItems;
		ItemMgr->CreateItemsByID(ItemID, ItemCount, LootedItems, Inventory);

		// 添加到背包
		bool bAllAdded = true;
		for (UItemBase* Item : LootedItems)
		{
			if (!Inventory->AddItem(Item, true))
			{
				bAllAdded = false;
				UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Failed to add item %d to inventory (capacity full)"), ItemID);
			}
		}

		// 触发 OnPickupLoot 事件
		FString Context = FString::Printf(TEXT("RoomType=%s"), *RoomType);
		ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnPickupLoot, Inventory->GetOwner(), Context);

		return bAllAdded;
	}

	/**
	 * 处理金币获得
	 * 调用时机：击败敌人、开启宝箱、打破物体等
	 * 
	 * @param Currency 金币状态
	 * @param GoldAmount 获得的金币数
	 */
	inline void HandleGoldPickup(FGT_CurrencyState& Currency, int32 GoldAmount)
	{
		Currency.AddPendingGold(GoldAmount);
		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Gold pickup: +%d (Pending: %d)"), GoldAmount, Currency.PendingGold);
	}

	/**
	 * 广播游戏事件到物品系统
	 * 调用时机：游戏内各类事件发生时
	 * 
	 * @param ItemMgr 物品系统管理器
	 * @param TriggerType 物品效果触发类型
	 * @param Player 玩家角色
	 * @param Context 事件上下文信息
	 */
	inline void BroadcastGameEvent(
		UItemSystemManager* ItemMgr,
		EGT_ItemEffectTrigger TriggerType,
		APawn* Player,
		const FString& Context = TEXT(""))
	{
		if (!ItemMgr || !Player)
		{
			return;
		}

		ItemMgr->BroadcastItemTriggerEvent(TriggerType, Player, Context);
	}

	/**
	 * 处理撤离成功
	 * 调用时机：OnExtractionSuccess()
	 * 
	 * @param ItemMgr 物品系统管理器
	 * @param Inventory 玩家背包
	 * @param Currency 金币状态
	 * @param Player 玩家角色
	 * @return 是否成功处理
	 */
	inline bool HandleExtractionSuccess(
		UItemSystemManager* ItemMgr,
		UInventory* Inventory,
		FGT_CurrencyState& Currency,
		APawn* Player)
	{
		if (!ItemMgr || !Inventory)
		{
			return false;
		}

		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Handling extraction success..."));

		// 1. 触发 OnExtractionSuccess 事件（用于"公司工牌"等效果）
		ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnExtractionSuccess, Player, TEXT(""));

		// 2. 计算金币倍数修饰
		// 检查是否装备公司工牌（+15%）
		float GoldMultiplier = 1.f;
		UItemBase* ToolEquipment = Inventory->GetEquippedItem(EGT_EquipmentSlot::Tool);
		if (ToolEquipment && ToolEquipment->GetItemID() == 10)  // 公司工牌 ID=10
		{
			GoldMultiplier = 1.15f;
			UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Company Badge detected, gold multiplier: 1.15x"));
		}

		// 3. 待结算币 -> 安全结算币（包含倍数）
		Currency.ConvertPendingToSecured(GoldMultiplier);
		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Converted pending to secured: %.0f (multiplier: %.2f)"),
			static_cast<float>(Currency.SecuredGold), GoldMultiplier);

		// 4. 背包所有物品 -> 仓库
		UVault* Vault = ItemMgr->GetGlobalVault();
		if (!Vault)
		{
			UE_LOG(LogTemp, Error, TEXT("[ItemSystem] Vault not found"));
			return false;
		}

		int32 ItemsTransferred = 0;
		for (UItemBase* Item : Inventory->GetAllItems())
		{
			if (Item && Vault->StoreItem(Item))
			{
				ItemsTransferred++;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Transferred %d items to vault"), ItemsTransferred);

		// 5. 清空背包
		Inventory->Clear();

		return true;
	}

	/**
	 * 处理冒险失败 - 应用可抢救负重规则
	 * 调用时机：OnRunFailed()
	 * 
	 * @param ItemMgr 物品系统管理器
	 * @param Inventory 玩家背包
	 * @param Currency 金币状态
	 * @param RescuableWeightPercent 可抢救的负重百分比（默认40%）
	 * @return 是否成功处理
	 */
	inline bool HandleRunFailed(
		UItemSystemManager* ItemMgr,
		UInventory* Inventory,
		FGT_CurrencyState& Currency,
		float RescuableWeightPercent = 0.4f)
	{
		if (!ItemMgr || !Inventory)
		{
			return false;
		}

		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Handling run failed..."));

		UVault* Vault = ItemMgr->GetGlobalVault();
		if (!Vault)
		{
			UE_LOG(LogTemp, Error, TEXT("[ItemSystem] Vault not found"));
			return false;
		}

		// 1. 计算可抢救的负重
		float MaxRescuableWeight = Inventory->GetMaxWeight() * RescuableWeightPercent;
		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Rescue capacity: %.1f / %.1f"), MaxRescuableWeight, Inventory->GetMaxWeight());

		// 2. 按价值比例选择优先保留的物品
		// 策略：排序物品，优先保留高价值物品
		TArray<UItemBase*> AllItems = Inventory->GetAllItems();
		
		// 按单位重量价值排序（价值 / 重量）
		AllItems.Sort([](const UItemBase& A, const UItemBase& B)
		{
			float ValuePerWeightA = A.GetWeight() > 0 ? static_cast<float>(A.GetValue()) / A.GetWeight() : 0;
			float ValuePerWeightB = B.GetWeight() > 0 ? static_cast<float>(B.GetValue()) / B.GetWeight() : 0;
			return ValuePerWeightA > ValuePerWeightB;
		});

		// 3. 选择可抢救的物品
		float CurrentRescueWeight = 0;
		TArray<UItemBase*> RescuedItems;
		for (UItemBase* Item : AllItems)
		{
			if (Item && CurrentRescueWeight + Item->GetTotalWeight() <= MaxRescuableWeight)
			{
				RescuedItems.Add(Item);
				CurrentRescueWeight += Item->GetTotalWeight();
			}
		}

		// 4. 物品进入仓库
		for (UItemBase* Item : RescuedItems)
		{
			Vault->StoreItem(Item);
		}

		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Rescued %d items (%.1f weight)"), RescuedItems.Num(), CurrentRescueWeight);

		// 5. 待结算币清零，安全结算币保留
		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Pending gold lost: %d, Secured gold kept: %d"), 
			Currency.PendingGold, Currency.SecuredGold);
		Currency.PendingGold = 0;

		// 6. 清空背包
		Inventory->Clear();

		return true;
	}

	/**
	 * 处理返回大厅
	 * 调用时机：OnReturnToHub()
	 * 
	 * @param Currency 金币状态
	 * @return 转换后的全局金币数
	 */
	inline int32 HandleReturnToHub(FGT_CurrencyState& Currency)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Returning to hub..."));

		// 安全结算币 -> 全局结算币
		Currency.ConvertSecuredToGlobal();

		UE_LOG(LogTemp, Warning, TEXT("[ItemSystem] Global gold: %d (Total earned this run: %d)"),
			Currency.GlobalGold, Currency.GoldEarnedThisRun);

		return Currency.GlobalGold;
	}

	/**
	 * 获取背包状态信息
	 * 用于 UI 显示或调试
	 */
	inline FString GetInventoryStatus(UInventory* Inventory)
	{
		if (!Inventory)
		{
			return TEXT("Invalid inventory");
		}

		return FString::Printf(
			TEXT("Items: %d | Weight: %.1f / %.1f | Value: %d"),
			Inventory->GetItemCount(),
			Inventory->GetCurrentWeight(),
			Inventory->GetMaxWeight(),
			Inventory->GetTotalValue()
		);
	}

	/**
	 * 获取金币状态信息
	 * 用于 UI 显示或调试
	 */
	inline FString GetCurrencyStatus(const FGT_CurrencyState& Currency)
	{
		return FString::Printf(
			TEXT("Pending: %d | Secured: %d | Global: %d (Total earned: %d)"),
			Currency.PendingGold,
			Currency.SecuredGold,
			Currency.GlobalGold,
			Currency.GoldEarnedThisRun
		);
	}

} // namespace GT_ItemSystemIntegration

// ============================================================================
// 集成代码使用示例
// ============================================================================

/*

示例1：在 RunSubsystem 中的使用

class UGT_RunSubsystem : public UGameInstanceSubsystem
{
private:
    UPROPERTY(VisibleAnywhere, Category = "ItemSystem")
    UInventory* CurrentInventory = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "ItemSystem")
    FGT_CurrencyState CurrentCurrency;

public:
    virtual void OnRunStart()
    {
        UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
        APawn* Player = GetPlayerPawn();
        
        GT_ItemSystemIntegration::InitializeRunInventory(
            ItemMgr, Player, CurrentInventory, CurrentCurrency, 20.f
        );
    }

    virtual void OnLootPickup(int32 ItemID, int32 Count, const FString& RoomType)
    {
        UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
        GT_ItemSystemIntegration::HandleLootPickup(
            ItemMgr, CurrentInventory, ItemID, Count, RoomType
        );
    }

    virtual void OnDefeatMonster()
    {
        UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
        GT_ItemSystemIntegration::BroadcastGameEvent(
            ItemMgr, EGT_ItemEffectTrigger::OnDefeatMonster, GetPlayerPawn()
        );
    }

    virtual void OnExtractionSuccess()
    {
        UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
        GT_ItemSystemIntegration::HandleExtractionSuccess(
            ItemMgr, CurrentInventory, CurrentCurrency, GetPlayerPawn()
        );
    }

    virtual void OnRunFailed()
    {
        UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
        GT_ItemSystemIntegration::HandleRunFailed(
            ItemMgr, CurrentInventory, CurrentCurrency, 0.4f
        );
    }

    virtual void OnReturnToHub()
    {
        GT_ItemSystemIntegration::HandleReturnToHub(CurrentCurrency);
    }
};

示例2：打印状态信息

void DebugPrintStatus()
{
    FString InventoryStatus = GT_ItemSystemIntegration::GetInventoryStatus(CurrentInventory);
    FString CurrencyStatus = GT_ItemSystemIntegration::GetCurrencyStatus(CurrentCurrency);
    
    UE_LOG(LogTemp, Warning, TEXT("Inventory: %s"), *InventoryStatus);
    UE_LOG(LogTemp, Warning, TEXT("Currency: %s"), *CurrencyStatus);
}

*/
