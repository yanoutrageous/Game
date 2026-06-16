#pragma once

#include "CoreMinimal.h"
#include "GT_ItemBase.h"
#include "GT_Equipment.generated.h"

/**
 * 装备类
 * 包括：公司标准手套
 * 特点：
 * - 只能装备一件
 * - 装备时激活被动效果
 * - 某些效果可能是一次性的（如雷房伤害减免）
 */
UCLASS(Blueprintable, BlueprintType)
class GRAYTAIL_API UEquipment : public UItemBase
{
	GENERATED_BODY()

public:
	UEquipment();

	/**
	 * 获取装备槽位
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	EGT_EquipmentSlot GetRequiredSlot() const { return ItemData.RequiredSlot; }

	/**
	 * 装备是否可用
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	bool IsEquipmentValid() const { return ItemData.ItemType == EGT_ItemType::Equipment; }

protected:
	/**
	 * 应用装备效果
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void ApplyEquipmentEffect(APawn* Owner);

	/**
	 * 移除装备效果
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void RemoveEquipmentEffect();

	virtual void ApplyEquipmentEffect_Implementation(APawn* Owner);
	virtual void RemoveEquipmentEffect_Implementation();
};
