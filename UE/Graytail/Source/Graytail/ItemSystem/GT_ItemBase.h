#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GT_ItemTypes.h"
#include "GT_ItemBase.generated.h"

class APawn;
class UInventory;
class UVault;

/**
 * 物品基类 - 所有物品的根基
 * 支持堆叠、装备、使用、效果触发
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GRAYTAIL_API UItemBase : public UObject
{
	GENERATED_BODY()

public:
	UItemBase();

	virtual void BeginDestroy() override;

	// ========== 初始化 ==========

	/**
	 * 从 ItemData 初始化物品
	 */
	virtual void InitializeFromData(const FGT_ItemData& InData);

	// ========== 属性访问 ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	int32 GetItemID() const { return ItemData.ItemID; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	const FString& GetItemName() const { return ItemData.ItemName; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	EGT_ItemType GetItemType() const { return ItemData.ItemType; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	EGT_ItemRarity GetRarity() const { return ItemData.Rarity; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	float GetWeight() const { return ItemData.Weight; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	int32 GetValue() const { return ItemData.Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	int32 GetStackSize() const { return StackSize; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	int32 GetMaxStackSize() const { return ItemData.MaxStackSize; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	bool CanStack() const { return ItemData.MaxStackSize > 1; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	const FGT_ItemData& GetItemData() const { return ItemData; }

	// ========== 堆叠相关 ==========

	/**
	 * 添加堆叠数
	 * @return 实际添加的数量
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Item")
	virtual int32 AddStack(int32 Count = 1);

	/**
	 * 移除堆叠数
	 * @return 实际移除的数量
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Item")
	virtual int32 RemoveStack(int32 Count = 1);

	/**
	 * 设置堆叠数
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Item")
	virtual void SetStackSize(int32 NewSize);

	/**
	 * 复制物品（用于拆分堆叠）
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Item")
	virtual UItemBase* Duplicate(int32 DuplicateCount = 1, UObject* OuterObject = nullptr);

	// ========== 生命周期事件 ==========

	/**
	 * 装备物品
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void OnEquip(APawn* Owner);

	/**
	 * 卸下物品
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void OnUnequip();

	/**
	 * 使用物品（仅消耗品）
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void OnUse(APawn* User);

	/**
	 * 销毁物品
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void OnDestroy();

	// ========== 效果系统 ==========

	/**
	 * 触发物品效果
	 * @param TriggerType 触发类型
	 * @param Target 目标角色
	 * @param Context 上下文信息（如"IsMineRoom=true"）
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void TriggerEffect(EGT_ItemEffectTrigger TriggerType, APawn* Target, const FString& Context);

	/**
	 * 检查是否应该触发指定类型的效果
	 */
	UFUNCTION(BlueprintCallable, Category = "Graytail|Item")
	virtual bool ShouldTriggerEffect(const FGT_ItemEffect& Effect, const FString& Context) const;

	/**
	 * 应用修饰符到目标
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Graytail|Item")
	void ApplyModifier(const FGT_Modifier& Modifier, APawn* Target);

	// ========== 状态查询 ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	bool IsEquipped() const { return bIsEquipped; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	APawn* GetOwner() const { return OwnerPawn; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Graytail|Item")
	float GetTotalWeight() const { return ItemData.Weight * static_cast<float>(StackSize); }

	// ========== 序列化 ==========

	/**
	 * 将物品保存到存档
	 */
	virtual void SaveToArchive(FArchive& Ar);

	/**
	 * 从存档加载物品
	 */
	virtual void LoadFromArchive(FArchive& Ar);

protected:
	// 物品数据（来自 DataTable）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	FGT_ItemData ItemData;

	// 堆叠数
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	int32 StackSize = 1;

	// 装备状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	bool bIsEquipped = false;

	// 所有者（装备时设置）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	TWeakObjectPtr<APawn> OwnerPawn;

	// 已触发过的一次性效果集合
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Item")
	TArray<int32> TriggeredOneTimeEffects;

	/**
	 * 应用单个修饰符的内部实现
	 */
	virtual void ApplyModifierInternal(const FGT_Modifier& Modifier, APawn* Target);

	/**
	 * 触发效果的内部实现
	 */
	virtual void TriggerEffectInternal(EGT_ItemEffectTrigger TriggerType, APawn* Target, const FString& Context);
};
