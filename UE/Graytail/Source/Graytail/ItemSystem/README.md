# 灰尾物品系统实现文档

## 概述

本目录下的代码实现了完整的灰尾物品系统，包括：
- 物品对象模型（12件物品的完整配置）
- 背包系统（局内物品管理）
- 仓库系统（局外物品存储）
- 金币系统（三层金币管理）
- 物品系统管理器（全局事件与物品创建）

## 文件结构

```
ItemSystem/
├── GT_ItemTypes.h              # 所有enum和数据结构定义
├── GT_ItemBase.h/cpp           # 物品基类
├── GT_PassiveArtifact.h/cpp    # 被动工艺品（8件）
├── GT_Consumable.h/cpp         # 消耗品（5件）
├── GT_Equipment.h/cpp          # 装备（1件）
├── GT_Inventory.h/cpp          # 背包系统
├── GT_Vault.h/cpp              # 仓库系统
├── GT_ItemSystemManager.h/cpp  # 全局管理器
├── GraytailItemSystem.h         # 总头文件
└── README.md                   # 本文件
```

## 12件物品配置对照表

| ItemID | 名称 | 类型 | 稀有度 | 负重 | 价值 | 堆叠 |
|--------|------|------|--------|------|------|------|
| 1 | 异常体犬牙 | PassiveArtifact | Rare | 1.0 | 80 | 1 |
| 2 | 受损扫描仪 | PassiveArtifact | Uncommon | 2.0 | 60 | 1 |
| 3 | 公司标准手套 | Equipment | Common | 1.0 | 30 | 1 |
| 4 | 幸运硬币 | Consumable | Uncommon | 0.0 | 50 | 1 |
| 5 | 应急绷带 | Consumable | Common | 1.0 | 20 | 3 |
| 6 | 封锁区结晶 | PassiveArtifact | Epic | 3.0 | 200 | 1 |
| 7 | 残缺地图 | Consumable | Uncommon | 1.0 | 45 | 1 |
| 8 | 超载零件箱 | PassiveArtifact | Rare | 4.0 | 120 | 1 |
| 9 | 灰尾应急灯 | Consumable | Common | 1.0 | 25 | 1 |
| 10 | 公司工牌 | PassiveArtifact | Epic | 0.0 | 150 | 1 |
| 11 | 回收磁石 | PassiveArtifact | Rare | 1.0 | 90 | 1 |
| 12 | 协议镇定剂 | Consumable | Rare | 1.0 | 100 | 1 |

## 核心类设计

### 1. FGT_ItemData（数据表行）
- **ItemID**：物品唯一标识（1-12）
- **ItemName**：物品名称
- **ItemType**：物品类型（PassiveArtifact/Consumable/Equipment）
- **Rarity**：稀有度
- **Weight**：负重（占用背包空间）
- **Value**：价值（出售获得的金币）
- **MaxStackSize**：最大堆叠数
- **Effects**：效果数组（支持多个触发条件和修饰符）

### 2. UItemBase（物品基类）
所有物品的根基，提供：
- 属性访问（GetItemName、GetWeight等）
- 堆叠管理（AddStack、RemoveStack）
- 装备/使用生命周期（OnEquip、OnUse）
- 效果触发系统（TriggerEffect、ApplyModifier）
- 序列化支持（SaveToArchive、LoadFromArchive）

### 3. UPassiveArtifact（被动工艺品）
包括：异常体犬牙、受损扫描仪、封锁区结晶、超载零件箱、公司工牌、回收磁石

特点：
- 装备后自动激活被动效果
- 无需主动使用
- 卸下时停用效果

### 4. UConsumable（消耗品）
包括：幸运硬币、应急绷带、残缺地图、灰尾应急灯、协议镇定剂

特点：
- 主动使用后消耗
- 消耗时自动减少堆叠
- 堆叠为0时自动销毁

### 5. UEquipment（装备）
包括：公司标准手套

特点：
- 装备槽位管理（Tool/Accessory）
- 装备时激活效果
- 一次性效果支持

### 6. UInventory（背包）
- 管理局内物品
- 负重计算与检查
- 物品添加/移除
- 堆叠合并
- 使用/出售/装备操作
- 事件广播

### 7. UVault（仓库）
- 永久存储局外物品
- 与存档系统绑定
- 计数缓存优化查询
- 存取物品管理

### 8. FGT_CurrencyState（金币状态）
三层金币架构：
- **PendingGold**：待结算币（本局临时，冒险中获得）
- **SecuredGold**：安全结算币（出售物品时锁定，失败保留）
- **GlobalGold**：全局结算币（局外消费）

### 9. UItemSystemManager（全局管理器）
GameInstanceSubsystem，职责：
- 加载物品DataTable
- 创建物品实例（工厂方法）
- 创建背包实例
- 全局仓库管理
- 物品触发事件广播
- 物品信息查询

## 使用指南

### 初始化

```cpp
// 在游戏启动时
UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();

// 加载物品数据表
UDataTable* ItemTable = LoadObject<UDataTable>(nullptr, TEXT("DataTable'/Game/Data/DT_Items.DT_Items'"));
ItemMgr->LoadItemDataTable(ItemTable);

// 初始化全局仓库
ItemMgr->InitializeGlobalVault();
```

### 创建物品

```cpp
// 方法1：根据 ItemID 创建
UItemBase* Bandage = ItemMgr->CreateItemByID(5);

// 方法2：根据名称创建
UItemBase* Coin = ItemMgr->CreateItemByName(TEXT("幸运硬币"));

// 方法3：便利方法
UItemBase* Badge = ItemMgr->CreateCompanyBadge();

// 方法4：批量创建
TArray<UItemBase*> Bandages;
ItemMgr->CreateItemsByID(5, 3, Bandages);  // 创建3个应急绷带
```

### 背包操作

```cpp
// 创建背包
UInventory* Inventory = ItemMgr->CreateInventory(20.f, PlayerCharacter);

// 添加物品
UItemBase* Item = ItemMgr->CreateItemByID(5);
bool bAdded = Inventory->AddItem(Item);

// 查询
int32 Weight = Inventory->GetCurrentWeight();
int32 Capacity = Inventory->GetAvailableCapacity();

// 使用消耗品
Inventory->UseConsumableAt(0);

// 出售物品
int32 GoldGained = Inventory->SellItemAt(0);

// 装备物品
Inventory->EquipItemAt(0, EGT_EquipmentSlot::Tool);
```

### 仓库操作

```cpp
UVault* Vault = ItemMgr->GetGlobalVault();

// 存入物品
UItemBase* Item = ItemMgr->CreateItemByID(5);
Vault->StoreItem(Item, 3);

// 取出物品
UItemBase* Withdrawn = Vault->WithdrawItem(5, 1);

// 查询
int32 BandageCount = Vault->GetStoredItemCount(5);
```

### 物品效果事件

```cpp
// 在角色进入房间时
UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnEnterRoom, PlayerCharacter, TEXT("RoomType=Treasure"));

// 在击败怪物时
ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnDefeatMonster, PlayerCharacter, TEXT(""));

// 在玩家受伤时
bool bIsMineRoom = true;
FString Context = bIsMineRoom ? TEXT("IsMineRoom=true") : TEXT("");
ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnTakeDamage, PlayerCharacter, Context);
```

## Run生命周期集成

### 1. 冒险开始（OnRunStart）
```cpp
// 初始化背包
UInventory* RunInventory = ItemMgr->CreateInventory(20.f, PlayerCharacter);

// 初始化金币状态
FGT_CurrencyState CurrencyState;
CurrencyState.Reset();

// 从仓库装备保存的物品
// ...
```

### 2. 物品掉落（OnLootPickup）
```cpp
UItemBase* LootedItem = ItemMgr->CreateItemByID(SomeItemID);
RunInventory->AddItem(LootedItem);

// 触发 OnPickupLoot 事件
ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnPickupLoot, PlayerCharacter, TEXT("RoomType=Treasure"));
```

### 3. 撤离成功（OnExtractionSuccess）
```cpp
// 1. 待结算币 -> 安全结算币（包含倍数修饰）
float GoldMultiplier = 1.f;
// 检查是否装备公司工牌 (+15%)
if (UItemBase* Badge = RunInventory->GetEquippedItem(EGT_EquipmentSlot::Tool))
{
    if (Badge->GetItemID() == 10)  // 公司工牌
    {
        GoldMultiplier = 1.15f;
    }
}
CurrencyState.ConvertPendingToSecured(GoldMultiplier);

// 2. 触发 OnExtractionSuccess 事件
ItemMgr->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnExtractionSuccess, PlayerCharacter, TEXT(""));

// 3. 背包所有物品 -> 仓库
for (UItemBase* Item : RunInventory->GetAllItems())
{
    ItemMgr->GetGlobalVault()->StoreItem(Item);
}
RunInventory->Clear();
```

### 4. 撤离失败（OnRunFailed）
```cpp
// 1. 应用"可抢救负重"规则
float RescuableWeight = RunInventory->GetMaxWeight() * 0.4f;  // 假设40%可抢救
TArray<UItemBase*> ItemsToRescue;
// ... 按价值比例选择优先保留的物品 ...

// 2. 进入仓库的物品
for (UItemBase* Item : ItemsToRescue)
{
    ItemMgr->GetGlobalVault()->StoreItem(Item);
}

// 3. 待结算币清零，安全结算币保留
CurrencyState.PendingGold = 0;
```

### 5. 返回HUB（OnReturnToHUB）
```cpp
// 安全结算币 -> 全局结算币
CurrencyState.ConvertSecuredToGlobal();

// 此时玩家可以在商店使用 GlobalGold 购买物品
```

## 特殊物品逻辑

### 受损扫描仪（ID=2）
需要自定义扫描逻辑，在 OnEnterRoom 时：
1. 扫描相邻4格
2. 返回地雷/敌人位置
3. 可通过蓝图或C++子类扩展

### 幸运硬币（ID=4）
使用时50%概率：
- 获得30金币，或
- 显示随机相邻房间

需要在 ExecuteEffect_Implementation 中实现

### 超载零件箱（ID=8）
两个效果：
1. Passive：增加3点负重容量
2. OnMoveStep（当 ProtocolLevel <= 3）：每步增加1点压力

### 回收磁石（ID=11）
OnPickupLoot 且 RoomType=Treasure 时：
- 额外获得一件物品

需要在游戏逻辑中根据条件触发

## 编译与验证

确保在 Graytail.Build.cs 中包含了所有必要的模块：
```cpp
PublicDependencyModuleNames.AddRange(new string[] { 
    "Core", 
    "CoreUObject", 
    "Engine",
    "UMG"  // 如果使用UI
});
```

编译命令：
```
Engine\Build\BatchFiles\Build.bat GraytailEditor Win64 Development -Project=<path>\Graytail.uproject -WaitMutex
```

## 数据表配置（蓝图中）

1. 创建 DataTable：
   - 在内容浏览器中右键 → 数据表
   - 选择 FGT_ItemData 作为行结构
   - 命名为 DT_Items

2. 添加行：
   - 每个 ItemID 对应一行
   - 填入所有字段值
   - 对应 12 件物品的配置

3. 设置效果：
   - 在 Effects 数组中添加 FGT_ItemEffect
   - 设置触发类型、条件、修饰符
   - 支持多个效果

## 扩展点

### 自定义物品类
```cpp
UCLASS()
class GRAYTAIL_API UMyCustomArtifact : public UPassiveArtifact
{
    GENERATED_BODY()
    
    virtual void ApplyModifierInternal(const FGT_Modifier& Modifier, APawn* Target) override;
    // ... 实现特殊逻辑 ...
};
```

### 自定义效果条件
重写 ShouldTriggerEffect 以添加自定义条件判断

### 事件监听
```cpp
Inventory->OnInventoryChanged.AddDynamic(this, &UMyClass::OnInventoryChanged);
Vault->OnVaultChanged.AddDynamic(this, &UMyClass::OnVaultChanged);
```

## 已知限制与未来改进

1. **修饰符应用**：当前框架支持定义，具体应用逻辑需通过接口或事件系统扩展
2. **特殊物品逻辑**：如扫描、随机探索等需要游戏逻辑配合实现
3. **数据表行名**：建议使用 ItemID 作为行名，便于查找
4. **序列化完善**：完整的存档/加载需要集成 SaveGame 系统

## 调试命令（PIE内）

```
gt.ItemDebug ListAll        # 列出所有物品定义
gt.ItemDebug CreateItem 5   # 创建ID为5的物品到背包
gt.ItemDebug ListInventory  # 列出当前背包内容
```

## 参考文档

- ItemSystemArchitecture.md：完整的设计文档
- CLAUDE.md：项目整体架构和构建说明
