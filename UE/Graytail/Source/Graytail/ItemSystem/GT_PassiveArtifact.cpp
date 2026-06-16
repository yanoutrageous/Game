#include "ItemSystem/GT_PassiveArtifact.h"
#include "GameFramework/Pawn.h"

UPassiveArtifact::UPassiveArtifact()
	: bIsActivated(false)
{
}

void UPassiveArtifact::OnEquip_Implementation(APawn* Owner)
{
	Super::OnEquip_Implementation(Owner);
	ActivatePassiveEffect(Owner);
}

void UPassiveArtifact::OnUnequip_Implementation()
{
	DeactivatePassiveEffect();
	Super::OnUnequip_Implementation();
}

void UPassiveArtifact::ActivatePassiveEffect_Implementation(APawn* Owner)
{
	if (!Owner || bIsActivated)
	{
		return;
	}

	bIsActivated = true;

	// 触发 Passive 类型的效果
	TriggerEffect(EGT_ItemEffectTrigger::Passive, Owner, TEXT(""));
}

void UPassiveArtifact::DeactivatePassiveEffect_Implementation()
{
	bIsActivated = false;
	// 取消被动效果的逻辑可在这里扩展
	// 例如：移除之前应用的修饰符
}
