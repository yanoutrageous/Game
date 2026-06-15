#include "Domains/Meta/GT_MetaCatalog.h"

namespace GT_MetaCatalog
{
	const TArray<FGT_EquipDef>& GetEquipDefs()
	{
		// 数值: Balance.shop(price/bonusHP/bonusPower/mineDmgReduce) + GetEquipBonus 语义。
		static const TArray<FGT_EquipDef> Defs = {
			// Id,                                DisplayName,  Price, HP, Pow, Imm,  MineRed, ExitHint, Search
			{ FName(TEXT("armor")),            TEXT("防护背心"), 110,  20,  0, false,  0, false,  0 },
			{ FName(TEXT("whetstone")),        TEXT("磨刀石"),    90,   0,  2, false,  0, false,  0 },
			{ FName(TEXT("medkit")),           TEXT("急救包"),   120,   0,  0, true,   0, false,  0 },
			{ FName(TEXT("insulated_gloves")), TEXT("绝缘套"),   140,   0,  0, false, 10, false,  0 },
			{ FName(TEXT("compass")),          TEXT("罗盘"),     160,   0,  0, false,  0, true,   0 },
			{ FName(TEXT("backpack")),         TEXT("大背包"),   220,   0,  0, false,  0, false, 50 },
		};
		return Defs;
	}

	const TArray<FGT_TalentDef>& GetTalentDefs()
	{
		// 数值: Balance.talents(price) + GetTalentEffects 语义。
		static const TArray<FGT_TalentDef> Defs = {
			// Id,                          DisplayName,  Price, MineRed, Flee, FailGold, Trade, MapHi
			{ FName(TEXT("talent_map")),     TEXT("邻域感知"), 100,  0, 0,  0,  0, true  },
			{ FName(TEXT("talent_mine")),    TEXT("厚皮"),     120, 10, 0,  0,  0, false },
			{ FName(TEXT("talent_monster")), TEXT("威压"),     120,  0, 2,  0,  0, false },
			{ FName(TEXT("talent_extract")), TEXT("抢救条款"), 140,  0, 0, 10,  0, false },
			{ FName(TEXT("talent_event")),   TEXT("议价"),     140,  0, 0,  0, 20, false },
		};
		return Defs;
	}

	const TArray<FGT_ConsumableDef>& GetConsumableDefs()
	{
		static const TArray<FGT_ConsumableDef> Defs = {
			{ FName(TEXT("emergency_bandage")), TEXT("应急止血贴"), 12, 25, 3 },
		};
		return Defs;
	}

	const FGT_EquipDef* FindEquip(FName Id)
	{
		return GetEquipDefs().FindByPredicate([Id](const FGT_EquipDef& D) { return D.Id == Id; });
	}

	const FGT_TalentDef* FindTalent(FName Id)
	{
		return GetTalentDefs().FindByPredicate([Id](const FGT_TalentDef& D) { return D.Id == Id; });
	}

	const FGT_ConsumableDef* FindConsumable(FName Id)
	{
		return GetConsumableDefs().FindByPredicate([Id](const FGT_ConsumableDef& D) { return D.Id == Id; });
	}
}
