#include "Core/GT_EventBus.h"

void UGT_EventBus::PublishEvent(const FGT_GameEvent& Event)
{
	FGT_GameEvent SequencedEvent = Event;
	if (SequencedEvent.SequenceId <= 0)
	{
		SequencedEvent.SequenceId = EventHistory.Num() + 1;
	}

	EventHistory.Add(SequencedEvent);
	OnGameEventPublished.Broadcast(SequencedEvent);
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

void UGT_EventBus::GetEventTypeCounts(TMap<FName, int32>& OutCounts) const
{
	OutCounts.Reset();

	for (const FGT_GameEvent& Event : EventHistory)
	{
		if (Event.EventType.IsNone())
		{
			continue;
		}

		int32& Count = OutCounts.FindOrAdd(Event.EventType);
		++Count;
	}
}

const TArray<FGT_GameEvent>& UGT_EventBus::GetEventHistory() const
{
	return EventHistory;
}
