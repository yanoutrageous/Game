#include "Domains/Events/GT_EventRules.h"

namespace GT_EventRules
{
	EGT_EventKind GetEventKindAt(int32 Seed, int32 X, int32 Y)
	{
		// 对齐 Lua: hash = (x*73 + y*137 + seed*31) % 10000; roll = hash % totalWeight(100)。
		// 用 int64 防溢出, 双取模保证非负。
		const int64 Hash = (static_cast<int64>(X) * 73
			+ static_cast<int64>(Y) * 137
			+ static_cast<int64>(Seed) * 31) % 10000;
		const int32 Roll = static_cast<int32>(((Hash % 100) + 100) % 100);

		// 权重表顺序与 Lua EVENT_TYPES 一致: trader 30 / dice 25 / altar 25 / trap 20。
		if (Roll < 30)
		{
			return EGT_EventKind::Trader;
		}
		if (Roll < 55)
		{
			return EGT_EventKind::Dice;
		}
		if (Roll < 80)
		{
			return EGT_EventKind::Altar;
		}
		return EGT_EventKind::Trap;
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

	int32 GetTraderSaleValue(int32 BaseValue)
	{
		if (BaseValue <= 0)
		{
			return 0;
		}
		// floor(baseValue * 0.75): 用整数乘除避免浮点误差。
		return FMath::Max(1, BaseValue * 3 / 4);
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
