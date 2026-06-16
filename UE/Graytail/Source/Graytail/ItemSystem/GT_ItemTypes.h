#pragma once

#include "CoreMinimal.h"
#include "GT_ItemTypes.generated.h"

// 物品类型枚举
UENUM(BlueprintType)
enum class EGT_ItemType : uint8
{
	PassiveArtifact UMETA(DisplayName = "Passive Artifact"),
	Consumable UMETA(DisplayName = "Consumable"),
	Equipment UMETA(DisplayName = "Equipment")
};

// 物品稀有度枚举
UENUM(BlueprintType)
enum class EGT_ItemRarity : uint8
{
	Common UMETA(DisplayName = "Common"),
	Uncommon UMETA(DisplayName = "Uncommon"),
	Rare UMETA(DisplayName = "Rare"),
	Epic UMETA(DisplayName = "Epic")
};

// 装备槽位枚举
UENUM(BlueprintType)
enum class EGT_EquipmentSlot : uint8
{
	Tool UMETA(DisplayName = "Tool"),
	Accessory UMETA(DisplayName = "Accessory")
};

// 物品效果触发时机
UENUM(BlueprintType)
enum class EGT_ItemEffectTrigger : uint8
{
	Passive UMETA(DisplayName = "Passive"),
	OnUse UMETA(DisplayName = "On Use"),
	OnEnterRoom UMETA(DisplayName = "On Enter Room"),
	OnExitRoom UMETA(DisplayName = "On Exit Room"),
	OnDefeatMonster UMETA(DisplayName = "On Defeat Monster"),
	OnTakeDamage UMETA(DisplayName = "On Take Damage"),
	OnPickupLoot UMETA(DisplayName = "On Pickup Loot"),
	OnExtractionSuccess UMETA(DisplayName = "On Extraction Success"),
	OnMoveStep UMETA(DisplayName = "On Move Step"),
	OnProtocolLevelChanged UMETA(DisplayName = "On Protocol Level Changed"),
	OnEquip UMETA(DisplayName = "On Equip"),
	OnUnequip UMETA(DisplayName = "On Unequip")
};

// 修饰符类型
UENUM(BlueprintType)
enum class EGT_ModifierType : uint8
{
	Attack UMETA(DisplayName = "Attack"),
	Defense UMETA(DisplayName = "Defense"),
	DamageReduction UMETA(DisplayName = "Damage Reduction"),
	CurrentHealth UMETA(DisplayName = "Current Health"),
	MaxHealth UMETA(DisplayName = "Max Health"),
	Gold UMETA(DisplayName = "Gold"),
	GoldMultiplier UMETA(DisplayName = "Gold Multiplier"),
	PriceDiscount UMETA(DisplayName = "Price Discount"),
	WeightCapacity UMETA(DisplayName = "Weight Capacity"),
	ProtocolPressure UMETA(DisplayName = "Protocol Pressure"),
	ProtocolImmunity UMETA(DisplayName = "Protocol Immunity")
};

// 单个修饰符数据
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_Modifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	EGT_ModifierType ModifierType = EGT_ModifierType::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	float Value = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	bool bStackable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	int32 MaxStack = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	int32 CurrentStack = 0;
};

// 物品效果定义
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ItemEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	EGT_ItemEffectTrigger TriggerType = EGT_ItemEffectTrigger::Passive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FString TriggerCondition = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	float TriggerProbability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	TArray<FGT_Modifier> Modifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	bool bOneTimeOnly = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	bool bTriggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FString EffectDescription = TEXT("");
};

// 物品数据表行结构
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	int32 ItemID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	FString ItemName = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	EGT_ItemType ItemType = EGT_ItemType::PassiveArtifact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	EGT_ItemRarity Rarity = EGT_ItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	int32 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	int32 MaxStackSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	EGT_EquipmentSlot RequiredSlot = EGT_EquipmentSlot::Tool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	TArray<FGT_ItemEffect> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Item")
	TArray<FString> Tags;
};

// 金币状态结构体
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_CurrencyState
{
	GENERATED_BODY()

	// 待结算币 - 本次冒险中获得但未锁定的金币
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Currency")
	int32 PendingGold = 0;

	// 安全结算币 - 出售物品时立即锁定的金币，失败时保留
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Currency")
	int32 SecuredGold = 0;

	// 全局结算币 - 用于商店购买和升级的金币
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Currency")
	int32 GlobalGold = 0;

	// 本局金币获得历史
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Currency")
	int32 GoldEarnedThisRun = 0;

	void Reset()
	{
		PendingGold = 0;
		SecuredGold = 0;
		GoldEarnedThisRun = 0;
	}

	void AddPendingGold(int32 Amount)
	{
		if (Amount > 0)
		{
			PendingGold += Amount;
			GoldEarnedThisRun += Amount;
		}
	}

	void AddSecuredGold(int32 Amount)
	{
		if (Amount > 0)
		{
			SecuredGold += Amount;
		}
	}

	void ConvertPendingToSecured(float Multiplier = 1.f)
	{
		int32 ConvertedAmount = FMath::FloorToInt(static_cast<float>(PendingGold) * Multiplier);
		SecuredGold += ConvertedAmount;
		PendingGold = 0;
	}

	void ConvertSecuredToGlobal()
	{
		GlobalGold += SecuredGold;
		SecuredGold = 0;
	}

	int32 GetTotalGoldThisRun() const
	{
		return PendingGold + SecuredGold;
	}
};
