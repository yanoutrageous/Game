#include "Core/GT_EventBus.h"

void UGT_EventBus::PublishEvent(const FGT_GameEvent& Event)
{
	EventHistory.Add(Event);
	OnGameEventPublished.Broadcast(Event);
}

void UGT_EventBus::ClearEventHistory()
{
	EventHistory.Reset();
}

int32 UGT_EventBus::GetEventCount() const
{
	return EventHistory.Num();
}

const TArray<FGT_GameEvent>& UGT_EventBus::GetEventHistory() const
{
	return EventHistory;
}
