# 远程攻击怪设计 — 蝙蝠 / 无人机

> 2026-06-23 brainstorm 拍板。引入"远程 kiting"威胁类型：怪物不再贴脸，保持距离输出。
> 现有框架已有 `RangedProjectile` 攻击模式 + 红线瞄准→单发飞弹渲染，散射/激光在此扩展。

## 目标
加两个远程怪，丰富战斗（当前只有近战 slime）：
- 🦇 **蝙蝠 Bat**：移动快、散射多发偏角子弹。"甩不掉的牛皮糖"。
- 🤖 **无人机 Drone**：慢悬停、蓄力持续激光。"惹不起可躲的炮台"。

## 枚举扩展（UENUM 末尾追加 + 全局补 switch）
- `EGT_MonsterType` += `Bat`, `Drone`
- `EGT_MonsterAttackPattern` += `RangedSpread`（散射）, `RangedLaser`（激光）
- `EGT_MonsterMovePattern` += `KeepDistance`（放风筝/保持距离）

## 行为原型数值（`GT_MonsterCatalog`，默认值可调）
| | HP | 移速 | 理想距离 | 攻击间隔 | 攻击参数 |
|---|---|---|---|---|---|
| 🦇 Bat | 12（脆） | 0.28（快，slime 0.16） | ~0.40 | 1.5s | 散射 3 发 ±25° |
| 🤖 Drone | 22（肉） | 0.10（慢） | ~0.50 | 2.5s | 蓄力 0.8s → 激光持续 1.2s，每 0.3s 扣血 |

## 移动模式 KeepDistance（kiting）
表现层 RoomView 逐帧：
- 玩家距离 **< 理想** → 朝远离玩家方向移动（后撤）。
- 玩家距离 **≈ 理想**（理想 ± 缓冲）→ 停下攻击。
- 玩家距离 **> 理想 + 缓冲** → 蝙蝠靠近保持射程（快、缠人）；无人机不强追（慢、可甩）。
- `EnemyNormPos` clamp 在 [0.08, 0.92]：撞墙背靠墙继续打，不卡死往墙里钻。

## 攻击：RangedSpread（蝙蝠散射）
- 攻击循环到点，朝玩家方向 **±25°** 发 **3 发**直线飞弹（中间正对、左右各偏 25°）。
- 飞弹直线飞、碰玩家扣血（复用现有飞弹判定）。
- 渲染需 **3 个并发飞弹实例**——现有只有单个 `EnemyProjectileImage`，改为飞弹数组/小对象池。

## 攻击：RangedLaser（无人机激光）
- **蓄力相**：红线瞄准、跟随玩家旋转 0.8s（可躲，复用现有 `EnemyWarningLine` 瞄准）。
- **发射相**：锁定蓄力结束瞬间的玩家方向，发射粗光束持续 1.2s（**方向固定、不再追**，给躲的机会）。
- **伤害**：发射期间，玩家到光束直线的垂距 < 阈值 且 在射程内 → 每 0.3s 扣一次血（复用 `PlayerIFrameTimer` 无敌帧）。
- 渲染：`EnemyWarningLine` 加粗 + 实心高亮成光束。

## 渲染占位（素材后补）
- 蝙蝠/无人机暂用 `enemy_slime` 贴图，用 `SetBrushTintColor` 染色区分（蝙蝠偏紫、无人机偏蓝灰）。
- 真贴图后续 AI 生成（128×128，对齐画风），改 `SpritePath` 即可。

## 出现分配（`PickTypeForCell`）
- **三种怪等概率**（slime : bat : drone = **1:1:1**），按 `(seed,x,y)` 确定性哈希取模 3 均分。
- ⚠ **BasicDebug 零影响**：163 冒烟的怪物房走固定 `CombatDebugDummy`，`PickTypeForCell` 只在 Standard 战斗解析时调用；实现时确认调用点，BasicDebug 恒 slime。

## 难度：怪物房数量
- 提高 **Hard / Elite / Nightmare** 的 `MonsterRoomRatio`（怪物房占比）。
- 配置位置：难度 → `FGT_MapGenerationSpec` 映射处（实现时定位，当前 `MonsterRoomRatio` 用于 `GT_MapGenerator.cpp:151`）。
- 具体提升值实现时定（如 Hard/Elite/Nightmare 比基准 +50%）。

## 作弊：传送特定怪物房
- `DebugGotoRoomType`（DebugSubsystem:390）扩展：参数 `bat` / `drone` → 传送到 Combat 房 + **强制该房怪物类型**（覆盖 `PickTypeForCell`，需要一个调试覆盖钩子）。
- gt.Goto 命令支持 `gt.Goto bat` / `gt.Goto drone`。
- 作弊面板加按钮"进蝙蝠房 / 进无人机房"（对齐 PR#16 作弊面板模式）。

## 验证
- 编译 exit 0 + 163 `Pass=163/Fail=0`（BasicDebug 不受影响）。
- 无头：Standard 局确认三种怪都会分配到（哈希取模）。
- PIE 目检：kiting 手感（进退风筝）、蝙蝠散射、无人机蓄力激光+持续掉血、作弊传送 bat/drone 房、困难/地狱怪物房变多。

## 架构注意
- UENUM 只末尾追加，加完全局搜枚举补 switch（RoomView 的 MovePattern/AttackPattern 分派、PickTypeForCell）。
- 移动/攻击的"规则"（伤害）仍走命令管线（DebugMonsterHit），表现层只管渲染+相位机。
- 怪物类型扩展成本：每加 1 种 = 枚举 + 原型表一行 + PickTypeForCell 权重 + RoomView 渲染分支。
