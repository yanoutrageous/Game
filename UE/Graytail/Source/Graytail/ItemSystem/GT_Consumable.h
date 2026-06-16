#pragma once

#include "CoreMinimal.h"
#include "GT_ItemBase.h"
#include "GT_Consumable.generated.h"

/**
 * 消耗品类
 * 包括：应急绷带、幸运硬币、残缺地图、灰尾应急灯、协议镇定剂
 * 特点：
 * - 主动使用后消耗
 * - 堆叠数量可能大于1
 * - 使用时触发效果
 */
UCLASS(Blueprintable, BlueprintType)
class GRAYTAIL_API UConsumable : public UItemBase
{
	GENERATED_BODY()

public:
	UConsumable();

	/**
	 * 使用消耗品
	 */
	virtual void OnUse_Implementation(APawn* User) override;

	/**
	 * 消耗品是否可用
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	bool IsConsumable() const { return ItemData.ItemType == EGT_ItemType::Consumable; }

protected:
	/**
	 * 执行消耗品效果
	 * 重写此函数以实现特殊消耗品逻辑
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void ExecuteEffect(APawn* User);

	virtual void ExecuteEffect_Implementation(APawn* User);
};
