#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GT_EventBus.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_GameEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	FGuid EventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	FName EventType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	FName SourceSystem = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	FName TargetActorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Event")
	bool bSuccess = false;

	FGT_GameEvent()
		: EventId(FGuid::NewGuid())
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGT_GameEventPublishedSignature, FGT_GameEvent, Event);

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_EventBus : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Graytail|Event")
	FGT_GameEventPublishedSignature OnGameEventPublished;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Event")
	void PublishEvent(const FGT_GameEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Event")
	void ClearEventHistory();

	UFUNCTION(BlueprintPure, Category = "Graytail|Event")
	int32 GetEventCount() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Event")
	bool HasEventOfType(FName EventType) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Event")
	int32 CountEventsOfType(FName EventType) const;

	void GetEventTypeCounts(TMap<FName, int32>& OutCounts) const;

	const TArray<FGT_GameEvent>& GetEventHistory() const;

private:
	UPROPERTY(Transient)
	TArray<FGT_GameEvent> EventHistory;
};
