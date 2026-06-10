#include "Core/GT_RunContext.h"

#include "Domains/Inventory/GT_LootRules.h"
#include "Domains/Map/GT_MapGenerator.h"

namespace
{
	const FName GTCombatResult_Success(TEXT("Combat_DebugResult_Success"));
	const FName GTRunSummaryOutcome_Extracted(TEXT("Extracted"));
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
	PlayerCombatState = FGT_PlayerCombatState();
	DefeatedCombatRooms.Reset();

	PlayerActorId = FName(TEXT("Player"));

	FGT_ActorRuntimeState PlayerState;
	PlayerState.ActorId.Value = PlayerActorId;
	PlayerState.ActorDefId = FName(TEXT("PlayerDefault"));
	PlayerState.Team = EGT_ActorTeam::Player;
	PlayerState.Faction = EGT_ActorFaction::Graytail;
	PlayerState.X = 0;
	PlayerState.Y = 0;
	PlayerState.bAlive = true;

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
	PlayerCombatState = FGT_PlayerCombatState();
	DefeatedCombatRooms.Reset();
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

bool UGT_RunContext::StartDummyCombat(int32 X, int32 Y, FName RoomContentId, FName RoomRuleId, int32 InitialDummyHp)
{
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

	// Standard 模式: 用确定性规则生成真怪(对齐 Combat.TrySpawnEnemy), 替换 1 血 dummy。
	if (MapMode == EGT_MapMode::Standard)
	{
		int32 AdjacentMineCount = 0;
		TruthMap.CountAdjacentMines8(X, Y, AdjacentMineCount);
		CombatRuntimeState.bStandardEnemy = true;
		GT_CombatRules::MakeEnemyForCell(Seed, X, Y, AdjacentMineCount, CombatRuntimeState.EnemyName, CombatRuntimeState.EnemyPower);
	}
	return true;
}

bool UGT_RunContext::AttackDummyCombat(FGT_CombatRuntimeState& OutState)
{
	OutState = CombatRuntimeState;
	if (!IsRunActive() || !CombatRuntimeState.bCombatActive || CombatRuntimeState.DummyEnemyHp <= 0)
	{
		return false;
	}

	// Standard 模式: 一击整场结算(对齐 Combat.FightEnemy):
	// 战力 >= 怪则无伤胜; 不够则扣差值血, 怪同样死。存活才拿奖励(金币+战力成长)。
	if (CombatRuntimeState.bStandardEnemy)
	{
		const int32 Damage = FMath::Max(0, CombatRuntimeState.EnemyPower - PlayerCombatState.Power);
		PlayerCombatState.ApplyDamage(Damage);

		CombatRuntimeState.DummyEnemyHp = 0;
		CombatRuntimeState.bCombatActive = false;
		CombatRuntimeState.bCombatResolved = true;
		CombatRuntimeState.LastCombatResultId = GTCombatResult_Success;
		DefeatedCombatRooms.AddUnique(FIntPoint(CombatRuntimeState.CombatX, CombatRuntimeState.CombatY));

		if (PlayerCombatState.IsAlive())
		{
			RunInventory.AddPendingGold(GT_CombatRules::KillRewardGold(CombatRuntimeState.EnemyPower));
			const int32 PowerGain = FMath::Min(GT_CombatRules::PowerGainPerKill, GT_CombatRules::PowerGainCap - PlayerCombatState.MonsterPowerBonus);
			if (PowerGain > 0)
			{
				PlayerCombatState.Power += PowerGain;
				PlayerCombatState.MonsterPowerBonus += PowerGain;
			}
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
	// 对齐 Combat.TakeMineHit: 雷伤 30(装备减免后最低 5, 减免待装备阶段)。
	OutDamage = FMath::Max(GT_CombatRules::MineDamageFloor, GT_CombatRules::MineDamage);
	PlayerCombatState.ApplyDamage(OutDamage);
	bOutDead = !PlayerCombatState.IsAlive();
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

	// 出生点固定 (0, 0)(InitializeFromSpec 放置), 不可搜刮。
	if (PlayerX == 0 && PlayerY == 0)
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

	// 普通房和宝箱房均可搜一次; 物品掉落仅宝箱房有(见 GT_LootRules), 普通房只出金币。
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
	RunInventory.AddPendingGold(Reward.Gold);
	RunInventory.Parts += Reward.Parts;
	for (const FGT_ItemStack& Stack : Reward.Items)
	{
		RunInventory.AddCarriedItem(Stack.ItemId, Stack.Count, Stack.Source);
	}

	OutOutcome.bSearched = true;
	OutOutcome.Status = FName(TEXT("searched"));
	OutOutcome.Reward = Reward;
	return true;
}
