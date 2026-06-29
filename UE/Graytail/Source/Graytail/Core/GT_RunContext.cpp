#include "Core/GT_RunContext.h"

#include "Core/GT_ProtocolState.h"
#include "Data/GT_GameDataSubsystem.h"
#include "Domains/Events/GT_EventRules.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Inventory/GT_LootRules.h"
#include "Domains/Map/GT_MapGenerator.h"
#include "Domains/Meta/GT_MetaCatalog.h"

namespace
{
	const FName GTCombatResult_Success(TEXT("Combat_DebugResult_Success"));
	const FName GTRunSummaryOutcome_Extracted(TEXT("Extracted"));
	// 逃跑掉物合格档: 仅最低稀有度(白)。值对齐 create_item_defs.py 的 rarity 字段。
	const FName GTRarity_Common(TEXT("common"));
	// 逃跑掉落明细来源标记。
	const FName GTItemSource_Flee(TEXT("flee"));
}

void UGT_RunContext::InitializeRun(int32 InSeed, int32 InWidth, int32 InHeight)
{
	// 老路径: 固定调试布局(供既有冒烟测试使用), 行为保持不变。
	FGT_MapGenerationSpec MapSpec;
	MapSpec.MapMode = EGT_MapMode::BasicDebug;
	MapSpec.Seed = InSeed;
	MapSpec.Width = InWidth;
	MapSpec.Height = InHeight;
	InitializeFromSpec(MapSpec);
}

void UGT_RunContext::InitializeRunStandard(int32 InSeed, EGT_Difficulty Difficulty)
{
	// 新路径: 按难度档位生成 Standard 随机地图。
	InitializeFromSpec(UGT_MapGenerator::MakeSpecForDifficulty(Difficulty, InSeed));
	CurrentDifficulty = Difficulty;   // 记录难度供满压惩罚分档(InitializeFromSpec 不知难度, 故在此设)。
}

void UGT_RunContext::InitializeFromSpec(const FGT_MapGenerationSpec& MapSpec)
{
	FGT_MapGenerationResult MapResult;
	UGT_MapGenerator::GenerateMap(MapSpec, MapResult);

	RunId = FGuid::NewGuid();
	RunState = EGT_RunState::Running;
	RunEndReason = NAME_None;
	MapMode = MapResult.MapMode;
	Seed = MapResult.Seed;
	MapWidth = MapResult.Width;
	MapHeight = MapResult.Height;
	TruthMap = MapResult.TruthMap;
	PlayerIntelMap.Initialize(MapWidth, MapHeight, FName(TEXT("Player")));
	CombatRuntimeState = FGT_CombatRuntimeState();
	RunSummary = FGT_RunSummary();
	RunInventory.Reset();
	LastSearchOutcome = FGT_SearchOutcome();
	LastEventOutcome = FGT_EventOutcome();
	LastConsumableOutcome = FGT_ConsumableOutcome();
	EventRoomStates.Reset();
	PlayerCombatState = FGT_PlayerCombatState();
	const FGT_GameDataSnapshot* GameData = GT_GameData::GetSnapshot();
	checkf(GameData, TEXT("Run context initialized without valid game data."));
	PlayerCombatState.MaxHp = GameData->Core.Player.BaseHp;
	PlayerCombatState.Hp = PlayerCombatState.MaxHp;
	PlayerCombatState.Power = GameData->Core.Player.BasePower;
	ProtocolState.Reset();
	DefeatedCombatRooms.Reset();
	CombatEnemyIdCounter = 0;
	PressureExploredCells.Reset();
	LoadoutMineDmgReduce = 0;
	bLoadoutMineImmunityAvailable = false;
	LoadoutSearchBonusPercent = 0;
	LoadoutTradeBonusPercent = 0;
	bLoadoutShowExitHint = false;
	bLoadoutMapHighlight = false;
	LoadoutMonsterFleeBonus = 0;
	bLoadoutKillPowerStack = false; KillPowerStackCap = 0; KillPowerStackAmount = 0; KillPowerStacksUsed = 0;
	bLoadoutProtocolHeal = false; ProtocolHealCap = 0; ProtocolHealAmount = 0; ProtocolHealsUsed = 0;
	bLoadoutChestBonusLoot = false; ChestBonusGrantedCells.Reset();

	PlayerActorId = FName(TEXT("Player"));

	FGT_ActorRuntimeState PlayerState;
	PlayerState.ActorId.Value = PlayerActorId;
	PlayerState.ActorDefId = FName(TEXT("PlayerDefault"));
	PlayerState.Team = EGT_ActorTeam::Player;
	PlayerState.Faction = EGT_ActorFaction::Graytail;
	// 出生点由生成器给出: BasicDebug 固定 (0,0), Standard 随机(对齐 Lua normal 模式)。
	PlayerState.X = MapResult.SpawnCoord.X;
	PlayerState.Y = MapResult.SpawnCoord.Y;
	PlayerState.bAlive = true;
	SpawnCellCoord = MapResult.SpawnCoord;   // 出生格定位(搜刮判定/小地图用), 跟随真实出生点, 非写死 (0,0)

	ActorStates.Reset();
	ActorStates.Add(PlayerState);

	MarkPlayerIntelCellExplored(PlayerState.X, PlayerState.Y);

	// Standard 模式对齐 Lua 扫雷规则: 所在格自动亮出 8 邻域雷数。
	if (MapMode == EGT_MapMode::Standard)
	{
		int32 SpawnAdjacentMines = 0;
		if (TruthMap.CountAdjacentMines8(PlayerState.X, PlayerState.Y, SpawnAdjacentMines))
		{
			PlayerIntelMap.SetScannedNumber(PlayerState.X, PlayerState.Y, SpawnAdjacentMines);
		}

		// 开局消耗品由 RunSubsystem 在 ApplyMetaLoadout(S3)按真实 loadout 灌入(占位绷带已移除)。
	}

	// 教程: 开局把撤离信标在情报图上标为可见(对齐 Lua 固定可见撤离点), 让新手一开始就知道目标在哪。
	// 大图 GetDebugMiniMapViewData 见 bVisible 即从真值填 "Exit" 图标, 自动画黄框, 无需额外渲染改动。
	if (MapSpec.bRevealExitsAtStart)
	{
		for (int32 CellY = 0; CellY < MapHeight; ++CellY)
		{
			for (int32 CellX = 0; CellX < MapWidth; ++CellX)
			{
				if (TruthMap.IsExit(CellX, CellY))
				{
					PlayerIntelMap.MarkVisible(CellX, CellY, true);
				}
			}
		}
	}
}

void UGT_RunContext::ResetRun()
{
	RunId.Invalidate();
	RunState = EGT_RunState::NotStarted;
	RunEndReason = NAME_None;
	Seed = 0;
	MapWidth = 0;
	MapHeight = 0;
	TruthMap.Reset();
	PlayerIntelMap.Reset();
	ActorStates.Reset();
	PlayerActorId = NAME_None;
	CombatRuntimeState = FGT_CombatRuntimeState();
	RunSummary = FGT_RunSummary();
	RunInventory.Reset();
	LastSearchOutcome = FGT_SearchOutcome();
	LastEventOutcome = FGT_EventOutcome();
	LastConsumableOutcome = FGT_ConsumableOutcome();
	EventRoomStates.Reset();
	PlayerCombatState = FGT_PlayerCombatState();
	ProtocolState.Reset();
	DefeatedCombatRooms.Reset();
	CombatEnemyIdCounter = 0;
	PressureExploredCells.Reset();
	LoadoutMineDmgReduce = 0;
	bLoadoutMineImmunityAvailable = false;
	LoadoutSearchBonusPercent = 0;
	LoadoutTradeBonusPercent = 0;
	bLoadoutShowExitHint = false;
	bLoadoutMapHighlight = false;
	LoadoutMonsterFleeBonus = 0;
	bLoadoutKillPowerStack = false; KillPowerStackCap = 0; KillPowerStackAmount = 0; KillPowerStacksUsed = 0;
	bLoadoutProtocolHeal = false; ProtocolHealCap = 0; ProtocolHealAmount = 0; ProtocolHealsUsed = 0;
	bLoadoutChestBonusLoot = false; ChestBonusGrantedCells.Reset();
	SpawnCellCoord = FIntPoint::ZeroValue;
	MapMode = EGT_MapMode::Unknown;
}

FGuid UGT_RunContext::GetRunId() const
{
	return RunId;
}

int32 UGT_RunContext::GetSeed() const
{
	return Seed;
}

int32 UGT_RunContext::GetMapWidth() const
{
	return MapWidth;
}

int32 UGT_RunContext::GetMapHeight() const
{
	return MapHeight;
}

const FGT_TruthMap& UGT_RunContext::GetTruthMapForDebugOnly() const
{
	return TruthMap;
}

const FGT_IntelMap& UGT_RunContext::GetPlayerIntelMap() const
{
	return PlayerIntelMap;
}

const TArray<FGT_ActorRuntimeState>& UGT_RunContext::GetActorStates() const
{
	return ActorStates;
}

FGT_ActorRuntimeState* UGT_RunContext::FindActorStateMutable(FName ActorId)
{
	if (ActorId.IsNone())
	{
		return nullptr;
	}

	return ActorStates.FindByPredicate([ActorId](const FGT_ActorRuntimeState& ActorState)
	{
		return ActorState.ActorId.ToName() == ActorId;
	});
}

const FGT_ActorRuntimeState* UGT_RunContext::FindActorState(FName ActorId) const
{
	if (ActorId.IsNone())
	{
		return nullptr;
	}

	return ActorStates.FindByPredicate([ActorId](const FGT_ActorRuntimeState& ActorState)
	{
		return ActorState.ActorId.ToName() == ActorId;
	});
}

FName UGT_RunContext::GetPlayerActorId() const
{
	return PlayerActorId;
}

bool UGT_RunContext::TryGetPlayerPosition(int32& OutX, int32& OutY) const
{
	const FGT_ActorRuntimeState* PlayerState = FindActorState(PlayerActorId);
	if (!PlayerState)
	{
		OutX = 0;
		OutY = 0;
		return false;
	}

	OutX = PlayerState->X;
	OutY = PlayerState->Y;
	return true;
}

bool UGT_RunContext::IsValidMapCoord(int32 X, int32 Y) const
{
	return TruthMap.IsValidCoord(X, Y);
}

EGT_RunState UGT_RunContext::GetRunState() const
{
	return RunState;
}

bool UGT_RunContext::IsRunActive() const
{
	return RunState == EGT_RunState::Running;
}

bool UGT_RunContext::IsRunFailed() const
{
	return RunState == EGT_RunState::Failed;
}

bool UGT_RunContext::IsRunSucceeded() const
{
	return RunState == EGT_RunState::Succeeded;
}

bool UGT_RunContext::MarkRunFailed(FName Reason)
{
	if (RunState != EGT_RunState::Running)
	{
		return false;
	}

	RunState = EGT_RunState::Failed;
	RunEndReason = Reason;
	return true;
}

bool UGT_RunContext::MarkRunSucceeded(FName Reason)
{
	if (RunState != EGT_RunState::Running)
	{
		return false;
	}

	RunState = EGT_RunState::Succeeded;
	RunEndReason = Reason;
	return true;
}

bool UGT_RunContext::MarkPlayerIntelCellExplored(int32 X, int32 Y)
{
	return PlayerIntelMap.MarkExplored(X, Y);
}

bool UGT_RunContext::MarkPlayerIntelCellVisible(int32 X, int32 Y, bool bVisible)
{
	return PlayerIntelMap.MarkVisible(X, Y, bVisible);
}

bool UGT_RunContext::CountAdjacentMines8(int32 X, int32 Y, int32& OutMineCount) const
{
	return TruthMap.CountAdjacentMines8(X, Y, OutMineCount);
}

bool UGT_RunContext::SetPlayerIntelCellScannedNumber(int32 X, int32 Y, int32 InDisplayedNumber)
{
	return PlayerIntelMap.SetScannedNumber(X, Y, InDisplayedNumber);
}

bool UGT_RunContext::GetTruthCellSnapshot(int32 X, int32 Y, FGT_TruthCell& OutCell) const
{
	OutCell = FGT_TruthCell();

	const FGT_TruthCell* TruthCell = TruthMap.GetCellConst(X, Y);
	if (!TruthCell)
	{
		return false;
	}

	OutCell = *TruthCell;
	return true;
}

bool UGT_RunContext::MarkTruthCellEntered(int32 X, int32 Y)
{
	return TruthMap.MarkCellEntered(X, Y);
}

bool UGT_RunContext::MarkTruthCellResolved(int32 X, int32 Y)
{
	return TruthMap.MarkCellResolved(X, Y);
}

bool UGT_RunContext::StartDummyCombat(
	int32 X,
	int32 Y,
	FName RoomContentId,
	FName RoomRuleId,
	int32 InitialDummyHp,
	bool& bOutStarted)
{
	bOutStarted = false;
	if (!IsRunActive() || !IsValidMapCoord(X, Y))
	{
		return false;
	}

	if (CombatRuntimeState.CombatX == X
		&& CombatRuntimeState.CombatY == Y
		&& CombatRuntimeState.bCombatResolved)
	{
		return true;
	}

	// Standard 模式: 该战斗房已打过(真怪已死)就不再重开。
	if (MapMode == EGT_MapMode::Standard && DefeatedCombatRooms.Contains(FIntPoint(X, Y)))
	{
		return true;
	}

	CombatRuntimeState.bCombatActive = true;
	CombatRuntimeState.bCombatResolved = false;
	CombatRuntimeState.DummyEnemyHp = FMath::Max(1, InitialDummyHp);
	CombatRuntimeState.CombatX = X;
	CombatRuntimeState.CombatY = Y;
	CombatRuntimeState.RoomContentId = RoomContentId;
	CombatRuntimeState.RoomRuleId = RoomRuleId;
	CombatRuntimeState.LastCombatResultId = NAME_None;
	CombatRuntimeState.bStandardEnemy = false;
	CombatRuntimeState.EnemyName.Reset();
	CombatRuntimeState.EnemyPower = 0;
	CombatRuntimeState.EnemyType = EGT_MonsterType::Slime;
	CombatRuntimeState.EnemyHp = 0;
	CombatRuntimeState.EnemyMaxHp = 0;
	CombatRuntimeState.EnemyDamage = 0;
	bOutStarted = true;

	// Standard 模式: 用确定性规则生成真怪(对齐 Combat.TrySpawnEnemy + ensureMonsterState), 替换 1 血 dummy。
	// 行为原型由 GT_MonsterCatalog 选型(决定 HP 基底/移动/攻击模式); 战力缩放沿用共享公式。
	if (MapMode == EGT_MapMode::Standard)
	{
		int32 AdjacentMineCount = 0;
		TruthMap.CountAdjacentMines8(X, Y, AdjacentMineCount);
		CombatRuntimeState.bStandardEnemy = true;
		GT_CombatRules::MakeEnemyForCell(Seed, X, Y, AdjacentMineCount, CombatRuntimeState.EnemyName, CombatRuntimeState.EnemyPower);
		if (bDebugForceMonsterType)
		{
			CombatRuntimeState.EnemyType = DebugForcedMonsterType;
			bDebugForceMonsterType = false;   // 一次性: 用完即清, 不污染其它战斗房
		}
		else
		{
			CombatRuntimeState.EnemyType = GT_MonsterCatalog::PickTypeForCell(Seed, X, Y, AdjacentMineCount);
		}
		const FGT_MonsterArchetype& Archetype = GT_MonsterCatalog::GetArchetype(CombatRuntimeState.EnemyType);
		CombatRuntimeState.EnemyMaxHp = Archetype.HpBase + FMath::Max(0, CombatRuntimeState.EnemyPower);
		CombatRuntimeState.EnemyHp = CombatRuntimeState.EnemyMaxHp;
		// 按怪种伤害系数分层(无人机最疼/史莱姆次之/蝙蝠基准)。
		CombatRuntimeState.EnemyDamage = FMath::Max(1, FMath::RoundToInt(
			GT_CombatRules::MonsterDamageForPower(CombatRuntimeState.EnemyPower) * Archetype.DamageMult));
		// DummyEnemyHp 镜像真血量, 供仍读旧字段的快照/UI 平滑过渡。
		CombatRuntimeState.DummyEnemyHp = CombatRuntimeState.EnemyHp;

		// 多怪列表: 推入 1 个母体怪。仅史莱姆母体死亡分裂(bSplitsOnDeath); 蝙蝠/无人机单怪不分裂。
		CombatRuntimeState.Enemies.Reset();
		FGT_CombatEnemy Mother;
		Mother.EnemyId = ++CombatEnemyIdCounter;
		Mother.Type = CombatRuntimeState.EnemyType;
		Mother.Hp = CombatRuntimeState.EnemyHp;
		Mother.MaxHp = CombatRuntimeState.EnemyMaxHp;
		Mother.Power = CombatRuntimeState.EnemyPower;
		Mother.Damage = CombatRuntimeState.EnemyDamage;
		Mother.Name = CombatRuntimeState.EnemyName;
		Mother.bSplitsOnDeath = (CombatRuntimeState.EnemyType == EGT_MonsterType::Slime);
		CombatRuntimeState.Enemies.Add(Mother);
	}
	return true;
}

bool UGT_RunContext::AttackDummyCombat(const TArray<int32>& HitEnemyIds, FGT_CombatRuntimeState& OutState)
{
	OutState = CombatRuntimeState;
	if (!IsRunActive() || !CombatRuntimeState.bCombatActive || CombatRuntimeState.DummyEnemyHp <= 0)
	{
		return false;
	}

	// Standard 模式: 实时战斗一次挥砍(对齐 Combat.PlayerAttackEnemy):
	// 扣怪 EnemyHp = Power, 怪未死则战斗继续; 血尽才结算(标记已击杀 + 金币 + 战力成长)。
	// 玩家反伤不在此处 —— 真伤害来自怪物实时攻击(MonsterHitPlayer)。
	if (CombatRuntimeState.bStandardEnemy)
	{
		// 命中目标集合(来自表现层按玩家朝向锥形判定)。空集合 = 旧路径/无头回退, 只扣代表怪一只。
		const int32 Damage = FMath::Max(1, PlayerCombatState.Power);
		TArray<FGT_CombatEnemy> NextEnemies;
		NextEnemies.Reserve(CombatRuntimeState.Enemies.Num() * 2);
		for (int32 Idx = 0; Idx < CombatRuntimeState.Enemies.Num(); ++Idx)
		{
			FGT_CombatEnemy& E = CombatRuntimeState.Enemies[Idx];
			const bool bHitThis = HitEnemyIds.Num() == 0
				? (Idx == 0)                            // 旧路径/无头: 只打代表怪
				: HitEnemyIds.Contains(E.EnemyId);      // 新路径: 只打朝向锥内覆盖的怪
			if (bHitThis)
			{
				E.Hp = FMath::Max(0, E.Hp - Damage);
			}
			if (E.Hp > 0)
			{
				NextEnemies.Add(E);   // 存活
			}
			else if (E.bSplitsOnDeath)
			{
				// 母体死亡: 原地裂成 2 子史莱姆(确定性派生数值, 无随机)。Power=floor(母体×0.45),
				// HP/Damage 用 Slimeling 原型派生(HpBase=9 / DamageMult=0.6)。
				const FGT_MonsterArchetype& ParentArch = GT_MonsterCatalog::GetArchetype(E.Type);
				const int32 ChildPower = FMath::FloorToInt(E.Power * ParentArch.SplitPowerMultiplier);
				const FGT_MonsterArchetype& ChildArch = GT_MonsterCatalog::GetArchetype(EGT_MonsterType::Slimeling);
				for (int32 i = 0; i < ParentArch.SplitCount; ++i)
				{
					FGT_CombatEnemy Child;
					Child.EnemyId = ++CombatEnemyIdCounter;
					Child.Type = EGT_MonsterType::Slimeling;
					Child.Power = ChildPower;
					Child.MaxHp = ChildArch.HpBase + FMath::Max(0, ChildPower);
					Child.Hp = Child.MaxHp;
					Child.Damage = FMath::Max(1, FMath::RoundToInt(GT_CombatRules::MonsterDamageForPower(ChildPower) * ChildArch.DamageMult));
					Child.Name = TEXT("小史莱姆");
					Child.bSplitsOnDeath = false;   // 子体不再分裂(代数上限 1 → 全程最多 2 只)
					NextEnemies.Add(Child);
				}
			}
			// else: 普通死亡(子体/蝙蝠/无人机), 不加 = 移除。
		}
		CombatRuntimeState.Enemies = MoveTemp(NextEnemies);

		if (CombatRuntimeState.Enemies.Num() > 0)
		{
			// 仍有存活怪: 标量镜像同步到代表怪(第一个)。EnemyPower 不覆盖 —— 保持开战母体战力做奖励基准。
			const FGT_CombatEnemy& Rep = CombatRuntimeState.Enemies[0];
			CombatRuntimeState.EnemyType = Rep.Type;
			CombatRuntimeState.EnemyHp = Rep.Hp;
			CombatRuntimeState.EnemyMaxHp = Rep.MaxHp;
			CombatRuntimeState.EnemyDamage = Rep.Damage;
			CombatRuntimeState.EnemyName = Rep.Name;
			CombatRuntimeState.DummyEnemyHp = Rep.Hp;   // 镜像供旧读者
			OutState = CombatRuntimeState;
			return true;
		}

		// 列表空才结束战斗 + 奖励一次(经济保持原版: 按开战母体 EnemyPower 给金/战力, 不按子体翻倍)。
		CombatRuntimeState.EnemyHp = 0;
		CombatRuntimeState.DummyEnemyHp = 0;
		CombatRuntimeState.bCombatActive = false;
		CombatRuntimeState.bCombatResolved = true;
		CombatRuntimeState.LastCombatResultId = GTCombatResult_Success;
		DefeatedCombatRooms.AddUnique(FIntPoint(CombatRuntimeState.CombatX, CombatRuntimeState.CombatY));

		RunInventory.AddPendingGold(GT_CombatRules::KillRewardGold(CombatRuntimeState.EnemyPower));
		const int32 PowerGain = FMath::Min(
			GT_CombatRules::GetPowerGainPerKill(),
			GT_CombatRules::GetPowerGainCap() - PlayerCombatState.MonsterPowerBonus);
		if (PowerGain > 0)
		{
			PlayerCombatState.Power += PowerGain;
			PlayerCombatState.MonsterPowerBonus += PowerGain;
		}

		// S6 异常体犬牙: 战斗胜利本局攻击叠加(≤Cap 层, 每层 +Amount)。
		if (bLoadoutKillPowerStack && KillPowerStacksUsed < KillPowerStackCap)
		{
			PlayerCombatState.Power += KillPowerStackAmount;
			++KillPowerStacksUsed;
		}

		OutState = CombatRuntimeState;
		return true;
	}

	CombatRuntimeState.DummyEnemyHp = FMath::Max(0, CombatRuntimeState.DummyEnemyHp - 1);
	if (CombatRuntimeState.DummyEnemyHp == 0)
	{
		CombatRuntimeState.bCombatActive = false;
		CombatRuntimeState.bCombatResolved = true;
		CombatRuntimeState.LastCombatResultId = GTCombatResult_Success;
	}

	OutState = CombatRuntimeState;
	return true;
}

int32 UGT_RunContext::ApplyPlayerDamage(int32 RawDamage)
{
	// 调试无敌: 危险伤害一律归零(踩雷/怪物/协议满压/机关失败都走这里)。
	if (bCheatGodMode)
	{
		return 0;
	}
	return PlayerCombatState.ApplyDamage(RawDamage);
}

void UGT_RunContext::CheatSetPlayerHp(int32 NewHp)
{
	PlayerCombatState.Hp = FMath::Clamp(NewHp, 1, PlayerCombatState.MaxHp);
}

bool UGT_RunContext::MonsterHitPlayer(int32& OutDamage, bool& bOutDead)
{
	OutDamage = 0;
	bOutDead = false;

	// 仅 Standard 实时战斗、战斗激活、怪还活着时生效(无敌帧由表现层门控)。
	if (!IsRunActive()
		|| MapMode != EGT_MapMode::Standard
		|| !CombatRuntimeState.bStandardEnemy
		|| !CombatRuntimeState.bCombatActive
		|| CombatRuntimeState.Enemies.Num() == 0)
	{
		return false;
	}

	const int32 Damage = FMath::Max(0, CombatRuntimeState.EnemyDamage);
	OutDamage = ApplyPlayerDamage(Damage);
	bOutDead = !PlayerCombatState.IsAlive();
	return true;
}

bool UGT_RunContext::FleeFromCombat(int32& OutGoldDropped, TArray<FGT_ItemStack>& OutDroppedItems)
{
	OutGoldDropped = 0;
	OutDroppedItems.Reset();

	// 守卫: 仅 Standard 模式、战斗激活时可逃跑(BasicDebug 163 夹具走 DummyEnemyHp, 不受影响)。
	if (MapMode != EGT_MapMode::Standard || !CombatRuntimeState.bCombatActive)
	{
		return false;
	}

	const int32 GoldDrop = RunInventory.PendingGold * GT_LootRules::GetFleeGoldDropPercent() / 100;
	if (GoldDrop > 0)
	{
		RunInventory.SpendPendingGold(GoldDrop);
	}
	OutGoldDropped = GoldDrop;

	// 掉物(确定性哈希, 不用 FRand): 只掉最低稀有度(common 白)且非消耗品的回收物。
	// 合格池按 Count 展开成"件"实例; 同档均匀随机, 无权重曲线。池空 → 不掉物只掉钱。
	int32 PlayerX = 0;
	int32 PlayerY = 0;
	TryGetPlayerPosition(PlayerX, PlayerY);

	TArray<FName> EligiblePool;
	for (const FGT_ItemStack& Stack : RunInventory.CarriedItems)
	{
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
		if (!Def || Def->Rarity != GTRarity_Common)
		{
			continue;   // 只掉白货(更稀有的蓝紫金红逃跑永不丢)
		}
		if (Def->Kind == EGT_ItemKind::Consumable)
		{
			continue;   // 排消耗品(止血贴 rarity 也是 common, 只能按 Kind 排)
		}
		for (int32 i = 0; i < Stack.Count; ++i)
		{
			EligiblePool.Add(Stack.ItemId);
		}
	}

	if (EligiblePool.Num() > 0)
	{
		const int32 DropCount = FMath::DivideAndRoundUp(
			EligiblePool.Num() * GT_LootRules::GetFleeItemDropPercent(),
			100);
		for (int32 i = 0; i < DropCount && EligiblePool.Num() > 0; ++i)
		{
			const int32 Index = GT_LootRules::RollRange(Seed, PlayerX, PlayerY,
				GT_LootRules::GetFleeDropSalt() + i, 0, EligiblePool.Num() - 1);
			const FName DroppedId = EligiblePool[Index];

			RunInventory.RemoveCarriedItem(DroppedId, 1);
			RunInventory.Parts = FMath::Max(0, RunInventory.Parts - 1);   // 同步零件总数(对齐搜索入账/出账)
			EligiblePool.RemoveAt(Index);

			// 记入掉落明细(同 id 合并成堆)。
			if (FGT_ItemStack* Found = OutDroppedItems.FindByPredicate(
				[DroppedId](const FGT_ItemStack& S) { return S.ItemId == DroppedId; }))
			{
				++Found->Count;
			}
			else
			{
				FGT_ItemStack& NewStack = OutDroppedItems.AddDefaulted_GetRef();
				NewStack.ItemId = DroppedId;
				NewStack.Count = 1;
				NewStack.Source = GTItemSource_Flee;
			}
		}
	}

	// 结束战斗(逃跑≠击杀): 仅清战斗激活标记。不置 bCombatResolved、不进 DefeatedCombatRooms → 可重刷。
	// 单怪字段(EnemyHp 等)保留无害: 重进该房 StartDummyCombat 会整体重置重生。
	CombatRuntimeState.bCombatActive = false;
	CombatRuntimeState.Enemies.Reset();
	return true;
}

bool UGT_RunContext::ResolveDummyCombat(FName ResultId, FGT_CombatRuntimeState& OutState)
{
	OutState = CombatRuntimeState;
	if (!IsRunActive())
	{
		return false;
	}

	if (!CombatRuntimeState.bCombatActive && !CombatRuntimeState.bCombatResolved)
	{
		return false;
	}

	CombatRuntimeState.bCombatActive = false;
	CombatRuntimeState.bCombatResolved = true;
	CombatRuntimeState.LastCombatResultId = ResultId;
	if (ResultId == GTCombatResult_Success)
	{
		CombatRuntimeState.DummyEnemyHp = 0;
	}

	OutState = CombatRuntimeState;
	return true;
}

bool UGT_RunContext::GetCombatStateSnapshot(FGT_CombatRuntimeState& OutState) const
{
	OutState = CombatRuntimeState;
	return CombatRuntimeState.bCombatActive
		|| CombatRuntimeState.bCombatResolved
		|| CombatRuntimeState.CombatX != INDEX_NONE
		|| CombatRuntimeState.CombatY != INDEX_NONE;
}

bool UGT_RunContext::GenerateExtractSummary(int32 TotalEventCount)
{
	if (!IsRunSucceeded())
	{
		return false;
	}

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	if (!TryGetPlayerPosition(PlayerX, PlayerY))
	{
		return false;
	}

	RunSummary = FGT_RunSummary();
	RunSummary.bSummaryAvailable = true;
	RunSummary.Outcome = GTRunSummaryOutcome_Extracted;
	RunSummary.bExtracted = true;
	RunSummary.FinalPlayerX = PlayerX;
	RunSummary.FinalPlayerY = PlayerY;
	RunSummary.TotalEventCount = TotalEventCount;
	RunSummary.Seed = Seed;
	RunSummary.MapWidth = MapWidth;
	RunSummary.MapHeight = MapHeight;
	RunSummary.bCombatActive = CombatRuntimeState.bCombatActive;
	RunSummary.bCombatResolved = CombatRuntimeState.bCombatResolved;
	RunSummary.LastCombatResultId = CombatRuntimeState.LastCombatResultId;
	return true;
}

bool UGT_RunContext::GetRunSummarySnapshot(FGT_RunSummary& OutSummary) const
{
	OutSummary = RunSummary;
	return RunSummary.bSummaryAvailable;
}

const FGT_RunInventoryState& UGT_RunContext::GetRunInventory() const
{
	return RunInventory;
}

const FGT_PlayerCombatState& UGT_RunContext::GetPlayerCombatState() const
{
	return PlayerCombatState;
}

EGT_MapMode UGT_RunContext::GetMapMode() const
{
	return MapMode;
}

bool UGT_RunContext::IsPlayerAlive() const
{
	return PlayerCombatState.IsAlive();
}

void UGT_RunContext::ApplyMineHitToPlayer(int32& OutDamage, bool& bOutDead)
{
	// 急救包: 首次踩雷免疫(一次性, 伤害 0 绕过下限)。
	if (bLoadoutMineImmunityAvailable)
	{
		bLoadoutMineImmunityAvailable = false;
		OutDamage = 0;
		bOutDead = !PlayerCombatState.IsAlive();
		return;
	}
	// 对齐 Combat.TakeMineHit: 雷伤 30, 装备(绝缘套)/天赋(厚皮)减免后最低 5。
	OutDamage = FMath::Max(
		GT_CombatRules::GetMineDamageFloor(),
		GT_CombatRules::GetMineDamage() - LoadoutMineDmgReduce);
	OutDamage = ApplyPlayerDamage(OutDamage);
	bOutDead = !PlayerCombatState.IsAlive();
}

void UGT_RunContext::ApplyMetaLoadout(const FGT_EquipBonus& Equip, const FGT_TalentEffects& Talents, const TMap<FName, int32>& Consumables, const TArray<FName>& EquippedItemIds)
{
	// 属性加成(开局直接设, 开局满血)。
	PlayerCombatState.MaxHp += Equip.BonusHP;
	PlayerCombatState.Hp = PlayerCombatState.MaxHp;
	PlayerCombatState.Power += Equip.BonusPower;

	// 规则层加成(雷伤减免 = 装备 + 天赋叠加; 首次免雷; 搜索奖励 %; 议价 → 旅商收购价 +%)。
	LoadoutMineDmgReduce = Equip.MineDmgReduce + Talents.MineDmgReduce;
	bLoadoutMineImmunityAvailable = Equip.bMineImmunity;
	LoadoutSearchBonusPercent = Equip.SearchBonus;
	LoadoutTradeBonusPercent = Talents.TradePrice;   // 议价天赋: 0(无)/ 20(已解锁) = 收购价加成百分比
	bLoadoutShowExitHint = Equip.bShowExitHint;          // 罗盘: 开局播报撤离信标方向
	bLoadoutMapHighlight = Talents.bMapHighlight;        // 邻域感知: 进房高亮 8 邻域
	LoadoutMonsterFleeBonus = Talents.MonsterFleeBonus;  // 怪物避让: 战斗开始犹豫窗口秒数

	// 带入消耗品(替换原写死占位, 开局强制入包不检查容量)。
	for (const TPair<FName, int32>& Pair : Consumables)
	{
		if (!Pair.Key.IsNone() && Pair.Value > 0)
		{
			RunInventory.ForceAddCarriedItem(Pair.Key, Pair.Value, FName(TEXT("loadout")));
		}
	}

	// S6: 已装备的触发型装备 -> 激活本局触发态(SettleGoldBonus 在 Meta 侧算, 不需 RunContext 态)。
	for (const FName& EquipId : EquippedItemIds)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(EquipId);
		if (!Def) { continue; }
		switch (Def->Trigger)
		{
		case EGT_ItemTrigger::KillPowerStack:
			bLoadoutKillPowerStack = true; KillPowerStackCap = Def->TriggerCap; KillPowerStackAmount = Def->TriggerAmount; break;
		case EGT_ItemTrigger::ProtocolHeal:
			bLoadoutProtocolHeal = true; ProtocolHealCap = Def->TriggerCap; ProtocolHealAmount = Def->TriggerAmount; break;
		case EGT_ItemTrigger::ChestBonusLoot:
			bLoadoutChestBonusLoot = true; break;
		default: break;
		}
	}
}

TArray<FIntPoint> UGT_RunContext::GetExitCells() const
{
	TArray<FIntPoint> Exits;
	for (int32 Y = 0; Y < MapHeight; ++Y)
	{
		for (int32 X = 0; X < MapWidth; ++X)
		{
			if (TruthMap.IsExit(X, Y))
			{
				Exits.Add(FIntPoint(X, Y));
			}
		}
	}
	return Exits;
}

void UGT_RunContext::SetDebugForcedMonsterType(EGT_MonsterType Type, bool bEnable)
{
	DebugForcedMonsterType = Type;
	bDebugForceMonsterType = bEnable;
}

void UGT_RunContext::DebugClearDefeatedCombatRoom(int32 X, int32 Y)
{
	DefeatedCombatRooms.Remove(FIntPoint(X, Y));
}

bool UGT_RunContext::TryGrantChestMagnetLoot(int32 X, int32 Y)
{
	if (!bLoadoutChestBonusLoot || !IsRunActive())
	{
		return false;
	}

	FGT_TruthCell Cell;
	if (!GetTruthCellSnapshot(X, Y, Cell) || Cell.RoomBaseType != EGT_RoomBaseType::Chest)
	{
		return false;
	}

	const FIntPoint Coord(X, Y);
	if (ChestBonusGrantedCells.Contains(Coord))
	{
		return false;   // 每格只发一次
	}
	ChestBonusGrantedCells.Add(Coord);

	// 额外掉 1 件低价值回收物(取低档品质物品, 确定性)。
	const FName ItemId = GT_ItemCatalog::GetQualityItemId(EGT_ItemQuality::Low);
	if (ItemId.IsNone())
	{
		return false;
	}
	return RunInventory.AddCarriedItem(ItemId, 1, FName(TEXT("recovered")));
}

bool UGT_RunContext::MarkExploredForPressure(int32 X, int32 Y)
{
	const FIntPoint Coord(X, Y);
	if (PressureExploredCells.Contains(Coord))
	{
		return false;
	}
	PressureExploredCells.Add(Coord);
	return true;
}

bool UGT_RunContext::IsExploredForPressure(int32 X, int32 Y) const
{
	return PressureExploredCells.Contains(FIntPoint(X, Y));
}

UGT_RunContext::FProtocolPressureResult UGT_RunContext::AddProtocolPressure(int32 Amount)
{
	FProtocolPressureResult Result;

	if (!IsRunActive())
	{
		Result.Level = ProtocolState.Level;
		Result.Pressure = ProtocolState.Pressure;
		return Result;
	}

	Amount = FMath::Max(0, Amount);
	ProtocolState.LastDelta = Amount;
	ProtocolState.Pressure = FMath::Clamp(ProtocolState.Pressure + Amount, 0, ProtocolState.MaxPressure);

	const int32 PrevLevel = ProtocolState.Level;
	ProtocolState.Level = GT_ProtocolRules::ComputeLevel(ProtocolState.Pressure);
	ProtocolState.bLevelChanged = ProtocolState.Level != PrevLevel;

	// S6 封锁区结晶: 协议每升一级(等级下降, 压力跨阈值)回血, ≤Cap 次。
	if (bLoadoutProtocolHeal && ProtocolState.Level < PrevLevel && ProtocolHealsUsed < ProtocolHealCap)
	{
		if (PlayerCombatState.Heal(ProtocolHealAmount) > 0)
		{
			++ProtocolHealsUsed;
		}
	}

	Result.Level = ProtocolState.Level;
	Result.Pressure = ProtocolState.Pressure;
	Result.bLevelChanged = ProtocolState.bLevelChanged;

	// 满压处理:
	//  - BasicDebug 测试夹具: 保持"满压即信号中断败北"老行为(护 163, 永不改)。
	//  - Standard: 不再直接败北, 改由"满压后每进新房按难度扣血"惩罚(见 ApplyMaxPressureRoomPenalty)。
	if (ProtocolState.Pressure >= ProtocolState.MaxPressure && MapMode == EGT_MapMode::BasicDebug)
	{
		Result.bForcedFail = MarkRunFailed(FName(TEXT("Protocol")));
	}

	return Result;
}

namespace
{
	// 满压后每进一个新房的扣血(难度分档): 简单档 2 / 中档 3 / 困难档 5。
	int32 MaxPressureRoomDamageForDifficulty(EGT_Difficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EGT_Difficulty::Tutorial:
		case EGT_Difficulty::Easy:      return 2;
		case EGT_Difficulty::Standard:
		case EGT_Difficulty::Veteran:   return 3;
		case EGT_Difficulty::Hard:
		case EGT_Difficulty::Elite:
		case EGT_Difficulty::Nightmare: return 5;
		default:                        return 3;
		}
	}
}

bool UGT_RunContext::ApplyMaxPressureRoomPenalty(int32& OutDamage, bool& bOutDead)
{
	OutDamage = 0;
	bOutDead = false;
	// 仅 Standard、运行中、且压力已满时生效(BasicDebug 由 AddProtocolPressure 的 insta-fail 处理)。
	if (MapMode != EGT_MapMode::Standard || !IsRunActive())
	{
		return false;
	}
	if (ProtocolState.Pressure < ProtocolState.MaxPressure)
	{
		return false;
	}
	OutDamage = ApplyPlayerDamage(MaxPressureRoomDamageForDifficulty(CurrentDifficulty));
	bOutDead = !PlayerCombatState.IsAlive();
	return OutDamage > 0;
}

const FGT_ProtocolState& UGT_RunContext::GetProtocolState() const
{
	return ProtocolState;
}

int32 UGT_RunContext::GetProtocolLevel() const
{
	return ProtocolState.Level;
}

int32 UGT_RunContext::GetProtocolPressure() const
{
	return ProtocolState.Pressure;
}

int32 UGT_RunContext::GetProtocolMaxPressure() const
{
	return ProtocolState.MaxPressure;
}

float UGT_RunContext::GetProtocolPressureRatio() const
{
	return ProtocolState.GetPressureRatio();
}

bool UGT_RunContext::EvaluateSearchAtPlayer(FName& OutReason, bool& bOutIsChest) const
{
	OutReason = NAME_None;
	bOutIsChest = false;

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	if (!IsRunActive() || !TryGetPlayerPosition(PlayerX, PlayerY))
	{
		OutReason = FName(TEXT("not_ready"));
		return false;
	}

	const FGT_TruthCell* Cell = TruthMap.GetCellConst(PlayerX, PlayerY);
	if (!Cell)
	{
		OutReason = FName(TEXT("not_ready"));
		return false;
	}

	// 检查顺序对齐 Lua GetSearchState: 雷格 -> 出生点 -> 撤离点 -> 怪物房 -> 事件房 -> 已搜过。
	if (Cell->bHasMine)
	{
		OutReason = FName(TEXT("unsafe"));
		return false;
	}

	// 出生格不可搜刮(纯空安全格, 生成器已排除其内容)。出生点 BasicDebug 固定 (0,0)、Standard 随机, 用真实出生格坐标判定。
	if (PlayerX == SpawnCellCoord.X && PlayerY == SpawnCellCoord.Y)
	{
		OutReason = FName(TEXT("spawn"));
		return false;
	}

	if (Cell->bIsExit)
	{
		OutReason = FName(TEXT("exit"));
		return false;
	}

	if (Cell->RoomBaseType == EGT_RoomBaseType::Combat)
	{
		OutReason = FName(TEXT("monster"));
		return false;
	}

	if (Cell->RoomBaseType == EGT_RoomBaseType::Event)
	{
		OutReason = FName(TEXT("event"));
		return false;
	}

	if (RunInventory.IsRoomSearched(PlayerX, PlayerY))
	{
		OutReason = FName(TEXT("searched"));
		return false;
	}

	// 普通房和宝箱房均可搜一次; 掉落概率/品质见 GT_LootRules(普通房 65% 掉 1 件, 宝箱 1-2 件)。
	bOutIsChest = Cell->RoomBaseType == EGT_RoomBaseType::Chest;
	return true;
}

bool UGT_RunContext::SearchCurrentRoom(FGT_SearchOutcome& OutOutcome)
{
	OutOutcome = FGT_SearchOutcome();

	FName Reason = NAME_None;
	bool bIsChest = false;
	if (!EvaluateSearchAtPlayer(Reason, bIsChest))
	{
		OutOutcome.Status = Reason;
		return false;
	}

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	TryGetPlayerPosition(PlayerX, PlayerY);

	int32 AdjacentMineCount = 0;
	TruthMap.CountAdjacentMines8(PlayerX, PlayerY, AdjacentMineCount);

	// 入账顺序对齐 Lua SearchCurrentRoom: 标记已搜 -> 待结算金币 -> parts -> 物品堆叠。
	const FGT_SearchReward Reward = GT_LootRules::ComputeSearchReward(Seed, PlayerX, PlayerY, AdjacentMineCount, bIsChest);
	RunInventory.MarkRoomSearched(PlayerX, PlayerY);
	// 大背包搜索奖励 +SearchBonus%(无背包 = 0% 不变, 对齐 Lua RunInventory.searchBonus)。
	RunInventory.AddPendingGold((Reward.Gold * (100 + LoadoutSearchBonusPercent)) / 100);
	RunInventory.Parts += Reward.Parts;
	// 回收物全部入包(已无背包容量限制)。
	for (const FGT_ItemStack& Stack : Reward.Items)
	{
		RunInventory.AddCarriedItem(Stack.ItemId, Stack.Count, Stack.Source);
	}
	OutOutcome.bSearched = true;
	OutOutcome.Status = FName(TEXT("searched"));
	OutOutcome.Reward = Reward;
	LastSearchOutcome = OutOutcome;
	return true;
}

bool UGT_RunContext::UseConsumableAtPlayer(FName ItemId, FGT_ConsumableOutcome& OutOutcome)
{
	OutOutcome = FGT_ConsumableOutcome();
	OutOutcome.ItemId = ItemId;

	if (!IsRunActive())
	{
		OutOutcome.Status = FName(TEXT("not_ready"));
		LastConsumableOutcome = OutOutcome;
		return false;
	}

	// 必须是消耗品类(对齐 Lua: def.type ~= "consumable" 拒绝)。
	const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(ItemId);
	if (!Def || Def->Kind != EGT_ItemKind::Consumable)
	{
		OutOutcome.Status = FName(TEXT("not_consumable"));
		LastConsumableOutcome = OutOutcome;
		return false;
	}

	if (RunInventory.GetItemCount(ItemId) <= 0)
	{
		OutOutcome.Status = FName(TEXT("not_enough"));
		LastConsumableOutcome = OutOutcome;
		return false;
	}

	// 应急止血贴: 回血 min(25, MaxHp-Hp), 满血拒绝(对齐 Lua RunInventory.UseConsumable)。
	if (ItemId == FName(TEXT("emergency_bandage")))
	{
		if (PlayerCombatState.Hp >= PlayerCombatState.MaxHp)
		{
			OutOutcome.Status = FName(TEXT("hp_full"));
			LastConsumableOutcome = OutOutcome;
			return false;
		}

		const FGT_ConsumableDef* Consumable = GT_MetaCatalog::FindConsumable(ItemId);
		check(Consumable);
		const int32 HealTarget = FMath::Min(Consumable->Heal, PlayerCombatState.MaxHp - PlayerCombatState.Hp);
		OutOutcome.HealAmount = PlayerCombatState.Heal(HealTarget);
		RunInventory.RemoveCarriedItem(ItemId, 1);
		OutOutcome.bUsed = true;
		OutOutcome.Status = FName(TEXT("used"));
		OutOutcome.RemainingCount = RunInventory.GetItemCount(ItemId);
		LastConsumableOutcome = OutOutcome;
		return true;
	}

	if (ItemId == FName(TEXT("lucky_coin")))
	{
		const FGT_LuckyCoinBalanceConfig& LuckyCoin = GT_EventRules::GetLuckyCoin();
		int32 PX = 0;
		int32 PY = 0;
		TryGetPlayerPosition(PX, PY);
		const uint32 Hash = static_cast<uint32>(Seed * 1103515245 + PX * 928371 + PY * 364479 + 7919);
		const bool bGold = static_cast<int32>(Hash % 100u) < LuckyCoin.GoldChancePercent;

		bool bRevealed = false;
		if (!bGold)
		{
			const int32 DX[4] = { 1, -1, 0, 0 };
			const int32 DY[4] = { 0, 0, 1, -1 };
			int32 RevealRemaining = LuckyCoin.RevealCount;
			for (int32 Dir = 0; Dir < 4 && RevealRemaining > 0; ++Dir)
			{
				const int32 NX = PX + DX[Dir];
				const int32 NY = PY + DY[Dir];
				const FGT_IntelCell* Cell = PlayerIntelMap.GetCellConst(NX, NY);
				if (IsValidMapCoord(NX, NY) && Cell && !Cell->bExplored)
				{
					// 揭示该相邻未知房(标已探索 + 亮雷数), 不加协议压力(免压)。
					MarkPlayerIntelCellExplored(NX, NY);
					int32 Adj = 0;
					if (TruthMap.CountAdjacentMines8(NX, NY, Adj))
					{
						SetPlayerIntelCellScannedNumber(NX, NY, Adj);
					}
					bRevealed = true;
					--RevealRemaining;
				}
			}
		}

		if (bGold || !bRevealed)   // 选金, 或无未知相邻格可揭示时给金兜底
		{
			RunInventory.AddSafeGold(LuckyCoin.SafeGoldReward);
			OutOutcome.Status = FName(TEXT("lucky_gold"));
		}
		else
		{
			OutOutcome.Status = FName(TEXT("lucky_reveal"));
		}

		RunInventory.RemoveCarriedItem(ItemId, 1);
		OutOutcome.bUsed = true;
		OutOutcome.RemainingCount = RunInventory.GetItemCount(ItemId);
		LastConsumableOutcome = OutOutcome;
		return true;
	}

	OutOutcome.Status = FName(TEXT("not_implemented"));
	LastConsumableOutcome = OutOutcome;
	return false;
}

namespace
{
	const FName GTEventOption_Leave(TEXT("leave"));
	const FName GTEventOption_BetSmall(TEXT("bet_small"));
	const FName GTEventOption_OfferHp(TEXT("offer_hp"));
	const FName GTEventOption_Disarm(TEXT("disarm"));
	const FName GTEventOption_NoItem(TEXT("no_item"));
	const FString GTEventSellPrefix(TEXT("sell:"));
	const FName GTEventItemSource(TEXT("event"));

	// 组一个事件选项视图(对齐 Lua EventSystem.option)。
	FGT_EventOptionView MakeEventOption(
		FName OptionId,
		const FString& Label,
		const FString& Description,
		const FString& Cost,
		const FString& Reward,
		const FString& Risk,
		bool bEnabled,
		const FString& DisabledReason)
	{
		FGT_EventOptionView Option;
		Option.OptionId = OptionId;
		Option.Label = Label;
		Option.Description = Description;
		Option.CostText = Cost;
		Option.RewardText = Reward;
		Option.RiskText = Risk;
		Option.bEnabled = bEnabled;
		Option.DisabledReason = DisabledReason;
		return Option;
	}
}

const FGT_EventRoomState* UGT_RunContext::FindEventRoomState(int32 X, int32 Y) const
{
	return EventRoomStates.FindByPredicate([X, Y](const FGT_EventRoomState& State)
	{
		return State.X == X && State.Y == Y;
	});
}

FGT_EventRoomState& UGT_RunContext::FindOrAddEventRoomState(int32 X, int32 Y)
{
	FGT_EventRoomState* Found = EventRoomStates.FindByPredicate([X, Y](const FGT_EventRoomState& State)
	{
		return State.X == X && State.Y == Y;
	});
	if (Found)
	{
		return *Found;
	}

	FGT_EventRoomState& NewState = EventRoomStates.AddDefaulted_GetRef();
	NewState.X = X;
	NewState.Y = Y;
	return NewState;
}

bool UGT_RunContext::GetEventMenuAtPlayer(FGT_EventMenuView& OutMenu) const
{
	OutMenu = FGT_EventMenuView();

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	if (!IsRunActive() || MapMode != EGT_MapMode::Standard || !TryGetPlayerPosition(PlayerX, PlayerY))
	{
		return false;
	}

	const FGT_TruthCell* Cell = TruthMap.GetCellConst(PlayerX, PlayerY);
	if (!Cell || Cell->RoomBaseType != EGT_RoomBaseType::Event)
	{
		return false;
	}

	const EGT_EventKind Kind = GT_EventRules::GetEventKindAt(Seed, PlayerX, PlayerY);
	const FGT_EventRoomState* State = FindEventRoomState(PlayerX, PlayerY);
	const bool bCompleted = State && State->bCompleted;
	const int32 AltarStep = State ? State->AltarStep : 0;

	OutMenu.bAvailable = true;
	OutMenu.Kind = Kind;
	OutMenu.bCompleted = bCompleted;
	OutMenu.Title = GT_EventRules::GetEventTitle(Kind);
	OutMenu.Description = bCompleted ? GT_EventRules::GetEventDoneText(Kind) : GT_EventRules::GetEventEnterText(Kind);

	// 选项构建对齐 Lua EventSystem.GetOptions。
	if (bCompleted)
	{
		OutMenu.Options.Add(MakeEventOption(GTEventOption_Leave,
			TEXT("关闭"), TEXT("事件已完成。"), TEXT("无"), TEXT("无"), TEXT("无"), true, FString()));
		return true;
	}

	switch (Kind)
	{
	case EGT_EventKind::Trader:
	{
		for (const FGT_ItemStack& Stack : RunInventory.CarriedItems)
		{
			if (Stack.Count <= 0)
			{
				continue;
			}
			const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
			const FString DisplayName = Def ? Def->DisplayName : Stack.ItemId.ToString();
			const int32 BaseValue = Def ? Def->Value : 0;
			const int32 Price = GT_EventRules::GetTraderSaleValue(BaseValue, LoadoutTradeBonusPercent);
			OutMenu.Options.Add(MakeEventOption(
				FName(*(GTEventSellPrefix + Stack.ItemId.ToString())),
				FString::Printf(TEXT("%s｜估值 %d｜出售价 %d"), *DisplayName, BaseValue, Price),
				TEXT("本轮可出售 1 件异常回收物。出售收益将作为已锁定收益保留。"),
				FString::Printf(TEXT("%s x1"), *DisplayName),
				FString::Printf(TEXT("已锁定 +%d"), Price),
				TEXT("该物品会从回收包移除"),
				Price > 0,
				TEXT("该物品无可结算价值")));
		}
		if (OutMenu.Options.IsEmpty())
		{
			OutMenu.Options.Add(MakeEventOption(GTEventOption_NoItem,
				TEXT("异常回收物不足。你不能出售空气，至少今天不行。"),
				TEXT("异常回收物不足。你不能出售空气，至少今天不行。"),
				TEXT("无"), TEXT("无"), TEXT("无"), false,
				TEXT("异常回收物不足。你不能出售空气，至少今天不行。")));
		}
		OutMenu.Options.Add(MakeEventOption(GTEventOption_Leave,
			TEXT("结束交易"), TEXT("暂不交易。"), TEXT("无"), TEXT("无"), TEXT("无"), true, FString()));
		break;
	}
	case EGT_EventKind::Dice:
	{
		const FGT_GamblerBalanceConfig& Gambler = GT_EventRules::GetGambler();
		const bool bCanBet = RunInventory.PendingGold >= Gambler.Bet;
		OutMenu.Options.Add(MakeEventOption(GTEventOption_BetSmall,
			FString::Printf(TEXT("下注 %d 待结算币"), Gambler.Bet),
			FString::Printf(
				TEXT("下注 %d 待结算币。1-%d 亏损，其他点数小赢，%d 大赢。收益不会立即入账。"),
				Gambler.Bet,
				Gambler.LoseMaxRoll,
				Gambler.BigWinRoll),
			FString::Printf(TEXT("待结算币 %d"), Gambler.Bet),
			FString::Printf(TEXT("小赢:+%d / 大赢:+%d"), Gambler.SmallWinNet, Gambler.BigWinNet),
			FString::Printf(TEXT("失败:-%d"), Gambler.Bet),
			bCanBet,
			TEXT("待结算币不足。狐狸不会借，赌徒也不会。")));
		OutMenu.Options.Add(MakeEventOption(GTEventOption_Leave,
			TEXT("离开"), TEXT("不下注。"), TEXT("无"), TEXT("无"), TEXT("无"), true, FString()));
		break;
	}
	case EGT_EventKind::Altar:
	{
		const TArray<FGT_AltarStepConfig>& AltarSteps = GT_EventRules::GetAltarSteps();
		const int32 Step = AltarStep + 1;
		if (!AltarSteps.IsValidIndex(Step - 1))
		{
			OutMenu.Options.Add(MakeEventOption(GTEventOption_Leave,
				TEXT("关闭"), TEXT("祭坛已经沉默。"), TEXT("无"), TEXT("无"), TEXT("无"), true, FString()));
			break;
		}
		const int32 Cost = AltarSteps[Step - 1].HpCost;
		const int32 RewardGold = AltarSteps[Step - 1].RewardGold;
		const bool bCanOffer = PlayerCombatState.Hp > Cost;
		OutMenu.Options.Add(MakeEventOption(GTEventOption_OfferHp,
			FString::Printf(TEXT("献祭生命 %d"), Cost),
			TEXT("生命消耗逐次增加，奖励也会递增。不会额外增加协议压力。"),
			FString::Printf(TEXT("生命 %d"), Cost),
			FString::Printf(TEXT("待结算币 +%d / 异常回收物 x1"), RewardGold),
			TEXT("当前生命不足则不可献祭"),
			bCanOffer,
			TEXT("当前生命不足，祭坛拒收。")));
		OutMenu.Options.Add(MakeEventOption(GTEventOption_Leave,
			TEXT("停止献祭"), TEXT("暂不献祭。"), TEXT("无"), TEXT("无"), TEXT("无"), true, FString()));
		break;
	}
	case EGT_EventKind::Trap:
	{
		const FGT_TrapBalanceConfig& Trap = GT_EventRules::GetTrap();
		OutMenu.Options.Add(MakeEventOption(GTEventOption_Disarm,
			TEXT("处理机关"),
			TEXT("使用战斗力进行一次检定。"),
			TEXT("一次检定"),
			FString::Printf(TEXT("成功: 待结算币 +%d / 回收物 x2"), Trap.SuccessGold),
			FString::Printf(TEXT("失败: 生命 -%d / 协议压力 +%d"), Trap.FailHpLoss, Trap.FailPressure),
			true,
			FString()));
		OutMenu.Options.Add(MakeEventOption(GTEventOption_Leave,
			TEXT("离开"), TEXT("不处理机关。"), TEXT("无"), TEXT("无"), TEXT("无"), true, FString()));
		break;
	}
	default:
		break;
	}

	return true;
}

bool UGT_RunContext::ExecuteEventOptionAtPlayer(FName OptionId, FGT_EventOutcome& OutOutcome)
{
	OutOutcome = FGT_EventOutcome();

	FGT_EventMenuView Menu;
	if (!GetEventMenuAtPlayer(Menu))
	{
		OutOutcome.Message = TEXT("这里没有可交互的事件。");
		LastEventOutcome = OutOutcome;
		return false;
	}

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	TryGetPlayerPosition(PlayerX, PlayerY);
	OutOutcome.Kind = Menu.Kind;

	// 空选项 = 默认项(对齐 Lua EventSystem.Execute), 供 gt.ChooseEventOption 无参调用。
	if (OptionId.IsNone() && !Menu.Options.IsEmpty())
	{
		OptionId = Menu.Options[0].OptionId;
	}
	OutOutcome.OptionId = OptionId;

	if (OptionId == GTEventOption_Leave)
	{
		OutOutcome.bOk = true;
		OutOutcome.Message = TEXT("事件交互取消。");
		OutOutcome.bClosePanel = true;
		LastEventOutcome = OutOutcome;
		return true;
	}

	if (Menu.bCompleted)
	{
		OutOutcome.Message = TEXT("此处事件已完成。");
		LastEventOutcome = OutOutcome;
		return false;
	}

	const FGT_EventOptionView* Selected = Menu.Options.FindByPredicate([OptionId](const FGT_EventOptionView& Option)
	{
		return Option.OptionId == OptionId;
	});
	if (!Selected)
	{
		OutOutcome.Message = TEXT("未知事件选项。");
		LastEventOutcome = OutOutcome;
		return false;
	}
	if (!Selected->bEnabled)
	{
		OutOutcome.Message = Selected->DisabledReason.IsEmpty() ? TEXT("条件不足。") : Selected->DisabledReason;
		LastEventOutcome = OutOutcome;
		return false;
	}

	FGT_EventRoomState& State = FindOrAddEventRoomState(PlayerX, PlayerY);

	// 按品质奖励 1 件回收物(对齐 Lua AddRewardItemByQuality: 进背包堆叠且 parts +1)。
	auto GrantQualityItem = [this, &OutOutcome](EGT_ItemQuality Quality)
	{
		const FName ItemId = GT_ItemCatalog::GetQualityItemId(Quality);
		if (ItemId.IsNone())
		{
			return;
		}
		RunInventory.AddCarriedItem(ItemId, 1, GTEventItemSource);
		RunInventory.Parts += 1;
		FGT_ItemStack& Granted = OutOutcome.GrantedItems.AddDefaulted_GetRef();
		Granted.ItemId = ItemId;
		Granted.Count = 1;
		Granted.Source = GTEventItemSource;
	};

	switch (Menu.Kind)
	{
	case EGT_EventKind::Trader:
	{
		// 选项 id 形如 "sell:<itemId>"。
		const FString OptionText = OptionId.ToString();
		if (!OptionText.StartsWith(GTEventSellPrefix))
		{
			OutOutcome.Message = TEXT("未知交易项。");
			LastEventOutcome = OutOutcome;
			return false;
		}
		const FName SellItemId(*OptionText.Mid(GTEventSellPrefix.Len()));
		FGT_ItemStack* Stack = RunInventory.CarriedItems.FindByPredicate([SellItemId](const FGT_ItemStack& Existing)
		{
			return Existing.ItemId == SellItemId;
		});
		if (!Stack || Stack->Count <= 0)
		{
			OutOutcome.Message = TEXT("异常回收物不足。你不能出售空气，至少今天不行。");
			LastEventOutcome = OutOutcome;
			return false;
		}

		const int32 Price = GT_EventRules::GetTraderSaleValue(GT_ItemCatalog::GetItemValue(SellItemId), LoadoutTradeBonusPercent);
		RunInventory.AddSafeGold(Price);
		// 对齐 Lua RemoveTradableItem: 物品 -1, parts 同步 -1。
		Stack->Count -= 1;
		if (Stack->Count <= 0)
		{
			RunInventory.CarriedItems.RemoveAll([SellItemId](const FGT_ItemStack& Existing)
			{
				return Existing.ItemId == SellItemId;
			});
		}
		RunInventory.Parts = FMath::Max(0, RunInventory.Parts - 1);
		State.bCompleted = true;

		OutOutcome.bOk = true;
		OutOutcome.SafeGoldDelta = Price;
		OutOutcome.SoldItemId = SellItemId;
		OutOutcome.bCompleted = true;
		OutOutcome.bClosePanel = true;
		OutOutcome.Message = FString::Printf(TEXT("交易完成。公司不会知道，大概。已锁定 +%d。"), Price);
		break;
	}
	case EGT_EventKind::Dice:
	{
		const FGT_GamblerBalanceConfig& Gambler = GT_EventRules::GetGambler();
		if (RunInventory.PendingGold < Gambler.Bet)
		{
			OutOutcome.Message = TEXT("待结算币不足。狐狸不会借，赌徒也不会。");
			LastEventOutcome = OutOutcome;
			return false;
		}
		// 骰点掺入下注前的待结算币(对齐 Lua)。
		const int32 Roll = GT_EventRules::RollDiceAt(Seed, PlayerX, PlayerY, RunInventory.PendingGold);
		State.bCompleted = true;
		OutOutcome.bOk = true;
		OutOutcome.bCompleted = true;
		OutOutcome.bClosePanel = true;
		if (Roll <= Gambler.LoseMaxRoll)
		{
			RunInventory.AddPendingGold(-Gambler.Bet);
			OutOutcome.PendingGoldDelta = -Gambler.Bet;
			OutOutcome.Message = FString::Printf(TEXT("骰点 %d。下注失败，待结算 -%d。"), Roll, Gambler.Bet);
		}
		else if (Roll == Gambler.BigWinRoll)
		{
			RunInventory.AddPendingGold(Gambler.BigWinNet);
			OutOutcome.PendingGoldDelta = Gambler.BigWinNet;
			OutOutcome.Message = FString::Printf(
				TEXT("骰点 %d。大赢一把，待结算 +%d。"),
				Roll,
				Gambler.BigWinNet);
		}
		else
		{
			RunInventory.AddPendingGold(Gambler.SmallWinNet);
			OutOutcome.PendingGoldDelta = Gambler.SmallWinNet;
			OutOutcome.Message = FString::Printf(
				TEXT("骰点 %d。小赢一把，待结算 +%d。"),
				Roll,
				Gambler.SmallWinNet);
		}
		break;
	}
	case EGT_EventKind::Altar:
	{
		const TArray<FGT_AltarStepConfig>& AltarSteps = GT_EventRules::GetAltarSteps();
		const int32 Step = State.AltarStep + 1;
		if (!AltarSteps.IsValidIndex(Step - 1))
		{
			State.bCompleted = true;
			OutOutcome.Message = TEXT("祭坛已完成全部回应。");
			LastEventOutcome = OutOutcome;
			return false;
		}
		const int32 Cost = AltarSteps[Step - 1].HpCost;
		if (PlayerCombatState.Hp <= Cost)
		{
			OutOutcome.Message = TEXT("当前生命不足，祭坛拒收。");
			LastEventOutcome = OutOutcome;
			return false;
		}

		State.AltarStep = Step;
		// 检定保证 Hp > Cost, 献祭后至少剩 1 点, 不会致死。
		PlayerCombatState.Hp -= Cost;
		const int32 RewardGold = AltarSteps[Step - 1].RewardGold;
		RunInventory.AddPendingGold(RewardGold);
		GrantQualityItem(GT_EventRules::GetAltarRewardQuality(Step - 1));

		const bool bAltarDone = Step >= AltarSteps.Num();
		State.bCompleted = bAltarDone;
		OutOutcome.bOk = true;
		OutOutcome.HpDelta = -Cost;
		OutOutcome.PendingGoldDelta = RewardGold;
		OutOutcome.bCompleted = bAltarDone;
		OutOutcome.bClosePanel = false;
		OutOutcome.Message = FString::Printf(TEXT("祭坛响应：生命 -%d，待结算 +%d，异常回收物 +1。"), Cost, RewardGold);
		break;
	}
	case EGT_EventKind::Trap:
	{
		const FGT_TrapBalanceConfig& Trap = GT_EventRules::GetTrap();
		State.bCompleted = true;
		OutOutcome.bOk = true;
		OutOutcome.bCompleted = true;
		OutOutcome.bClosePanel = true;
		const int32 EffectivePower = PlayerCombatState.Power + PlayerCombatState.MonsterPowerBonus;
		if (EffectivePower >= Trap.PowerRequirement)
		{
			RunInventory.AddPendingGold(Trap.SuccessGold);
			GrantQualityItem(EGT_ItemQuality::Common);
			GrantQualityItem(EGT_ItemQuality::Low);
			OutOutcome.PendingGoldDelta = Trap.SuccessGold;
			OutOutcome.Message = FString::Printf(
				TEXT("机关处理成功: 待结算币 +%d，回收物 +2。"), Trap.SuccessGold);
		}
		else
		{
			const int32 TrapDamage = ApplyPlayerDamage(Trap.FailHpLoss);
			AddProtocolPressure(Trap.FailPressure);
			OutOutcome.HpDelta = -TrapDamage;
			OutOutcome.PressureDelta = Trap.FailPressure;
			OutOutcome.Message = FString::Printf(
				TEXT("机关失控: 生命 -%d，协议压力 +%d。"), TrapDamage, Trap.FailPressure);
		}
		break;
	}
	default:
		OutOutcome.Message = TEXT("未知事件类型。");
		LastEventOutcome = OutOutcome;
		return false;
	}

	// 事件扣血可能致死(对齐 Lua: Combat.hp <= 0 -> 失败结算)。
	if (!PlayerCombatState.IsAlive())
	{
		MarkRunFailed(FName(TEXT("Event")));
	}

	LastEventOutcome = OutOutcome;
	return OutOutcome.bOk;
}
