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

bool UGT_EventBus::HasEventOfType(FName EventType) const
{
	return CountEventsOfType(EventType) > 0;
}

int32 UGT_EventBus::CountEventsOfType(FName EventType) const
{
	if (EventType.IsNone())
	{
		return 0;
	}

	int32 Count = 0;
	for (const FGT_GameEvent& Event : EventHistory)
	{
		if (Event.EventType == EventType)
		{
			++Count;
		}
	}

	return Count;
}

const TArray<FGT_GameEvent>& UGT_EventBus::GetEventHistory() const
{
	return EventHistory;
}
