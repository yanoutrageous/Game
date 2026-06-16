# 灰尾公司物品系统完整设计文档（含 12 件物品配置）

## 概述

本文档基于《灰尾公司：格外危险》游戏策划文档 v0.3 及物品信息表，定义了物品系统的完整 C++ 架构与 **12 件物品的精确数据**。所有物品效果均可通过 `FItemData` 数据表配置实现，特殊逻辑将在实现部分给出蓝图或 C++ 扩展指南。

### 核心特点

- **数据驱动**：所有物品静态属性通过 UE DataTable 定义（基于下方数据表直接填写）
- **多态设计**：`UItemBase` → `UPassiveArtifact` / `UConsumable` / `UEquipment`
- **事件驱动效果**：支持 12 种触发时机（`EItemEffectTrigger`），包含概率、冷却、一次性标记
- **接口化属性修改**：角色实现 `ICombatInterface` 等接口，物品修饰符直接调用
- **完整背包 & 仓库系统**：支持堆叠、负重、跨局存储

---

## 一、物品数据表（DataTable）完整内容

下表列出了 12 件物品的 `FItemData` 字段值，可直接填入 UE 的 DataTable 中（行结构为 `FItemData`）。

### 物品 1：异常体犬牙

| 字段 | 值 |
|------|-----|
| ItemID | 1 |
| ItemName | 异常体犬牙 |
| ItemType | PassiveArtifact |
| Rarity | Rare |
| Weight | 1.0 |
| Value | 80 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnDefeatMonster, Modifiers=[{ModifierType="Attack", Value=2.0, bStackable=true, MaxStack=5}] |
| Tags | ["DamageStack"] |

### 物品 2：受损扫描仪

| 字段 | 值 |
|------|-----|
| ItemID | 2 |
| ItemName | 受损扫描仪 |
| ItemType | PassiveArtifact |
| Rarity | Uncommon |
| Weight | 2.0 |
| Value | 60 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnEnterRoom, EffectDescription="扫描相邻4格（需自定义逻辑）" |
| 注 | 此效果无法仅靠修饰符实现，需在 OnEnterRoom 广播时由扫描系统额外处理，或物品蓝图重写 TriggerEffect。 |

### 物品 3：公司标准手套

| 字段 | 值 |
|------|-----|
| ItemID | 3 |
| ItemName | 公司标准手套 |
| ItemType | Equipment |
| Rarity | Common |
| Weight | 1.0 |
| Value | 30 |
| MaxStackSize | 1 |
| RequiredSlot | Tool |
| Effects | TriggerType=OnTakeDamage, TriggerCondition="IsMineRoom", Modifiers=[{ModifierType="DamageReduction", Value=0.5}], bOneTimeOnly=true |
| 注 | 需游戏逻辑在受伤时传入 Context="IsMineRoom"；一次性触发后应自动卸下并销毁物品。 |

### 物品 4：幸运硬币

| 字段 | 值 |
|------|-----|
| ItemID | 4 |
| ItemName | 幸运硬币 |
| ItemType | Consumable |
| Rarity | Uncommon |
| Weight | 0.0 |
| Value | 50 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnUse, TriggerProbability=0.5, Modifiers=[{ModifierType="Gold", Value=30}] （另一效果需自定义逻辑：50%概率随机探索相邻房间） |

### 物品 5：应急绷带

| 字段 | 值 |
|------|-----|
| ItemID | 5 |
| ItemName | 应急绷带 |
| ItemType | Consumable |
| Rarity | Common |
| Weight | 1.0 |
| Value | 20 |
| MaxStackSize | 3 |
| Effects | TriggerType=OnUse, Modifiers=[{ModifierType="CurrentHealth", Value=2.0}] |

### 物品 6：封锁区结晶

| 字段 | 值 |
|------|-----|
| ItemID | 6 |
| ItemName | 封锁区结晶 |
| ItemType | PassiveArtifact |
| Rarity | Epic |
| Weight | 3.0 |
| Value | 200 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnProtocolLevelChanged, TriggerCondition="LevelDecreased", Modifiers=[{ModifierType="CurrentHealth", Value=1.0, MaxStack=4}] |

### 物品 7：残缺地图

| 字段 | 值 |
|------|-----|
| ItemID | 7 |
| ItemName | 残缺地图 |
| ItemType | Consumable |
| Rarity | Uncommon |
| Weight | 1.0 |
| Value | 45 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnUse, EffectDescription="显示撤离方向（需UI配合）" |

### 物品 8：超载零件箱

| 字段 | 值 |
|------|-----|
| ItemID | 8 |
| ItemName | 超载零件箱 |
| ItemType | PassiveArtifact |
| Rarity | Rare |
| Weight | 4.0 |
| Value | 120 |
| MaxStackSize | 1 |
| Effects | TriggerType=Passive, Modifiers=[{ModifierType="WeightCapacity", Value=3.0}]TriggerType=OnMoveStep, TriggerCondition="ProtocolLevel<=3", Modifiers=[{ModifierType="ProtocolPressure", Value=1.0}] |

### 物品 9：灰尾应急灯

| 字段 | 值 |
|------|-----|
| ItemID | 9 |
| ItemName | 灰尾应急灯 |
| ItemType | Consumable |
| Rarity | Common |
| Weight | 1.0 |
| Value | 25 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnUse, EffectDescription="扫描周围9格（自定义逻辑）" |

### 物品 10：公司工牌

| 字段 | 值 |
|------|-----|
| ItemID | 10 |
| ItemName | 公司工牌 |
| ItemType | PassiveArtifact |
| Rarity | Epic |
| Weight | 0.0 |
| Value | 150 |
| MaxStackSize | 1 |
| Effects | TriggerType=Passive, Modifiers=[{ModifierType="PriceDiscount", Value=-0.2}]TriggerType=OnExtractionSuccess, Modifiers=[{ModifierType="GoldMultiplier", Value=0.15}] |

### 物品 11：回收磁石

| 字段 | 值 |
|------|-----|
| ItemID | 2 |
| ItemName | 受损扫描仪 |
| ItemType | PassiveArtifact |
| Rarity | Uncommon |
| Weight | 2.0 |
| Value | 60 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnEnterRoom, EffectDescription="扫描相邻4格（需自定义逻辑）" |
| 注 | 此效果无法仅靠修饰符实现，需在 OnEnterRoom 广播时由扫描系统额外处理，或物品蓝图重写 TriggerEffect。 |0

### 物品 12：协议镇定剂

| 字段 | 值 |
|------|-----|
| ItemID | 2 |
| ItemName | 受损扫描仪 |
| ItemType | PassiveArtifact |
| Rarity | Uncommon |
| Weight | 2.0 |
| Value | 60 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnEnterRoom, EffectDescription="扫描相邻4格（需自定义逻辑）" |
| 注 | 此效果无法仅靠修饰符实现，需在 OnEnterRoom 广播时由扫描系统额外处理，或物品蓝图重写 TriggerEffect。 |1

---

## 二、架构核心类（摘要）

完整头文件与实现请参考 `Source/Graytail/ItemSystem/GraytailItemSystem.h/.cpp`。以下仅列出与物品数据直接相关的核心成员。

### UItemBase（物品实例）

- `FItemData ItemData` – 从 DataTable 加载的静态数据
- `int32 StackSize` – 当前堆叠数
- `void Equip(APawn* Owner)` / `void Unequip()` / `void UseItem(APawn* User)`
- `void TriggerEffect(EItemEffectTrigger TriggerType, APawn* Target, const FString& Context)` – 核心效果触发入口

### UItemSystemManager（全局子系统）

- `void LoadItemDataTable(UDataTable* InDataTable)` – 加载物品表
- `UItemBase* CreateItem(int32 ItemID, APawn* Owner)` – 根据 ID 创建实例
- `void BroadcastTriggerEvent(EItemEffectTrigger Type, APawn* Source, const FString& Context)` – 游戏内事件发生时调用此函数，所有已装备物品会自动响应

---

## 三、游戏事件与物品触发的对应关系

为实现上述物品效果，您需要在游戏的相应模块中调用 `UItemSystemManager::BroadcastTriggerEvent`。

| 字段 | 值 |
|------|-----|
| ItemID | 2 |
| ItemName | 受损扫描仪 |
| ItemType | PassiveArtifact |
| Rarity | Uncommon |
| Weight | 2.0 |
| Value | 60 |
| MaxStackSize | 1 |
| Effects | TriggerType=OnEnterRoom, EffectDescription="扫描相邻4格（需自定义逻辑）" |
| 注 | 此效果无法仅靠修饰符实现，需在 OnEnterRoom 广播时由扫描系统额外处理，或物品蓝图重写 TriggerEffect。 |2

**示例代码（在角色受伤函数中）**：

```cpp
void AGraytailCharacter::TakeDamage(float Damage, bool bIsMineRoom)
{
    // 实际扣血逻辑...

    UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
    FString Context = bIsMineRoom ? TEXT("IsMineRoom=true") : TEXT("");
    ItemMgr->BroadcastTriggerEvent(EItemEffectTrigger::OnTakeDamage, this, Context);
}
```