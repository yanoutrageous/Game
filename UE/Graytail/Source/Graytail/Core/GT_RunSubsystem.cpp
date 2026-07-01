#include "Core/GT_RunSubsystem.h"

#include "Core/GT_CommandBus.h"
#include "Core/GT_CommandProcessor.h"
#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EffectSystem.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunContext.h"
#include "Data/GT_GameDataSubsystem.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaSettlement.h"
#include "Engine/GameInstance.h"

void UGT_RunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CommandBus = NewObject<UGT_CommandBus>(this);
	CommandProcessor = NewObject<UGT_CommandProcessor>(this);
	EventBus = NewObject<UGT_EventBus>(this);
	if (EventBus)
	{
		// 订阅局终事件 -> 局外结算(S2)。一处覆盖撤离/各失败路径。
		EventBus->OnGameEventPublished.AddDynamic(this, &UGT_RunSubsystem::HandleRunEvent);
	}
	EffectSystem = NewObject<UGT_EffectSystem>(this);
	ContentRegistry = NewObject<UGT_ContentRegistry>(this);
	if (ContentRegistry)
	{
		ContentRegistry->InitializeDefaultRoomDefinitions();
	}
	QueryFacade = NewObject<UGT_QueryFacade>(this);
}

void UGT_RunSubsystem::Deinitialize()
{
	ClearCurrentRun();

	CommandBus = nullptr;
	CommandProcessor = nullptr;
	EventBus = nullptr;
	EffectSystem = nullptr;
	ContentRegistry = nullptr;
	QueryFacade = nullptr;

	Super::Deinitialize();
}

UGT_RunContext* UGT_RunSubsystem::StartNewRun(int32 Seed, int32 Width, int32 Height)
{
	if (!GT_GameData::IsReady())
	{
		UE_LOG(LogTemp, Display, TEXT("StartNewRun blocked by invalid game data: %s"), *GT_GameData::GetErrorText());
		return nullptr;
	}

	const FGT_MetaOperationResult EndResult = EndCurrentRun();
	if (!EndResult.bSuccess)
	{
		return nullptr;
	}
	CurrentRunContext = NewObject<UGT_RunContext>(this);
	bRunSettled = false;
	CurrentRunContext->InitializeRun(Seed, Width, Height);
	FinishStartRun();
	return CurrentRunContext;
}

UGT_RunContext* UGT_RunSubsystem::StartNewRunStandard(int32 Seed, EGT_Difficulty Difficulty)
{
	if (!GT_GameData::IsReady())
	{
		UE_LOG(LogTemp, Display, TEXT("StartNewRunStandard blocked by invalid game data: %s"), *GT_GameData::GetErrorText());
		return nullptr;
	}

	TMap<FName, int32> ConsumedConsumables;
	UGT_MetaProgressSubsystem* Meta = nullptr;
	if (Difficulty != EGT_Difficulty::Tutorial)
	{
		UGameInstance* GameInstance = GetGameInstance();
		Meta = GameInstance ? GameInstance->GetSubsystem<UGT_MetaProgressSubsystem>() : nullptr;
		if (!Meta)
		{
			return nullptr;
		}
		FGuid RunId;
		const FGT_MetaOperationResult EscrowResult = Meta->BeginRunEscrow(
			Seed,
			Difficulty,
			RunId,
			ConsumedConsumables);
		if (!EscrowResult.bSuccess)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("StartNewRunStandard blocked by save failure: %s"),
				*EscrowResult.Message.ToString());
			return nullptr;
		}
	}

	const FGT_MetaOperationResult EndResult = EndCurrentRun();
	if (!EndResult.bSuccess)
	{
		if (Meta)
		{
			Meta->RecoverInterruptedRun();
		}
		return nullptr;
	}
	CurrentRunContext = NewObject<UGT_RunContext>(this);
	bRunSettled = false;
	PendingSettlementEvent = NAME_None;
	CurrentRunContext->InitializeRunStandard(Seed, Difficulty);
	if (CurrentRunContext->IsTutorialRun())
	{
		// 教程独立: 不接入局外装备系统(不扣库存/不带真实装备), 只塞一个占位止血贴教学按 Q 使用。
		CurrentRunContext->CheatGiveItem(FName(TEXT("emergency_bandage")), 1);
	}
	else
	{
		ApplyMetaLoadoutToRun(ConsumedConsumables);
	}
	FinishStartRun();
	return CurrentRunContext;
}

void UGT_RunSubsystem::FinishStartRun()
{
	if (QueryFacade)
	{
		QueryFacade->Initialize(CurrentRunContext);
	}

	if (CommandProcessor)
	{
		CommandProcessor->Initialize(CurrentRunContext, EventBus, ContentRegistry);
	}

	if (EventBus)
	{
		int32 PlayerX = 0;
		int32 PlayerY = 0;
		CurrentRunContext->TryGetPlayerPosition(PlayerX, PlayerY);

		FGT_GameEvent Event;
		Event.EventType = FName(TEXT("RunStarted"));
		Event.SourceSystem = FName(TEXT("RunSubsystem"));
		Event.SourceActorId = CurrentRunContext->GetPlayerActorId();
		Event.TargetActorId = CurrentRunContext->GetPlayerActorId();
		Event.X = PlayerX;
		Event.Y = PlayerY;
		Event.RoomCoord = FIntPoint(PlayerX, PlayerY);
		Event.bSuccess = true;
		EventBus->PublishEvent(Event);
	}
}

bool UGT_RunSubsystem::SubmitCommand(const FGT_Command& Command)
{
	if (!CommandBus || !CommandProcessor)
	{
		return false;
	}

	CommandBus->SubmitCommand(Command);
	const bool bProcessed = CommandProcessor->ProcessCommand(Command);
	CommandBus->ClearPendingCommands();
	return bProcessed;
}

UGT_RunContext* UGT_RunSubsystem::GetCurrentRunContext() const
{
	return CurrentRunContext;
}

FGT_MetaOperationResult UGT_RunSubsystem::EndCurrentRun()
{
	if (HasPendingSettlement())
	{
		const FGT_MetaOperationResult RetryResult = RetryPendingSettlement();
		if (!RetryResult.bSuccess)
		{
			return RetryResult;
		}
	}
	if (CurrentRunContext
		&& CurrentRunContext->IsRunActive()
		&& CurrentRunContext->GetMapMode() == EGT_MapMode::Standard
		&& !CurrentRunContext->IsTutorialRun())
	{
		return AbandonRun();
	}
	ClearCurrentRun();
	return FGT_MetaOperationResult::Success();
}

void UGT_RunSubsystem::ClearCurrentRun()
{
	if (CurrentRunContext)
	{
		CurrentRunContext->ResetRun();
		CurrentRunContext = nullptr;
	}

	if (QueryFacade)
	{
		QueryFacade->Reset();
	}

	if (CommandProcessor)
	{
		CommandProcessor->Initialize(nullptr, EventBus, ContentRegistry);
	}

	if (CommandBus)
	{
		CommandBus->ClearPendingCommands();
	}

	if (EventBus)
	{
		EventBus->ClearEventHistory();
	}

	bRunSettled = false;
	PendingSettlementEvent = NAME_None;
}

void UGT_RunSubsystem::ApplyMetaLoadoutToRun(
	const TMap<FName, int32>& ConsumedConsumables)
{
	// 仅 Standard 局应用(BasicDebug/163 夹具不动)。
	if (!CurrentRunContext || CurrentRunContext->GetMapMode() != EGT_MapMode::Standard)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UGT_MetaProgressSubsystem* Meta = GameInstance ? GameInstance->GetSubsystem<UGT_MetaProgressSubsystem>() : nullptr;
	if (!Meta)
	{
		return;
	}

	const FGT_EquipBonus Equip = Meta->GetEquipBonus();
	const FGT_TalentEffects Talents = Meta->GetTalentEffects();
	const TArray<FName> EquippedIds = Meta->GetEquippedItems();      // S6: 激活触发型装备
	CurrentRunContext->ApplyMetaLoadout(Equip, Talents, ConsumedConsumables, EquippedIds);
}

void UGT_RunSubsystem::HandleRunEvent(FGT_GameEvent Event)
{
	// 局终结算: 仅 Standard 模式触发(BasicDebug/163 夹具不结算); 教程局也排除(训练工单不登记/不结算, 对齐 Lua)。
	if (!CurrentRunContext || CurrentRunContext->GetMapMode() != EGT_MapMode::Standard || CurrentRunContext->IsTutorialRun())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UGT_MetaProgressSubsystem* Meta = GameInstance ? GameInstance->GetSubsystem<UGT_MetaProgressSubsystem>() : nullptr;
	if (!Meta)
	{
		return;
	}

	const FName Type = Event.EventType;
	if (Type == FName(TEXT("RunSucceeded")) || Type == FName(TEXT("RunFailed")))
	{
		if (!bRunSettled)
		{
			PendingSettlementEvent = Type;
			RetryPendingSettlement();
		}
	}
}

FGT_MetaOperationResult UGT_RunSubsystem::RetryPendingSettlement()
{
	if (!CurrentRunContext || PendingSettlementEvent.IsNone())
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("no_pending_settlement")),
			FText::FromString(TEXT("There is no pending run settlement.")));
	}
	UGameInstance* GameInstance = GetGameInstance();
	UGT_MetaProgressSubsystem* Meta = GameInstance
		? GameInstance->GetSubsystem<UGT_MetaProgressSubsystem>()
		: nullptr;
	if (!Meta)
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("meta_unavailable")),
			FText::FromString(TEXT("Meta progress is unavailable.")));
	}

	const FGT_MetaOperationResult Result =
		PendingSettlementEvent == FName(TEXT("RunSucceeded"))
			? GT_MetaSettlement::SettleExtraction(*CurrentRunContext, *Meta)
			: GT_MetaSettlement::SettleFailure(*CurrentRunContext, *Meta);
	if (Result.bSuccess)
	{
		bRunSettled = true;
		PendingSettlementEvent = NAME_None;
	}
	return Result;
}

FGT_MetaOperationResult UGT_RunSubsystem::AbandonRun()
{
	if (!CurrentRunContext || !CurrentRunContext->IsRunActive())
	{
		return FGT_MetaOperationResult::Success();
	}
	if (CurrentRunContext->GetMapMode() == EGT_MapMode::Standard && !CurrentRunContext->IsTutorialRun())
	{
		UGameInstance* GameInstance = GetGameInstance();
		UGT_MetaProgressSubsystem* Meta = GameInstance ? GameInstance->GetSubsystem<UGT_MetaProgressSubsystem>() : nullptr;
		if (!Meta)
		{
			return FGT_MetaOperationResult::Failure(
				FName(TEXT("meta_unavailable")),
				FText::FromString(TEXT("Meta progress is unavailable.")));
		}
		FGT_FailureSettlement Settlement;
		Settlement.Reason = FName(TEXT("Abandoned"));
		const FGT_MetaOperationResult Result = Meta->CommitFailure(Settlement);
		if (!Result.bSuccess)
		{
			return Result;
		}
		bRunSettled = true;
	}

	CurrentRunContext->MarkRunFailed(FName(TEXT("Abandoned")));
	ClearCurrentRun();
	return FGT_MetaOperationResult::Success();
}

UGT_CommandBus* UGT_RunSubsystem::GetCommandBus() const
{
	return CommandBus;
}

UGT_EventBus* UGT_RunSubsystem::GetEventBus() const
{
	return EventBus;
}

UGT_EffectSystem* UGT_RunSubsystem::GetEffectSystem() const
{
	return EffectSystem;
}

UGT_ContentRegistry* UGT_RunSubsystem::GetContentRegistry() const
{
	return ContentRegistry;
}

UGT_QueryFacade* UGT_RunSubsystem::GetQueryFacade() const
{
	return QueryFacade;
}
