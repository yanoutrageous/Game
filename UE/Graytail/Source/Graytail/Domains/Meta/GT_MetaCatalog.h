#pragma once

#include "CoreMinimal.h"

// 局外定义目录(C++ 静态表)。数值照搬 Lua Balance.shop / Balance.talents /
// MetaProgress.ITEMS/TALENTS/CONSUMABLES。纯 C++ 结构(不反射), 仅内部使用。
// 回收物的 UGT_ItemDef 资产体系与此无关, 不受影响。

// 物品触发标签(被动/主动效果在现有触发点派发)。**只往后追加**(守 UENUM 同款铁律)。
// 纯 C++ 枚举(def 结构不反射), 不需 .generated。
enum class EGT_ItemTrigger : uint8
{
	None = 0,
	KillPowerStack,   // 战斗胜利: 本局攻击力叠加(异常体犬牙)
	ProtocolHeal,     // 协议升级: 回血(封锁区结晶)
	SettleGoldBonus,  // 撤离结算: 金币百分比加成(公司工牌, 在 Meta 侧算)
	ChestBonusLoot,   // 进宝箱房: 额外掉 1 件低价值回收物(回收磁石)
	LuckyCoin,        // 消耗品: RNG 得金/揭示相邻房(幸运硬币)
};

struct FGT_EquipDef
{
	FName Id;
	const TCHAR* DisplayName;
	int32 Price;
	// 加成(累加进 FGT_EquipBonus)
	int32 BonusHP = 0;
	int32 BonusPower = 0;
	bool bMineImmunity = false;
	int32 MineDmgReduce = 0;
	bool bShowExitHint = false;
	int32 SearchBonus = 0;
	// S6 触发型效果(默认 None = 纯静态加成装备)。TriggerCap/TriggerAmount 含义随 Trigger 而定:
	// KillPowerStack: Cap=最大叠加层数, Amount=每层 +攻击; ProtocolHeal: Cap=最大回血次数, Amount=每次回血;
	// SettleGoldBonus: Amount=金币加成百分比。
	EGT_ItemTrigger Trigger = EGT_ItemTrigger::None;
	int32 TriggerCap = 0;
	int32 TriggerAmount = 0;
};

struct FGT_TalentDef
{
	FName Id;
	const TCHAR* DisplayName;
	int32 Price;
	// 效果(写进 FGT_TalentEffects)
	int32 MineDmgReduce = 0;
	int32 MonsterFleeBonus = 0;
	int32 FailureGoldBonus = 0;
	int32 TradePrice = 0;       // S4: 议价 → 旅商收购价加成百分比(0 = 无此效果, 20 = +20%)
	bool bMapHighlight = false;
};

struct FGT_ConsumableDef
{
	FName Id;
	const TCHAR* DisplayName;
	int32 Price;
	int32 Heal = 0;
	int32 MaxCarry = 1;
	// S6: 非回血消耗品的效果分派标签(默认 None = 纯回血, 走 Heal)。
	EGT_ItemTrigger Trigger = EGT_ItemTrigger::None;
};

namespace GT_MetaCatalog
{
	const TArray<FGT_EquipDef>& GetEquipDefs();
	const TArray<FGT_TalentDef>& GetTalentDefs();
	const TArray<FGT_ConsumableDef>& GetConsumableDefs();

	const FGT_EquipDef* FindEquip(FName Id);
	const FGT_TalentDef* FindTalent(FName Id);
	const FGT_ConsumableDef* FindConsumable(FName Id);

	GRAYTAIL_API int32 GetMaxEquipped();
	GRAYTAIL_API int32 GetRecentRecoveryMax();
}
