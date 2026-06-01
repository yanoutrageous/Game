# REFACTOR_ARCHITECTURE.md

> 文件定位：引擎无关的核心重构架构文档。  
> 目标：定义《灰尾回收 / 五四三二一》未来正式版所需的稳定内核、可变外壳、模块边界、增删改查规则和扩展协议。  
> 注意：本文件不绑定 UE、Unity、Godot 或其他引擎。UE 实训版本的落地实现请参考 `UE_REFACTOR_IMPLEMENTATION.md`。

## 0. 重构总目标

本项目核心玩法已经验证成立：

> 扫雷式小地图情报 + 房间探索 + 搜打撤撤离结算 + 肉鸽构筑 + 规则变异。

后续可能扩展：欺骗数字、移动雷、跳格移动、特殊雷房、动作躲避、真扫雷、黑暗追逐、局内战斗、玩家技能、背包、安全箱、藏品、图鉴、研究、多地图模式、多难度、多人协作与信息不对称。

重构目标不是“把代码拆成更多文件”，而是：

> 建立一个能够吸收未来变化的稳定内核，使新内容、新机制、新系统能够通过注册、事件、效果、管线和数据资产接入，而不是侵入旧系统。

---

## 1. 稳定内核与可变外壳

### 稳定内核

稳定内核是游戏的“操作系统”，不应频繁变化：

- RunContext；
- StateStore；
- CommandBus；
- ActionPipeline；
- EventBus；
- EffectSystem；
- ModifierSystem；
- ContentRegistry；
- QueryFacade；
- ViewModelLayer；
- SaveSystem；
- Debug / Validation Tools。

### 可变外壳

可变外壳是未来频繁扩展的内容与系统：

- 房间类型；
- 雷房机制；
- 怪物；
- 技能；
- 消耗品；
- 藏品；
- 图鉴；
- 事件房；
- 地图模式；
- 移动规则；
- 规则变异；
- 场景包；
- 多人规则。

目标：

> 可变外壳通过稳定内核提供的协议接入，不反向污染内核。

---

## 2. 总体架构

```text
GameKernel
├─ RunContext
├─ StateStore
├─ CommandBus
├─ ActionPipeline
├─ EventBus
├─ EffectSystem
├─ ModifierSystem
├─ RulePipeline
├─ ContentRegistry
├─ QueryFacade
├─ ViewModelLayer
├─ SaveSystem
└─ Debug / Validation Tools
```

---

## 3. RunContext

RunContext 管理当前单局上下文：

```text
RunContext
├─ runId
├─ seed
├─ difficulty
├─ mapMode
├─ sceneTheme
├─ players
├─ truthMap
├─ intelMaps
├─ activeModifiers
├─ inventoryStates
├─ pressureState
├─ extractionState
├─ encounterState
├─ runFlags
└─ contentPools
```

要求：

- 单局状态从 RunContext 获取；
- RunContext 不成为上帝类；
- 多人预留 actorId / playerId / teamId；
- 不允许各模块私自维护重复的当前局全局状态。

---

## 4. StateStore：状态所有权

每类状态必须有唯一 owner。

| 状态 | 唯一写入模块 | 其他模块访问方式 |
|---|---|---|
| 真实地图 | MapDomain | Query |
| 玩家可见情报 | IntelDomain | Query |
| 玩家位置 | MovementDomain | Query |
| 当前房间状态 | RoomDomain | Query |
| 当前战斗会话 | CombatDomain | Query |
| 背包内容 | InventoryDomain | Command / Query |
| 安全箱内容 | InventoryDomain | Command / Query |
| 协议压力 | PressureDomain | Command / Event |
| 撤离状态 | ExtractionDomain | Event / Query |
| 图鉴进度 | CodexDomain | Event |
| 当前 Modifier | ModifierDomain | Query |

原则：

> 一个状态只能由一个 Domain 写入。其他模块只能通过 Command、Effect 或 Event 请求变化。

---

## 5. CommandBus

Command 表示“请求做某件事”。

常见 Command：

```text
MoveCommand
TeleportCommand
JumpMoveCommand
EnterRoomCommand
UseItemCommand
UseSkillCommand
MarkCellCommand
OpenChestCommand
ChooseEventOptionCommand
StartExtractionCommand
ConfirmFailureRecoveryCommand
```

Command 必须包含：

```text
commandId
actorId
source
target
payload
sequence
```

要求：

- UI 只发 Command；
- Command 进入 ActionPipeline；
- 关键 Command 防重复执行；
- Command 不直接改状态。

---

## 6. ActionPipeline

ActionPipeline 负责把一次玩家行为拆成稳定步骤。

移动进入房间示例：

```text
MoveCommand
→ ValidateActor
→ ValidateMovementRule
→ ApplyMovementModifiers
→ CommitPosition
→ RevealIntel
→ EnterRoom
→ ResolveRoomTrigger
→ ApplyRoomModifiers
→ EmitEvents
→ UpdateViewModels
```

踩雷示例：

```text
MineTriggered
→ BaseMineDamage
→ DifficultyModifier
→ RoomTagModifier
→ EquipmentModifier
→ ActiveModifier
→ Shield / Immunity
→ FinalDamage
→ ApplyHPChange
→ PressureEffects
→ CodexRecord
→ RoomResolved
→ EmitResultEvents
```

要求：

- 每个阶段可插入 Modifier；
- 每个阶段可记录日志；
- 尽量 Resolve 后统一 Commit；
- 防止边算边改导致半状态错误。

---

## 7. EventBus

Event 表示“某事已经发生”。

常见 Event：

```text
OnRunStarted
OnRoomRevealed
OnRoomEntered
OnRoomResolved
OnMineTriggered
OnCombatStarted
OnCombatResolved
OnChestOpened
OnLootGained
OnItemUsed
OnSkillUsed
OnPressureChanged
OnExtractionFound
OnExtractionSuccess
OnRunFailed
OnCodexDiscovered
OnResearchCompleted
```

Event 必须包含：

```text
eventId
eventType
source
actorId
payload
sequence
```

原则：

- Event 不应被滥用为 Command；
- Event 广播事实；
- Event 可被日志、UI、音效、Modifier、Codex 监听。

---

## 8. EffectSystem

Effect 是技能、道具、事件、藏品、天赋对游戏状态产生影响的统一语言。

常见 Effect：

```text
ModifyHP
ModifyPressure
RevealCell
DistortIntelNumber
MoveMine
StartEncounter
DamageEnemy
AddItem
RemoveItem
ProtectItem
AddModifier
RemoveModifier
UnlockCodex
ChangeMovementRule
ChangeExtractionRule
ChangeRoomTag
CreateTemporaryExit
```

要求：

- 技能、道具、事件、藏品都通过 Effect 表达；
- 每种 Effect 由唯一 Handler 处理；
- 新增 Effect 必须注册 Handler；
- Effect 不应直接写入无关系统内部状态。

---

## 9. ModifierSystem

Modifier 表示持续规则变化。

来源：

- 事件房；
- 藏品；
- 技能；
- 消耗品；
- 天赋；
- 地图模式；
- 难度；
- 多人职业。

Modifier 应声明：

```text
id
source
duration
priority
stackRule
conflictTags
hooks
uiDisplay
```

必须解决：

| 问题 | 规则 |
|---|---|
| 同类是否叠加 | stackRule |
| 谁先结算 | priority |
| 新增是否立即生效 | 默认下一个事件生效 |
| 移除后本轮是否继续 | 默认当前队列继续，下一轮失效 |
| 冲突如何处理 | conflictTags + priority |

---

## 10. ContentRegistry

所有内容必须通过 ContentRegistry 注册。

内容类型：

```text
RoomDef
HazardDef
EnemyDef
SkillDef
ItemDef
CollectibleDef
EventDef
ModifierDef
LootTableDef
MapModeDef
DifficultyDef
CodexEntryDef
TalentDef
SceneThemeDef
```

内容资产必须包含：

```text
id
type
tags
dependencies
unlockConditions
effects
hooks
uiAssets
localizationKeys
saveSchemaVersion
deprecatedState
```

要求：

- ID 稳定；
- ID 与显示文本分离；
- 内容包可启用 / 禁用；
- 删除内容使用 tombstone；
- 启动时执行引用校验。

---

## 11. QueryFacade

系统之间不应深入读取内部字段。

QueryFacade 提供：

```text
GetRoomTruth(position)
GetIntelCell(actorId, position)
GetAvailableMoves(actorId)
GetActiveModifiers(actorId)
GetInventory(actorId)
GetCombatContext(actorId, encounterId)
GetExtractionState(actorId)
GetCodexProgress(actorId)
```

Query 只读，不能产生副作用。

---

## 12. ViewModelLayer

UI 不直接读取 Domain 内部状态，而是读取 ViewModel。

包括：

```text
HUDViewModel
MiniMapViewModel
MapOverlayViewModel
InventoryViewModel
CombatViewModel
EventChoiceViewModel
CodexViewModel
ResultViewModel
```

小地图格子 ViewModel 示例：

```text
MiniMapCellView {
  position
  visible
  explored
  primaryText
  roomIcon
  markerIcon
  reliabilityState
  staleState
  highlightState
  tooltipKey
}
```

---

## 13. TruthMap / IntelMap

这是本项目的关键边界。

### TruthMap

真实地图状态：

- 真实房间；
- 真实雷；
- 真实撤离点；
- 真实房间标签；
- 真实 resolved 状态；
- 真实移动雷状态。

### IntelMap

玩家认知状态：

- 已知格；
- 显示数字；
- 数字可靠性；
- 标记；
- 扫描结果；
- 过期情报；
- 多人信息差。

原则：

> UI 和玩家判断只看 IntelMap；规则真相由 TruthMap 管理。

---

## 14. Domain 模块

建议 Domain：

- MapDomain；
- IntelDomain；
- RoomDomain；
- EncounterDomain；
- CombatDomain；
- AbilityDomain；
- MovementDomain；
- InventoryDomain；
- ExtractionDomain；
- CodexDomain；
- PressureDomain；
- ModifierDomain。

每个 Domain 必须声明：

1. Own State；
2. Commands；
3. Queries；
4. Events In；
5. Events Out；
6. Effects Handled；
7. Save Schema；
8. Tests。

---

## 15. Content Pack

未来内容按包注册，而不是写进核心系统。

示例：

```text
BasePack
DungeonPack
PollutionZonePack
ForestTemplePack
FogModePack
HexModePack
CombatBasicPack
EngineerSkillPack
DarkMinePack
ReverseRuleEventPack
CodexArtifactPack
```

每个包声明：

```text
packId
requires
providedSystems
registeredContent
enabledByDefault
debugOnly
```

---

## 16. 增删改查规则

### 新增系统

必须提供：

1. Domain；
2. State ownership；
3. Command handler；
4. Effect handler；
5. Events emitted；
6. Queries exposed；
7. ViewModel；
8. Save schema；
9. Tests；
10. Content pack entry。

### 删除内容

禁止直接删除关键 ID，必须 tombstone：

```text
DeprecatedContent {
  id
  deprecated = true
  replacement
  keepForSaveLoad = true
}
```

### 修改内容

- 数值修改走资产；
- 规则修改走 Effect / Modifier；
- 存档字段修改走 schemaVersion + migration；
- 局内实例使用 snapshot 或 instanceData；
- 删除字段需兼容旧存档。

### 查询状态

- 走 QueryFacade；
- UI 走 ViewModel；
- Debug 读取快照；
- 不允许 UI 或调试工具直接改核心状态。

---

## 17. 多人预留

即使当前不做多人，也必须预留：

```text
actorId
playerId
teamId
sourcePlayer
ownerPlayer
sharedIntel
personalIntel
```

要求：

- Command / Event 必须能追踪 actorId；
- IntelMap 支持按 playerId 拆分；
- Inventory 支持 owner；
- Pressure 可未来改为团队共享。

---

## 18. 当前阶段边界

当前工程重构阶段只需要：

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

不应急着实现：

- 完整战斗；
- 完整技能；
- 移动雷；
- 特殊雷房；
- 多人；
- 复杂 UI。

一句话：

> 现在要搭“插槽”，不是把所有“插卡”都做出来。
