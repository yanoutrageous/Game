#include "ItemSystem/GT_Equipment.h"
#include "GameFramework/Pawn.h"

UEquipment::UEquipment()
{
}

void UEquipment::ApplyEquipmentEffect_Implementation(APawn* Owner)
{
	if (!Owner)
	{
		return;
	}

	// 触发 OnEquip 事件
	TriggerEffect(EGT_ItemEffectTrigger::OnEquip, Owner, TEXT(""));

	// 如果有 Passive 效果，也触发
	TriggerEffect(EGT_ItemEffectTrigger::Passive, Owner, TEXT(""));
}

void UEquipment::RemoveEquipmentEffect_Implementation()
{
	if (OwnerPawn.IsValid())
	{
		// 触发 OnUnequip 事件
		TriggerEffect(EGT_ItemEffectTrigger::OnUnequip, OwnerPawn.Get(), TEXT(""));
	}
}
