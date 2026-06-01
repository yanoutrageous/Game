#include "Core/GT_EffectSystem.h"

void UGT_EffectSystem::ApplyEffectById(FName EffectId)
{
	if (!EffectId.IsNone())
	{
		QueuedEffectIds.Add(EffectId);
	}
}

void UGT_EffectSystem::ClearQueuedEffects()
{
	QueuedEffectIds.Reset();
}

int32 UGT_EffectSystem::GetQueuedEffectCount() const
{
	return QueuedEffectIds.Num();
}
