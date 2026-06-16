#include "ItemSystem/GT_Consumable.h"
#include "GameFramework/Pawn.h"

UConsumable::UConsumable()
{
}

void UConsumable::OnUse_Implementation(APawn* User)
{
	if (!User || StackSize <= 0)
	{
		return;
	}

	// 执行消耗品效果
	ExecuteEffect(User);

	// 触发 OnUse 事件
	TriggerEffect(EGT_ItemEffectTrigger::OnUse, User, TEXT(""));

	// 消耗一个堆叠
	RemoveStack(1);

	// 如果堆叠为0则销毁
	if (StackSize <= 0)
	{
		OnDestroy();
	}
}

void UConsumable::ExecuteEffect_Implementation(APawn* User)
{
	// 基类不实现具体效果，由子类或蓝图重写
	// 此处可添加通用的消耗品效果处理
}
