#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_RoomResolver.generated.h"

class UGT_EventBus;
class UGT_ContentRegistry;
class UGT_RunContext;
struct FGT_CombatResultDef;
struct FGT_CombatRuntimeState;
struct FGT_EventOptionDef;

UENUM(BlueprintType)
enum class EGT_RoomResolveOutcome : uint8
{
	None UMETA(DisplayName = "None"),
	NormalResolved UMETA(DisplayName = "Normal Resolved"),
	MineEncountered UMETA(DisplayName = "Mine Encountered"),
	ExitFound UMETA(DisplayName = "Exit Found"),
	Unsupported UMETA(DisplayName = "Unsupported"),
	EventPresented UMETA(DisplayName = "Event Presented"),
	CombatStarted UMETA(DisplayName = "Combat Started"),
	CombatAlreadyResolved UMETA(DisplayName = "Combat Already Resolved")
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RoomResolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	EGT_RoomResolveOutcome Outcome = EGT_RoomResolveOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	EGT_RoomBaseType RoomBaseType = EGT_RoomBaseType::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	bool bTriggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	bool bResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	FName RoomDefId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	FName RoomContentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	FName RoomRuleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Room")
	FName RoomInstanceId = NAME_None;
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_RoomResolver : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UGT_RunContext* InRunContext, UGT_EventBus* InEventBus, UGT_ContentRegistry* InContentRegistry);
	bool ResolveRoomAt(int32 X, int32 Y, FGT_RoomResolveResult& OutResult);
	bool ChooseEventOptionAt(int32 X, int32 Y, FName OptionId, FGT_RoomResolveResult& OutResult);
	bool ResolveCombatAt(int32 X, int32 Y, FName ResultId, FGT_RoomResolveResult& OutResult);
	bool AttackCombatAt(int32 X, int32 Y, FGT_RoomResolveResult& OutResult, const TArray<int32>& HitEnemyIds = TArray<int32>());

private:
	bool BuildResultFromTruthCell(int32 X, int32 Y, FGT_RoomResolveResult& OutResult) const;
	bool ResolveRoomByHandler(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool ResolveNormalRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool ResolveMineRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool ResolveExitRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool ResolveEventRoomPlaceholder(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool ResolveCombatRoomPlaceholder(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool ResolveUnsupportedRoom(const FGT_TruthCell& TruthCell, FGT_RoomResolveResult& OutResult) const;
	bool TryGetRoomDefinitions(
		const FGT_RoomResolveResult& Result,
		EGT_RoomBaseType ExpectedRoomBaseType,
		FString& OutFailureReason) const;
	FName GetDefaultEventOptionId(const FGT_RoomResolveResult& Result) const;
	FName GetDefaultCombatResultId(const FGT_RoomResolveResult& Result) const;
	FString BuildRoomDefinitionPayloadText(const FGT_RoomResolveResult& Result, const FString& Prefix) const;
	FString BuildEventOptionPayloadText(const FGT_RoomResolveResult& Result, const FGT_EventOptionDef& OptionDefinition, const FString& Prefix) const;
	FString BuildCombatResultPayloadText(const FGT_RoomResolveResult& Result, const FGT_CombatResultDef& ResultDefinition, const FString& Prefix) const;
	FString BuildCombatStatePayloadText(const FGT_RoomResolveResult& Result, const FGT_CombatRuntimeState& CombatState, const FString& Prefix) const;
	void PublishResolverEvent(FName EventType, const FGT_RoomResolveResult& Result, bool bSuccess) const;
	void PublishDefinitionFailureEvent(const FGT_RoomResolveResult& Result, const FString& FailureReason) const;
	void PublishInteractionEvent(FName EventType, const FGT_RoomResolveResult& Result, FName PayloadId, bool bSuccess, const FString& PayloadText) const;

	UPROPERTY(Transient)
	UGT_RunContext* RunContext = nullptr;

	UPROPERTY(Transient)
	UGT_EventBus* EventBus = nullptr;

	UPROPERTY(Transient)
	UGT_ContentRegistry* ContentRegistry = nullptr;
};
