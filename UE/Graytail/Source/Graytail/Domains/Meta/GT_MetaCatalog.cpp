#include "Domains/Meta/GT_MetaCatalog.h"

#include "Data/GT_GameDataSubsystem.h"

namespace
{
	const TCHAR* EquipDisplayName(const FString& Id)
	{
		if (Id == TEXT("armor")) return TEXT("防护背心");
		if (Id == TEXT("whetstone")) return TEXT("磨刀石");
		if (Id == TEXT("medkit")) return TEXT("急救包");
		if (Id == TEXT("insulated_gloves")) return TEXT("绝缘套");
		if (Id == TEXT("compass")) return TEXT("罗盘");
		if (Id == TEXT("backpack")) return TEXT("大背包");
		if (Id == TEXT("anomaly_fang")) return TEXT("异常体犬牙");
		if (Id == TEXT("lockdown_crystal")) return TEXT("封锁区结晶");
		if (Id == TEXT("company_badge")) return TEXT("公司工牌");
		if (Id == TEXT("salvage_magnet")) return TEXT("回收磁石");
		return TEXT("未知装备");
	}

	const TCHAR* TalentDisplayName(const FString& Id)
	{
		if (Id == TEXT("talent_map")) return TEXT("邻域感知");
		if (Id == TEXT("talent_mine")) return TEXT("厚皮");
		if (Id == TEXT("talent_monster")) return TEXT("威压");
		if (Id == TEXT("talent_extract")) return TEXT("抢救条款");
		if (Id == TEXT("talent_event")) return TEXT("议价");
		return TEXT("未知天赋");
	}

	const TCHAR* ConsumableDisplayName(const FString& Id)
	{
		if (Id == TEXT("emergency_bandage")) return TEXT("应急止血贴");
		if (Id == TEXT("lucky_coin")) return TEXT("幸运硬币");
		return TEXT("未知消耗品");
	}

	EGT_ItemTrigger ParseTrigger(const FString& Id)
	{
		if (Id == TEXT("killPowerStack")) return EGT_ItemTrigger::KillPowerStack;
		if (Id == TEXT("protocolHeal")) return EGT_ItemTrigger::ProtocolHeal;
		if (Id == TEXT("settleGoldBonus")) return EGT_ItemTrigger::SettleGoldBonus;
		if (Id == TEXT("chestBonusLoot")) return EGT_ItemTrigger::ChestBonusLoot;
		if (Id == TEXT("luckyCoin")) return EGT_ItemTrigger::LuckyCoin;
		return EGT_ItemTrigger::None;
	}

	struct FGT_MetaCatalogCache
	{
		uint64 Revision = MAX_uint64;
		TArray<FGT_EquipDef> Equipment;
		TArray<FGT_TalentDef> Talents;
		TArray<FGT_ConsumableDef> Consumables;

		void Refresh()
		{
			const uint64 CurrentRevision = GT_GameData::GetRevision();
			if (Revision == CurrentRevision)
			{
				return;
			}

			const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
			checkf(Snapshot, TEXT("Meta catalog accessed without valid game data."));
			Equipment.Reset(Snapshot->MetaCatalog.Equipment.Num());
			Talents.Reset(Snapshot->MetaCatalog.Talents.Num());
			Consumables.Reset(Snapshot->MetaCatalog.Consumables.Num());

			for (const FGT_EquipBalanceRow& Row : Snapshot->MetaCatalog.Equipment)
			{
				FGT_EquipDef& Def = Equipment.AddDefaulted_GetRef();
				Def.Id = FName(*Row.Id);
				Def.DisplayName = EquipDisplayName(Row.Id);
				Def.Price = Row.Price;
				Def.BonusHP = Row.BonusHp;
				Def.BonusPower = Row.BonusPower;
				Def.bMineImmunity = Row.bMineImmunity;
				Def.MineDmgReduce = Row.MineDamageReduce;
				Def.bShowExitHint = Row.bShowExitHint;
				Def.SearchBonus = Row.SearchBonus;
				Def.Trigger = ParseTrigger(Row.Trigger);
				Def.TriggerCap = Row.TriggerCap;
				Def.TriggerAmount = Row.TriggerAmount;
			}

			for (const FGT_TalentBalanceRow& Row : Snapshot->MetaCatalog.Talents)
			{
				FGT_TalentDef& Def = Talents.AddDefaulted_GetRef();
				Def.Id = FName(*Row.Id);
				Def.DisplayName = TalentDisplayName(Row.Id);
				Def.Price = Row.Price;
				Def.MineDmgReduce = Row.MineDamageReduce;
				Def.MonsterFleeBonus = Row.MonsterFleeBonus;
				Def.FailureGoldBonus = Row.FailureGoldBonus;
				Def.TradePrice = Row.TradePrice;
				Def.bMapHighlight = Row.bMapHighlight;
			}

			for (const FGT_ConsumableBalanceRow& Row : Snapshot->MetaCatalog.Consumables)
			{
				FGT_ConsumableDef& Def = Consumables.AddDefaulted_GetRef();
				Def.Id = FName(*Row.Id);
				Def.DisplayName = ConsumableDisplayName(Row.Id);
				Def.Price = Row.Price;
				Def.Heal = Row.Heal;
				Def.MaxCarry = Row.MaxCarry;
				Def.Trigger = ParseTrigger(Row.Trigger);
			}
			Revision = CurrentRevision;
		}
	};

	FGT_MetaCatalogCache& GetCache()
	{
		static FGT_MetaCatalogCache Cache;
		Cache.Refresh();
		return Cache;
	}
}

namespace GT_MetaCatalog
{
	const TArray<FGT_EquipDef>& GetEquipDefs()
	{
		return GetCache().Equipment;
	}

	const TArray<FGT_TalentDef>& GetTalentDefs()
	{
		return GetCache().Talents;
	}

	const TArray<FGT_ConsumableDef>& GetConsumableDefs()
	{
		return GetCache().Consumables;
	}

	const FGT_EquipDef* FindEquip(FName Id)
	{
		return GetEquipDefs().FindByPredicate([Id](const FGT_EquipDef& Def) { return Def.Id == Id; });
	}

	const FGT_TalentDef* FindTalent(FName Id)
	{
		return GetTalentDefs().FindByPredicate([Id](const FGT_TalentDef& Def) { return Def.Id == Id; });
	}

	const FGT_ConsumableDef* FindConsumable(FName Id)
	{
		return GetConsumableDefs().FindByPredicate([Id](const FGT_ConsumableDef& Def) { return Def.Id == Id; });
	}

	int32 GetMaxEquipped()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Meta catalog accessed without valid game data."));
		return Snapshot->MetaCatalog.MaxEquipped;
	}

	int32 GetRecentRecoveryMax()
	{
		const FGT_GameDataSnapshot* Snapshot = GT_GameData::GetSnapshot();
		checkf(Snapshot, TEXT("Meta catalog accessed without valid game data."));
		return Snapshot->MetaCatalog.RecentRecoveryMax;
	}
}
