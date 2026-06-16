#pragma once

#include "CoreMinimal.h"

// 局外定义目录(C++ 静态表)。数值照搬 Lua Balance.shop / Balance.talents /
// MetaProgress.ITEMS/TALENTS/CONSUMABLES。纯 C++ 结构(不反射), 仅内部使用。
// 回收物的 UGT_ItemDef 资产体系与此无关, 不受影响。

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
};

namespace GT_MetaCatalog
{
	const TArray<FGT_EquipDef>& GetEquipDefs();
	const TArray<FGT_TalentDef>& GetTalentDefs();
	const TArray<FGT_ConsumableDef>& GetConsumableDefs();

	const FGT_EquipDef* FindEquip(FName Id);
	const FGT_TalentDef* FindTalent(FName Id);
	const FGT_ConsumableDef* FindConsumable(FName Id);

	// 上限常量(对齐 Lua MAX_EQUIPPED / RECENT_RECOVERY_MAX)。
	constexpr int32 MaxEquipped = 2;
	constexpr int32 RecentRecoveryMax = 5;
}
