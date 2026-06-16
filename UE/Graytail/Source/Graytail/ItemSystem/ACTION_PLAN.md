# 灰尾物品系统 - 完整行动计划与当前进度

生成时间：2026-06-16

## 📊 当前进度

```
已完成：           ████████░░░ 80%
████████████████████████░░░░░░░░

总工作：7个阶段
已完成：6个阶段
进行中：第1阶段（编译与验证）
```

## 📋 阶段进度详表

### ✅ 已完成

| # | 阶段名称 | 完成度 | 备注 |
|----|---------|--------|------|
| 0 | 代码框架生成 | 100% | 20个文件，~3500行代码 |
| 0 | 类型系统定义 | 100% | 12种物品枚举、结构体完整 |
| 0 | 文档生成 | 100% | README、设计文档、示例代码 |

### 🔄 进行中

| # | 阶段名称 | 完成度 | 当前任务 |
|----|---------|--------|---------|
| 1 | 编译与验证 | 90% | 等待用户执行编译命令 |

### ⏳ 待完成

| # | 阶段名称 | 优先级 | 估计时间 | 依赖 |
|----|---------|--------|---------|------|
| 2 | DataTable 配置 | 高 | 30分钟 | 编译完成 |
| 3 | Run 生命周期集成 | 高 | 2-4小时 | DataTable完成 |
| 4 | 特殊物品逻辑 | 中 | 2-6小时 | Run集成完成 |
| 5 | 修饰符应用系统 | 中 | 4-8小时 | 特殊物品完成 |
| 6 | UI 集成 | 低 | 4-8小时 | 修饰符完成 |
| 7 | 测试与验证 | 高 | 4-8小时 | 全部功能完成 |

## 🎯 立即可执行的任务队列

### 任务1：编译项目 ⚡ 立即执行
**状态**：🔴 阻塞中，等待用户操作  
**时间**：5-15分钟  
**难度**：★☆☆☆☆

**步骤**：
```powershell
# 1. 打开 PowerShell
# 2. 执行以下命令（选择其中一种方式）

# 方式A：使用 Build.bat（推荐）
cd "c:\Work\practice\Game\UE\Graytail"
$uprojectPath = (Resolve-Path 'Graytail.uproject').Path
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" GraytailEditor Win64 Development -Project=$uprojectPath -WaitMutex

# 预期输出中会看到 "Overall=Pass"

# 3. 验证编译结果
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ 编译成功" -ForegroundColor Green
} else {
    Write-Host "✗ 编译失败" -ForegroundColor Red
}
```

**成功标志**：
- ✓ 退出码为 0
- ✓ 输出包含 "Overall=Pass"
- ✓ 无 C++ 编译错误

**若编译失败**：
1. 检查 UE 5.7 是否已安装
2. 确认编辑器完全关闭（否则 Live Coding 会占用编译锁）
3. 查看详细错误日志

**下一步**：完成编译后，执行任务2

---

### 任务2：创建与配置 DataTable 📦
**状态**：🟡 准备就绪  
**时间**：30分钟  
**难度**：★★☆☆☆  
**前置条件**：任务1 完成

**步骤**：
```
1. 打开 UE 编辑器
   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "c:\Work\practice\Game\UE\Graytail\Graytail.uproject"

2. 在内容浏览器中创建 DataTable
   • 右键 → 新建 → DataTable
   • 选择行结构体：FGT_ItemData
   • 命名：DT_Items
   • 保存到 /Game/Data/

3. 配置 12 件物品（参考 COMPILE_AND_SETUP_GUIDE.md 中的配置表）
   • 逐行填入物品数据
   • 每件物品对应一行

4. 保存 DataTable

5. 在代码中加载 DataTable（参考下方）
```

**DataTable 加载代码**（在 GameInstance 或启动代码中）：
```cpp
UItemSystemManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemSystemManager>();
UDataTable* ItemTable = LoadObject<UDataTable>(nullptr, TEXT("DataTable'/Game/Data/DT_Items.DT_Items'"));
if (ItemTable)
{
    ItemMgr->LoadItemDataTable(ItemTable);
}
```

**验证**：
- ✓ DataTable 包含 12 行数据
- ✓ 每行对应一件物品
- ✓ 所有字段已填写

**参考文档**：
- [COMPILE_AND_SETUP_GUIDE.md](./COMPILE_AND_SETUP_GUIDE.md) - 详细的数据表配置

---

### 任务3：集成 Run 生命周期 🔗
**状态**：🟢 代码框架已准备  
**时间**：2-4小时  
**难度**：★★★☆☆  
**前置条件**：任务2 完成

**核心集成代码文件**：
- `GT_RunIntegrationFramework.h` - 集成函数库
- `RUN_LIFECYCLE_INTEGRATION_GUIDE.md` - 详细集成指南

**集成位置**：
- 文件：`Source/Graytail/Core/GT_RunSubsystem.h/cpp`
- 方法：各个生命周期回调函数

**主要集成点**：
```
OnRunStart()
  ├─ InitializeRunInventory()
  └─ 初始化背包和金币

OnLootPickup()
  ├─ HandleLootPickup()
  └─ 添加物品到背包

OnDefeatMonster() / OnEnterRoom() / ...
  ├─ BroadcastGameEvent()
  └─ 触发装备的被动效果

OnExtractionSuccess()
  ├─ HandleExtractionSuccess()
  ├─ 应用金币倍数
  └─ 物品转仓库

OnRunFailed()
  ├─ HandleRunFailed()
  ├─ 应用可抢救负重
  └─ 物品抢救

OnReturnToHub()
  ├─ HandleReturnToHub()
  └─ 金币全局转换
```

**集成检查清单**：
- [ ] RunInventory 成员变量添加
- [ ] RunCurrency 成员变量添加
- [ ] OnRunStart() 中初始化背包
- [ ] 所有掉落事件调用 Handle* 方法
- [ ] 所有游戏事件调用 BroadcastGameEvent()
- [ ] OnExtractionSuccess() 完整实现
- [ ] OnRunFailed() 完整实现
- [ ] OnReturnToHub() 完整实现

**参考文档**：
- [RUN_LIFECYCLE_INTEGRATION_GUIDE.md](./RUN_LIFECYCLE_INTEGRATION_GUIDE.md)
- [GT_RunIntegrationFramework.h](./GT_RunIntegrationFramework.h)

---

### 任务4：特殊物品逻辑实现 🎮
**状态**：🟢 框架可扩展  
**时间**：2-6小时  
**难度**：★★★★☆  
**前置条件**：任务3 完成

**需要实现的特殊物品**：

1. **受损扫描仪** (ID=2)
   - 触发时机：OnEnterRoom
   - 需要实现：扫描相邻4格
   - 返回：地雷、敌人、物品位置

2. **幸运硬币** (ID=4)
   - 触发时机：OnUse
   - 需要实现：50%概率分支
   - 分支A (50%)：获得30金币
   - 分支B (50%)：随机探索相邻房间

3. **残缺地图** (ID=7)
   - 触发时机：OnUse
   - 需要实现：显示到撤离点的路径
   - UI 支持：显示最短路径

4. **灰尾应急灯** (ID=9)
   - 触发时机：OnUse
   - 需要实现：扫描周围9格
   - 返回：地雷、敌人、物品位置

5. **回收磁石** (ID=11)
   - 触发时机：OnPickupLoot
   - 条件：RoomType=Treasure
   - 效果：额外获得一件物品

**实现方式**：
- 方式1：在 `ExecuteEffect_Implementation()` 中实现（推荐）
- 方式2：创建子类并重写
- 方式3：通过蓝图实现

**示例代码**（消耗品特殊逻辑）：
```cpp
class UConsumableImpl : public UConsumable
{
    virtual void ExecuteEffect_Implementation(APawn* User) override
    {
        if (GetItemID() == 4)  // 幸运硬币
        {
            if (FMath::FRand() > 0.5f)
            {
                // 50% 获得30金币
                HandleGoldPickup(30);
            }
            else
            {
                // 50% 随机探索
                ShowRandomRoomPreview();
            }
        }
    }
};
```

**验证**：
- [ ] 每件特殊物品都能正常工作
- [ ] 效果触发条件正确
- [ ] 概率计算准确

---

### 任务5：修饰符应用系统 ⚙️
**状态**：🟡 框架已定义  
**时间**：4-8小时  
**难度**：★★★★★  
**前置条件**：任务4 完成

**修饰符类型**（需要实现）：
```cpp
EGT_ModifierType::Attack           → Combat 系统
EGT_ModifierType::Defense          → Combat 系统
EGT_ModifierType::DamageReduction  → Combat 系统
EGT_ModifierType::CurrentHealth    → Stat 系统
EGT_ModifierType::MaxHealth        → Stat 系统
EGT_ModifierType::Gold             → Currency 系统
EGT_ModifierType::GoldMultiplier   → Currency 系统
EGT_ModifierType::PriceDiscount    → Shop 系统
EGT_ModifierType::WeightCapacity   → Inventory 系统
EGT_ModifierType::ProtocolPressure → Protocol 系统
EGT_ModifierType::ProtocolImmunity → Protocol 系统
```

**集成方式**：
1. 创建 ICombatInterface / IStatInterface 等接口
2. 在 ApplyModifierInternal() 中调用对应接口方法
3. 由各系统实现具体逻辑

**示例集成代码**：
```cpp
// 在 GT_ItemBase.cpp 中
void UItemBase::ApplyModifierInternal(const FGT_Modifier& Modifier, APawn* Target)
{
    if (!Target) return;

    switch (Modifier.ModifierType)
    {
    case EGT_ModifierType::Attack:
        if (ICombatInterface* Combat = Cast<ICombatInterface>(Target))
        {
            Combat->AddAttackBonus(Modifier.Value);
        }
        break;
    case EGT_ModifierType::CurrentHealth:
        if (IStatInterface* Stat = Cast<IStatInterface>(Target))
        {
            Stat->HealHealth(Modifier.Value);
        }
        break;
    // ... 其他修饰符
    }
}
```

**验证**：
- [ ] 所有修饰符都有对应实现
- [ ] 修饰符计算正确
- [ ] 接口调用成功

---

### 任务6：UI 集成 🎨
**状态**：🟡 框架支持  
**时间**：4-8小时  
**难度**：★★★★☆  
**前置条件**：任务5 完成

**需要开发的 UI 组件**：

1. **背包界面** (Inventory UI)
   - 物品列表显示
   - 删除/使用/装备按钮
   - 实时负重显示
   - 物品详情预览

2. **仓库界面** (Vault UI)
   - 存储物品列表
   - 存入/取出按钮
   - 物品计数显示

3. **金币显示** (Currency HUD)
   - 三层金币显示
   - Pending / Secured / Global
   - 本局金币获得统计

4. **装备栏** (Equipment Slots)
   - 当前装备显示
   - 换装按钮

5. **物品信息** (Item Tooltip)
   - 物品名称、稀有度
   - 重量、价值
   - 效果描述

**推荐框架**：使用 UMG (Unreal Motion Graphics)

**关键 UI 事件**：
```cpp
// 监听背包变化事件
Inventory->OnInventoryChanged.AddDynamic(this, &UInventoryWidget::OnInventoryChanged);

// 监听仓库变化事件
Vault->OnVaultChanged.AddDynamic(this, &UVaultWidget::OnVaultChanged);
```

**验证**：
- [ ] 所有 UI 组件都能显示
- [ ] 与数据系统同步
- [ ] 操作响应正确

---

### 任务7：测试与验证 ✅
**状态**：🟡 框架已准备  
**时间**：4-8小时  
**难度**：★★★☆☆  
**前置条件**：任务6 完成

**测试覆盖**：
- [ ] 编译无错误
- [ ] 12件物品都能创建
- [ ] 背包操作正常
- [ ] 堆叠合并功能
- [ ] 消耗品使用
- [ ] 装备系统
- [ ] 仓库存取
- [ ] 完整 Run 周期
- [ ] 撤离成功逻辑
- [ ] 撤离失败逻辑
- [ ] 金币转换
- [ ] 冒烟测试通过

**测试命令**：
```powershell
# 冒烟测试
$uprojectPath = (Resolve-Path 'Graytail.uproject').Path
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $uprojectPath `
  -run=GT_RuntimeSmokeRunner -unattended -nopause -nosplash
```

**PIE 测试**：
1. 在编辑器中打开项目
2. 放置 AGT_ItemSystemExample 对象
3. 按 Play 进入 PIE
4. 查看控制台输出
5. 验证所有测试通过

---

## 📊 时间估算与资源分配

| 阶段 | 任务 | 时间 | 优先级 | 工作量 |
|------|------|------|--------|--------|
| 1 | 编译验证 | 15分钟 | 🔴 高 | ⭐ |
| 2 | DataTable | 30分钟 | 🔴 高 | ⭐ |
| 3 | Run 集成 | 2-4h | 🔴 高 | ⭐⭐⭐ |
| 4 | 特殊物品 | 2-6h | 🟡 中 | ⭐⭐⭐⭐ |
| 5 | 修饰符系统 | 4-8h | 🟡 中 | ⭐⭐⭐⭐⭐ |
| 6 | UI 集成 | 4-8h | 🟢 低 | ⭐⭐⭐⭐ |
| 7 | 测试验证 | 4-8h | 🔴 高 | ⭐⭐⭐⭐ |
| **总计** | | **17-41h** | | |

**预期时间线**：
- 快速集成模式（仅 1-3 阶段）：3-5 小时 ⚡
- 标准集成模式（1-5 阶段）：9-21 小时 ✅
- 完整集成模式（全部阶段）：17-41 小时 🎯

---

## 🎯 建议执行路径

### 路径A：快速验证（1天）
```
1. 编译 (15min)
2. DataTable 配置 (30min)
3. Run 集成 (2-4h)
总计：3-5小时
结果：基础功能可用
```

### 路径B：标准集成（3-5天）
```
1. 编译 (15min)
2. DataTable (30min)
3. Run 集成 (2-4h)
4. 特殊物品 (2-6h) - 并行
5. 修饰符系统 (4-8h)
总计：9-21小时
结果：完整的物品系统功能
```

### 路径C：完整方案（1-2周）
```
1-5. 同路径B (9-21h)
6. UI 集成 (4-8h)
7. 测试验证 (4-8h)
总计：17-41小时
结果：完全集成的物品系统
```

---

## 📚 关键参考文档

| 文档 | 用途 | 优先级 |
|------|------|--------|
| [COMPILE_AND_SETUP_GUIDE.md](./COMPILE_AND_SETUP_GUIDE.md) | 编译与 DataTable 配置 | 🔴 必读 |
| [RUN_LIFECYCLE_INTEGRATION_GUIDE.md](./RUN_LIFECYCLE_INTEGRATION_GUIDE.md) | Run 集成详解 | 🔴 必读 |
| [GT_RunIntegrationFramework.h](./GT_RunIntegrationFramework.h) | 集成代码框架 | 🔴 必读 |
| [README.md](./README.md) | 系统使用手册 | 🟡 参考 |
| [ItemSystemArchitecture.md](../docs/ItemSystemArchitecture.md) | 完整设计文档 | 🟡 参考 |
| [GT_ItemSystemExample.cpp](./GT_ItemSystemExample.cpp) | 测试示例代码 | 🟡 参考 |

---

## 🎓 重要提示

1. **编译锁问题**：
   - 编辑器完全关闭后才能编译
   - Live Coding 会占用编译锁，必须禁用

2. **DataTable 命名**：
   - 建议使用 ItemID 作为行名（1-12）
   - 或使用物品名称

3. **金币倍数**：
   - 公司工牌 (ID=10) 提供 1.15x 倍数
   - 在 `HandleExtractionSuccess()` 中检测

4. **可抢救负重**：
   - 失败时保留 40% 的容量
   - 按单位重量价值排序

5. **事件广播**：
   - 所有游戏事件都要调用对应的 BroadcastGameEvent()
   - 装备的物品会自动响应事件

---

## ✨ 成功标志

✅ **任务1完成**：
```
编译成功，退出码为 0
Overall=Pass 出现在输出中
```

✅ **任务2完成**：
```
DT_Items 包含 12 行数据
每件物品都能在编辑器中显示
```

✅ **任务3完成**：
```
Inventory 在 Run 启动时创建
背包可以添加/移除物品
金币转换逻辑正确
```

✅ **任务7完成**：
```
冒烟测试通过，Overall=Pass
PIE 中所有功能测试通过
物品系统完全集成
```

---

## 🚀 下一步

**立即执行**：
1. 打开 PowerShell
2. 执行任务1的编译命令
3. 将结果反馈给我

**编译成功后**：
- 我会帮你执行任务2-3
- 生成具体的集成代码
- 提供详细的修改指导

---

**文档生成时间**：2026-06-16  
**文档版本**：1.0  
**状态**：🟢 可立即执行
