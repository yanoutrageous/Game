#include "Debug/GT_DebugSubsystem.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Domains/Events/GT_EventRules.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Map/GT_MapGenerator.h"
#include "Engine/GameInstance.h"

namespace
{
	const FName GTDebugCommandType_Move(TEXT("Move"));
	const FName GTDebugCommandType_Scan(TEXT("Scan"));
	const FName GTDebugCommandType_Search(TEXT("Search"));
	const FName GTDebugCommandType_Extract(TEXT("Extract"));
	const FName GTDebugCommandType_ChooseEventOption(TEXT("ChooseEventOption"));
	const FName GTDebugCommandType_ResolveCombat(TEXT("ResolveCombat"));
	const FName GTDebugCommandType_Attack(TEXT("Attack"));
	const FName GTDebugCommandType_MonsterHit(TEXT("MonsterHit"));
	const FName GTDebugCommandType_UseConsumable(TEXT("UseConsumable"));
	const FName GTDebugActorId_Player(TEXT("Player"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTDebugEventType_EventResolved(TEXT("EventResolved"));
	const FName GTDebugEventType_CombatResolved(TEXT("CombatResolved"));
	const FName GTRoomIcon_Exit(TEXT("Exit"));
	const FName GTRoomIcon_Mine(TEXT("Mine"));
	const FName GTRoomIcon_Event(TEXT("Event"));
	const FName GTRoomIcon_Combat(TEXT("Combat"));
	const FName GTRoomIcon_Chest(TEXT("Chest"));

	FName GetDebugRoomIcon(EGT_RoomBaseType RoomBaseType)
	{
		switch (RoomBaseType)
		{
		case EGT_RoomBaseType::Exit:
			return GTRoomIcon_Exit;
		case EGT_RoomBaseType::Mine:
			return GTRoomIcon_Mine;
		case EGT_RoomBaseType::Event:
			return GTRoomIcon_Event;
		case EGT_RoomBaseType::Combat:
			return GTRoomIcon_Combat;
		case EGT_RoomBaseType::Chest:
			return GTRoomIcon_Chest;
		default:
			return NAME_None;
		}
	}

	FString GetRunStateText(EGT_RunState RunState)
	{
		switch (RunState)
		{
		case EGT_RunState::NotStarted:
			return TEXT("NotStarted");
		case EGT_RunState::Running:
			return TEXT("Running");
		case EGT_RunState::Failed:
			return TEXT("Failed");
		case EGT_RunState::Succeeded:
			return TEXT("Succeeded");
		case EGT_RunState::Ended:
			return TEXT("Ended");
		default:
			return TEXT("Unknown");
		}
	}

	FString GetRoomBaseTypeText(EGT_RoomBaseType RoomBaseType)
	{
		switch (RoomBaseType)
		{
		case EGT_RoomBaseType::Unknown:
			return TEXT("Unknown");
		case EGT_RoomBaseType::Normal:
			return TEXT("Normal");
		case EGT_RoomBaseType::Mine:
			return TEXT("Mine");
		case EGT_RoomBaseType::Exit:
			return TEXT("Exit");
		case EGT_RoomBaseType::Event:
			return TEXT("Event");
		case EGT_RoomBaseType::Combat:
			return TEXT("Combat");
		case EGT_RoomBaseType::Chest:
			return TEXT("Chest");
		default:
			return TEXT("Unknown");
		}
	}

	FString BuildDebugEventSummaryText(const TArray<FGT_DebugEventSummary>& EventSummary)
	{
		if (EventSummary.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(EventSummary.Num());
		for (const FGT_DebugEventSummary& Summary : EventSummary)
		{
			Parts.Add(FString::Printf(TEXT("%s=%d"), *Summary.EventType.ToString(), Summary.Count));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	FString BuildEventOptionDefsText(const TArray<FGT_EventOptionDef>& Definitions)
	{
		if (Definitions.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(Definitions.Num());
		for (const FGT_EventOptionDef& Definition : Definitions)
		{
			Parts.Add(FString::Printf(
				TEXT("%s(%s)"),
				*Definition.Id.ToString(),
				Definition.DisplayName.IsEmpty() ? TEXT("Unnamed") : *Definition.DisplayName));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	FString BuildCombatResultDefsText(const TArray<FGT_CombatResultDef>& Definitions)
	{
		if (Definitions.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(Definitions.Num());
		for (const FGT_CombatResultDef& Definition : Definitions)
		{
			Parts.Add(FString::Printf(
				TEXT("%s(%s)"),
				*Definition.Id.ToString(),
				Definition.DisplayName.IsEmpty() ? TEXT("Unnamed") : *Definition.DisplayName));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	FString BuildRunSummaryText(const FGT_RunSummary& Summary)
	{
		if (!Summary.bSummaryAvailable)
		{
			return TEXT("SummaryAvailable=false Outcome=None NoRunSummary");
		}

		return FString::Printf(
			TEXT("SummaryAvailable=true Outcome=%s Extracted=%s FinalPlayer=(%d,%d) TotalEventCount=%d Seed=%d Map=%dx%d CombatActive=%s CombatResolved=%s LastCombatResult=%s"),
			*Summary.Outcome.ToString(),
			Summary.bExtracted ? TEXT("true") : TEXT("false"),
			Summary.FinalPlayerX,
			Summary.FinalPlayerY,
			Summary.TotalEventCount,
			Summary.Seed,
			Summary.MapWidth,
			Summary.MapHeight,
			Summary.bCombatActive ? TEXT("true") : TEXT("false"),
			Summary.bCombatResolved ? TEXT("true") : TEXT("false"),
			*Summary.LastCombatResultId.ToString());
	}

	bool HasDebugEventSummaryType(const TArray<FGT_DebugEventSummary>& EventSummary, FName EventType)
	{
		return EventSummary.ContainsByPredicate([EventType](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == EventType && Summary.Count > 0;
		});
	}
}

void UGT_DebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGT_DebugSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

FString UGT_DebugSubsystem::GetCurrentRunDebugSummary() const
{
	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;

	if (!QueryFacade || !QueryFacade->HasValidRunContext())
	{
		return TEXT("No active run");
	}

	int32 PlayerX = 0;
	int32 PlayerY = 0;
	const bool bHasPlayerPosition = QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);

	TArray<FGT_ActorRuntimeState> ActorStates;
	QueryFacade->GetActorStates(ActorStates);

	return FString::Printf(
		TEXT("RunId=%s, Seed=%d, Size=%dx%d, PlayerActorId=%s, PlayerPosition=%s, ActorCount=%d"),
		*QueryFacade->GetRunId().ToString(),
		QueryFacade->GetSeed(),
		QueryFacade->GetMapWidth(),
		QueryFacade->GetMapHeight(),
		*QueryFacade->GetPlayerActorId().ToString(),
		bHasPlayerPosition ? *FString::Printf(TEXT("(%d,%d)"), PlayerX, PlayerY) : TEXT("None"),
		ActorStates.Num());
}

bool UGT_DebugSubsystem::DebugStartNewRun(int32 Seed, int32 Width, int32 Height, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	if (!RunSubsystem)
	{
		OutSnapshot.Summary = TEXT("RunSubsystem is not valid");
		return false;
	}

	const bool bStarted = RunSubsystem->StartNewRun(Seed, Width, Height) != nullptr;
	GetDebugRunSnapshot(OutSnapshot);
	return bStarted;
}

bool UGT_DebugSubsystem::DebugStartStandardRun(int32 Seed, EGT_Difficulty Difficulty, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	if (!RunSubsystem)
	{
		OutSnapshot.Summary = TEXT("RunSubsystem is not valid");
		return false;
	}

	const bool bStarted = RunSubsystem->StartNewRunStandard(Seed, Difficulty) != nullptr;
	GetDebugRunSnapshot(OutSnapshot);
	return bStarted;
}

bool UGT_DebugSubsystem::DebugMoveTo(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot)
{
	return SubmitDebugCommand(GTDebugCommandType_Move, X, Y, OutSnapshot);
}

bool UGT_DebugSubsystem::DebugScanCell(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot)
{
	return SubmitDebugCommand(GTDebugCommandType_Scan, X, Y, OutSnapshot);
}

bool UGT_DebugSubsystem::DebugTeleport(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
	if (!RunContext || !RunContext->IsRunActive())
	{
		OutSnapshot.Summary = TEXT("No active run");
		return false;
	}

	if (!RunContext->IsValidMapCoord(X, Y))
	{
		GetDebugRunSnapshot(OutSnapshot);
		OutSnapshot.Summary = FString::Printf(TEXT("Teleport rejected: (%d,%d) out of map. %s"), X, Y, *OutSnapshot.Summary);
		return false;
	}

	FGT_ActorRuntimeState* PlayerState = RunContext->FindActorStateMutable(RunContext->GetPlayerActorId());
	if (!PlayerState)
	{
		OutSnapshot.Summary = TEXT("Teleport rejected: player actor not found");
		return false;
	}

	PlayerState->X = X;
	PlayerState->Y = Y;
	RunContext->MarkPlayerIntelCellExplored(X, Y);

	// MarkExplored 会重置 bScanned, Standard 模式下与走路一致: 落点自动亮雷数。
	if (RunContext->GetMapMode() == EGT_MapMode::Standard)
	{
		int32 AdjacentMines = 0;
		if (RunContext->CountAdjacentMines8(X, Y, AdjacentMines))
		{
			RunContext->SetPlayerIntelCellScannedNumber(X, Y, AdjacentMines);
		}
	}

	GetDebugRunSnapshot(OutSnapshot);
	return true;
}

namespace
{
	// 取当前 run(活跃)。失败时填 OutSnapshot.Summary 并返回 nullptr。
	UGT_RunContext* GetActiveRunContext(UGT_RunSubsystem* RunSubsystem, FGT_DebugRunSnapshot& OutSnapshot)
	{
		UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
		if (!RunContext || !RunContext->IsRunActive())
		{
			OutSnapshot.Summary = TEXT("No active run");
			return nullptr;
		}
		return RunContext;
	}
}

bool UGT_DebugSubsystem::DebugSetGodMode(bool bEnabled, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();
	UGT_RunContext* RunContext = GetActiveRunContext(GetRunSubsystem(), OutSnapshot);
	if (!RunContext)
	{
		return false;
	}
	RunContext->SetCheatGodMode(bEnabled);
	GetDebugRunSnapshot(OutSnapshot);
	OutSnapshot.Summary = FString::Printf(TEXT("GodMode=%s. %s"), bEnabled ? TEXT("ON") : TEXT("OFF"), *OutSnapshot.Summary);
	return true;
}

bool UGT_DebugSubsystem::DebugAddGold(int32 Amount, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();
	UGT_RunContext* RunContext = GetActiveRunContext(GetRunSubsystem(), OutSnapshot);
	if (!RunContext)
	{
		return false;
	}
	RunContext->CheatAddPendingGold(Amount);
	GetDebugRunSnapshot(OutSnapshot);
	OutSnapshot.Summary = FString::Printf(TEXT("PendingGold += %d. %s"), Amount, *OutSnapshot.Summary);
	return true;
}

bool UGT_DebugSubsystem::DebugGiveItem(FName ItemId, int32 Count, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();
	UGT_RunContext* RunContext = GetActiveRunContext(GetRunSubsystem(), OutSnapshot);
	if (!RunContext)
	{
		return false;
	}
	if (Count < 1)
	{
		Count = 1;
	}
	if (!GT_ItemCatalog::FindItemDef(ItemId))
	{
		GetDebugRunSnapshot(OutSnapshot);
		OutSnapshot.Summary = FString::Printf(TEXT("GiveItem rejected: unknown item id '%s'. %s"), *ItemId.ToString(), *OutSnapshot.Summary);
		return false;
	}
	RunContext->CheatGiveItem(ItemId, Count);
	GetDebugRunSnapshot(OutSnapshot);
	OutSnapshot.Summary = FString::Printf(TEXT("GiveItem %s x%d. %s"), *ItemId.ToString(), Count, *OutSnapshot.Summary);
	return true;
}

bool UGT_DebugSubsystem::DebugSetHp(int32 NewHp, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();
	UGT_RunContext* RunContext = GetActiveRunContext(GetRunSubsystem(), OutSnapshot);
	if (!RunContext)
	{
		return false;
	}
	RunContext->CheatSetPlayerHp(NewHp);
	GetDebugRunSnapshot(OutSnapshot);
	OutSnapshot.Summary = FString::Printf(TEXT("SetHp -> %d. %s"), NewHp, *OutSnapshot.Summary);
	return true;
}

bool UGT_DebugSubsystem::DebugGotoRoomType(const FString& TypeArg, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutSnapshot = FGT_DebugRunSnapshot();
	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	UGT_RunContext* RunContext = GetActiveRunContext(RunSubsystem, OutSnapshot);
	if (!RunContext)
	{
		return false;
	}

	// 解析房型参数(中英都收)。事件子类需先匹配 Event 基底再按哈希定 kind。
	const FString T = TypeArg.ToLower();
	EGT_RoomBaseType WantBase = EGT_RoomBaseType::Unknown;
	EGT_EventKind WantEvent = EGT_EventKind::None;   // None = 任意事件房
	bool bWantExit = false;
	if (T == TEXT("chest") || T == TEXT("宝箱")) { WantBase = EGT_RoomBaseType::Chest; }
	else if (T == TEXT("combat") || T == TEXT("monster") || T == TEXT("怪物") || T == TEXT("怪")) { WantBase = EGT_RoomBaseType::Combat; }
	else if (T == TEXT("exit") || T == TEXT("撤离") || T == TEXT("出口")) { bWantExit = true; }
	else if (T == TEXT("event") || T == TEXT("事件")) { WantBase = EGT_RoomBaseType::Event; }
	else if (T == TEXT("trader") || T == TEXT("旅商")) { WantBase = EGT_RoomBaseType::Event; WantEvent = EGT_EventKind::Trader; }
	else if (T == TEXT("dice") || T == TEXT("赌徒")) { WantBase = EGT_RoomBaseType::Event; WantEvent = EGT_EventKind::Dice; }
	else if (T == TEXT("altar") || T == TEXT("祭坛")) { WantBase = EGT_RoomBaseType::Event; WantEvent = EGT_EventKind::Altar; }
	else if (T == TEXT("trap") || T == TEXT("机关")) { WantBase = EGT_RoomBaseType::Event; WantEvent = EGT_EventKind::Trap; }
	else
	{
		OutSnapshot.Summary = FString::Printf(TEXT("Goto: unknown type '%s'. Use chest/combat/event/exit/trader/dice/altar/trap."), *TypeArg);
		return false;
	}

	// 玩家位置(用于挑最近目标)。
	int32 PlayerX = 0, PlayerY = 0;
	if (const UGT_QueryFacade* QueryFacade = RunSubsystem->GetQueryFacade())
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const FGT_TruthMap& Truth = RunContext->GetTruthMapForDebugOnly();
	const int32 Width = RunContext->GetMapWidth();
	const int32 Height = RunContext->GetMapHeight();
	const int32 Seed = RunContext->GetSeed();

	int32 BestX = INDEX_NONE, BestY = INDEX_NONE, BestDist = MAX_int32;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FGT_TruthCell* Cell = Truth.GetCellConst(X, Y);
			if (!Cell)
			{
				continue;
			}
			bool bMatch = false;
			if (bWantExit)
			{
				bMatch = Cell->bIsExit || Cell->RoomBaseType == EGT_RoomBaseType::Exit;
			}
			else if (Cell->RoomBaseType == WantBase)
			{
				bMatch = (WantBase != EGT_RoomBaseType::Event || WantEvent == EGT_EventKind::None
					|| GT_EventRules::GetEventKindAt(Seed, X, Y) == WantEvent);
			}
			if (!bMatch)
			{
				continue;
			}
			const int32 Dist = FMath::Abs(X - PlayerX) + FMath::Abs(Y - PlayerY);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				BestX = X;
				BestY = Y;
			}
		}
	}

	if (BestX == INDEX_NONE)
	{
		GetDebugRunSnapshot(OutSnapshot);
		OutSnapshot.Summary = FString::Printf(TEXT("Goto: no '%s' room in this run. %s"), *TypeArg, *OutSnapshot.Summary);
		return false;
	}

	if (BestX == PlayerX && BestY == PlayerY)
	{
		GetDebugRunSnapshot(OutSnapshot);
		OutSnapshot.Summary = FString::Printf(TEXT("Goto '%s': already at (%d,%d). %s"), *TypeArg, BestX, BestY, *OutSnapshot.Summary);
		return true;
	}

	// 找目标的一个合法邻格: god 瞬移过去(不触发任何房间), 再真实走入目标格,
	// 让目标房按正常管线 ResolveRoomAt 触发内容(怪物房开战 / 事件房就绪 / 宝箱可搜)。
	const int32 NeighborDX[4] = { -1, 1, 0, 0 };
	const int32 NeighborDY[4] = { 0, 0, -1, 1 };
	int32 AdjX = INDEX_NONE, AdjY = INDEX_NONE;
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		const int32 NX = BestX + NeighborDX[Dir];
		const int32 NY = BestY + NeighborDY[Dir];
		if (RunContext->IsValidMapCoord(NX, NY))
		{
			AdjX = NX;
			AdjY = NY;
			break;
		}
	}

	if (AdjX == INDEX_NONE)
	{
		// 无邻格(1x1 地图): 退化成直接瞬移(不触发内容)。
		return DebugTeleport(BestX, BestY, OutSnapshot);
	}

	FGT_DebugRunSnapshot Discard;
	DebugTeleport(AdjX, AdjY, Discard);
	const bool bMoved = DebugMoveTo(BestX, BestY, OutSnapshot);
	OutSnapshot.Summary = FString::Printf(TEXT("Goto '%s' -> (%d,%d) %s. %s"),
		*TypeArg, BestX, BestY, bMoved ? TEXT("entered") : TEXT("move-failed"), *OutSnapshot.Summary);
	return bMoved;
}

bool UGT_DebugSubsystem::DebugSearch(FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_Search, PlayerX, PlayerY, OutSnapshot);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		// 拒绝时把判定原因(spawn/exit/monster/event/searched/unsafe)带给玩家。
		FName Reason = NAME_None;
		bool bIsChest = false;
		const UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
		if (RunContext)
		{
			RunContext->EvaluateSearchAtPlayer(Reason, bIsChest);
		}
		OutSnapshot.Summary = FString::Printf(TEXT("Search rejected: reason=%s. %s"), *Reason.ToString(), *OutSnapshot.Summary);
	}

	return bAccepted;
}

bool UGT_DebugSubsystem::DebugExtract(FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	return SubmitDebugCommand(GTDebugCommandType_Extract, PlayerX, PlayerY, OutSnapshot);
}

bool UGT_DebugSubsystem::DebugChooseEventOption(FName OptionId, FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_ChooseEventOption, PlayerX, PlayerY, OutSnapshot, OptionId);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		OutSnapshot.Summary = FString::Printf(
			TEXT("ChooseEventOption rejected: expected current Event room and a registry-valid Event option. OptionId=%s. %s"),
			OptionId.IsNone() ? TEXT("RegistryDefault") : *OptionId.ToString(),
			*OutSnapshot.Summary);
	}

	return bAccepted;
}

bool UGT_DebugSubsystem::DebugUseConsumable(FName ItemId, FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	// PayloadId 为空时命令层默认成应急止血贴。
	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_UseConsumable, PlayerX, PlayerY, OutSnapshot, ItemId);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		OutSnapshot.Summary = FString::Printf(
			TEXT("UseConsumable rejected (hp full / none carried / not a consumable). ItemId=%s. %s"),
			ItemId.IsNone() ? TEXT("emergency_bandage") : *ItemId.ToString(),
			*OutSnapshot.Summary);
	}
	return bAccepted;
}

bool UGT_DebugSubsystem::DebugGetEventMenu(FGT_EventMenuView& OutMenu) const
{
	OutMenu = FGT_EventMenuView();
	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
	return RunContext && RunContext->GetEventMenuAtPlayer(OutMenu);
}

bool UGT_DebugSubsystem::DebugResolveCombat(FName ResultId, FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_ResolveCombat, PlayerX, PlayerY, OutSnapshot, ResultId);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		const TCHAR* Reason = ResultId.IsNone()
			? TEXT("expected Combat room and a valid registry default result")
			: TEXT("expected Combat room and a registry-valid placeholder result");
		OutSnapshot.Summary = FString::Printf(
			TEXT("ResolveCombat rejected: %s. Result=%s. %s"),
			Reason,
			ResultId.IsNone() ? TEXT("RegistryDefault") : *ResultId.ToString(),
			*OutSnapshot.Summary);
	}

	return bAccepted;
}

bool UGT_DebugSubsystem::DebugAttack(FGT_DebugRunSnapshot& OutSnapshot)
{
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	const bool bAccepted = SubmitDebugCommand(GTDebugCommandType_Attack, PlayerX, PlayerY, OutSnapshot);
	if (!bAccepted && !OutSnapshot.Summary.Equals(TEXT("No active run")))
	{
		OutSnapshot.Summary = FString::Printf(TEXT("Attack rejected: expected active dummy combat in current Combat room. %s"), *OutSnapshot.Summary);
	}

	return bAccepted;
}

bool UGT_DebugSubsystem::DebugMonsterHit(FGT_DebugRunSnapshot& OutSnapshot)
{
	// 怪物对玩家落地一次攻击(Standard 实时战斗)。无敌帧/命中时机由 RoomView 表现层判定后调入,
	// 内核只在战斗激活、怪未死时扣血; 致死则判负。
	int32 PlayerX = 0;
	int32 PlayerY = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (QueryFacade)
	{
		QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	}

	return SubmitDebugCommand(GTDebugCommandType_MonsterHit, PlayerX, PlayerY, OutSnapshot);
}

bool UGT_DebugSubsystem::GetDebugRunSnapshot(FGT_DebugRunSnapshot& OutSnapshot) const
{
	OutSnapshot = FGT_DebugRunSnapshot();

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (!QueryFacade || !QueryFacade->HasValidRunContext())
	{
		OutSnapshot.Summary = TEXT("No active run");
		return false;
	}

	OutSnapshot.RunState = QueryFacade->GetRunState();
	OutSnapshot.MapWidth = QueryFacade->GetMapWidth();
	OutSnapshot.MapHeight = QueryFacade->GetMapHeight();
	QueryFacade->TryGetPlayerPosition(OutSnapshot.PlayerX, OutSnapshot.PlayerY);

	FGT_TruthCell CurrentTruthCell;
	if (QueryFacade->GetTruthCellDebugOnly(OutSnapshot.PlayerX, OutSnapshot.PlayerY, CurrentTruthCell))
	{
		OutSnapshot.CurrentRoomBaseType = CurrentTruthCell.RoomBaseType;
		OutSnapshot.CurrentRoomContentId = CurrentTruthCell.RoomContentId;
		OutSnapshot.CurrentRoomRuleId = CurrentTruthCell.RoomRuleId;
		OutSnapshot.CurrentRoomInstanceId = CurrentTruthCell.RoomInstanceId;
		OutSnapshot.bCurrentRoomTriggered = CurrentTruthCell.bTriggered;
		OutSnapshot.bCurrentRoomResolved = CurrentTruthCell.bResolved;

		const UGT_ContentRegistry* ContentRegistry = RunSubsystem ? RunSubsystem->GetContentRegistry() : nullptr;
		FGT_RoomContentDef ContentDefinition;
		FGT_RoomRuleDef RuleDefinition;
		FString DefinitionFailureReason;
		if (ContentRegistry
			&& ContentRegistry->FindRoomDefinitions(
				CurrentTruthCell.RoomContentId,
				CurrentTruthCell.RoomRuleId,
				CurrentTruthCell.RoomBaseType,
				ContentDefinition,
				RuleDefinition,
				DefinitionFailureReason))
		{
			OutSnapshot.CurrentRoomContentDisplayName = ContentDefinition.DisplayName;
			OutSnapshot.CurrentRoomContentDebugDescription = ContentDefinition.DebugDescription;
			OutSnapshot.CurrentRoomRuleDisplayName = RuleDefinition.DisplayName;
			OutSnapshot.CurrentRoomRuleDebugDescription = RuleDefinition.DebugDescription;
		}

		if (ContentRegistry && CurrentTruthCell.RoomBaseType == EGT_RoomBaseType::Event)
		{
			TArray<FGT_EventOptionDef> EventOptionDefinitions;
			if (ContentRegistry->GetEventOptionDefsForRoom(
				CurrentTruthCell.RoomContentId,
				CurrentTruthCell.RoomRuleId,
				EventOptionDefinitions,
				DefinitionFailureReason))
			{
				OutSnapshot.CurrentRoomAvailableEventOptions = BuildEventOptionDefsText(EventOptionDefinitions);
			}
		}
		else if (ContentRegistry && CurrentTruthCell.RoomBaseType == EGT_RoomBaseType::Combat)
		{
			TArray<FGT_CombatResultDef> CombatResultDefinitions;
			if (ContentRegistry->GetCombatResultDefsForRoom(
				CurrentTruthCell.RoomContentId,
				CurrentTruthCell.RoomRuleId,
				CombatResultDefinitions,
				DefinitionFailureReason))
			{
				OutSnapshot.CurrentRoomAvailableCombatResults = BuildCombatResultDefsText(CombatResultDefinitions);
			}
		}
	}

	const UGT_EventBus* EventBus = RunSubsystem ? RunSubsystem->GetEventBus() : nullptr;
	const UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
	FGT_CombatRuntimeState CombatState;
	if (RunContext && RunContext->GetCombatStateSnapshot(CombatState))
	{
		OutSnapshot.bCombatActive = CombatState.bCombatActive;
		OutSnapshot.bCombatResolved = CombatState.bCombatResolved;
		OutSnapshot.DummyEnemyHp = CombatState.DummyEnemyHp;
		OutSnapshot.EnemyHp = CombatState.EnemyHp;
		OutSnapshot.EnemyMaxHp = CombatState.EnemyMaxHp;
		OutSnapshot.EnemyName = CombatState.EnemyName;
		OutSnapshot.EnemyPower = CombatState.EnemyPower;
		OutSnapshot.EnemyType = CombatState.EnemyType;
		OutSnapshot.LastCombatResultId = CombatState.LastCombatResultId;
	}

	if (RunContext)
	{
		const FGT_PlayerCombatState& PlayerCombat = RunContext->GetPlayerCombatState();
		OutSnapshot.PlayerHp = PlayerCombat.Hp;
		OutSnapshot.PlayerMaxHp = PlayerCombat.MaxHp;
	}

	FGT_RunSummary RunSummary;
	if (RunContext && RunContext->GetRunSummarySnapshot(RunSummary))
	{
		OutSnapshot.bRunSummaryAvailable = RunSummary.bSummaryAvailable;
		OutSnapshot.RunSummaryOutcome = RunSummary.Outcome;
		OutSnapshot.bRunSummaryExtracted = RunSummary.bExtracted;
		OutSnapshot.RunSummaryFinalPlayerX = RunSummary.FinalPlayerX;
		OutSnapshot.RunSummaryFinalPlayerY = RunSummary.FinalPlayerY;
		OutSnapshot.RunSummaryTotalEventCount = RunSummary.TotalEventCount;
		OutSnapshot.RunSummarySeed = RunSummary.Seed;
		OutSnapshot.RunSummaryMapWidth = RunSummary.MapWidth;
		OutSnapshot.RunSummaryMapHeight = RunSummary.MapHeight;
		OutSnapshot.bRunSummaryCombatActive = RunSummary.bCombatActive;
		OutSnapshot.bRunSummaryCombatResolved = RunSummary.bCombatResolved;
		OutSnapshot.RunSummaryLastCombatResultId = RunSummary.LastCombatResultId;
	}

	OutSnapshot.EventCount = EventBus ? EventBus->GetEventCount() : 0;
	OutSnapshot.Summary = FString::Printf(
		TEXT("RunState=%d, Player=(%d,%d), Size=%dx%d, EventCount=%d, RoomBaseType=%d, RoomContentId=%s, RoomRuleId=%s, RoomContentName=%s, RoomRuleName=%s, AvailableEventOptions=%s, AvailableCombatResults=%s, CombatActive=%s, DummyEnemyHp=%d, EnemyHp=%d/%d, EnemyName=%s, PlayerHp=%d/%d, CombatResolved=%s, LastCombatResult=%s, SummaryAvailable=%s, SummaryOutcome=%s, SummaryFinalPlayer=(%d,%d), SummaryTotalEventCount=%d, RoomTriggered=%s, RoomResolved=%s"),
		static_cast<int32>(OutSnapshot.RunState),
		OutSnapshot.PlayerX,
		OutSnapshot.PlayerY,
		OutSnapshot.MapWidth,
		OutSnapshot.MapHeight,
		OutSnapshot.EventCount,
		static_cast<int32>(OutSnapshot.CurrentRoomBaseType),
		*OutSnapshot.CurrentRoomContentId.ToString(),
		*OutSnapshot.CurrentRoomRuleId.ToString(),
		*OutSnapshot.CurrentRoomContentDisplayName,
		*OutSnapshot.CurrentRoomRuleDisplayName,
		OutSnapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *OutSnapshot.CurrentRoomAvailableEventOptions,
		OutSnapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *OutSnapshot.CurrentRoomAvailableCombatResults,
		OutSnapshot.bCombatActive ? TEXT("true") : TEXT("false"),
		OutSnapshot.DummyEnemyHp,
		OutSnapshot.EnemyHp,
		OutSnapshot.EnemyMaxHp,
		OutSnapshot.EnemyName.IsEmpty() ? TEXT("none") : *OutSnapshot.EnemyName,
		OutSnapshot.PlayerHp,
		OutSnapshot.PlayerMaxHp,
		OutSnapshot.bCombatResolved ? TEXT("true") : TEXT("false"),
		*OutSnapshot.LastCombatResultId.ToString(),
		OutSnapshot.bRunSummaryAvailable ? TEXT("true") : TEXT("false"),
		*OutSnapshot.RunSummaryOutcome.ToString(),
		OutSnapshot.RunSummaryFinalPlayerX,
		OutSnapshot.RunSummaryFinalPlayerY,
		OutSnapshot.RunSummaryTotalEventCount,
		OutSnapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
		OutSnapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"));
	return true;
}

bool UGT_DebugSubsystem::GetDebugMiniMapViewData(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const
{
	OutCells.Reset();
	OutWidth = 0;
	OutHeight = 0;

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_QueryFacade* QueryFacade = RunSubsystem ? RunSubsystem->GetQueryFacade() : nullptr;
	if (!QueryFacade || !QueryFacade->HasValidRunContext())
	{
		return false;
	}

	QueryFacade->BuildMiniMapViewData(OutCells, OutWidth, OutHeight);
	for (FGT_MiniMapCellViewData& Cell : OutCells)
	{
		if (!Cell.bVisible && !Cell.bExplored && !Cell.bScanned)
		{
			continue;
		}

		FGT_TruthCell TruthCell;
		if (QueryFacade->GetTruthCellDebugOnly(Cell.X, Cell.Y, TruthCell))
		{
			Cell.VisibleRoomIcon = GetDebugRoomIcon(TruthCell.RoomBaseType);
		}
	}
	return true;
}

void UGT_DebugSubsystem::GetDebugEventSummary(TArray<FGT_DebugEventSummary>& OutEvents) const
{
	OutEvents.Reset();

	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_EventBus* EventBus = RunSubsystem ? RunSubsystem->GetEventBus() : nullptr;
	const UGT_ContentRegistry* ContentRegistry = RunSubsystem ? RunSubsystem->GetContentRegistry() : nullptr;
	if (!EventBus)
	{
		return;
	}

	TMap<FName, FGT_DebugEventSummary> EventSummaries;
	for (const FGT_GameEvent& Event : EventBus->GetEventHistory())
	{
		if (Event.EventType.IsNone())
		{
			continue;
		}

		FGT_DebugEventSummary& Summary = EventSummaries.FindOrAdd(Event.EventType);
		Summary.EventType = Event.EventType;
		++Summary.Count;
		Summary.LastContentId = Event.ContentId;
		Summary.LastRuleId = Event.RuleId;
		Summary.LastSequenceId = Event.SequenceId;
		Summary.bLastSuccess = Event.bSuccess;
		Summary.LastPayloadText = Event.PayloadText;

		if (ContentRegistry)
		{
			FGT_RoomContentDef ContentDefinition;
			if (ContentRegistry->FindRoomContentDef(Event.ContentId, ContentDefinition))
			{
				Summary.LastContentDisplayName = ContentDefinition.DisplayName;
			}

			FGT_RoomRuleDef RuleDefinition;
			if (ContentRegistry->FindRoomRuleDef(Event.RuleId, RuleDefinition))
			{
				Summary.LastRuleDisplayName = RuleDefinition.DisplayName;
			}

			if (!Summary.LastContentDisplayName.IsEmpty() || !Summary.LastRuleDisplayName.IsEmpty())
			{
				const FString ContentDescription = ContentDefinition.DebugDescription.IsEmpty() ? TEXT("None") : ContentDefinition.DebugDescription;
				const FString RuleDescription = RuleDefinition.DebugDescription.IsEmpty() ? TEXT("None") : RuleDefinition.DebugDescription;
				Summary.LastDebugDescription = FString::Printf(TEXT("Content=%s Rule=%s"), *ContentDescription, *RuleDescription);
			}
		}
	}

	TArray<FName> EventTypes;
	EventSummaries.GetKeys(EventTypes);
	EventTypes.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& EventType : EventTypes)
	{
		OutEvents.Add(EventSummaries.FindRef(EventType));
	}
}

void UGT_DebugSubsystem::GetDebugCommandHelpLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();
	OutLines.Add(TEXT("Graytail manual play console commands:"));
	OutLines.Add(TEXT("  gt.Help - Show commands and recommended demo flow."));
	OutLines.Add(TEXT("  gt.Commands - Alias for gt.Help."));
	OutLines.Add(TEXT("  gt.StartRun [Seed] [Width Height] - Start a debug/basic run."));
	OutLines.Add(TEXT("  gt.GenMap [Seed] [Width Height] - Preview a Standard random map without affecting the active run."));
	OutLines.Add(TEXT("  gt.StartStd [Difficulty] [Seed] - Start a Standard random run at a difficulty (Tutorial/Easy/Standard/Hard/Veteran/Elite/Nightmare)."));
	OutLines.Add(TEXT("  gt.Status - Show run state, player position, current room, and event counts."));
	OutLines.Add(TEXT("  gt.Room - Show current room details and placeholder action hints."));
	OutLines.Add(TEXT("  gt.Move X Y - Move to an adjacent coordinate through the command path."));
	OutLines.Add(TEXT("  gt.Scan X Y - Scan a cell through the command path."));
	OutLines.Add(TEXT("  gt.Search - Search the current room for gold and loot (once per room)."));
	OutLines.Add(TEXT("  gt.UseItem [ItemId] - Use a carried consumable (default emergency_bandage)."));
	OutLines.Add(TEXT("  gt.Teleport X Y - DEBUG godmode: move player directly, no mine/room triggers."));
	OutLines.Add(TEXT("  gt.Bag - Show run inventory: gold, parts, carried items, searched rooms."));
	OutLines.Add(TEXT("  gt.Extract - Attempt extraction at the current cell."));
	OutLines.Add(TEXT("  gt.Summary - Show the latest successful extract summary if one is available."));
	OutLines.Add(TEXT("  gt.Snapshot - Log the raw debug run snapshot."));
	OutLines.Add(TEXT("  gt.Minimap - Log a text minimap."));
	OutLines.Add(TEXT("  gt.Events - Log event type counts and recent payload fields."));
	OutLines.Add(TEXT("  gt.Event - Show the Standard event menu (trader/dice/altar/trap) at the player cell."));
	OutLines.Add(TEXT("  gt.ChooseEventOption [OptionId] - Execute an event option (Standard: real rules; BasicDebug: registry placeholder)."));
	OutLines.Add(TEXT("    Example (Standard): gt.ChooseEventOption bet_small / offer_hp / disarm / sell:dim_capacitor / leave"));
	OutLines.Add(TEXT("    Example (BasicDebug): gt.ChooseEventOption Event_DebugOption_Continue"));
	OutLines.Add(TEXT("  gt.ResolveCombat [Result] - Resolve the Combat placeholder room through registry result data."));
	OutLines.Add(TEXT("    Example: gt.ResolveCombat Combat_DebugResult_Success"));
	OutLines.Add(TEXT("    Example: gt.ResolveCombat Combat_DebugResult_Retreat"));
	OutLines.Add(TEXT("  gt.Attack - Attack active dummy combat once through the command path."));
	OutLines.Add(TEXT("  gt.RunDemo - Run a fixed Event, Combat, Attack, Extract, and Summary demo path."));
	OutLines.Add(TEXT("-- CHEAT (fast testing) --"));
	OutLines.Add(TEXT("  gt.God [0|1] - Toggle invincibility (mine/monster/protocol/trap damage -> 0). No arg = on."));
	OutLines.Add(TEXT("  gt.Goto <type> - Jump to & enter the nearest room of: chest/combat/event/exit/trader/dice/altar/trap."));
	OutLines.Add(TEXT("  gt.AddGold <n> - Add pending (in-run) gold."));
	OutLines.Add(TEXT("  gt.GiveItem <ItemId> [Count] - Add an item to the bag, ignoring capacity."));
	OutLines.Add(TEXT("  gt.SetHp <n> - Set player current HP (clamped 1..MaxHp)."));
	OutLines.Add(TEXT("Recommended manual flow: gt.StartRun -> gt.Minimap -> gt.Move 1 0 -> gt.Scan 1 1 -> gt.Status -> gt.Room."));
	OutLines.Add(TEXT("Event demo path: gt.StartRun -> gt.Move 1 0 -> gt.Move 2 0 -> gt.Move 3 0 -> gt.Move 4 0 -> gt.Move 4 1 -> gt.ChooseEventOption Event_DebugOption_Continue -> gt.Events."));
	OutLines.Add(TEXT("Combat demo path: gt.StartRun -> gt.Move 0 1 -> gt.Move 0 2 -> gt.Move 0 3 -> gt.Move 0 4 -> gt.Move 1 4 -> gt.Attack -> gt.Events."));
	OutLines.Add(TEXT("Extract summary path: gt.RunDemo or move to Exit (9,9) -> gt.Extract -> gt.Summary."));
}

bool UGT_DebugSubsystem::GetDebugInventoryText(FString& OutInventoryText) const
{
	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
	if (!RunContext)
	{
		OutInventoryText = TEXT("gt.Bag: No active run. Start with gt.StartRun or gt.StartStd.");
		return false;
	}

	const FGT_RunInventoryState& Inventory = RunContext->GetRunInventory();

	FString ItemsText;
	for (const FGT_ItemStack& Stack : Inventory.CarriedItems)
	{
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
		if (!ItemsText.IsEmpty())
		{
			ItemsText += TEXT(" / ");
		}
		ItemsText += FString::Printf(TEXT("%s x%d (value %d)"),
			Def ? *Def->DisplayName : *Stack.ItemId.ToString(),
			Stack.Count,
			Def ? Def->Value : 0);
	}

	const FGT_PlayerCombatState& PlayerCombat = RunContext->GetPlayerCombatState();
	FString EnemyText;
	FGT_CombatRuntimeState CombatState;
	if (RunContext->GetCombatStateSnapshot(CombatState) && CombatState.bStandardEnemy && CombatState.bCombatActive)
	{
		EnemyText = FString::Printf(TEXT(" Enemy={%s Power=%d}"), *CombatState.EnemyName, CombatState.EnemyPower);
	}

	const FGT_ProtocolState& Protocol = RunContext->GetProtocolState();

	OutInventoryText = FString::Printf(
		TEXT("gt.Bag: Hp=%d/%d Power=%d%s Protocol=L%d:%d/%d PendingGold=%d SafeGold=%d Parts=%d LooseParts=%d CarriedItemCount=%d CarriedItemValue=%d BackpackWeight=%d/%d SearchedRooms=%d Items={%s}"),
		PlayerCombat.Hp,
		PlayerCombat.MaxHp,
		PlayerCombat.Power,
		*EnemyText,
		Protocol.Level,
		Protocol.Pressure,
		Protocol.MaxPressure,
		Inventory.PendingGold,
		Inventory.SafeGold,
		Inventory.Parts,
		Inventory.GetLooseParts(),
		Inventory.GetCarriedItemCount(),
		GT_ItemCatalog::GetCarriedItemsValue(Inventory.CarriedItems),
		Inventory.GetCurrentWeight(),
		Inventory.BackpackCapacity,
		Inventory.SearchedRooms.Num(),
		ItemsText.IsEmpty() ? TEXT("none") : *ItemsText);
	return true;
}

bool UGT_DebugSubsystem::GetDebugStatusText(FString& OutStatus) const
{
	FGT_DebugRunSnapshot Snapshot;
	if (!GetDebugRunSnapshot(Snapshot))
	{
		OutStatus = TEXT("gt.Status: No active run. Start with gt.StartRun.");
		return false;
	}

	TArray<FGT_DebugEventSummary> EventSummary;
	GetDebugEventSummary(EventSummary);
	OutStatus = FString::Printf(
		TEXT("gt.Status: RunState=%s PlayerPosition=(%d,%d) RoomBaseType=%s RoomContentId=%s RoomRuleId=%s ContentName=%s RuleName=%s ContentDescription=%s RuleDescription=%s EventOptions=%s CombatResults=%s CombatActive=%s DummyEnemyHp=%d CombatResolved=%s LastCombatResult=%s SummaryAvailable=%s SummaryOutcome=%s SummaryFinalPlayer=(%d,%d) SummaryTotalEventCount=%d Triggered=%s Resolved=%s EventCount=%d Events={%s}"),
		*GetRunStateText(Snapshot.RunState),
		Snapshot.PlayerX,
		Snapshot.PlayerY,
		*GetRoomBaseTypeText(Snapshot.CurrentRoomBaseType),
		*Snapshot.CurrentRoomContentId.ToString(),
		*Snapshot.CurrentRoomRuleId.ToString(),
		*Snapshot.CurrentRoomContentDisplayName,
		*Snapshot.CurrentRoomRuleDisplayName,
		*Snapshot.CurrentRoomContentDebugDescription,
		*Snapshot.CurrentRoomRuleDebugDescription,
		Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions,
		Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults,
		Snapshot.bCombatActive ? TEXT("true") : TEXT("false"),
		Snapshot.DummyEnemyHp,
		Snapshot.bCombatResolved ? TEXT("true") : TEXT("false"),
		*Snapshot.LastCombatResultId.ToString(),
		Snapshot.bRunSummaryAvailable ? TEXT("true") : TEXT("false"),
		*Snapshot.RunSummaryOutcome.ToString(),
		Snapshot.RunSummaryFinalPlayerX,
		Snapshot.RunSummaryFinalPlayerY,
		Snapshot.RunSummaryTotalEventCount,
		Snapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
		Snapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"),
		Snapshot.EventCount,
		*BuildDebugEventSummaryText(EventSummary));
	return true;
}

bool UGT_DebugSubsystem::GetDebugRoomText(FString& OutRoomText) const
{
	FGT_DebugRunSnapshot Snapshot;
	if (!GetDebugRunSnapshot(Snapshot))
	{
		OutRoomText = TEXT("gt.Room: No active run. Start with gt.StartRun.");
		return false;
	}

	FString Hint = TEXT("No placeholder room action is available here.");
	if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event)
	{
		// Standard 模式: 真实事件(旅商/赌徒/祭坛/机关)的进入/完成文案自带 "按 T" 提示。
		FGT_EventMenuView EventMenu;
		if (DebugGetEventMenu(EventMenu) && EventMenu.bAvailable)
		{
			Hint = EventMenu.Description;
		}
		else
		{
			Hint = FString::Printf(
				TEXT("Event placeholder action: gt.ChooseEventOption [OptionId]. Available=%s."),
				Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions);
		}
	}
	else if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat)
	{
		Hint = FString::Printf(
			TEXT("Combat placeholder actions: gt.Attack or gt.ResolveCombat [Result]. Available=%s."),
			Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults);
	}
	else if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Chest)
	{
		Hint = TEXT("Chest room action: gt.Search to open the chest (better loot, once only).");
	}

	OutRoomText = FString::Printf(
		TEXT("gt.Room: Position=(%d,%d) RoomBaseType=%s RoomContentId=%s RoomRuleId=%s RoomInstanceId=%s ContentName=%s RuleName=%s ContentDescription=%s RuleDescription=%s EventOptions=%s CombatResults=%s CombatActive=%s DummyEnemyHp=%d CombatResolved=%s LastCombatResult=%s Triggered=%s Resolved=%s Hint=%s"),
		Snapshot.PlayerX,
		Snapshot.PlayerY,
		*GetRoomBaseTypeText(Snapshot.CurrentRoomBaseType),
		*Snapshot.CurrentRoomContentId.ToString(),
		*Snapshot.CurrentRoomRuleId.ToString(),
		*Snapshot.CurrentRoomInstanceId.ToString(),
		*Snapshot.CurrentRoomContentDisplayName,
		*Snapshot.CurrentRoomRuleDisplayName,
		*Snapshot.CurrentRoomContentDebugDescription,
		*Snapshot.CurrentRoomRuleDebugDescription,
		Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions,
		Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults,
		Snapshot.bCombatActive ? TEXT("true") : TEXT("false"),
		Snapshot.DummyEnemyHp,
		Snapshot.bCombatResolved ? TEXT("true") : TEXT("false"),
		*Snapshot.LastCombatResultId.ToString(),
		Snapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
		Snapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"),
		*Hint);
	return true;
}

bool UGT_DebugSubsystem::GetDebugRunSummaryText(FString& OutSummaryText) const
{
	const UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	const UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
	if (!RunContext)
	{
		OutSummaryText = TEXT("gt.Summary: SummaryAvailable=false Outcome=None NoRunSummary Reason=No active run. Start with gt.StartRun.");
		return false;
	}

	FGT_RunSummary RunSummary;
	const bool bHasSummary = RunContext->GetRunSummarySnapshot(RunSummary);
	OutSummaryText = FString::Printf(TEXT("gt.Summary: %s"), *BuildRunSummaryText(RunSummary));
	return bHasSummary;
}

bool UGT_DebugSubsystem::DebugRunDemo(TArray<FString>& OutLogLines, FGT_DebugRunSnapshot& OutSnapshot)
{
	OutLogLines.Reset();
	OutSnapshot = FGT_DebugRunSnapshot();

	auto AppendStep = [&OutLogLines](const FString& StepName, bool bAccepted, const FGT_DebugRunSnapshot& Snapshot)
	{
		OutLogLines.Add(FString::Printf(TEXT("%s %s: %s"), *StepName, bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary));
	};

	auto MovePath = [this, &OutLogLines, &OutSnapshot, &AppendStep](const TCHAR* Label, const TArray<FIntPoint>& Path) -> bool
	{
		for (const FIntPoint& Coord : Path)
		{
			const bool bMoved = DebugMoveTo(Coord.X, Coord.Y, OutSnapshot);
			AppendStep(FString::Printf(TEXT("%s gt.Move %d %d"), Label, Coord.X, Coord.Y), bMoved, OutSnapshot);
			if (!bMoved)
			{
				return false;
			}
		}

		return true;
	};

	bool bDemoOk = DebugStartNewRun(42345, 10, 10, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo StartRun"), bDemoOk, OutSnapshot);
	if (!bDemoOk)
	{
		return false;
	}

	const bool bScanned = DebugScanCell(1, 1, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo Scan 1 1"), bScanned, OutSnapshot);
	bDemoOk = bDemoOk && bScanned;

	const TArray<FIntPoint> EventPath = {
		FIntPoint(1, 0),
		FIntPoint(2, 0),
		FIntPoint(3, 0),
		FIntPoint(4, 0),
		FIntPoint(4, 1)
	};
	bDemoOk = bDemoOk && MovePath(TEXT("gt.RunDemo EventPath"), EventPath);

	const bool bChooseEvent = bDemoOk && DebugChooseEventOption(GTEventOption_DefaultContinue, OutSnapshot);
	AppendStep(TEXT("gt.RunDemo ChooseEventOption Event_DebugOption_Continue"), bChooseEvent, OutSnapshot);
	bDemoOk = bDemoOk && bChooseEvent;

	const TArray<FIntPoint> CombatPath = {
		FIntPoint(3, 1),
		FIntPoint(2, 1),
		FIntPoint(1, 1),
		FIntPoint(1, 2),
		FIntPoint(1, 3),
		FIntPoint(1, 4)
	};
	bDemoOk = bDemoOk && MovePath(TEXT("gt.RunDemo CombatPath"), CombatPath);

	const bool bAttack = bDemoOk && DebugAttack(OutSnapshot);
	AppendStep(TEXT("gt.RunDemo Attack"), bAttack, OutSnapshot);
	bDemoOk = bDemoOk && bAttack;

	const TArray<FIntPoint> ExitPath = {
		FIntPoint(2, 4),
		FIntPoint(3, 4),
		FIntPoint(4, 4),
		FIntPoint(5, 4),
		FIntPoint(6, 4),
		FIntPoint(7, 4),
		FIntPoint(8, 4),
		FIntPoint(9, 4),
		FIntPoint(9, 5),
		FIntPoint(9, 6),
		FIntPoint(9, 7),
		FIntPoint(9, 8),
		FIntPoint(9, 9)
	};
	bDemoOk = bDemoOk && MovePath(TEXT("gt.RunDemo ExitPath"), ExitPath);

	const bool bExtract = bDemoOk && DebugExtract(OutSnapshot);
	AppendStep(TEXT("gt.RunDemo Extract"), bExtract, OutSnapshot);
	bDemoOk = bDemoOk && bExtract;

	FString RunSummaryText;
	const bool bHasRunSummary = GetDebugRunSummaryText(RunSummaryText);
	OutLogLines.Add(RunSummaryText);

	TArray<FGT_DebugEventSummary> EventSummary;
	GetDebugEventSummary(EventSummary);
	const bool bHasEventResolved = HasDebugEventSummaryType(EventSummary, GTDebugEventType_EventResolved);
	const bool bHasCombatResolved = HasDebugEventSummaryType(EventSummary, GTDebugEventType_CombatResolved);
	OutLogLines.Add(FString::Printf(
		TEXT("gt.RunDemo Events: %s EventResolved=%s CombatResolved=%s"),
		*BuildDebugEventSummaryText(EventSummary),
		bHasEventResolved ? TEXT("true") : TEXT("false"),
		bHasCombatResolved ? TEXT("true") : TEXT("false")));

	FString StatusText;
	GetDebugStatusText(StatusText);
	OutLogLines.Add(StatusText);
	return bDemoOk && bHasEventResolved && bHasCombatResolved && bHasRunSummary;
}

void UGT_DebugSubsystem::GetCurrentMiniMapDebugCells(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const
{
	GetDebugMiniMapViewData(OutCells, OutWidth, OutHeight);
}

bool UGT_DebugSubsystem::RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults)
{
	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	UGT_RuntimeSmokeValidator* Validator = NewObject<UGT_RuntimeSmokeValidator>(this);
	Validator->Initialize(RunSubsystem);
	Validator->SetDebugSubsystem(this);
	return Validator->RunMinimalMovementSmokeTest(OutResults);
}

UGT_RunSubsystem* UGT_DebugSubsystem::GetRunSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
}

bool UGT_DebugSubsystem::SubmitDebugCommand(FName CommandType, int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot, FName PayloadId)
{
	OutSnapshot = FGT_DebugRunSnapshot();

	UGT_RunSubsystem* RunSubsystem = GetRunSubsystem();
	if (!RunSubsystem)
	{
		OutSnapshot.Summary = TEXT("RunSubsystem is not valid");
		return false;
	}

	const UGT_QueryFacade* QueryFacade = RunSubsystem->GetQueryFacade();
	const FName PlayerActorId = QueryFacade && QueryFacade->HasValidRunContext()
		? QueryFacade->GetPlayerActorId()
		: GTDebugActorId_Player;

	FGT_Command Command;
	Command.CommandType = CommandType;
	Command.SourceActorId = PlayerActorId;
	Command.TargetActorId = PlayerActorId;
	Command.TargetX = X;
	Command.TargetY = Y;
	Command.PayloadId = PayloadId;

	const bool bAccepted = RunSubsystem->SubmitCommand(Command);
	GetDebugRunSnapshot(OutSnapshot);
	return bAccepted;
}

void UGT_DebugSubsystem::BuildStandardMapPreviewLines(int32 Seed, int32 Width, int32 Height, TArray<FString>& OutLines) const
{
	OutLines.Reset();

	FGT_MapGenerationSpec Spec;
	Spec.MapMode = EGT_MapMode::Standard;
	Spec.Seed = Seed;
	Spec.Width = Width;
	Spec.Height = Height;

	FGT_MapGenerationResult Result;
	if (!UGT_MapGenerator::GenerateMap(Spec, Result))
	{
		OutLines.Add(FString::Printf(
			TEXT("gt.GenMap failed: generator returned false for mode=Standard seed=%d size=%dx%d"),
			Seed, Width, Height));
		return;
	}

	const FGT_TruthMap& Map = Result.TruthMap;
	OutLines.Add(FString::Printf(
		TEXT("gt.GenMap: mode=Standard seed=%d size=%dx%d legend S=spawn E=exit *=mine C=combat V=event B=chest 0-8=adjacent mines"),
		Result.Seed, Map.Width, Map.Height));

	int32 TotalMines = 0;
	for (int32 Y = 0; Y < Map.Height; ++Y)
	{
		FString Row;
		Row.Reserve(Map.Width);
		for (int32 X = 0; X < Map.Width; ++X)
		{
			const FGT_TruthCell* Cell = Map.GetCellConst(X, Y);
			TCHAR Symbol = TEXT('?');
			if (X == Result.SpawnCoord.X && Y == Result.SpawnCoord.Y)
			{
				// 出生点随机(Standard 模式对齐 Lua), 位置由生成结果给出。
				Symbol = TEXT('S');
			}
			else if (Cell && Cell->bIsExit)
			{
				Symbol = TEXT('E');
			}
			else if (Cell && Cell->bHasMine)
			{
				Symbol = TEXT('*');
				++TotalMines;
			}
			else if (Cell && Cell->RoomBaseType == EGT_RoomBaseType::Combat)
			{
				Symbol = TEXT('C');
			}
			else if (Cell && Cell->RoomBaseType == EGT_RoomBaseType::Event)
			{
				Symbol = TEXT('V');
			}
			else if (Cell && Cell->RoomBaseType == EGT_RoomBaseType::Chest)
			{
				Symbol = TEXT('B');
			}
			else
			{
				int32 AdjacentMines = 0;
				Map.CountAdjacentMines8(X, Y, AdjacentMines);
				Symbol = static_cast<TCHAR>('0' + FMath::Clamp(AdjacentMines, 0, 8));
			}
			Row.AppendChar(Symbol);
		}
		OutLines.Add(FString::Printf(TEXT("gt.GenMap row %02d: %s"), Y, *Row));
	}

	OutLines.Add(FString::Printf(TEXT("gt.GenMap: total mines=%d"), TotalMines));
}