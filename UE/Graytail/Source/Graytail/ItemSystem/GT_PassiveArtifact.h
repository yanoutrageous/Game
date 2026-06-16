#pragma once

#include "CoreMinimal.h"
#include "GT_ItemBase.h"
#include "GT_PassiveArtifact.generated.h"

/**
 * 被动工艺品类
 * 包括：异常体犬牙、受损扫描仪、封锁区结晶、超载零件箱、公司工牌、回收磁石
 * 特点：
 * - 被动触发效果（无需主动使用）
 * - 装备后自动生效
 * - 某些物品有特殊逻辑（如扫描、额外掉落等）
 */
UCLASS(Blueprintable, BlueprintType)
class GRAYTAIL_API UPassiveArtifact : public UItemBase
{
	GENERATED_BODY()

public:
	UPassiveArtifact();

	virtual void OnEquip_Implementation(APawn* Owner) override;
	virtual void OnUnequip_Implementation() override;

protected:
	// 是否已激活被动效果
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	bool bIsActivated = false;

	/**
	 * 激活被动效果（在装备时调用）
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void ActivatePassiveEffect(APawn* Owner);

	/**
	 * 停用被动效果（在卸下时调用）
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void DeactivatePassiveEffect();

	virtual void ActivatePassiveEffect_Implementation(APawn* Owner);
	virtual void DeactivatePassiveEffect_Implementation();
};
