#include "CoreMinimal.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailMetaCmd, Log, All);

namespace
{
	// 取 Meta 子系统(带 GEngine 兜底, 适配无头 -game)。仿 GT_EditorManualPlayCommands 的 FindDebugSubsystem。
	UGT_MetaProgressSubsystem* FindMetaSubsystem(UWorld* World)
	{
		if (World)
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UGT_MetaProgressSubsystem* S = GI->GetSubsystem<UGT_MetaProgressSubsystem>())
				{
					return S;
				}
			}
		}
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.OwningGameInstance)
				{
					if (UGT_MetaProgressSubsystem* S = Ctx.OwningGameInstance->GetSubsystem<UGT_MetaProgressSubsystem>())
					{
						return S;
					}
				}
			}
		}
		return nullptr;
	}

	bool ParseInt(const TArray<FString>& Args, int32 Index, int32& Out)
	{
		return Args.IsValidIndex(Index) && LexTryParseString(Out, *Args[Index]);
	}

	FName ArgName(const TArray<FString>& Args, int32 Index)
	{
		return Args.IsValidIndex(Index) ? FName(*Args[Index]) : NAME_None;
	}

	void PrintState(UGT_MetaProgressSubsystem* S)
	{
		const FGT_MetaProgressState& St = S->GetState();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta: gold=%d owned=[%s] equipped=[%s] talents=[%s]"),
			St.Gold,
			*FString::JoinBy(St.OwnedItems, TEXT(","), [](const FName& N) { return N.ToString(); }),
			*FString::JoinBy(St.EquippedItems, TEXT(","), [](const FName& N) { return N.ToString(); }),
			*FString::JoinBy(St.UnlockedTalents, TEXT(","), [](const FName& N) { return N.ToString(); }));
		for (const FGT_WarehouseEntry& E : St.Warehouse)
		{
			UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta warehouse: %s x%d value=%d src=%s"),
				*E.ItemId.ToString(), E.Count, E.Value, *E.Source.ToString());
		}
		for (const TPair<FName, int32>& P : St.ConsumableStock)
		{
			UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta consumable: %s x%d (loadout=%d)"),
				*P.Key.ToString(), P.Value, St.LoadoutConsumables.FindRef(P.Key));
		}
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.Meta stats: runs=%d extractions=%d goldEarned=%d recoveryItems=%d"),
			St.Stats.TotalRuns, St.Stats.TotalExtractions, St.Stats.TotalGoldEarned, St.Recovery.TotalItems);
	}

	void HandleMeta(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.Meta failed: no Meta subsystem (need game world).")); return; }
		PrintState(S);
	}

	void HandleMetaBuy(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaBuy failed: no Meta subsystem.")); return; }
		FName Err;
		const bool bOk = S->BuyItem(ArgName(Args, 0), Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBuy %s: %s%s"),
			*ArgName(Args, 0).ToString(), bOk ? TEXT("ok") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaEquip(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaEquip failed: no Meta subsystem.")); return; }
		FName Err;
		const bool bOk = S->ToggleEquip(ArgName(Args, 0), Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaEquip %s: %s%s"),
			*ArgName(Args, 0).ToString(), bOk ? TEXT("toggled") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaTalent(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaTalent failed: no Meta subsystem.")); return; }
		FName Err;
		const bool bOk = S->UnlockTalent(ArgName(Args, 0), Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaTalent %s: %s%s"),
			*ArgName(Args, 0).ToString(), bOk ? TEXT("unlocked") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaBuyConsumable(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaBuyConsumable failed: no Meta subsystem.")); return; }
		int32 Count = 1; ParseInt(Args, 1, Count);
		FName Err;
		const bool bOk = S->BuyConsumable(ArgName(Args, 0), Count, Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBuyConsumable %s x%d: %s%s"),
			*ArgName(Args, 0).ToString(), Count, bOk ? TEXT("ok") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString());
		PrintState(S);
	}

	void HandleMetaLoadout(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaLoadout failed: no Meta subsystem.")); return; }
		int32 Count = 0; ParseInt(Args, 1, Count);
		S->SetLoadoutConsumable(ArgName(Args, 0), Count);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaLoadout %s -> %d"), *ArgName(Args, 0).ToString(),
			S->GetLoadout().FindRef(ArgName(Args, 0)));
		PrintState(S);
	}

	void HandleMetaSell(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaSell failed: no Meta subsystem.")); return; }
		int32 Count = 1; ParseInt(Args, 1, Count);
		int32 Gold = 0; FName Err;
		const bool bOk = S->SellWarehouseItem(ArgName(Args, 0), Count, Gold, Err);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaSell %s x%d: %s%s gold+=%d"),
			*ArgName(Args, 0).ToString(), Count, bOk ? TEXT("ok") : TEXT("failed "), bOk ? TEXT("") : *Err.ToString(), bOk ? Gold : 0);
		PrintState(S);
	}

	void HandleMetaBonus(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaBonus failed: no Meta subsystem.")); return; }
		const FGT_EquipBonus B = S->GetEquipBonus();
		const FGT_TalentEffects T = S->GetTalentEffects();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBonus equip: HP+%d Power+%d MineImmunity=%d MineDmgReduce=%d ExitHint=%d Search+%d%%"),
			B.BonusHP, B.BonusPower, B.bMineImmunity ? 1 : 0, B.MineDmgReduce, B.bShowExitHint ? 1 : 0, B.SearchBonus);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaBonus talent: MineDmgReduce=%d Flee+%d FailGold+%d TradePrice=%d MapHighlight=%d"),
			T.MineDmgReduce, T.MonsterFleeBonus, T.FailureGoldBonus, T.TradePrice, T.bMapHighlight ? 1 : 0);
	}

	void HandleMetaAddGold(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaAddGold failed: no Meta subsystem.")); return; }
		int32 Amount = 0; ParseInt(Args, 0, Amount);
		S->AddGold(Amount);
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaAddGold %d -> gold=%d"), Amount, S->GetGold());
	}

	void HandleMetaReset(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaReset failed: no Meta subsystem.")); return; }
		S->GMReset();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaReset: cleared."));
	}

	void HandleMetaSave(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaSave failed: no Meta subsystem.")); return; }
		S->Save();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaSave: written."));
	}

	void HandleMetaLoad(const TArray<FString>& Args, UWorld* World)
	{
		UGT_MetaProgressSubsystem* S = FindMetaSubsystem(World);
		if (!S) { UE_LOG(LogGraytailMetaCmd, Warning, TEXT("gt.MetaLoad failed: no Meta subsystem.")); return; }
		S->Load();
		UE_LOG(LogGraytailMetaCmd, Display, TEXT("gt.MetaLoad: reloaded."));
		PrintState(S);
	}

	FAutoConsoleCommandWithWorldAndArgs GTMetaCmd(TEXT("gt.Meta"), TEXT("打印局外元进度全状态"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMeta));
	FAutoConsoleCommandWithWorldAndArgs GTMetaBuyCmd(TEXT("gt.MetaBuy"), TEXT("买装备: gt.MetaBuy <id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaBuy));
	FAutoConsoleCommandWithWorldAndArgs GTMetaEquipCmd(TEXT("gt.MetaEquip"), TEXT("装备/卸下: gt.MetaEquip <id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaEquip));
	FAutoConsoleCommandWithWorldAndArgs GTMetaTalentCmd(TEXT("gt.MetaTalent"), TEXT("解锁天赋: gt.MetaTalent <id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaTalent));
	FAutoConsoleCommandWithWorldAndArgs GTMetaBuyConsumableCmd(TEXT("gt.MetaBuyConsumable"), TEXT("买消耗品: gt.MetaBuyConsumable <id> [n]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaBuyConsumable));
	FAutoConsoleCommandWithWorldAndArgs GTMetaLoadoutCmd(TEXT("gt.MetaLoadout"), TEXT("配 loadout: gt.MetaLoadout <id> <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaLoadout));
	FAutoConsoleCommandWithWorldAndArgs GTMetaSellCmd(TEXT("gt.MetaSell"), TEXT("卖仓库物品: gt.MetaSell <id> [n]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaSell));
	FAutoConsoleCommandWithWorldAndArgs GTMetaBonusCmd(TEXT("gt.MetaBonus"), TEXT("打印装备/天赋加成"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaBonus));
	FAutoConsoleCommandWithWorldAndArgs GTMetaAddGoldCmd(TEXT("gt.MetaAddGold"), TEXT("GM加币: gt.MetaAddGold <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaAddGold));
	FAutoConsoleCommandWithWorldAndArgs GTMetaResetCmd(TEXT("gt.MetaReset"), TEXT("GM清空存档"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaReset));
	FAutoConsoleCommandWithWorldAndArgs GTMetaSaveCmd(TEXT("gt.MetaSave"), TEXT("强制存档"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaSave));
	FAutoConsoleCommandWithWorldAndArgs GTMetaLoadCmd(TEXT("gt.MetaLoad"), TEXT("强制读档"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMetaLoad));
}
