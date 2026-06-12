#pragma once

#include "CoreMinimal.h"
#include "GT_InventoryTypes.generated.h"

// 掉落品质档位: 对齐 Lua Balance.search/chest dropTable 的 quality 字段。
// None 表示该次 roll 落在 "无掉落" 区间(仅普通搜索表有)。
UENUM(BlueprintType)
enum class EGT_ItemQuality : uint8
{
	None UMETA(DisplayName = "None (No Drop)"),
	Low UMETA(DisplayName = "Low"),
	Common UMETA(DisplayName = "Common"),
	Rare UMETA(DisplayName = "Rare"),
	Precious UMETA(DisplayName = "Precious"),
	Abnormal UMETA(DisplayName = "Abnormal")
};

// 物品大类: 对齐 Lua ITEM_DEFS 的 type 字段。
UENUM(BlueprintType)
enum class EGT_ItemKind : uint8
{
	Relic UMETA(DisplayName = "Relic"),
	Tool UMETA(DisplayName = "Tool"),
	Record UMETA(DisplayName = "Record"),
	Consumable UMETA(DisplayName = "Consumable")
};

// 物品静态表条目(一条 = Lua ITEM_DEFS 的一项)。运行期数据走 FGT_ItemStack。
// 这是 UGT_ItemDef 资产的运行时值视图(GT_ItemCatalog 加载时拷出), 名字避让 UHT 撞名。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ItemCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	EGT_ItemKind Kind = EGT_ItemKind::Relic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FName Rarity = NAME_None;

	// 基础价值(出售/结算用), 对齐 Lua 的 baseValue。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	int32 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FString EffectText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FString Description;
};

// 背包里的一摞同类物品。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	int32 Count = 0;

	// 来源标记(search/chest/event...), 仅用于结算展示与统计。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FName Source = NAME_None;
};

// 一次搜索/开箱结出的奖励(对齐 Lua RunInventory.GetReward 的返回表)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_SearchReward
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	int32 Gold = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	int32 Parts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	TArray<FGT_ItemStack> Items;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	bool bIsChest = false;
};

// 搜索命令的结果快照(成功或失败原因), 供命令层与调试命令展示。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_SearchOutcome
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	bool bSearched = false;

	// searched / unsafe / spawn / exit / monster / event / not_ready, 对齐 Lua GetSearchState 的 reason。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	FName Status = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	FGT_SearchReward Reward;
};

// 一局的背包状态(对齐 Lua RunInventory 的可变状态部分)。
// 挂在 UGT_RunContext 上, 一局一份, ResetRun 时整体重建。
// 金币双轨: PendingGold 待结算(死亡丢失), SafeGold 已锁定(死亡保留)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RunInventoryState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	int32 PendingGold = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	int32 SafeGold = 0;

	// 回收物零件总数(含已成堆的物品), 对齐 Lua 的 parts。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	int32 Parts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	TArray<FGT_ItemStack> CarriedItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory")
	TArray<FIntPoint> SearchedRooms;

	void Reset()
	{
		PendingGold = 0;
		SafeGold = 0;
		Parts = 0;
		CarriedItems.Reset();
		SearchedRooms.Reset();
	}

	int32 AddPendingGold(int32 Amount)
	{
		PendingGold = FMath::Max(0, PendingGold + Amount);
		return PendingGold;
	}

	int32 AddSafeGold(int32 Amount)
	{
		SafeGold = FMath::Max(0, SafeGold + Amount);
		return SafeGold;
	}

	bool SpendPendingGold(int32 Amount)
	{
		if (Amount <= 0)
		{
			return true;
		}
		if (PendingGold < Amount)
		{
			return false;
		}
		PendingGold -= Amount;
		return true;
	}

	void AddCarriedItem(FName ItemId, int32 Count, FName Source)
	{
		if (ItemId.IsNone() || Count < 1)
		{
			return;
		}

		FGT_ItemStack* Stack = CarriedItems.FindByPredicate([ItemId](const FGT_ItemStack& Existing)
		{
			return Existing.ItemId == ItemId;
		});
		if (!Stack)
		{
			Stack = &CarriedItems.AddDefaulted_GetRef();
			Stack->ItemId = ItemId;
		}
		Stack->Count += Count;
		Stack->Source = Source;
	}

	int32 GetCarriedItemCount() const
	{
		int32 Total = 0;
		for (const FGT_ItemStack& Stack : CarriedItems)
		{
			Total += Stack.Count;
		}
		return Total;
	}

	// 不在物品堆里的零散零件数(parts 与堆叠数的差), 对齐 Lua GetLooseParts。
	int32 GetLooseParts() const
	{
		return FMath::Max(0, Parts - GetCarriedItemCount());
	}

	bool IsRoomSearched(int32 X, int32 Y) const
	{
		return SearchedRooms.Contains(FIntPoint(X, Y));
	}

	void MarkRoomSearched(int32 X, int32 Y)
	{
		SearchedRooms.AddUnique(FIntPoint(X, Y));
	}
};
