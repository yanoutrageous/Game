#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/GT_CommandBus.h"
#include "Core/GT_RoomResolver.h"
#include "Core/GT_RunContext.h"
#include "GT_CommandProcessor.generated.h"

class UGT_EventBus;
class UGT_ContentRegistry;
class UGT_RunContext;

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_CommandProcessor : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Command")
	void Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus, UGT_ContentRegistry* InContentRegistry);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Command")
	bool ProcessCommand(const FGT_Command& Command);

private:
	bool ProcessMoveCommand(const FGT_Command& Command);
	bool ProcessScanCommand(const FGT_Command& Command);
	bool ProcessSearchCommand(const FGT_Command& Command);
	bool ProcessExtractCommand(const FGT_Command& Command);
	bool ProcessChooseEventOptionCommand(const FGT_Command& Command);
	bool ProcessResolveCombatCommand(const FGT_Command& Command);
	bool ProcessAttackCommand(const FGT_Command& Command);
	bool ProcessMonsterHitCommand(const FGT_Command& Command);
	bool ProcessFleeCombatCommand(const FGT_Command& Command);
	bool ProcessUseConsumableCommand(const FGT_Command& Command);
	void PublishCommandEvent(FName EventType, FName SourceActorId, FName TargetActorId, int32 X, int32 Y, bool bSuccess) const;

	// 协议压力变化广播: 总压力(ProtocolPressureChanged) + 等级变化(ProtocolLevelChanged) + 满压败北(RunFailed)。
	void PublishProtocolPressureEvent(FName ActorId, int32 X, int32 Y, int32 PressureDelta, const UGT_RunContext::FProtocolPressureResult& Result) const;

	UPROPERTY(Transient)
	UGT_RunContext* RunContext = nullptr;

	UPROPERTY(Transient)
	UGT_EventBus* EventBus = nullptr;

	UPROPERTY(Transient)
	UGT_ContentRegistry* ContentRegistry = nullptr;

	UPROPERTY(Transient)
	UGT_RoomResolver* RoomResolver = nullptr;
};
