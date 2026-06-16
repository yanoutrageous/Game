# Run 生命周期与物品系统集成指南

生成时间：2026-06-16  
阶段：第3阶段 - Run 生命周期集成

## 概述

本指南详细说明如何将物品系统与 Run 生命周期系统集成。包括：
- 背包初始化
- 物品掉落处理
- 效果事件触发
- 撤离成功逻辑
- 撤离失败逻辑
- 返回大厅处理

## 集成代码框架位置

**文件**：`GT_RunIntegrationFramework.h`  
**位置**：`Source/Graytail/ItemSystem/GT_RunIntegrationFramework.h`

包含 `GT_ItemSystemIntegration` 命名空间，提供了以下方法：
- `InitializeRunInventory()` - 初始化冒险背包
- `HandleLootPickup()` - 处理战利品拾取
- `HandleGoldPickup()` - 处理金币获得
- `BroadcastGameEvent()` - 广播游戏事件
- `HandleExtractionSuccess()` - 处理撤离成功
- `HandleRunFailed()` - 处理撤离失败
- `HandleReturnToHub()` - 处理返回大厅

## 集成步骤详解

### 第1步：在 RunSubsystem 中添加物品系统成员

**文件**：`Source/Graytail/Core/GT_RunSubsystem.h`

在类定义中添加：

```cpp
#include "ItemSystem/GraytailItemSystem.h"  // 添加头文件
#include "ItemSystem/GT_RunIntegrationFramework.h"  // 添加集成框架

class UGT_RunSubsystem : public UGameInstanceSubsystem
{
    // ... 其他成员 ...

private:
    // ===== 物品系统成员 =====
    
    /// 当前 Run 的玩家背包
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ItemSystem", meta = (AllowPrivateAccess = "true"))
    UInventory* RunInventory = nullptr;

    /// 当前 Run 的金币状态
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ItemSystem", meta = (AllowPrivateAccess = "true"))
    FGT_CurrencyState RunCurrency;

    /// 物品系统管理器（缓存）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ItemSystem", meta = (AllowPrivateAccess = "true"))
    UItemSystemManager* ItemSystemManager = nullptr;

    /// 玩家角色（缓存）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ItemSystem", meta = (AllowPrivateAccess = "true"))
    TWeakObjectPtr<APawn> PlayerPawn;

public:
    // 便利方法获取背包
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemSystem")
    UInventory* GetRunInventory() const { return RunInventory; }

    // 便利方法获取金币状态
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemSystem")
    FGT_CurrencyState GetRunCurrency() const { return RunCurrency; }
};
```

### 第2步：初始化物品系统

**文件**：`Source/Graytail/Core/GT_RunSubsystem.cpp`

在初始化函数中：

```cpp
#include "ItemSystem/GraytailItemSystem.h"
#include "ItemSystem/GT_RunIntegrationFramework.h"

void UGT_RunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 获取物品系统管理器
    ItemSystemManager = GetGameInstance()->GetSubsystem<UItemSystemManager>();
    if (!ItemSystemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[RunSubsystem] Failed to get ItemSystemManager"));
        return;
    }

    // 确保 ItemSystemManager 已初始化
    if (!ItemSystemManager->GetItemDataTable())
    {
        UE_LOG(LogTemp, Error, TEXT("[RunSubsystem] ItemDataTable not loaded in ItemSystemManager"));
    }

    UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] ItemSystem initialized"));
}
```

### 第3步：冒险开始 - 初始化背包

**调用时机**：当玩家开始一个新的冒险时

```cpp
void UGT_RunSubsystem::OnRunStart()
{
    if (!ItemSystemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[RunSubsystem] ItemSystemManager not available"));
        return;
    }

    // 获取玩家角色
    APawn* Player = GetPlayerPawn();  // 实现此方法返回当前玩家
    if (!Player)
    {
        UE_LOG(LogTemp, Error, TEXT("[RunSubsystem] Player not found"));
        return;
    }

    PlayerPawn = Player;

    // 初始化背包和金币状态
    GT_ItemSystemIntegration::InitializeRunInventory(
        ItemSystemManager,
        Player,
        RunInventory,
        RunCurrency,
        20.f  // 基础负重容量 20
    );

    // 从仓库装备保存的物品（可选）
    // 例如：如果玩家有之前保存的装备，从仓库取出并装备
    // ...

    UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Run started, inventory initialized"));
}
```

### 第4步：处理物品掉落

**调用时机**：当玩家进入房间或击败敌人时

#### 4.1 掉落战利品（例如在开启宝箱时）

```cpp
void UGT_RunSubsystem::OnTreasureChestOpened(int32 ChestItemID, int32 ItemCount)
{
    if (!ItemSystemManager || !RunInventory)
    {
        return;
    }

    // 处理战利品拾取
    bool bAdded = GT_ItemSystemIntegration::HandleLootPickup(
        ItemSystemManager,
        RunInventory,
        ChestItemID,
        ItemCount,
        TEXT("Treasure")  // 房间类型
    );

    if (!bAdded)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Item not fully added (inventory full)"));
    }
}
```

#### 4.2 掉落金币（例如击败敌人时）

```cpp
void UGT_RunSubsystem::OnMonsterDefeated(int32 MonsterGoldReward)
{
    // 处理金币获得
    GT_ItemSystemIntegration::HandleGoldPickup(RunCurrency, MonsterGoldReward);

    // 广播击败怪物事件（物品效果触发）
    GT_ItemSystemIntegration::BroadcastGameEvent(
        ItemSystemManager,
        EGT_ItemEffectTrigger::OnDefeatMonster,
        PlayerPawn.Get()
    );
}
```

### 第5步：处理游戏事件

**调用时机**：各类游戏事件发生时

#### 5.1 进入房间

```cpp
void UGT_RunSubsystem::OnEnterRoom(const FString& RoomType)
{
    GT_ItemSystemIntegration::BroadcastGameEvent(
        ItemSystemManager,
        EGT_ItemEffectTrigger::OnEnterRoom,
        PlayerPawn.Get(),
        FString::Printf(TEXT("RoomType=%s"), *RoomType)
    );
}
```

#### 5.2 玩家受伤（雷房检测）

```cpp
void UGT_RunSubsystem::OnPlayerTakeDamage(bool bIsMineRoom)
{
    FString Context = bIsMineRoom ? TEXT("IsMineRoom=true") : TEXT("");
    
    GT_ItemSystemIntegration::BroadcastGameEvent(
        ItemSystemManager,
        EGT_ItemEffectTrigger::OnTakeDamage,
        PlayerPawn.Get(),
        Context
    );
}
```

#### 5.3 移动一步

```cpp
void UGT_RunSubsystem::OnPlayerMoveStep()
{
    GT_ItemSystemIntegration::BroadcastGameEvent(
        ItemSystemManager,
        EGT_ItemEffectTrigger::OnMoveStep,
        PlayerPawn.Get()
    );
}
```

#### 5.4 协议等级变化

```cpp
void UGT_RunSubsystem::OnProtocolLevelChanged(bool bDecreased)
{
    FString Context = bDecreased ? TEXT("LevelDecreased") : TEXT("LevelIncreased");
    
    GT_ItemSystemIntegration::BroadcastGameEvent(
        ItemSystemManager,
        EGT_ItemEffectTrigger::OnProtocolLevelChanged,
        PlayerPawn.Get(),
        Context
    );
}
```

### 第6步：撤离成功

**调用时机**：当玩家成功到达撤离点时

```cpp
void UGT_RunSubsystem::OnExtractionSuccess()
{
    if (!ItemSystemManager || !RunInventory)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Extraction successful!"));

    // 处理撤离成功
    // 这包括：
    // 1. 触发 OnExtractionSuccess 事件
    // 2. 应用金币倍数（例如公司工牌）
    // 3. Pending -> Secured 金币转换
    // 4. 背包物品 -> 仓库转移
    // 5. 清空背包
    bool bSuccess = GT_ItemSystemIntegration::HandleExtractionSuccess(
        ItemSystemManager,
        RunInventory,
        RunCurrency,
        PlayerPawn.Get()
    );

    if (bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Items transferred to vault"));
        
        // 开始结算界面（可选）
        ShowExtractionResultsUI();
    }
}
```

### 第7步：撤离失败

**调用时机**：当玩家血量降为 0 时

```cpp
void UGT_RunSubsystem::OnRunFailed()
{
    if (!ItemSystemManager || !RunInventory)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Run failed..."));

    // 处理撤离失败
    // 这包括：
    // 1. 应用"可抢救负重"规则（40%）
    // 2. 按价值比例选择优先保留的物品
    // 3. 优先保留的物品 -> 仓库
    // 4. 其他物品丢失
    // 5. Pending 金币清零，Secured 保留
    bool bSuccess = GT_ItemSystemIntegration::HandleRunFailed(
        ItemSystemManager,
        RunInventory,
        RunCurrency,
        0.4f  // 可抢救 40% 的负重
    );

    if (bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Run failure handled, items rescued"));
        
        // 显示失败界面
        ShowRunFailedUI();
    }
}
```

### 第8步：返回大厅

**调用时机**：当玩家从结果界面返回大厅时

```cpp
void UGT_RunSubsystem::OnReturnToHub()
{
    // 处理返回大厅
    // 这包括：
    // 1. Secured 金币 -> Global 转换
    // 2. 清理 Run 相关数据
    int32 GlobalGold = GT_ItemSystemIntegration::HandleReturnToHub(RunCurrency);

    UE_LOG(LogTemp, Warning, TEXT("[RunSubsystem] Returned to hub, global gold: %d"), GlobalGold);

    // 清理背包（可选）
    if (RunInventory)
    {
        RunInventory->Clear();
        RunInventory->ConditionalBeginDestroy();
        RunInventory = nullptr;
    }

    // 更新 UI（全局金币显示等）
    // ...
}
```

## 调试与状态查询

### 打印背包状态

```cpp
void UGT_RunSubsystem::DebugPrintInventoryStatus()
{
    if (!RunInventory)
    {
        return;
    }

    FString Status = GT_ItemSystemIntegration::GetInventoryStatus(RunInventory);
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] %s"), *Status);
}
```

### 打印金币状态

```cpp
void UGT_RunSubsystem::DebugPrintCurrencyStatus()
{
    FString Status = GT_ItemSystemIntegration::GetCurrencyStatus(RunCurrency);
    UE_LOG(LogTemp, Warning, TEXT("[Currency] %s"), *Status);
}
```

## 完整集成检查清单

### 背包管理
- [ ] OnRunStart() 中调用 `InitializeRunInventory()`
- [ ] 背包创建成功，容量为 20
- [ ] 金币状态初始化为 0

### 物品掉落
- [ ] 宝箱开启时调用 `HandleLootPickup()`
- [ ] 敌人掉落调用 `HandleGoldPickup()`
- [ ] 背包满时的处理

### 事件系统
- [ ] OnEnterRoom 事件广播
- [ ] OnDefeatMonster 事件广播
- [ ] OnTakeDamage 事件广播（含 IsMineRoom 上下文）
- [ ] OnMoveStep 事件广播
- [ ] OnProtocolLevelChanged 事件广播

### 撤离成功
- [ ] OnExtractionSuccess() 事件触发
- [ ] 公司工牌倍数应用（1.15x）
- [ ] Pending -> Secured 转换
- [ ] 背包物品 -> 仓库
- [ ] 背包清空

### 撤离失败
- [ ] 应用可抢救负重规则（40%）
- [ ] 按价值比例选择物品
- [ ] Pending 清零，Secured 保留
- [ ] 背包清空

### 返回大厅
- [ ] Secured -> Global 转换
- [ ] 金币显示更新
- [ ] Run 数据清理

## 常见问题与解决方案

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| 背包为 null | ItemSystemManager 未初始化 | 检查物品系统管理器初始化 |
| 物品添加失败 | 背包容量已满 | 检查负重计算和容量管理 |
| 事件不触发 | 物品未装备 | 确保物品已添加并注册 |
| 金币转换错误 | 倍数计算错误 | 检查修饰符应用逻辑 |
| 仓库为空 | Vault 未初始化 | 检查 GlobalVault 初始化 |

## 代码集成示例模板

**最小实现模板**（复制粘贴即用）：

```cpp
// 在 GT_RunSubsystem.h 中添加

#include "ItemSystem/GraytailItemSystem.h"
#include "ItemSystem/GT_RunIntegrationFramework.h"

private:
    UInventory* RunInventory = nullptr;
    FGT_CurrencyState RunCurrency;
    UItemSystemManager* ItemSystemManager = nullptr;

// 在 GT_RunSubsystem.cpp 中添加

void UGT_RunSubsystem::OnRunStart()
{
    ItemSystemManager = GetGameInstance()->GetSubsystem<UItemSystemManager>();
    APawn* Player = GetPlayerPawn();
    GT_ItemSystemIntegration::InitializeRunInventory(
        ItemSystemManager, Player, RunInventory, RunCurrency, 20.f);
}

void UGT_RunSubsystem::OnTreasureChestOpened(int32 ItemID, int32 Count)
{
    GT_ItemSystemIntegration::HandleLootPickup(
        ItemSystemManager, RunInventory, ItemID, Count, TEXT("Treasure"));
}

void UGT_RunSubsystem::OnExtractionSuccess()
{
    GT_ItemSystemIntegration::HandleExtractionSuccess(
        ItemSystemManager, RunInventory, RunCurrency, GetPlayerPawn());
}

void UGT_RunSubsystem::OnRunFailed()
{
    GT_ItemSystemIntegration::HandleRunFailed(
        ItemSystemManager, RunInventory, RunCurrency, 0.4f);
}

void UGT_RunSubsystem::OnReturnToHub()
{
    GT_ItemSystemIntegration::HandleReturnToHub(RunCurrency);
}
```

## 下一步行动

- [ ] 将集成代码添加到 GT_RunSubsystem
- [ ] 编译验证无错误
- [ ] 在 PIE 中测试完整 Run 流程
- [ ] 验证金币转换逻辑
- [ ] 测试背包容量限制
- [ ] 验证可抢救负重规则

---

**生成时间**：2026-06-16  
**版本**：1.0  
**状态**：可直接使用
