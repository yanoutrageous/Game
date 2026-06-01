# CODEX_TASKS.md

> 文件定位：本文件用于指导 CodeX 按步骤实现 UE 重构。  
> 前置阅读：必须先阅读 `REFACTOR_ARCHITECTURE.md` 和 `UE_REFACTOR_IMPLEMENTATION.md`。  
> 执行原则：先搭建稳定内核，再迁移核心玩法；先搭插槽，再做插卡；每轮执行大模块，但模块内部必须自检。  
> 项目目标：在不破坏现有 Lua / UrhoX 原型的前提下，在仓库内新建 UE 实训版本工程，并建立可扩展架构骨架。

## 0. 当前执行策略

本阶段不是要一次性做完整游戏，也不是要验证所有未来玩法。

本阶段目标：

> 建立 UE 工程壳 + 核心架构接口 + 数据结构 + DataAsset 类型 + ViewModel / Save / Debug 预留。

本阶段不要求实现：

- 完整战斗系统；
- 完整技能系统；
- 移动雷玩法；
- 欺骗数字玩法；
- 特殊雷房玩法；
- 完整背包图鉴；
- 多人联网；
- 完整 UI 美术；
- 完整可玩垂直切片。

---

## 1. 总体对话轮次建议

采用中粒度推进，预计 5–7 轮 CodeX 对话。

| 轮次 | 任务范围 | 主要产出 |
|---:|---|---|
| 1 | 上传原型 + 上传文档 + 建立分支 | 远程基线、文档、`refactor/ue-foundation` 分支 |
| 2 | 创建 UE 工程壳 + 目录 + `.gitignore` | `UE/Graytail/` 工程骨架 |
| 3 | Core + Map / Intel 骨架 | RunContext、CommandBus、EventBus、TruthMap、IntelMap |
| 4 | DataAsset + Effect / Modifier + ViewModel 骨架 | ContentDef、RoomDef、SkillDef、EffectSystem、ModifierSystem、MiniMapViewModel |
| 5 | Query / Save / Debug / actorId 预留 | QueryFacade、SaveGame、Debug Stub、多人字段 |
| 6 | 编译修正 + 结构自检 + 文档同步 | 可编译、TODO 清晰、架构自检报告 |
| 7 | 可选：最小运行初始化 | StartRun 初始化、10×10 空地图生成，不强制 |

---

## 2. 每轮通用执行要求

每一轮 CodeX 都必须：

1. 检查当前分支；
2. 阅读相关文档；
3. 执行本轮任务；
4. 按模块自检；
5. 尝试构建或说明无法构建原因；
6. commit；
7. push；
8. 输出下一轮建议。

每轮最终输出格式：

```markdown
## 执行摘要
## 当前分支
## 本轮完成内容
## 新增文件
## 修改文件
## 模块自检
## 构建 / 验证结果
## Commit Hash
## Push 结果
## 未完成事项
## 下一轮建议
```

---

## 3. 全局禁止事项

CodeX 执行时禁止：

1. 将核心规则写入 Level Blueprint；
2. 让 UMG 直接修改核心状态；
3. 让 Room Blueprint 直接操作背包、图鉴、撤离；
4. 在多个系统中保存同一份状态；
5. 用显示文本作为逻辑 ID；
6. 在 UI 中直接读取 TruthMap；
7. 让某个 Blueprint 成为上帝类；
8. 未经 Command / Effect / Event 直接跨模块调用；
9. 为了快速实现而绕过 DataAsset；
10. 在当前阶段实现复杂战斗、移动雷、特殊雷房或多人；
11. 移动、删除或破坏现有 Lua / UrhoX 原型目录。

---

## 4. 全局必须遵守事项

必须遵守：

1. C++ 管规则；
2. Blueprint 管表现；
3. DataAsset 管内容；
4. UMG 管界面；
5. TruthMap 与 IntelMap 分离；
6. 所有内容使用稳定 ContentId；
7. 玩家操作通过 Command；
8. 结果事实通过 Event；
9. 规则影响通过 Effect；
10. 持续规则通过 Modifier；
11. UI 通过 ViewModel 获取显示数据；
12. 每个模块有明确 owner；
13. 每轮结束 commit + push。

---

## 5. 第 1 轮：上传原型、文档、建立分支

### 目标

将现有 Lua / UrhoX 原型上传到远程仓库，并添加三份文档。

远程仓库：

```text
https://github.com/yanoutrageous/Game.git
```

### 任务

1. 检查当前 Git 状态；
2. 检查是否已关联远程；
3. 如未关联，添加 origin；
4. 检查 `.gitignore`；
5. 提交当前 Lua / UrhoX 原型基线；
6. 添加三份文档到 `docs/`：
   - `REFACTOR_ARCHITECTURE.md`
   - `UE_REFACTOR_IMPLEMENTATION.md`
   - `CODEX_TASKS.md`
7. push 到 `main`；
8. 创建 tag：
   ```text
   lua-prototype-baseline
   ```
9. push tag；
10. 创建并切换分支：
   ```text
   refactor/ue-foundation
   ```
11. push 分支。

建议 commit：

```text
chore: import Lua UrhoX prototype baseline
docs: add UE refactor architecture documents
```

禁止：

- 不创建 UE 工程；
- 不移动原型目录；
- 不改玩法代码；
- 不批量迁移 assets。

---

## 6. 第 2 轮：创建 UE 工程壳

### 目标

在仓库中创建独立 UE 工程目录：

```text
UE/Graytail/
```

### 创建目录

```text
UE/Graytail/
  Graytail.uproject
  Config/
  Content/
  Source/
```

```text
UE/Graytail/Content/
  Art/
  Data/
  Blueprints/
  UI/
  Maps/
```

```text
UE/Graytail/Source/Graytail/
  Core/
  Domains/
  Data/
  UI/ViewModels/
  Save/
  Debug/
```

### 创建 C++ 模块基础文件

```text
UE/Graytail/Source/Graytail/Graytail.Build.cs
UE/Graytail/Source/Graytail/Graytail.cpp
UE/Graytail/Source/Graytail/Graytail.h
UE/Graytail/Source/Graytail.Target.cs
UE/Graytail/Source/GraytailEditor.Target.cs
```

### 创建 Config

```text
UE/Graytail/Config/DefaultEngine.ini
UE/Graytail/Config/DefaultGame.ini
UE/Graytail/Config/DefaultInput.ini
```

### 更新 `.gitignore`

忽略：

```text
UE/Graytail/Binaries/
UE/Graytail/DerivedDataCache/
UE/Graytail/Intermediate/
UE/Graytail/Saved/
UE/Graytail/.vs/
UE/Graytail/*.sln
UE/Graytail/*.VC.db
UE/Graytail/*.opensdf
```

禁止：

- 不创建 `.uasset`；
- 不实现 RunSubsystem；
- 不迁移玩法；
- 不移动旧原型。

建议 commit：

```text
chore: add Unreal project shell
```

---

## 7. 第 3 轮：Core + Map / Intel 骨架

### 目标

创建可编译 C++ 骨架。

### Core

创建：

```text
UGT_RunSubsystem
UGT_RunContext
UGT_CommandBus
UGT_EventBus
UGT_EffectSystem
UGT_ContentRegistry
```

### Map / Intel

创建：

```text
FGT_TruthMap
FGT_TruthCell
FGT_IntelMap
FGT_IntelCell
EGT_RoomBaseType
EGT_IntelReliabilityState
```

### 最低要求

1. RunSubsystem 可以创建 RunContext；
2. RunContext 保存 RunId、Seed、基础地图尺寸；
3. TruthMap 可以初始化 10×10 网格；
4. TruthCell 可以表达普通房、雷房、撤离房；
5. IntelMap 可以保存玩家可见状态；
6. IntelCell 可以保存显示数字、可见状态、标记状态、可靠性状态；
7. 所有类和结构尽量 BlueprintType / Blueprintable；
8. 添加 TODO，但不要实现完整玩法。

### 自检

1. RunContext 是否没有变成上帝类；
2. TruthMap / IntelMap 是否分离；
3. EventBus 是否只是事件通道；
4. EffectSystem 是否只是效果分发骨架；
5. ContentRegistry 是否只做注册入口。

建议 commit：

```text
feat: add core runtime and map intel skeleton
```

---

## 8. 第 4 轮：DataAsset + Effect / Modifier + ViewModel 骨架

### 目标

创建数据资产基类、核心玩法资产类型、Effect / Modifier 基础结构和小地图 ViewModel。

### DataAsset

创建：

```text
UGT_ContentDef
UGT_RoomDef
UGT_ItemDef
UGT_SkillDef
UGT_ModifierDef
UGT_EncounterDef
UGT_LootTableDef
UGT_MapModeDef
UGT_CodexEntryDef
```

### Effect / Modifier

完善：

```text
UGT_EffectSystem
UGT_ModifierSystem
FGT_EffectSpec
FGT_ActiveModifier
```

预留 Effect 类型：

```text
ModifyHP
ModifyPressure
RevealCell
DistortIntelNumber
MoveMine
StartEncounter
AddItem
RemoveItem
ProtectItem
AddModifier
RemoveModifier
UnlockCodex
ChangeMovementRule
```

### ViewModel

创建：

```text
UGT_MiniMapViewModel
FGT_MiniMapCellView
```

MiniMapCellView 包含：

```text
Coord
bVisible
bCurrentPlayerPosition
PrimaryText
RoomIcon
MarkerIcon
ReliabilityState
bStale
```

禁止：

- 不创建 UMG 实际界面；
- 不实现完整技能；
- 不实现真实 Modifier 玩法；
- 不实现移动雷。

建议 commit：

```text
feat: add data asset and effect modifier skeleton
```

---

## 9. 第 5 轮：Query / Save / Debug / actorId 预留

### 目标

补齐未来增删改查、存档、调试、多人预留所需接口。

### 创建

```text
UGT_QueryFacade
UGT_SaveGame
UGT_DebugSubsystem
```

### 预留字段

所有 Command / Event / Effect 相关结构应预留：

```text
ActorId
PlayerId
TeamId
SourceId
TargetId
```

### SaveGame 预留

保存：

```text
SaveVersion
RunSnapshot
MetaProgress
CodexProgress
UnlockedContent
```

当前不要求完整序列化。

### Debug Stub

提供：

- 打印 RunContext 摘要；
- 打印 TruthMap / IntelMap 摘要；
- 打印 Active Modifier 摘要；
- 打印最近 Event 摘要。

建议 commit：

```text
feat: add query save debug and actor identity stubs
```

---

## 10. 第 6 轮：编译修正与结构自检

### 目标

本轮不新增大系统，专门用于修正和自检。

### 任务

1. 尝试生成 UE 工程文件；
2. 尝试编译；
3. 修正头文件、宏、模块依赖；
4. 检查 `.gitignore`；
5. 检查目录结构；
6. 检查文档与代码命名是否一致；
7. 添加必要 TODO；
8. 输出架构自检报告。

### 自检报告必须回答

1. TruthMap 与 IntelMap 是否分离？
2. UI 是否仍未直接依赖 TruthMap？
3. DataAsset 类型是否可创建？
4. Effect / Modifier 是否只是通道，没有硬编码未来玩法？
5. actorId 是否已预留？
6. 是否没有破坏 Lua / UrhoX 原型？
7. 是否没有提交 UE 生成目录？
8. 后续新增战斗系统时是否有 Encounter / Skill / Effect 接口？
9. 后续新增欺骗数字时是否有 ReliabilityState / DistortIntel 接口？
10. 后续新增移动雷时是否有 MoveMine Effect 预留？

建议 commit：

```text
chore: fix Unreal skeleton compile and architecture checks
```

---

## 11. 可选第 7 轮：最小初始化验证

仅在前 6 轮稳定后执行。

目标：

1. StartRun 创建 10×10 TruthMap；
2. 创建对应 IntelMap；
3. 输出日志；
4. 可在 DebugSubsystem 中查看摘要；
5. 不要求 UMG；
6. 不要求玩家移动。

建议 commit：

```text
test: add minimal run initialization smoke test
```

---

## 12. 当前阶段最终验收

当前工程重构阶段完成标准：

1. `UE/Graytail/` 独立存在；
2. 原 Lua / UrhoX 原型未被破坏；
3. UE C++ 模块可生成 / 可编译；
4. RunContext / Command / Event / Effect / Registry 骨架存在；
5. TruthMap / IntelMap 分离；
6. DataAsset 类型存在；
7. ViewModel 类型存在；
8. Save / Debug / actorId 预留存在；
9. `.gitignore` 正确；
10. 每轮均 commit + push；
11. 文档与代码命名基本一致；
12. 未来新增玩法有明确插槽。
