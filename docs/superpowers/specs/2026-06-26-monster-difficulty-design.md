# 怪物难度改善 — 史莱姆分裂 / 无人机脱困 / 怪物房逃跑确认

> 2026-06-26 brainstorm 拍板。三项独立改动,提升史莱姆与无人机的战斗深度,并给"怪物房逃跑"加门槛+代价。
> 现状:战斗内核严格 1v1(`FGT_CombatRuntimeState` 单 `EnemyHp/EnemyType`),RoomView 表现层全套单怪状态;
> 战斗中走出门=换格=判定"逃跑",内核直接放行、零惩罚、门没锁。

## 目标
- 🟢 **史莱姆**:绿色母体,死亡裂成 **2 只**更小更快更弱、半随机乱窜的子史莱姆(并发同屏)。
- 🤖 **无人机**:被逼墙角受击后**极快冲刺脱困**,不再贴墙挨砍。
- 🚪 **怪物房逃跑**:战斗中不能直接走门跑路;必须门口 **F → 确认条(不暂停)→ 再 F** 确认,逃跑**掉钱 + 随机掉回收物**。

---

## A. 史莱姆:绿色母体 + 死亡分裂

### 行为
- **母体(现 `EGT_MonsterType::Slime`)**:近战 `ChasePlayer` + `MeleeCircle` 行为**不变**;改绿色;**死亡时不结束战斗,原地裂成 2 只子史莱姆**。
- **子体(新怪种 `EGT_MonsterType::Slimeling`,UENUM 末尾追加)**:
  - 并发 **2 只**同屏,各自**独立血量**。
  - 数值:`HpBase` 更低、`MoveSpeed` 更快、`DamageMult` 更低(均低于母体)。
  - 移动:**新移动模式 `EGT_MonsterMovePattern::RandomRoam`(UENUM 末尾追加)** —— 高速半随机乱窜(大体随机方向 + **轻微朝玩家偏置**,免得飘进墙角变无害),撞墙(clamp)反弹。
  - 攻击:复用 `MeleeCircle` 相位机,**间歇触发**(攻击计时到点→在当前位置起预警圈→判定);不直线逼近。
  - **子体不再分裂**(代数上限 1)→ 全程**最多 2 只同屏**。

### 绿色(连死亡动画)— ⚠ 改用新美术,tint 在洋红占位上变不绿
- 实测占位 `enemy_slime` / `slime_shatter` 是**洋红**底图;绿 tint 是乘法,洋红绿通道≈0,×绿 = muddy,**做不出绿**。→ "绿色"必须靠**新绿色贴图**,不能 tint。
- 代码侧:`Slime` / `Slimeling` 的 `TintColor` 保持 **White**(让贴图自身的绿显出来);受击闪白照旧从贴图自然色提亮。绿色随美术导入到位(本机 import 流程),落地前史莱姆暂显洋红占位、机制照常跑。
- 新美术(用户 AI 生成,规格见 `docs/monster-art-spec-slime.md`):**绿史莱姆本体** + **小史莱姆**(可选)+ **绿死亡碎裂 5 帧**(母体/子体死亡都绿)。多为**同名覆盖**(`enemy_slime` / `slime_shatter`)→ 重导即生效、零改码。
- 子体**缩小**显"小" = 渲染 scale ~0.7(纯表现,与贴图无关,先于美术生效)。

### 架构代价(本次最重一块)
- **内核** `FGT_CombatRuntimeState`:单 `EnemyHp/EnemyMaxHp/EnemyType` → **小怪列表**(`TArray`,实战 ≤2)。
  - 玩家 `MonsterHit` 命令需指定/选定目标怪;某怪 HP 归 0:带"分裂规格"(母体)→**替换成 2 子体**(子体 HP/伤害由母体 power 经 `GT_CombatRules` 派生并下调),否则移除。
  - 战斗在**列表空**时才结束(`bCombatActive=false`)。
  - 子体生成是**确定性**的(固定 2 只 + 派生数值,无随机),满足铁律。
- **表现层** `GT_RoomViewWidget`:单怪状态(`EnemyImage/EnemyNormPos/EnemyMeleePhase/血条/影子/受击闪/死亡碎裂/朝向缓存`)→ **数组**(上限 2),每只独立跑相位机 + 渲染。
  - 玩家挥砍锥命中:倾向**锥内 cleave 全部命中**(2 只史莱姆手感更爽);细节(cleave vs 最近单体)实现期定+playtest。
  - 子体在房内的初始位置/乱窜方向是表现层 juice,可用 `FRand`(不改内核状态,不违背确定性铁律)。
- ⚠ **BasicDebug 163 冒烟夹具走 `DummyEnemyHp` 独立路径,保持 1v1 不动**;多怪只在 **Standard** 战斗生效。`PickTypeForCell` 仍每格选 1 种;`Slimeling` **不进** `PickTypeForCell`(只由分裂产生)。

---

## B. 无人机:受击冲刺脱困(纯表现层)

- 检测到掉血瞬间(复用已有 `PrevEnemyHp` / 受击闪机制),触发 **~0.4s 冲刺**:
  - 方向 = (远离玩家) + (朝房间中心 / 离开最近墙角) **混合**。
  - 速度 ~**3-4×** 正常 kiting`MoveSpeed`;冲刺期覆盖正常 kiting,结束恢复。
  - 仍 clamp 在活动边界,但"朝中心"分量保证能脱离墙角。
- 仅 `Drone` 生效:加原型 flag `bDashAwayOnHit`(`FGT_MonsterArchetype`),只对 Drone 置 true;其它怪不受影响。
- 纯 `GT_RoomViewWidget` 表现层(`EnemyNormPos` 是表现层),**不动内核**,风险低。冲刺方向/参数可 `FRand` 抖动(juice)。

---

## C. 怪物房逃跑:门口 F 确认 + 掉钱 + 掉回收物

### 封堵白嫖
- **战斗激活时(`bCombatActive` 且 Combat 房)**:走进门边**不再自动过门** —— `TryCrossDoor` 在此情形下不调 `DebugMoveTo` 过门,改撞墙处理 + 提示"按 F 逃跑"。
- 唯一出路 = F 确认流程。**清房(战斗结束)后恢复自由走门**(现行为)。

### 流程(关键:不暂停)
1. 玩家站**门口**(`PlayerPos` 贴近某条有相邻格的边)按 **F** → 弹"确认逃跑"条。
2. 再按 **F** → 确认:走命令管线扣惩罚 + 过该门。
3. 移动 / 其它键 = 取消,收起确认条。
- **不暂停**:确认条是 **RoomView 内置 overlay**(参照现有 `CombatHintLabel`/`EdgeHintLabel`),**绝不抢键盘焦点** → 战斗模拟的 `if(!HasKeyboardFocus()) return;` 门控不被打断,怪继续追/打/射(确认期间有被打死风险,这正是"代价"的一部分)。
- **F 键上下文复用**:战斗房 + 门口 = 触发逃跑确认;其它情况 F 仍是搜索/开箱(`OnSearchRequested`)。"哪扇门"= `PlayerPos` 最近的有效门边。

### 惩罚(走命令管线,新命令 `FleeCombat`:扣惩罚 + 过门)
- **掉钱**:扣 `PendingGold` 的 **10%**(待结算/有风险的金币;`SafeGold` 不动)。无随机。
- **掉回收物**:**只掉最低稀有度(白 / `common`)的回收物**,更稀有的(蓝紫金红)逃跑**永不丢**:
  - 合格池 = `CarriedItems` 里 `Rarity == common`(白,最低档)**且** `Kind != Consumable` 的物品。
    - ⚠ 注意按 `Kind` 排消耗品,不是按 rarity:止血贴等消耗品的 rarity 也是 `common`,按 rarity 排会误伤(planner 核对 `emergency_bandage`/`lucky_coin` 均 `common`)。
  - 数量默认 ~**丢合格池件数的 25%**(向上取整,池非空时至少丢 1);可调。**池为空(身上没白货)= 不掉物,只掉钱**。
  - 选择在同档内**均匀**随机(都是白货,无需权重曲线)。
  - ⚠ **确定性铁律**:这是改内核状态的玩家动作 → 丢落选择用**确定性哈希 (seed, x, y, salt)**,不用 `FRand`。同房同包状态逃跑结果稳定。掉物时同步 `Parts -= 1`(对齐搜索入账/出账)。
- 命令在内核扣完后,RoomView/HUD 读快照刷新背包/金币 + 提示(可加"逃跑掉落 X 金 + N 件"飘字)。

---

## 默认 / 可调项(有异议提)
| 项 | 默认 | 可调 |
|---|---|---|
| 子体数值(HP/移速/伤害) | 母体派生下调 | playtest |
| 子体乱窜朝玩家偏置 | 轻微 | 偏置强度 |
| 挥砍命中 2 怪 | cleave 全部 | vs 最近单体 |
| 无人机冲刺 | 目标点式 ~0.5s / 位移 ≥3⁄4 对角线(~0.8) / 仅 Drone | 时长/距离 |
| 逃跑掉钱 | PendingGold 10% | % 数值 |
| 逃跑掉物 | **只掉最低稀有度(白/common)**;数量 = 白货池 25%(≥1) | 数量 |
| 掉物范围 | 排除消耗品(按 Kind) | 是否含消耗品 |

---

## 验证
- 编译 exit 0;163 冒烟 `Overall=Pass`(BasicDebug 不受影响,**多怪/逃跑只在 Standard**)。
- 无头:Standard 局打史莱姆确认分裂成 2 子体、子体乱窜+间歇攻击、全清才结束战斗;`gt.*` 验逃跑扣金/掉物。
- PIE 目检:史莱姆绿色(活体+死亡碎裂绿)、分裂围殴手感、子体乱窜、无人机被逼角受击冲刺脱困、门口 F→确认条(怪不冻继续打)→F 逃跑掉钱掉物、清房后可自由走门。

## 架构铁律对照
- **UENUM 只末尾追加**:`Slimeling`(MonsterType)、`RandomRoam`(MovePattern);加完全局搜 switch 补分支(RoomView 分派、GetArchetype)。
- **改状态走命令管线**:`FleeCombat` 新命令(扣金/掉物/过门);多怪 `MonsterHit` 选目标。
- **确定性随机**:子体派生(无随机)、逃跑掉物(确定性哈希);表现层 juice(乱窜/冲刺/子体落点)可 FRand。
- **BasicDebug 冻结**:163 夹具 `DummyEnemyHp` 1v1 路径不动。

## 留给 planner 的待定点
- 内核多怪的最小改法:`TArray<FGT_CombatEnemy>` vs 母体+sibling 特例;`MonsterHit` 如何选目标;快照 struct 如何带多怪给表现层。
- `GT_CombatRules` 里子体 HP/伤害的派生公式(下调系数)。
- 掉物只取最低稀有度 `common`(白,planner 已核 FName);确定性哈希 salt(planner 定 `FleeDropSalt=5407`)。
- 门口判定阈值;确认条 overlay 的具体挂载(RoomView vs HUD,确认不夺焦点)。
- 挥砍 cleave vs 单体的最终取舍。
