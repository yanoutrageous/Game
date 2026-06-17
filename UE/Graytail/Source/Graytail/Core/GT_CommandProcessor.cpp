#include "Core/GT_CommandProcessor.h"

#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_ProtocolState.h"
#include "Core/GT_RunContext.h"

namespace
{
	const FName GTCommandType_Move(TEXT("Move"));
	const FName GTCommandType_Scan(TEXT("Scan"));
	const FName GTCommandType_Search(TEXT("Search"));
	const FName GTCommandType_Extract(TEXT("Extract"));
	const FName GTCommandType_ChooseEventOption(TEXT("ChooseEventOption"));
	const FName GTCommandType_ResolveCombat(TEXT("ResolveCombat"));
	const FName GTCommandType_Attack(TEXT("Attack"));
	const FName GTCommandType_MonsterHit(TEXT("MonsterHit"));
	const FName GTCommandType_UseConsumable(TEXT("UseConsumable"));
	const FName GTEventType_ActorMoved(TEXT("ActorMoved"));
	const FName GTEventType_EventOptionChosen(TEXT("EventOptionChosen"));
	const FName GTEventType_CellScanned(TEXT("CellScanned"));
	const FName GTEventType_RoomSearched(TEXT("RoomSearched"));
	const FName GTEventType_ConsumableUsed(TEXT("ConsumableUsed"));
	const FName GTEventType_CommandFailed(TEXT("CommandFailed"));
	const FName GTEventType_RunFailed(TEXT("RunFailed"));
	const FName GTEventType_RunSucceeded(TEXT("RunSucceeded"));
	const FName GTRunFailureReason_Mine(TEXT("Mine"));
	const FName GTRunFailureReason_CombatDeath(TEXT("CombatDeath"));
	const FName GTRunFailureReason_Protocol(TEXT("Protocol"));
	const FName GTEventType_MineDamaged(TEXT("MineDamaged"));
	const FName GTEventType_CombatPlayerHit(TEXT("CombatPlayerHit"));
	const FName GTEventType_ProtocolPressureChanged(TEXT("ProtocolPressureChanged"));
	const FName GTEventType_ProtocolLevelChanged(TEXT("ProtocolLevelChanged"));
	const FName GTEventType_ProtocolChanged(TEXT("ProtocolChanged"));
	const FName GTEventType_PressureAdded(TEXT("PressureAdded"));
	const FName GTRunSuccessReason_Extract(TEXT("Extract"));
	const FName GTSourceSystem_CommandProcessor(TEXT("CommandProcessor"));
}

void UGT_CommandProcessor::Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus, UGT_ContentRegistry* InContentRegistry)
{
	RunContext = InRunContext;
	EventBus = InEventBus;
	ContentRegistry = InContentRegistry;

	if (!RoomResolver)
	{
		RoomResolver = NewObject<UGT_RoomResolver>(this);
	}

	if (RoomResolver)
	{
		RoomResolver->Initialize(RunContext, EventBus, ContentRegistry);
	}
}

bool UGT_CommandProcessor::ProcessCommand(const FGT_Command& Command)
{
	if (!IsValid(RunContext) || !RunContext->IsRunActive())
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	if (Command.CommandType == GTCommandType_Move)
	{
		return ProcessMoveCommand(Command);
	}

	if (Command.CommandType == GTCommandType_Scan)
	{
		return ProcessScanCommand(Command);
	}

	if (Command.CommandType == GTCommandType_Search)
	{
		return ProcessSearchCommand(Command);
	}

	if (Command.CommandType == GTCommandType_Extract)
	{
		return ProcessExtractCommand(Command);
	}

	if (Command.CommandType == GTCommandType_ChooseEventOption)
	{
		return ProcessChooseEventOptionCommand(Command);
	}

	if (Command.CommandType == GTCommandType_ResolveCombat)
	{
		return ProcessResolveCombatCommand(Command);
	}

	if (Command.CommandType == GTCommandType_Attack)
	{
		return ProcessAttackCommand(Command);
	}

	if (Command.CommandType == GTCommandType_MonsterHit)
	{
		return ProcessMonsterHitCommand(Command);
	}

	if (Command.CommandType == GTCommandType_UseConsumable)
	{
		return ProcessUseConsumableCommand(Command);
	}

	PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
	return false;
}

bool UGT_CommandProcessor::ProcessMoveCommand(const FGT_Command& Command)
{
	if (!IsValid(RunContext) || Command.TargetActorId.IsNone())
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	if (!RunContext->IsValidMapCoord(Command.TargetX, Command.TargetY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	FGT_ActorRuntimeState* ActorState = RunContext->FindActorStateMutable(Command.TargetActorId);
	if (!ActorState)
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	if (FMath::Abs(Command.TargetX - ActorState->X) + FMath::Abs(Command.TargetY - ActorState->Y) != 1)
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	ActorState->X = Command.TargetX;
	ActorState->Y = Command.TargetY;

	if (Command.TargetActorId == RunContext->GetPlayerActorId())
	{
		RunContext->MarkPlayerIntelCellExplored(Command.TargetX, Command.TargetY);

		// Standard 模式对齐 Lua 扫雷规则: 走到哪格自动亮出哪格的雷数(无需手动 Scan)。
		if (RunContext->GetMapMode() == EGT_MapMode::Standard)
		{
			int32 AutoScanMines = 0;
			if (RunContext->CountAdjacentMines8(Command.TargetX, Command.TargetY, AutoScanMines))
			{
				RunContext->SetPlayerIntelCellScannedNumber(Command.TargetX, Command.TargetY, AutoScanMines);
			}

			// 首次探索未知房加协议压力(对齐 Balance.pressure.explore = 2)。
			if (RunContext->MarkExploredForPressure(Command.TargetX, Command.TargetY))
			{
				const auto PressureResult = RunContext->AddProtocolPressure(GT_ProtocolRules::ExplorePressure);
				PublishProtocolPressureEvent(Command.TargetActorId, Command.TargetX, Command.TargetY, GT_ProtocolRules::ExplorePressure, PressureResult);

				// 满压惩罚: 已满压时每进一个新房按难度扣血(替代直接败北)。血量显示走移动后常规刷新; 这里只处理致死。
				int32 PressureDamage = 0;
				bool bPressureDead = false;
				if (RunContext->ApplyMaxPressureRoomPenalty(PressureDamage, bPressureDead) && bPressureDead)
				{
					if (RunContext->MarkRunFailed(FName(TEXT("Protocol"))))
					{
						PublishCommandEvent(GTEventType_RunFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, true);
					}
				}
			}

			// S6 回收磁石: 进宝箱房额外掉 1 件(内部判激活/宝箱房/去重)。
			RunContext->TryGrantChestMagnetLoot(Command.TargetX, Command.TargetY);
		}
	}

	PublishCommandEvent(GTEventType_ActorMoved, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, true);
	if (Command.TargetActorId == RunContext->GetPlayerActorId() && IsValid(RoomResolver))
	{
		// 雷只炸一次(对齐 Lua): 进房前先记下是否已触发过, ResolveRoomAt 内部会无条件置 bTriggered。
		FGT_TruthCell PreMoveCell;
		const bool bMineAlreadyTriggered = RunContext->GetTruthCellSnapshot(Command.TargetX, Command.TargetY, PreMoveCell)
			&& PreMoveCell.bHasMine && PreMoveCell.bTriggered;

		FGT_RoomResolveResult RoomResolveResult;
		if (RoomResolver->ResolveRoomAt(Command.TargetX, Command.TargetY, RoomResolveResult)
			&& RoomResolveResult.Outcome == EGT_RoomResolveOutcome::MineEncountered)
		{
			if (RunContext->GetMapMode() == EGT_MapMode::Standard)
			{
				if (bMineAlreadyTriggered)
				{
					// 已炸过的雷格视为废墟, 通行无伤。
					return true;
				}
				// Standard 真规则(对齐 Combat.TakeMineHit): 雷只扣血, 血尽才败北。
				int32 MineDamage = 0;
				bool bPlayerDead = false;
				RunContext->ApplyMineHitToPlayer(MineDamage, bPlayerDead);

				FGT_GameEvent MineEvent;
				MineEvent.EventType = GTEventType_MineDamaged;
				MineEvent.SourceSystem = GTSourceSystem_CommandProcessor;
				MineEvent.SourceActorId = Command.TargetActorId;
				MineEvent.TargetActorId = Command.TargetActorId;
				MineEvent.X = Command.TargetX;
				MineEvent.Y = Command.TargetY;
				MineEvent.RoomCoord = FIntPoint(Command.TargetX, Command.TargetY);
				MineEvent.NumericValue = MineDamage;
				MineEvent.bSuccess = true;
				if (IsValid(EventBus))
				{
					EventBus->PublishEvent(MineEvent);
				}

				// 踩雷加协议压力(对齐 Balance.pressure.mine = 10)。
				{
					const auto MinePressureResult = RunContext->AddProtocolPressure(GT_ProtocolRules::MinePressure);
					PublishProtocolPressureEvent(Command.TargetActorId, Command.TargetX, Command.TargetY, GT_ProtocolRules::MinePressure, MinePressureResult);
				}

				if (bPlayerDead && RunContext->MarkRunFailed(GTRunFailureReason_Mine))
				{
					PublishCommandEvent(GTEventType_RunFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, true);
				}
			}
			else if (RunContext->MarkRunFailed(GTRunFailureReason_Mine))
			{
				// BasicDebug 测试夹具: 保持"踩雷即败北"的老行为。
				PublishCommandEvent(GTEventType_RunFailed, Command.SourceActorId, Command.TargetActorId, Command.TargetX, Command.TargetY, true);
			}
		}
	}

	return true;
}

bool UGT_CommandProcessor::ProcessScanCommand(const FGT_Command& Command)
{
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;

	if (!IsValid(RunContext) || !RunContext->IsValidMapCoord(Command.TargetX, Command.TargetY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	int32 AdjacentMineCount = 0;
	if (!RunContext->CountAdjacentMines8(Command.TargetX, Command.TargetY, AdjacentMineCount)
		|| !RunContext->SetPlayerIntelCellScannedNumber(Command.TargetX, Command.TargetY, AdjacentMineCount))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	PublishCommandEvent(GTEventType_CellScanned, Command.SourceActorId, EventTargetActorId, Command.TargetX, Command.TargetY, true);
	return true;
}

bool UGT_CommandProcessor::ProcessSearchCommand(const FGT_Command& Command)
{
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	if (!IsValid(RunContext) || !RunContext->TryGetPlayerPosition(PlayerX, PlayerY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, Command.TargetX, Command.TargetY, false);
		return false;
	}

	FGT_SearchOutcome Outcome;
	if (!RunContext->SearchCurrentRoom(Outcome))
	{
		// 失败原因(spawn/exit/monster/searched...)放进 RuleId, 供监听方区分。
		FGT_GameEvent FailedEvent;
		FailedEvent.EventType = GTEventType_CommandFailed;
		FailedEvent.SourceSystem = GTSourceSystem_CommandProcessor;
		FailedEvent.SourceActorId = Command.SourceActorId.IsNone() ? EventTargetActorId : Command.SourceActorId;
		FailedEvent.TargetActorId = EventTargetActorId;
		FailedEvent.X = PlayerX;
		FailedEvent.Y = PlayerY;
		FailedEvent.RoomCoord = FIntPoint(PlayerX, PlayerY);
		FailedEvent.RuleId = Outcome.Status;
		FailedEvent.bSuccess = false;
		if (IsValid(EventBus))
		{
			EventBus->PublishEvent(FailedEvent);
		}
		return false;
	}

	// 成功事件: NumericValue 带本次金币收益, 物品明细由查询层从背包状态读取。
	FGT_GameEvent SearchedEvent;
	SearchedEvent.EventType = GTEventType_RoomSearched;
	SearchedEvent.SourceSystem = GTSourceSystem_CommandProcessor;
	SearchedEvent.SourceActorId = Command.SourceActorId.IsNone() ? EventTargetActorId : Command.SourceActorId;
	SearchedEvent.TargetActorId = EventTargetActorId;
	SearchedEvent.X = PlayerX;
	SearchedEvent.Y = PlayerY;
	SearchedEvent.RoomCoord = FIntPoint(PlayerX, PlayerY);
	SearchedEvent.NumericValue = Outcome.Reward.Gold;
	SearchedEvent.bSuccess = true;
	if (IsValid(EventBus))
	{
		EventBus->PublishEvent(SearchedEvent);
	}
	return true;
}

bool UGT_CommandProcessor::ProcessUseConsumableCommand(const FGT_Command& Command)
{
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	if (IsValid(RunContext))
	{
		RunContext->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	// PayloadId 为空 = 底栏 Q 的默认动作(应急止血贴)。
	const FName ItemId = Command.PayloadId.IsNone() ? FName(TEXT("emergency_bandage")) : Command.PayloadId;

	FGT_ConsumableOutcome Outcome;
	if (!IsValid(RunContext) || !RunContext->UseConsumableAtPlayer(ItemId, Outcome))
	{
		// 失败原因(hp_full/not_enough/not_consumable...)放进 RuleId, 供 UI 区分提示。
		FGT_GameEvent FailedEvent;
		FailedEvent.EventType = GTEventType_CommandFailed;
		FailedEvent.SourceSystem = GTSourceSystem_CommandProcessor;
		FailedEvent.SourceActorId = Command.SourceActorId.IsNone() ? EventTargetActorId : Command.SourceActorId;
		FailedEvent.TargetActorId = EventTargetActorId;
		FailedEvent.X = PlayerX;
		FailedEvent.Y = PlayerY;
		FailedEvent.RoomCoord = FIntPoint(PlayerX, PlayerY);
		FailedEvent.RuleId = Outcome.Status;
		FailedEvent.bSuccess = false;
		if (IsValid(EventBus))
		{
			EventBus->PublishEvent(FailedEvent);
		}
		return false;
	}

	// 成功事件: NumericValue 带回血量, RuleId 带物品 id。
	FGT_GameEvent UsedEvent;
	UsedEvent.EventType = GTEventType_ConsumableUsed;
	UsedEvent.SourceSystem = GTSourceSystem_CommandProcessor;
	UsedEvent.SourceActorId = Command.SourceActorId.IsNone() ? EventTargetActorId : Command.SourceActorId;
	UsedEvent.TargetActorId = EventTargetActorId;
	UsedEvent.X = PlayerX;
	UsedEvent.Y = PlayerY;
	UsedEvent.RoomCoord = FIntPoint(PlayerX, PlayerY);
	UsedEvent.NumericValue = Outcome.HealAmount;
	UsedEvent.RuleId = Outcome.ItemId;
	UsedEvent.bSuccess = true;
	if (IsValid(EventBus))
	{
		EventBus->PublishEvent(UsedEvent);
	}
	return true;
}

bool UGT_CommandProcessor::ProcessExtractCommand(const FGT_Command& Command)
{
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const bool bPlayerPositionReadable = IsValid(RunContext) && RunContext->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bPlayerAtExit = bPlayerPositionReadable
		&& RunContext->GetTruthMapForDebugOnly().IsExit(PlayerX, PlayerY);
	if (!bPlayerAtExit)
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	if (!RunContext->MarkRunSucceeded(GTRunSuccessReason_Extract))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	PublishCommandEvent(GTEventType_RunSucceeded, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, true);
	RunContext->GenerateExtractSummary(EventBus ? EventBus->GetEventCount() : 0);
	return true;
}

bool UGT_CommandProcessor::ProcessChooseEventOptionCommand(const FGT_Command& Command)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;
	if (!IsValid(RunContext) || !IsValid(RoomResolver) || !RunContext->TryGetPlayerPosition(PlayerX, PlayerY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	// Standard 模式: 事件房真实规则(旅商/赌徒/祭坛/机关)在 RunContext 内核执行;
	// BasicDebug 保持注册表占位路径(163 冒烟测试夹具行为不动)。
	if (RunContext->GetMapMode() == EGT_MapMode::Standard)
	{
		FGT_EventOutcome EventOutcome;
		if (!RunContext->ExecuteEventOptionAtPlayer(Command.PayloadId, EventOutcome))
		{
			PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
			return false;
		}

		PublishCommandEvent(GTEventType_EventOptionChosen, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, true);
		if (!RunContext->IsRunActive() && RunContext->IsRunFailed())
		{
			// 事件扣血致死(机关失控)或压力满载强制败北。
			PublishCommandEvent(GTEventType_RunFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, true);
		}
		return true;
	}

	FGT_RoomResolveResult InteractionResult;
	if (!RoomResolver->ChooseEventOptionAt(PlayerX, PlayerY, Command.PayloadId, InteractionResult))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	return true;
}

bool UGT_CommandProcessor::ProcessResolveCombatCommand(const FGT_Command& Command)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;
	if (!IsValid(RunContext) || !IsValid(RoomResolver) || !RunContext->TryGetPlayerPosition(PlayerX, PlayerY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	FGT_RoomResolveResult InteractionResult;
	if (!RoomResolver->ResolveCombatAt(PlayerX, PlayerY, Command.PayloadId, InteractionResult))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	return true;
}

bool UGT_CommandProcessor::ProcessAttackCommand(const FGT_Command& Command)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;
	if (!IsValid(RunContext) || !IsValid(RoomResolver) || !RunContext->TryGetPlayerPosition(PlayerX, PlayerY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	// 攻击前是否已结算 —— 用于区分"这一刀打死了怪"与"还没死的中间一刀"。
	FGT_CombatRuntimeState PreAttackState;
	RunContext->GetCombatStateSnapshot(PreAttackState);
	const bool bWasResolvedBefore = PreAttackState.bCombatResolved;

	FGT_RoomResolveResult AttackResult;
	if (!RoomResolver->AttackCombatAt(PlayerX, PlayerY, AttackResult))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	if (RunContext->GetMapMode() == EGT_MapMode::Standard)
	{
		// Standard 实时战斗: 玩家挥砍只削怪血, 不反伤(伤害走 MonsterHit)。
		// 仅"这一刀把怪打死"(本次由未结算 -> 结算)时加击杀压力, 中间刀不加。
		FGT_CombatRuntimeState PostAttackState;
		RunContext->GetCombatStateSnapshot(PostAttackState);
		const bool bJustKilled = !bWasResolvedBefore && PostAttackState.bCombatResolved;
		if (bJustKilled)
		{
			// 击杀怪物加协议压力(对齐 Balance.pressure.monsterKill = 5)。
			const UGT_RunContext::FProtocolPressureResult KillPressureResult = RunContext->AddProtocolPressure(GT_ProtocolRules::CombatKillPressure);
			PublishProtocolPressureEvent(EventTargetActorId, PlayerX, PlayerY, GT_ProtocolRules::CombatKillPressure, KillPressureResult);
		}
	}

	return true;
}

bool UGT_CommandProcessor::ProcessMonsterHitCommand(const FGT_Command& Command)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const FName EventTargetActorId = Command.TargetActorId.IsNone() && IsValid(RunContext)
		? RunContext->GetPlayerActorId()
		: Command.TargetActorId;
	if (!IsValid(RunContext) || !RunContext->TryGetPlayerPosition(PlayerX, PlayerY))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	// 怪物对玩家造成一次伤害(无敌帧由表现层门控)。未真打到则视为无效命令。
	int32 Damage = 0;
	bool bPlayerDead = false;
	if (!RunContext->MonsterHitPlayer(Damage, bPlayerDead))
	{
		PublishCommandEvent(GTEventType_CommandFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, false);
		return false;
	}

	PublishCommandEvent(GTEventType_CombatPlayerHit, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, true);

	if (bPlayerDead)
	{
		if (RunContext->MarkRunFailed(GTRunFailureReason_CombatDeath))
		{
			PublishCommandEvent(GTEventType_RunFailed, Command.SourceActorId, EventTargetActorId, PlayerX, PlayerY, true);
		}
	}

	return true;
}

void UGT_CommandProcessor::PublishProtocolPressureEvent(FName ActorId, int32 X, int32 Y, int32 PressureDelta, const UGT_RunContext::FProtocolPressureResult& Result) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent PressureEvent;
	PressureEvent.EventType = GTEventType_ProtocolPressureChanged;
	PressureEvent.SourceSystem = GTSourceSystem_CommandProcessor;
	PressureEvent.SourceActorId = ActorId;
	PressureEvent.TargetActorId = ActorId;
	PressureEvent.X = X;
	PressureEvent.Y = Y;
	PressureEvent.RoomCoord = FIntPoint(X, Y);
	PressureEvent.NumericValue = Result.Pressure;
	PressureEvent.bSuccess = true;
	EventBus->PublishEvent(PressureEvent);

	if (Result.bLevelChanged)
	{
		FGT_GameEvent LevelEvent = PressureEvent;
		LevelEvent.EventId = FGuid::NewGuid();
		LevelEvent.EventType = GTEventType_ProtocolLevelChanged;
		LevelEvent.NumericValue = Result.Level;
		EventBus->PublishEvent(LevelEvent);
	}

	// 满压败北: MarkRunFailed 已在 AddProtocolPressure 内部完成, 此处只补广播。
	if (Result.bForcedFail)
	{
		PublishCommandEvent(GTEventType_RunFailed, ActorId, ActorId, X, Y, true);
	}
}

void UGT_CommandProcessor::PublishCommandEvent(FName EventType, FName SourceActorId, FName TargetActorId, int32 X, int32 Y, bool bSuccess) const
{
	if (!IsValid(EventBus))
	{
		return;
	}

	FGT_GameEvent Event;
	Event.EventType = EventType;
	Event.SourceSystem = GTSourceSystem_CommandProcessor;
	Event.SourceActorId = SourceActorId.IsNone() ? TargetActorId : SourceActorId;
	Event.TargetActorId = TargetActorId;
	Event.X = X;
	Event.Y = Y;
	Event.RoomCoord = FIntPoint(X, Y);
	Event.bSuccess = bSuccess;
	EventBus->PublishEvent(Event);
}
