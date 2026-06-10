# 本周分工计划（UE 迁移阶段）

> 基线：`feature/map-generation` 分支（地图生成/难度、背包金币/宝箱搜刮、战斗规则层、可玩 HUD+房间视图+原版贴图小地图、163 项冒烟测试管线均已完成并推送）。
> 建议开工前先把该分支合入 `main`，四人从同一基线各开 `feature/*` 分支。

## 分工

### 战斗系统（2 人）

现状：内核已有"战力检定一击结算"（对齐 `Combat.lua` 规则层：战力 ≥ 怪则无伤胜，不足扣差值血，怪必死，击杀得金币+战力成长）。`Combat.lua` 下半段还有完整的实时动作层参数（怪物攻击前摇/判定圈/玩家无敌帧/攻击冷却），尚未迁移。

建议两人分轨：

| 轨道 | 内容 | 主要文件 |
|---|---|---|
| 规则与数值 | 出战斗设计文档（保留检定制还是升级实时动作制）；扩展怪物种类/数值表（对齐 `Balance.monster` 并扩充）；战斗事件与奖励结算完善 | `Domains/Combat/GT_CombatRules`、`Core/GT_RunContext` 战斗段 |
| 表现与操作 | 房间视图内的战斗演出：怪物行为（移动/前摇/攻击判定）、玩家攻击操作与反馈（命中闪烁/受击无敌帧），素材用 `assets/image` 的怪物图 | `UI/GT_RoomViewWidget`、新增战斗演出件 |

内核接口已就位（进战斗房自动 StartCombat、`Attack` 命令走 CommandProcessor），表现层只发命令/读状态/听事件，不要绕过管线。

### 资产迁移 + UI（豪豪）

1. 把 `assets/` 的 PNG 正式导入为 `.uasset`（像素画设置：Filter=Nearest、关 Mipmap、压缩 UserInterface2D），替换当前"运行时读盘"的临时方案；目录规范 `Content/Graytail/{UI,Sprites,Rooms,Items}`
2. 物品表从硬编码 `GT_ItemCatalog` 迁到 DataTable / 框架预留的 `UGT_ItemDef` 资产体系
3. UI 补齐原版观感：M 键全屏扫描图（含未知格标雷）、搜索结果弹窗、左侧竖排信息面板、协议压力面板（界面先做，数值等协议系统）

### 局外系统（1 人）

把"一局"连成"多局成长"，按依赖顺序：

1. 撤离结算明细（待结算→已锁定入账、回收物估值，对齐 `RunInventory.GetExtractionReward`）
2. 失败抢救条款（保 safeGold + 最值钱一件，对齐 `GetFailureSalvageOptions/ApplyFailureSalvage`）
3. 局外金币/商店/装备雏形（`MetaProgress.lua`）
4. SaveGame 存档

主要文件：`Core/GT_RunContext` 结算段、新建 `Domains/Meta/`。

## 待认领（机动任务，谁有余力谁接）

- **协议压力系统**（`Protocol.lua`）：压力随移动/踩雷/杀怪上涨、5 级协议阈值、压满强制失败——核心循环的"倒计时紧迫感"，量不大但优先级高，建议战斗双人组里先完成规则轨的人顺手接
- **事件房真内容**（`EventSystem.lua`）：骰子/祭坛/旅商/陷阱替换当前 Debug 假选项
- 零碎 TODO：教学难度可见撤离点、特殊房唯一 RoomInstanceId、`gt.StartStd` 难度名拼错静默回退

## 协作铁律（详见根目录 CLAUDE.md）

1. 推送前 163 项冒烟测试必须全过：`UnrealEditor-Cmd -run=GT_RuntimeSmokeRunner`
2. BasicDebug 模式是冻结测试夹具，永不改其行为；新规则按 `GetMapMode()` 分流加平行路径
3. 改状态的玩家动作必须走 Command 管线；UI 只发命令、读状态、听事件
4. 随机必须确定性（seed+坐标哈希），数值逐位对齐 Lua 原型
5. 引擎统一 UE 5.7；编辑器自动改写的 `Config/*.ini`、`.uproject` 改动不要提交

## 冲突高发区

- `UI/GT_RoomViewWidget`：战斗表现轨 与 豪豪(UI) 都会改——约定：战斗演出归战斗组，布局/资产替换归豪豪，动手前打招呼
- `Core/GT_RunContext`：战斗规则轨 与 局外(结算) 共用——小步提交，先拉后改
