#include "ItemSystem/GT_ItemBase.h"
#include "GameFramework/Pawn.h"

UItemBase::UItemBase()
	: StackSize(1)
	, bIsEquipped(false)
{
}

void UItemBase::BeginDestroy()
{
	OnDestroy_Implementation();
	Super::BeginDestroy();
}

void UItemBase::InitializeFromData(const FGT_ItemData& InData)
{
	ItemData = InData;
	StackSize = 1;
	bIsEquipped = false;
	OwnerPawn = nullptr;
	TriggeredOneTimeEffects.Empty();
}

int32 UItemBase::AddStack(int32 Count)
{
	if (!CanStack() || Count <= 0)
	{
		return 0;
	}

	int32 MaxStackable = ItemData.MaxStackSize - StackSize;
	int32 ActualAdd = FMath::Min(Count, MaxStackable);
	StackSize += ActualAdd;
	return ActualAdd;
}

int32 UItemBase::RemoveStack(int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}

	int32 ActualRemove = FMath::Min(Count, StackSize);
	StackSize -= ActualRemove;
	return ActualRemove;
}

void UItemBase::SetStackSize(int32 NewSize)
{
	StackSize = FMath::Clamp(NewSize, 1, ItemData.MaxStackSize);
}

UItemBase* UItemBase::Duplicate(int32 DuplicateCount, UObject* OuterObject)
{
	if (DuplicateCount <= 0)
	{
		return nullptr;
	}

	UObject* DuplicateOuter = OuterObject ? OuterObject : GetOuter();
	UItemBase* NewItem = NewObject<UItemBase>(DuplicateOuter, GetClass());

	if (NewItem)
	{
		NewItem->InitializeFromData(ItemData);
		NewItem->SetStackSize(FMath::Min(DuplicateCount, ItemData.MaxStackSize));
	}

	return NewItem;
}

void UItemBase::OnEquip_Implementation(APawn* Owner)
{
	OwnerPawn = Owner;
	bIsEquipped = true;
	TriggerEffect(EGT_ItemEffectTrigger::OnEquip, Owner, TEXT(""));
}

void UItemBase::OnUnequip_Implementation()
{
	if (OwnerPawn.IsValid())
	{
		TriggerEffect(EGT_ItemEffectTrigger::OnUnequip, OwnerPawn.Get(), TEXT(""));
	}
	bIsEquipped = false;
	OwnerPawn = nullptr;
}

void UItemBase::OnUse_Implementation(APawn* User)
{
	// 基类不实现使用逻辑，由子类重写
}

void UItemBase::OnDestroy_Implementation()
{
	if (bIsEquipped)
	{
		OnUnequip();
	}
}

void UItemBase::TriggerEffect_Implementation(EGT_ItemEffectTrigger TriggerType, APawn* Target, const FString& Context)
{
	TriggerEffectInternal(TriggerType, Target, Context);
}

bool UItemBase::ShouldTriggerEffect(const FGT_ItemEffect& Effect, const FString& Context) const
{
	// 检查触发类型是否匹配
	if (Effect.TriggerType != EGT_ItemEffectTrigger::Passive)
	{
		// 检查触发条件
		if (!Effect.TriggerCondition.IsEmpty() && !Context.Contains(Effect.TriggerCondition))
		{
			return false;
		}
	}

	// 检查概率
	if (Effect.TriggerProbability < 1.f)
	{
		if (FMath::FRand() > Effect.TriggerProbability)
		{
			return false;
		}
	}

	return true;
}

void UItemBase::ApplyModifier_Implementation(const FGT_Modifier& Modifier, APawn* Target)
{
	ApplyModifierInternal(Modifier, Target);
}

void UItemBase::SaveToArchive(FArchive& Ar)
{
	Ar << ItemData.ItemID;
	Ar << StackSize;
	Ar << bIsEquipped;

	int32 TriggeredCount = TriggeredOneTimeEffects.Num();
	Ar << TriggeredCount;
	for (int32 Id : TriggeredOneTimeEffects)
	{
		Ar << Id;
	}
}

void UItemBase::LoadFromArchive(FArchive& Ar)
{
	int32 LoadedItemID;
	Ar << LoadedItemID;
	Ar << StackSize;
	Ar << bIsEquipped;

	int32 TriggeredCount;
	Ar << TriggeredCount;
	TriggeredOneTimeEffects.Empty(TriggeredCount);
	for (int32 i = 0; i < TriggeredCount; ++i)
	{
		int32 Id;
		Ar << Id;
		TriggeredOneTimeEffects.Add(Id);
	}
}

void UItemBase::ApplyModifierInternal(const FGT_Modifier& Modifier, APawn* Target)
{
	if (!Target)
	{
		return;
	}

	// 具体的修饰符应用逻辑将在这里实现
	// 这是接口化的设计，实现可通过事件或接口调用扩展
	switch (Modifier.ModifierType)
	{
	case EGT_ModifierType::Attack:
	case EGT_ModifierType::Defense:
	case EGT_ModifierType::DamageReduction:
	case EGT_ModifierType::CurrentHealth:
	case EGT_ModifierType::MaxHealth:
	case EGT_ModifierType::Gold:
	case EGT_ModifierType::GoldMultiplier:
	case EGT_ModifierType::PriceDiscount:
	case EGT_ModifierType::WeightCapacity:
	case EGT_ModifierType::ProtocolPressure:
	case EGT_ModifierType::ProtocolImmunity:
		// 具体实现通过接口或事件系统调用
		break;
	}
}

void UItemBase::TriggerEffectInternal(EGT_ItemEffectTrigger TriggerType, APawn* Target, const FString& Context)
{
	if (!Target)
	{
		return;
	}

	for (int32 i = 0; i < ItemData.Effects.Num(); ++i)
	{
		FGT_ItemEffect& Effect = ItemData.Effects[i];

		// 检查效果是否应该触发
		if (Effect.TriggerType != TriggerType)
		{
			continue;
		}

		// 检查一次性效果是否已触发过
		if (Effect.bOneTimeOnly && TriggeredOneTimeEffects.Contains(i))
		{
			continue;
		}

		// 检查效果触发条件
		if (!ShouldTriggerEffect(Effect, Context))
		{
			continue;
		}

		// 应用修饰符
		for (const FGT_Modifier& Modifier : Effect.Modifiers)
		{
			ApplyModifier(Modifier, Target);
		}

		// 标记为已触发
		if (Effect.bOneTimeOnly)
		{
			TriggeredOneTimeEffects.Add(i);
		}
	}
}
