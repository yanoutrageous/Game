#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "GT_MetaProgressSubsystem.generated.h"

// 局外元进度子系统:持久状态 + 操作 API + 局内加成查询。
// 操作走公有 C++ 方法(不走局内 Command 管线, 因无 RunContext)。
UCLASS()
class GRAYTAIL_API UGT_MetaProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- 存读档 ---
	void Save();
	void Load();

	// --- 币(对齐 Lua GetGold/AddGold/SpendGold) ---
	int32 GetGold() const { return State.Gold; }
	void AddGold(int32 Amount);
	bool SpendGold(int32 Amount);

	// --- 装备(对齐 Lua OwnsItem/BuyItem/ToggleEquip/IsEquipped/GetEquippedItems) ---
	bool OwnsItem(FName ItemId) const { return State.OwnedItems.Contains(ItemId); }
	// 返回 false 时 OutError = already_owned / not_found / not_enough_gold
	bool BuyItem(FName ItemId, FName& OutError);
	// 返回 false 时 OutError = not_owned / max_equipped
	bool ToggleEquip(FName ItemId, FName& OutError);
	bool IsEquipped(FName ItemId) const { return State.EquippedItems.Contains(ItemId); }
	const TArray<FName>& GetEquippedItems() const { return State.EquippedItems; }
	// 撤离失败丢装备(用户拍板): 移除所有已装备的装备(从拥有+装备列表), 未装备的拥有装备保留。返回损失 id 列表。
	TArray<FName> LoseEquippedItemsOnFailure();
	// 最近一次失败损失的装备(供结算面板显示; 非持久, 不存档)。
	const TArray<FName>& GetLastFailureLostEquips() const { return LastFailureLostEquips; }

	// --- 天赋(对齐 Lua HasTalent/UnlockTalent) ---
	bool HasTalent(FName TalentId) const { return State.UnlockedTalents.Contains(TalentId); }
	// 返回 false 时 OutError = already_unlocked / not_found / not_enough_gold
	bool UnlockTalent(FName TalentId, FName& OutError);

	// --- 消耗品库存(对齐 Lua GetConsumableCount/Buy/Add/Remove) ---
	int32 GetConsumableCount(FName ItemId) const { return State.ConsumableStock.FindRef(ItemId); }
	bool BuyConsumable(FName ItemId, int32 Count, FName& OutError);  // not_found/invalid_count/not_enough_gold
	bool AddConsumable(FName ItemId, int32 Count);
	bool RemoveConsumable(FName ItemId, int32 Count);               // false = not_enough

	// --- loadout(对齐 Lua SetLoadoutConsumable/GetLoadout/ConsumeLoadoutForRun) ---
	// 把 Count 夹到 [0, min(stock, maxCarry)]; 0 表示移除。
	void SetLoadoutConsumable(FName ItemId, int32 Count);
	const TMap<FName, int32>& GetLoadout() const { return State.LoadoutConsumables; }
	// 开局调:从库存扣掉 loadout 选定量, 返回本局携带的 id->数量(S3 用)。
	TMap<FName, int32> ConsumeLoadoutForRun();

	// --- 仓库(对齐 Lua AddWarehouseItems/CanSell/Sell/Remove/Count) ---
	void AddWarehouseItems(const TArray<FGT_RewardItem>& Items, FName Source);
	int32 GetWarehouseItemCount(FName ItemId) const;
	bool CanSellItem(FName ItemId, FName& OutReason) const; // not_owned/unique/not_sellable/equipped/no_value
	bool SellWarehouseItem(FName ItemId, int32 Count, int32& OutGold, FName& OutError);
	bool RemoveWarehouseItem(FName ItemId, int32 Count);
	const TArray<FGT_WarehouseEntry>& GetWarehouse() const { return State.Warehouse; }

	// --- 统计 / 结算(RecordExtractionReward 由 S2 调) ---
	void RecordRun();
	void RecordExtractionReward(const FGT_ExtractionReward& Reward);
	const FGT_MetaStats& GetStats() const { return State.Stats; }
	const FGT_RecoverySummary& GetRecoverySummary() const { return State.Recovery; }

	// --- 局内加成查询(S3 开局读) ---
	FGT_EquipBonus GetEquipBonus() const;       // 对齐 Lua GetEquipBonus
	FGT_TalentEffects GetTalentEffects() const; // 对齐 Lua GetTalentEffects

	// --- GM 调试(免费, 不扣币) ---
	void GMGrantItem(FName ItemId);
	void GMGrantTalent(FName TalentId);
	void GMEquipAll();
	void GMUnequipAll();
	void GMReset();

	// 只读访问(调试命令打印用)
	const FGT_MetaProgressState& GetState() const { return State; }

private:
	FGT_MetaProgressState State;

	// 最近一次撤离失败损失的装备 id(非持久, 仅供结算面板本次显示)。
	TArray<FName> LastFailureLostEquips;

	static FString SaveSlotName();
	static constexpr int32 SaveUserIndex = 0;

	// 读档后清洗(对齐 Lua Load 的校验): 已装备项必须确实拥有; loadout 数量夹到库存。
	void SanitizeAfterLoad();

	FGT_WarehouseEntry* FindWarehouse(FName ItemId);
	const FGT_WarehouseEntry* FindWarehouse(FName ItemId) const;

	static int32 ClampNonNegative(int32 V) { return FMath::Max(0, V); }
};
