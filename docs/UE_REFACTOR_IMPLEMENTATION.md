# UE_REFACTOR_IMPLEMENTATION.md

> 文件定位：本文件是《REFACTOR_ARCHITECTURE.md》的 Unreal Engine 落地实现说明。  
> 适用项目：《灰尾回收 / 五四三二一》UE 实训版本与后续正式版重构。  
> 核心原则：玩法方向不变，工程实现针对 UE 特化。  
> 实现目标：像素风 2.5D 扫雷情报撤离肉鸽。

## 0. 与通用重构策划案的关系

本文件不是独立架构文档，而是《REFACTOR_ARCHITECTURE.md》的 UE 实现映射。

若两份文档存在冲突：

1. 架构原则以《REFACTOR_ARCHITECTURE.md》为准；
2. UE 中的类、资产、目录、实现方式以本文件为准；
3. 本文件允许根据 UE 工程实际情况调整实现细节，但不得破坏以下原则：
   - TruthMap 与 IntelMap 分离；
   - C++ 管规则；
   - Blueprint 管表现；
   - DataAsset 管内容；
   - UMG / ViewModel 管界面；
   - 系统之间通过 Command / Event / Effect / Query 交互；
   - 不允许 UI 或表现层直接修改核心运行状态。

---

## 1. UE 版本项目定位

UE 版本仍然保持原有核心玩法：

> 玩家在扫雷式小地图提供的情报下探索房间，判断雷房风险，搜刮物资，处理怪物与事件，在五四三二一协议压力下寻找撤离并完成结算。

UE 版本采用：

> 像素风 2.5D 俯视角表现。

含义：

- 角色、怪物、NPC、道具、UI 继续使用像素风 2D 资产；
- 房间在 UE 3D 空间中组织；
- 摄像机采用固定俯视或正交视角；
- 地板、背景、房间装饰可使用贴图 Plane 或简单几何体；
- 墙体、门、障碍物可使用低模几何体或贴图平面；
- 特效、光照、后处理可使用 UE 能力增强表现；
- 不要求全面 3D 建模；
- 不建议将项目改造成写实 3D。

---

## 2. 现有资产迁移策略

| 现有资产类型 | UE 中用途 |
|---|---|
| 主角像素帧 | Sprite / Flipbook / Billboard Actor |
| 怪物像素帧 | Sprite / Flipbook / Billboard Actor |
| NPC 图像 | Sprite Actor / UI 立绘 |
| 房间背景图 | Plane 材质 / 地板贴图 / 房间背景 |
| 宝箱、撤离装置 | Sprite Actor / Interactable Actor |
| 小地图图标 | UMG Image / Slate Brush |
| 物资、藏品、Buff 图标 | 背包、图鉴、结算 UI |
| 菜单背景 | UMG 背景 |
| CG / 图片 | UMG / Media Player / 过场界面 |
| 对话框素材 | UMG Widget |

像素资产导入 UE 时应注意：

1. 关闭或降低纹理平滑，避免像素图模糊；
2. 保持统一像素规格；
3. UI 图标单独维护高清晰度版本；
4. 小地图数字和图标优先保证可读性；
5. 正交相机下避免非整数缩放导致像素抖动；
6. 所有资产命名必须与逻辑 ID 分离。

---

## 3. UE 工程目录建议

### Content 目录

```text
Content/
  Art/
    Characters/
    Enemies/
    Rooms/
    Interactables/
    Icons/
    UI/
    VFX/
    Audio/

  Data/
    Rooms/
    Encounters/
    Items/
    Skills/
    Modifiers/
    Events/
    Enemies/
    LootTables/
    MapModes/
    Difficulty/
    Codex/
    Talents/

  Blueprints/
    Actors/
    Characters/
    Rooms/
    Interactables/
    Components/
    VFX/
    Debug/

  UI/
    HUD/
    MiniMap/
    Inventory/
    Codex/
    EventChoice/
    Combat/
    Result/
    Common/

  Maps/
    Test/
    Prototype/
    Main/
```

### Source 目录

```text
Source/Graytail/
  Core/
    GT_RunContext.*
    GT_CommandBus.*
    GT_EventBus.*
    GT_ActionPipeline.*
    GT_EffectSystem.*
    GT_ContentRegistry.*
    GT_QueryFacade.*

  Domains/
    Map/
    Intel/
    Room/
    Encounter/
    Movement/
    Combat/
    Ability/
    Inventory/
    Extraction/
    Pressure/
    Codex/
    Modifier/

  Data/
    GT_RoomDef.*
    GT_ItemDef.*
    GT_SkillDef.*
    GT_ModifierDef.*
    GT_EventDef.*
    GT_EnemyDef.*
    GT_LootTableDef.*
    GT_MapModeDef.*
    GT_CodexEntryDef.*

  UI/
    ViewModels/

  Save/
    GT_SaveGame.*
    GT_SaveMigration.*

  Debug/
    GT_DebugSubsystem.*
```

---

## 4. C++ / Blueprint / DataAsset / UMG 分工

> C++ 管规则，Blueprint 管表现，DataAsset 管内容，UMG 管界面。

| 内容 | 推荐实现 |
|---|---|
| 地图真实数据 TruthMap | C++ UObject / Struct |
| 玩家情报 IntelMap | C++ UObject / Struct |
| 地图生成 | C++ |
| 雷数计算 | C++ |
| 移动合法性 | C++ |
| Command / Event / Effect | C++ |
| Modifier 结算 | C++ |
| 房间定义 | UPrimaryDataAsset / UDataAsset |
| 道具定义 | UPrimaryDataAsset / UDataAsset |
| 技能定义 | UPrimaryDataAsset / UDataAsset |
| 怪物定义 | UPrimaryDataAsset / UDataAsset |
| 事件定义 | UPrimaryDataAsset / UDataAsset |
| 图鉴条目 | UPrimaryDataAsset / UDataAsset |
| 房间表现 | Blueprint Actor |
| 交互物表现 | Blueprint Actor |
| 角色动画表现 | Blueprint / Flipbook |
| UI | UMG |
| UI 状态 | ViewModel UObject |
| 调试界面 | UMG / Editor Utility |
| 存档 | USaveGame + Schema Version |

禁止：

- 将核心规则写进 Level Blueprint；
- 让 UMG 直接修改 TruthMap / Inventory / RunState；
- 让 Room Blueprint 直接结算背包、图鉴、撤离；
- 用房间显示名作为逻辑判断条件；
- 在多个系统中重复保存同一份状态。

---

## 5. 核心类设计

建议创建：

- `UGT_RunSubsystem`
- `UGT_RunContext`
- `UGT_CommandBus`
- `UGT_EventBus`
- `UGT_EffectSystem`
- `UGT_ModifierSystem`
- `UGT_ContentRegistry`
- `UGT_QueryFacade`

### RunContext 包含

```text
RunId
Seed
DifficultyId
MapModeId
SceneThemeId
Players
TruthMap
IntelMaps
ActiveModifiers
InventoryStates
PressureState
ExtractionState
CurrentEncounter
RunFlags
ContentPools
```

---

## 6. DataAsset 设计

建议创建统一基类：

```text
UGT_ContentDef
```

基础字段：

```text
ContentId
DisplayNameKey
DescriptionKey
GameplayTags
Icon
bDeprecated
ReplacementId
SaveSchemaVersion
Dependencies
```

建议创建内容资产类型：

- `UGT_RoomDef`
- `UGT_EncounterDef`
- `UGT_ItemDef`
- `UGT_SkillDef`
- `UGT_EventDef`
- `UGT_ModifierDef`
- `UGT_LootTableDef`
- `UGT_MapModeDef`
- `UGT_CodexEntryDef`

---

## 7. Gameplay Tag 规范

示例：

```text
Room.Normal
Room.Mine
Room.Monster
Room.Chest
Room.Event
Room.Exit

Hazard.Poison
Hazard.Dark
Hazard.Electric
Hazard.Explosive

Intel.Accurate
Intel.Distorted
Intel.Stale
Intel.Hidden

Movement.FourDir
Movement.Jump
Movement.Teleport
Movement.Diagonal

Modifier.Pressure.Reverse
Modifier.Mine.Immunity
Modifier.Chest.Tax

Item.Consumable
Item.Collectible
Item.Tool
Item.Equipment

Encounter.Combat
Encounter.Hazard
Encounter.EventChoice
```

优先判断 Tag，而不是判断具体资产 ID。

---

## 8. TruthMap / IntelMap UE 实现

### TruthMap

```text
FGT_TruthMap
- Width
- Height
- Cells
- MapShape
- Seed
```

```text
FGT_TruthCell
- Coord
- RoomDefId
- bHasMine
- bIsExit
- GameplayTags
- Payload
- bResolved
- bTriggered
```

### IntelMap

```text
FGT_IntelMap
- OwnerActorId
- Cells
```

```text
FGT_IntelCell
- Coord
- bVisible
- bExplored
- bScanned
- DisplayedNumber
- ReliabilityState
- MarkerState
- VisibleRoomIcon
- bStale
- ScanTags
```

ReliabilityState：

```text
Accurate
OffsetPossible
Unknown
Distorted
Stale
Hidden
```

UMG 小地图不得读取 TruthMap。

流程：

```text
IntelMap
→ MiniMapViewModel
→ UMG MiniMap
```

---

## 9. UI / UMG 设计

建议创建：

- `UGT_HUDViewModel`
- `UGT_MiniMapViewModel`
- `UGT_InventoryViewModel`
- `UGT_CodexViewModel`
- `UGT_EventChoiceViewModel`
- `UGT_CombatViewModel`
- `UGT_ResultViewModel`

UMG 只绑定 ViewModel，不直接读 Domain。

---

## 10. SaveGame 设计

建议创建：

```text
UGT_SaveGame
```

保存：

```text
SaveVersion
MetaProgress
CodexProgress
UnlockedContent
CurrentRunSnapshot
Settings
```

当前局保存：

```text
RunId
Seed
RunConfig
TruthMapState
IntelMapState
PlayerStates
InventoryInstances
ActiveModifiers
PressureState
ExtractionState
```

---

## 11. Debug / Validation 工具

最低要求：

1. RunContext 查看器；
2. TruthMap / IntelMap 对照；
3. 当前 Active Modifier 列表；
4. ActionPipeline 日志；
5. Effect 结算展开；
6. Pressure 变化来源；
7. ContentRegistry 引用检查；
8. 固定 Seed 重放；
9. 强制生成指定房间；
10. 小地图真实雷调试显示。

---

## 12. 当前重构阶段范围

当前阶段优先：

- 工程壳；
- 核心内核；
- 数据结构；
- 接口；
- 空 Domain；
- DataAsset 类型；
- ViewModel 类型；
- Effect / Modifier / Command / Event 通道；
- 编译通过；
- TODO 清晰。

不要求当前阶段实现完整玩法验证。

