#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/GT_CommandBus.h"
#include "GT_CommandProcessor.generated.h"

class UGT_EventBus;
class UGT_RunContext;

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_CommandProcessor : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Command")
	void Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Command")
	bool ProcessCommand(const FGT_Command& Command);

private:
	bool ProcessMoveCommand(const FGT_Command& Command);
	bool ProcessScanCommand(const FGT_Command& Command);
	void PublishCommandEvent(FName EventType, FName TargetActorId, int32 X, int32 Y, bool bSuccess) const;

	UPROPERTY(Transient)
	UGT_RunContext* RunContext = nullptr;

	UPROPERTY(Transient)
	UGT_EventBus* EventBus = nullptr;
};
