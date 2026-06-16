# 物品系统集成编译指南

生成日期：2026-06-16

## 第1阶段：编译与验证 ✅ 准备就绪

### 编译前检查清单

- [x] ItemSystem 所有文件已生成 (20个文件)
- [x] 代码无编译错误 (VS Code 验证通过)
- [x] Graytail.Build.cs 配置完整
- [x] uproject 文件配置正确

### 编译步骤

#### 方案1：通过命令行编译（推荐）

打开 PowerShell，执行以下命令：

```powershell
# 1. 进入项目目录
cd "c:\Work\practice\Game\UE\Graytail"

# 2. 获取 uproject 绝对路径
$uprojectPath = (Resolve-Path 'Graytail.uproject').Path
Write-Host "Project: $uprojectPath"

# 3. 编译命令（根据 CLAUDE.md）
$buildCmd = "& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' GraytailEditor Win64 Development -Project=$uprojectPath -WaitMutex"
Write-Host "Executing: $buildCmd"
Invoke-Expression $buildCmd

# 4. 检查编译结果
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ 编译成功" -ForegroundColor Green
} else {
    Write-Host "✗ 编译失败，退出码: $LASTEXITCODE" -ForegroundColor Red
}
```

**预期输出**：
```
...
Compiling source code for Graytail...
Generating code...
Running AutomationTool...
...
Overall=Pass
```

#### 方案2：通过 Unreal Editor 生成编译

1. 打开 Graytail.uproject
2. 点击"Create" or "Generate Visual Studio files" 
3. 选择 Visual Studio Solution 并打开
4. 在 Visual Studio 中编译 GraytailEditor 配置

#### 方案3：使用 Unreal Automation Tool

```powershell
cd "c:\Work\practice\Game\UE\Graytail"
$uprojectPath = (Resolve-Path 'Graytail.uproject').Path

# 使用 Unreal Automation Tool 编译
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildProject -Project=$uprojectPath -Target=GraytailEditor -Platform=Win64 -Configuration=Development -BuildMode=SubmittedOnly
```

### 编译后验证

编译完成后，执行：

```powershell
# 冒烟测试命令（根据 CLAUDE.md）
$uprojectPath = (Resolve-Path 'Graytail.uproject').Path
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $uprojectPath -run=GT_RuntimeSmokeRunner -unattended -nopause -nosplash

# 检查输出
$logPath = "Saved\Logs\Graytail.log"
if (Test-Path $logPath) {
    Select-String "Overall=Pass" $logPath
}
```

### 常见编译问题

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| "Unable to build while Live Coding is active" | 编辑器正在运行 | 完全关闭 UE 编辑器 |
| "Project does not exist" | 路径错误 | 使用绝对路径：`Resolve-Path` |
| "Engine not found" | UE 5.7 未安装 | 检查 Epic Games Launcher |
| 编译缓慢 | 首次编译或变更多 | 正常，可能需要 5-10 分钟 |
| "Missing dependencies" | ItemSystem 依赖问题 | 参考下方"依赖检查" |

### 依赖检查

ItemSystem 模块依赖：
- ✅ Core - 基础类型
- ✅ CoreUObject - UObject 系统
- ✅ Engine - 游戏引擎功能
- ✅ UMG - UI 框架（背包 UI 时使用）

**Graytail.Build.cs 已包含所有必要依赖，无需修改**

---

## 第2阶段：DataTable 配置 📋

### 创建 DataTable

1. **打开 UE 编辑器**
   ```powershell
   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "c:\Work\practice\Game\UE\Graytail\Graytail.uproject"
   ```

2. **在内容浏览器中创建 DataTable**
   - 右键 → 新建 → DataTable
   - 选择行结构体：`FGT_ItemData`
   - 命名：`DT_Items`
   - 保存到 `/Game/Data/` 目录

3. **配置物品数据**

### 12件物品配置数据

#### 将以下数据复制到 DT_Items DataTable 中

**行 1：异常体犬牙**
```
RowName: 1 (或 AnomalousFang)
ItemID: 1
ItemName: 异常体犬牙
ItemType: PassiveArtifact
Rarity: Rare
Weight: 1.0
Value: 80
MaxStackSize: 1
RequiredSlot: Tool
Tags: DamageStack
Effects:
  [0] TriggerType: OnDefeatMonster
      TriggerProbability: 1.0
      Modifiers: [{ModifierType: Attack, Value: 2.0, bStackable: true, MaxStack: 5}]
```

**行 2：受损扫描仪**
```
RowName: 2 (或 DamagedScanner)
ItemID: 2
ItemName: 受损扫描仪
ItemType: PassiveArtifact
Rarity: Uncommon
Weight: 2.0
Value: 60
MaxStackSize: 1
RequiredSlot: Tool
EffectDescription: 进入房间时扫描相邻4格
Effects:
  [0] TriggerType: OnEnterRoom
      EffectDescription: 扫描相邻4格（需自定义逻辑）
```

**行 3：公司标准手套**
```
RowName: 3 (或 StandardGloves)
ItemID: 3
ItemName: 公司标准手套
ItemType: Equipment
Rarity: Common
Weight: 1.0
Value: 30
MaxStackSize: 1
RequiredSlot: Tool
Effects:
  [0] TriggerType: OnTakeDamage
      TriggerCondition: IsMineRoom
      bOneTimeOnly: true
      Modifiers: [{ModifierType: DamageReduction, Value: 0.5}]
```

**行 4：幸运硬币**
```
RowName: 4 (或 LuckyCoin)
ItemID: 4
ItemName: 幸运硬币
ItemType: Consumable
Rarity: Uncommon
Weight: 0.0
Value: 50
MaxStackSize: 1
Effects:
  [0] TriggerType: OnUse
      TriggerProbability: 0.5
      Modifiers: [{ModifierType: Gold, Value: 30}]
```

**行 5：应急绷带**
```
RowName: 5 (或 EmergencyBandage)
ItemID: 5
ItemName: 应急绷带
ItemType: Consumable
Rarity: Common
Weight: 1.0
Value: 20
MaxStackSize: 3
Effects:
  [0] TriggerType: OnUse
      Modifiers: [{ModifierType: CurrentHealth, Value: 2.0}]
```

**行 6：封锁区结晶**
```
RowName: 6 (或 LockedZoneCrystal)
ItemID: 6
ItemName: 封锁区结晶
ItemType: PassiveArtifact
Rarity: Epic
Weight: 3.0
Value: 200
MaxStackSize: 1
Effects:
  [0] TriggerType: OnProtocolLevelChanged
      TriggerCondition: LevelDecreased
      Modifiers: [{ModifierType: CurrentHealth, Value: 1.0, MaxStack: 4}]
```

**行 7：残缺地图**
```
RowName: 7 (或 IncompleteMap)
ItemID: 7
ItemName: 残缺地图
ItemType: Consumable
Rarity: Uncommon
Weight: 1.0
Value: 45
MaxStackSize: 1
Effects:
  [0] TriggerType: OnUse
      EffectDescription: 显示撤离方向（需UI配合）
```

**行 8：超载零件箱**
```
RowName: 8 (或 OverloadedPartBox)
ItemID: 8
ItemName: 超载零件箱
ItemType: PassiveArtifact
Rarity: Rare
Weight: 4.0
Value: 120
MaxStackSize: 1
Effects:
  [0] TriggerType: Passive
      Modifiers: [{ModifierType: WeightCapacity, Value: 3.0}]
  [1] TriggerType: OnMoveStep
      TriggerCondition: ProtocolLevel<=3
      Modifiers: [{ModifierType: ProtocolPressure, Value: 1.0}]
```

**行 9：灰尾应急灯**
```
RowName: 9 (或 EmergencyLight)
ItemID: 9
ItemName: 灰尾应急灯
ItemType: Consumable
Rarity: Common
Weight: 1.0
Value: 25
MaxStackSize: 1
Effects:
  [0] TriggerType: OnUse
      EffectDescription: 扫描周围9格（自定义逻辑）
```

**行 10：公司工牌**
```
RowName: 10 (或 CompanyBadge)
ItemID: 10
ItemName: 公司工牌
ItemType: PassiveArtifact
Rarity: Epic
Weight: 0.0
Value: 150
MaxStackSize: 1
Effects:
  [0] TriggerType: Passive
      Modifiers: [{ModifierType: PriceDiscount, Value: -0.2}]
  [1] TriggerType: OnExtractionSuccess
      Modifiers: [{ModifierType: GoldMultiplier, Value: 0.15}]
```

**行 11：回收磁石**
```
RowName: 11 (或 RecoveryMagnet)
ItemID: 11
ItemName: 回收磁石
ItemType: PassiveArtifact
Rarity: Rare
Weight: 1.0
Value: 90
MaxStackSize: 1
Effects:
  [0] TriggerType: OnPickupLoot
      TriggerCondition: RoomType=Treasure
      EffectDescription: 额外获得一件物品（自定义逻辑）
```

**行 12：协议镇定剂**
```
RowName: 12 (或 ProtocolSedative)
ItemID: 12
ItemName: 协议镇定剂
ItemType: Consumable
Rarity: Rare
Weight: 1.0
Value: 100
MaxStackSize: 1
Effects:
  [0] TriggerType: OnUse
      bOneTimeOnly: true
      Modifiers: [{ModifierType: ProtocolImmunity, Value: 1.0}]
```

### DataTable 导入建议

**方式1：编辑器手动填写** (推荐用于小数据量)
- 在编辑器中打开 DT_Items
- 逐行填写数据
- 保存

**方式2：CSV 导入**
- 创建 CSV 文件（见下方格式）
- 在编辑器中导入
- 更快但需要正确的格式

**CSV 格式示例**：
```csv
RowName,ItemID,ItemName,ItemType,Rarity,Weight,Value,MaxStackSize
1,1,异常体犬牙,PassiveArtifact,Rare,1.0,80,1
5,5,应急绷带,Consumable,Common,1.0,20,3
10,10,公司工牌,PassiveArtifact,Epic,0.0,150,1
```

---

## 第3阶段：在 PIE 中测试 🧪

### 运行测试示例

1. **打开项目**
   ```powershell
   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "c:\Work\practice\Game\UE\Graytail\Graytail.uproject"
   ```

2. **部署测试 Actor**
   - 在关卡中放置 `AGT_ItemSystemExample` Actor
   - 或创建一个蓝图继承 `AGT_ItemSystemExample`

3. **运行 PIE**
   - 点击 Play
   - 在屏幕上可看到测试日志输出
   - 或查看 Output Log 窗口

4. **测试覆盖范围**
   - [x] 物品创建
   - [x] 背包操作
   - [x] 堆叠合并
   - [x] 消耗品使用
   - [x] 装备系统
   - [x] 仓库操作
   - [x] 物品效果触发
   - [x] 金币系统
   - [x] Run 生命周期

### 预期测试输出

```
========== Item System Examples Started ==========

[1] 初始化物品系统...
✓ 物品系统初始化成功

[2] 测试物品创建...
✓ 按ID创建成功: 异常体犬牙
✓ 按名称创建成功: 应急绷带
✓ 便利方法创建成功: 公司工牌

[3] 测试背包操作...
✓ 创建背包成功
✓ 添加物品成功，当前背包重量: 1.0

...

========== 测试结果汇总 ==========
[PASS] Initialize ItemSystem - 
[PASS] Create Item by ID - 异常体犬牙
[PASS] Create Item by Name - 应急绷带
...
========== 测试完成 ==========
```

---

## 快速集成检查清单

- [ ] 第1阶段：编译完成，无错误，冒烟测试通过
- [ ] 第2阶段：DataTable 已创建并配置12件物品
- [ ] 第3阶段：PIE 测试运行成功，所有功能验证正常
- [ ] ItemSystemManager 已注册到 GameInstance
- [ ] 物品系统已加载到项目
- [ ] 所有 12 件物品可正常创建

---

## 下一步行动

✅ **完成**：代码生成与编译准备  
⏭️ **下一步**：Run 生命周期集成

参考：`COMPLETION_REPORT.md` 中的"第3阶段"

---

**生成时间**：2026-06-16  
**版本**：1.0
