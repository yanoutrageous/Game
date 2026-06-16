#pragma once

#include "CoreMinimal.h"
#include "Domains/Events/GT_EventTypes.h"

// 事件房规则常量与确定性随机(逐位对齐 Lua EventSystem.lua / Balance.lua)。
// 纯函数, 不持有状态; 运行态(完成标记/祭坛档数)在 UGT_RunContext 上。
namespace GT_EventRules
{
	// 赌徒(对齐 Balance.gambler): 押 20 待结算币, 1-4 输光, 5 净 +20, 6 净 +60。
	namespace Gambler
	{
		constexpr int32 Bet = 20;
		constexpr int32 LoseMaxRoll = 4;
		constexpr int32 SmallWinNet = 20;
		constexpr int32 BigWinRoll = 6;
		constexpr int32 BigWinNet = 60;
	}

	// 祭坛(对齐 Balance.altar): 5 档递增献祭, 奖励待结算币 + 1 件对应品质回收物, 不加协议压力。
	namespace Altar
	{
		constexpr int32 StepCount = 5;
		constexpr int32 HpCosts[StepCount] = { 10, 15, 25, 35, 50 };
		constexpr int32 RewardGold[StepCount] = { 8, 12, 18, 24, 35 };
		constexpr EGT_ItemQuality RewardQuality[StepCount] = {
			EGT_ItemQuality::Low,
			EGT_ItemQuality::Common,
			EGT_ItemQuality::Rare,
			EGT_ItemQuality::Precious,
			EGT_ItemQuality::Abnormal,
		};
	}

	// 机关(对齐 Lua _ExecTrap): 战斗力检定, 成功结金币+2件回收物, 失败扣血+加压。
	namespace Trap
	{
		constexpr int32 PowerRequirement = 8;
		constexpr int32 SuccessGold = 25;
		constexpr int32 FailHpLoss = 1;
		constexpr int32 FailPressure = 5;
	}

	// 事件类型指派(对齐 Lua GetEventType): (x*73 + y*137 + seed*31) % 10000 % 100,
	// 权重段 旅商[0,30) / 赌徒[30,55) / 祭坛[55,80) / 机关[80,100)。
	GRAYTAIL_API EGT_EventKind GetEventKindAt(int32 Seed, int32 X, int32 Y);

	// 赌徒骰点 1-6(对齐 Lua _ExecDice): 掺入下注时的待结算币, 同格重赌结果可变。
	GRAYTAIL_API int32 RollDiceAt(int32 Seed, int32 X, int32 Y, int32 PendingGold);

	// 旅商收购价(对齐 Balance.TraderSaleValue): floor(基础价 * 0.75), 有价物最低 1。
	// BonusPercent = 议价天赋的收购价加成(S4); 0 时与原值完全一致(基础不变)。
	GRAYTAIL_API int32 GetTraderSaleValue(int32 BaseValue, int32 BonusPercent = 0);

	// 事件文案(对齐 Lua GameText.events)。
	GRAYTAIL_API FString GetEventTitle(EGT_EventKind Kind);
	GRAYTAIL_API FString GetEventEnterText(EGT_EventKind Kind);
	GRAYTAIL_API FString GetEventDoneText(EGT_EventKind Kind);
	// 房间内 NPC 头顶提示(对齐 DungeonRoom.lua evtVisual.hint): "T:与旅商交易" 等。
	GRAYTAIL_API FString GetEventHotkeyCaption(EGT_EventKind Kind);

	// NPC 脚下名字标签(对齐 evtVisual.label): "旅商/赌徒/祭坛/机关"。
	GRAYTAIL_API FString GetEventShortLabel(EGT_EventKind Kind);
}
