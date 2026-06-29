#include "Domains/Events/GT_EventRules.h"

#include "Data/GT_GameDataSubsystem.h"

namespace
{
	const FGT_LootEventsBalanceFile& GetBalance()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Event rules accessed without valid game data."));
		return Snapshot->LootEvents;
	}

	EGT_EventKind ParseEventKind(const FString& Id)
	{
		if (Id == TEXT("trader")) return EGT_EventKind::Trader;
		if (Id == TEXT("dice")) return EGT_EventKind::Dice;
		if (Id == TEXT("altar")) return EGT_EventKind::Altar;
		if (Id == TEXT("trap")) return EGT_EventKind::Trap;
		return EGT_EventKind::None;
	}
}

namespace GT_EventRules
{
	const FGT_GamblerBalanceConfig& GetGambler()
	{
		return GetBalance().Gambler;
	}

	const TArray<FGT_AltarStepConfig>& GetAltarSteps()
	{
		return GetBalance().AltarSteps;
	}

	EGT_ItemQuality GetAltarRewardQuality(int32 StepIndex)
	{
		const TArray<FGT_AltarStepConfig>& Steps = GetAltarSteps();
		if (!Steps.IsValidIndex(StepIndex))
		{
			return EGT_ItemQuality::None;
		}
		const FString& Quality = Steps[StepIndex].RewardQuality;
		if (Quality == TEXT("low")) return EGT_ItemQuality::Low;
		if (Quality == TEXT("common")) return EGT_ItemQuality::Common;
		if (Quality == TEXT("rare")) return EGT_ItemQuality::Rare;
		if (Quality == TEXT("precious")) return EGT_ItemQuality::Precious;
		if (Quality == TEXT("abnormal")) return EGT_ItemQuality::Abnormal;
		return EGT_ItemQuality::None;
	}

	const FGT_TrapBalanceConfig& GetTrap()
	{
		return GetBalance().Trap;
	}

	const FGT_LuckyCoinBalanceConfig& GetLuckyCoin()
	{
		return GetBalance().LuckyCoin;
	}

	EGT_EventKind GetEventKindAt(int32 Seed, int32 X, int32 Y)
	{
		// 对齐 Lua: hash = (x*73 + y*137 + seed*31) % 10000; roll = hash % totalWeight(100)。
		// 用 int64 防溢出, 双取模保证非负。
		const int64 Hash = (static_cast<int64>(X) * 73
			+ static_cast<int64>(Y) * 137
			+ static_cast<int64>(Seed) * 31) % 10000;
		const int32 Roll = static_cast<int32>(((Hash % 100) + 100) % 100);

		int32 Threshold = 0;
		for (const FGT_EventWeightConfig& Entry : GetBalance().EventWeights)
		{
			Threshold += Entry.Weight;
			if (Roll < Threshold)
			{
				return ParseEventKind(Entry.Id);
			}
		}
		return EGT_EventKind::None;
	}

	int32 RollDiceAt(int32 Seed, int32 X, int32 Y, int32 PendingGold)
	{
		// 对齐 Lua: (x*197 + y*83 + seed*59 + pendingGold) % 6 + 1。
		const int64 Hash = static_cast<int64>(X) * 197
			+ static_cast<int64>(Y) * 83
			+ static_cast<int64>(Seed) * 59
			+ static_cast<int64>(PendingGold);
		return static_cast<int32>(((Hash % 6) + 6) % 6) + 1;
	}

	int32 GetTraderSaleValue(int32 BaseValue, int32 BonusPercent)
	{
		if (BaseValue <= 0)
		{
			return 0;
		}
		const int32 Pct = FMath::Max(0, BonusPercent);
		return FMath::Max(
			1,
			BaseValue * GetBalance().Trader.BaseSalePercent * (100 + Pct) / 10000);
	}

	FString GetEventTitle(EGT_EventKind Kind)
	{
		switch (Kind)
		{
		case EGT_EventKind::Trader: return TEXT("狐狸旅商");
		case EGT_EventKind::Dice: return TEXT("赌徒区");
		case EGT_EventKind::Altar: return TEXT("祭坛区");
		case EGT_EventKind::Trap: return TEXT("机关房");
		default: return TEXT("事件");
		}
	}

	FString GetEventEnterText(EGT_EventKind Kind)
	{
		switch (Kind)
		{
		case EGT_EventKind::Trader: return TEXT("检测到非公司交易对象。按 T 与旅商交易。");
		case EGT_EventKind::Dice: return TEXT("赌徒把骰盅推到你面前。按 T 下注。");
		case EGT_EventKind::Altar: return TEXT("祭坛仍在低声运转。按 T 献祭生命。");
		case EGT_EventKind::Trap: return TEXT("机关房的旧装置还在咬合。按 T 尝试处理。");
		default: return TEXT("发现事件房。");
		}
	}

	FString GetEventDoneText(EGT_EventKind Kind)
	{
		switch (Kind)
		{
		case EGT_EventKind::Trader: return TEXT("旅商收摊了。公司暂未记录此事。");
		case EGT_EventKind::Dice: return TEXT("赌徒已经离开。");
		case EGT_EventKind::Altar: return TEXT("祭坛沉默了。");
		case EGT_EventKind::Trap: return TEXT("机关已经停机。");
		default: return TEXT("事件已完成。");
		}
	}

	FString GetEventHotkeyCaption(EGT_EventKind Kind)
	{
		// 文案对齐 Lua DungeonRoom.lua evtVisual.hint(冒号形式)。
		switch (Kind)
		{
		case EGT_EventKind::Trader: return TEXT("T:与旅商交易");
		case EGT_EventKind::Dice: return TEXT("T:与赌徒交涉");
		case EGT_EventKind::Altar: return TEXT("T:查看祭坛");
		case EGT_EventKind::Trap: return TEXT("T:处理机关");
		default: return TEXT("T:处理事件");
		}
	}

	FString GetEventShortLabel(EGT_EventKind Kind)
	{
		// NPC 脚下名字标签(对齐 Lua evtVisual.label)。
		switch (Kind)
		{
		case EGT_EventKind::Trader: return TEXT("旅商");
		case EGT_EventKind::Dice: return TEXT("赌徒");
		case EGT_EventKind::Altar: return TEXT("祭坛");
		case EGT_EventKind::Trap: return TEXT("机关");
		default: return TEXT("事件");
		}
	}
}
