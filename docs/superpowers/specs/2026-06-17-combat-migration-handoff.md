# 局内战斗迁移 + 局内 bug 修复 交接(2026-06-17)

> ✅ **全部完成**。#1/#2 提交 87bd9a3; #3/#4/#5 实时战斗迁移提交 **b0c9a1c**。编译 exit 0 + 163 全过 + 无头实测三 bug 均修复。仅余 **PIE 目检实时手感** + 问豪豪 push/PR。
>
> compact 续接用。一次"紧急修复 5 个局内问题"会话: #1/#2 已修完提交; #3/#4/#5(战斗)= 真战斗模型迁移, 已实现并验证。

## 分支
- 工作分支 `fix/run-combat-fixes`(基于 origin/main, 含战斗 PR#9 + S4/S5)。
- 已提交 **87bd9a3**(#1 + #2)。combat 迁移在此分支续做, 未 push。
- main 现状: S4/S5/战斗(#9)/源图归档(#11)/integ(#10) 全已合入。docs/s6-items-design 等 S6 实现后再合。

## 5 个问题根因(Phase 1 已查清)
1. 雷房重复红特效 —— GT_RoomViewWidget 进雷房型就放红光, 没判已引爆。**已修**: 仅本次移动 HP 真降才闪(snapshot 加 PlayerHp/MaxHp + SyncToCurrentCell 同步基准)。
2. 满压直接死 —— AddProtocolPressure 压力到顶 MarkRunFailed。**已修**: 仅 BasicDebug 保留(护163); Standard 改满压后每进新房按难度扣血(简单/Tutorial=2, Standard/Veteran=3, Hard/Elite/Nightmare=5), 血尽才败。RunContext 加 CurrentDifficulty + ApplyMaxPressureRoomPenalty; CommandProcessor 在"新房探索"块里调。
3/4/5 战斗(同一根)—— **真战斗模型未迁移**。现状 = 占位 DummyCombat(StartDummyCombat InitialDummyHp=1 → 一击必杀, 无真怪 HP)+ PR#9 一套**脱节的实时动画**(GT_RoomViewWidget tick: 怪物 idle/瞄准/子弹, 但子弹命中写死"0 伤害"、从不扣玩家血)。

## Lua 真战斗模型(scripts/systems/Combat.lua, 448 行, 迁移逐位对齐)
实时动作战斗:
- 玩家: hp/maxHp=100, power=10(+monsterPowerBonus); 攻击射程 playerAttackRange=0.21, 攻击冷却 playerAttackCooldown=0.45, 受击无敌 playerInvincibleDuration=0.90。
- 敌人(MakeEnemyForCell 给 name + power∈[5,20], 周围雷数×2 加成): monsterMaxHP = monsterHpBase(18) + power; monsterDamage = max(monsterDamageMin(4), floor(power/3)); 位置 (0.35,0.45); attackRadius=0.20; 命中闪白 monsterHitFlashDuration=0.20。
- 攻击三相位(怪物循环): idle(1.10s) → warning(0.75s) → active(0.28s) → cooldown(0.55s); active 相位判定命中玩家(玩家在 attackRadius 内且无敌帧结束)→ 扣 monsterDamage + 给玩家无敌帧。
- 玩家攻击: 在 playerAttackRange 内、冷却到 → 扣怪 monsterHP(**待确认每刀伤害公式: 读 Combat.lua resolvePlayerAttack/玩家挥砍处, 可能 = Combat.power 或固定值**)。
- 怪死: monsterHP<=0 → makeReward(gold = goldMin + power%span; powerGain 击杀+1 上限+5 = monsterPowerBonus)→ 清怪、战斗结束。
- 玩家死: hp<=0 → 败北。

## 已敲定迁移方案(豪豪认可)
**在 PR#9 已有的 RoomView 实时循环上接真数据, 状态归 RunContext**:
- RunContext 加实时战斗状态(怪 HP/maxHP/damage、玩家无敌帧、相位计时)+ 方法 `PlayerAttackMonster()`(扣怪HP, 死→奖励/清怪)、`MonsterHitPlayer()`(扣玩家HP, 带无敌帧, 血尽→MarkRunFailed)。可考虑把现有 StartDummyCombat 系列升级为真 HP(InitialDummyHp 从 1 改 monsterMaxHP), 或新开实时战斗状态。
- GT_RoomViewWidget tick(现 ~835-946 行: 怪物攻击; ~912 行子弹命中"0 伤害"是要接 MonsterHitPlayer 的点; 玩家挥砍判定要接 PlayerAttackMonster)在动画时刻调 RunContext 方法。
- 血条 UI: snapshot 已加 PlayerHp/MaxHp; **补 EnemyHp/EnemyMaxHp**(snapshot 现只有占位 DummyEnemyHp)。
- 实时战斗不走回合 Command 管线(对齐 Lua 在 Combat 系统内做), 但**状态全在 RunContext**(可查/可测)。**BasicDebug 不进 Standard 战斗 → 163 不受影响**(务必守铁律: 占位 DummyCombat 路径若 163 依赖, 用 GetMapMode 分流, 别动 BasicDebug 行为)。

## 验证
- 每改一处: 关编辑器 → Build.bat(exit 0) → 163 冒烟(Overall=Pass/163/0)。
- 战斗手感无头测不了, 需 PIE 目检; 兜底演示 = 现 main 动画版。
- 自截图法见 project_ue_migration 记忆(引擎自带 FScreenshotRequest 最稳)。

## 下一步(compact 后从这开始)
1. 读 Combat.lua 玩家挥砍处确认"每刀伤害"公式 + 怪物 active 相位命中判定细节。
2. RunContext 加真战斗状态 + PlayerAttackMonster/MonsterHitPlayer(GetMapMode 分流护 163)。
3. snapshot 补 EnemyHp/EnemyMaxHp; RoomView tick 把 0 伤害命中接到 RunContext, 加玩家/怪物血条。
4. Build + 163 每步验; 提交到 fix/run-combat-fixes; 完成后问豪豪 push/PR。

## ✅ 实现记录(b0c9a1c, 已全部完成)
**分层**: 核心层(RunContext, 规则/状态, 走命令管线可测) + 表现层(RoomView, 相位/无敌帧/挥砍冷却门控)。
- **核心**: FGT_CombatRuntimeState 加 EnemyHp/EnemyMaxHp/EnemyDamage; StartDummyCombat 的 Standard
  分支按 ensureMonsterState 生成真怪(HP=18+战力, 伤害=max(4,战力/3)); AttackDummyCombat 的
  Standard 分支改"每刀削战力点血、血尽才结算"(去一击必杀 + 去反伤); 新增 MonsterHitPlayer +
  MonsterHit 命令(怪攻击扣血、致死判负 CombatDeath); ProcessAttackCommand 击杀压力只在击杀刀加。
  GT_CombatRules 加 MonsterMaxHpForPower/MonsterDamageForPower。BasicDebug 仍走 1 血 DummyHp(163 未变)。
- **表现(GT_RoomViewWidget)**: PR#9 近战/子弹命中接 DebugMonsterHit(无敌帧 0.9s 门控)+ 全屏红闪;
  F 攻击受挥砍冷却 0.45s 门控(TryConsumePlayerAttack); 怪/我头顶血条 + 怪名牌; OnCombatStateChanged
  通知 HUD 轻量刷新(RefreshPanels, 不抢焦点)。HUD OnSearch 战斗分支用 TryConsumePlayerAttack 闸门。
- **调试**: 快照 + gt.Snapshot/gt.Status 增 EnemyHp/MaxHp/EnemyName/PlayerHp; 新增 gt.MonsterHit。
- **验证**: 编译 exit 0; 163 Pass=163 Fail=0; 无头 seed 12345 战斗格"空壳巡工"28HP 三刀击杀
  (28→18→8→0 玩家无反伤)、怪每击 -4、连击 25 刀致死转 Failed + RunFailed 事件。
- **待办**: PIE 目检实时手感(相位/闪避/血条/红闪); 问豪豪是否 push fix/run-combat-fixes + 开 PR。
- **简化取舍**: 玩家攻击不做距离门控(站位无关, 防 PIE 软锁), 仅冷却多刀; 怪相位时序沿用 PR#9
  动画(非逐位对齐 Lua 的 idle/warning/active/cooldown), 因关键 bug 是 0 伤害/秒杀/无 HP, 已全修。
