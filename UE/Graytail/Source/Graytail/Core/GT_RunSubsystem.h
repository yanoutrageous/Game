#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/GT_CommandBus.h"
#include "GT_RunSubsystem.generated.h"

class UGT_CommandProcessor;
class UGT_ContentRegistry;
class UGT_EffectSystem;
class UGT_EventBus;
class UGT_QueryFacade;
class UGT_RunContext;

UCLASS()
class GRAYTAIL_API UGT_RunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	UGT_RunContext* StartNewRun(int32 Seed, int32 Width = 10, int32 Height = 10);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	bool SubmitCommand(const FGT_Command& Command);

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	UGT_RunContext* GetCurrentRunContext() const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	void EndCurrentRun();

	UGT_CommandBus* GetCommandBus() const;
	UGT_EventBus* GetEventBus() const;
	UGT_EffectSystem* GetEffectSystem() const;
	UGT_ContentRegistry* GetContentRegistry() const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	UGT_QueryFacade* GetQueryFacade() const;

private:
	UPROPERTY(Transient)
	UGT_RunContext* CurrentRunContext = nullptr;

	UPROPERTY(Transient)
	UGT_CommandBus* CommandBus = nullptr;

	UPROPERTY(Transient)
	UGT_CommandProcessor* CommandProcessor = nullptr;

	UPROPERTY(Transient)
	UGT_EventBus* EventBus = nullptr;

	UPROPERTY(Transient)
	UGT_EffectSystem* EffectSystem = nullptr;

	UPROPERTY(Transient)
	UGT_ContentRegistry* ContentRegistry = nullptr;

	UPROPERTY(Transient)
	UGT_QueryFacade* QueryFacade = nullptr;
};
