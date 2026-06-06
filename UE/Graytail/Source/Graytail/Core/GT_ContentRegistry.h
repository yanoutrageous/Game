#pragma once

#include "CoreMinimal.h"
#include "Domains/Map/GT_MapTypes.h"
#include "UObject/Object.h"
#include "GT_ContentRegistry.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RoomContentDef
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName ContentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName DefaultRuleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	EGT_RoomBaseType RoomBaseType = EGT_RoomBaseType::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName DefaultOptionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName DefaultResultId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	TArray<FName> AvailableOptionIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	TArray<FName> AvailableResultIds;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RoomRuleDef
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName RuleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	EGT_RoomBaseType RoomBaseType = EGT_RoomBaseType::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName DefaultOptionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName DefaultResultId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	TArray<FName> AvailableOptionIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	TArray<FName> AvailableResultIds;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_EventOptionDef
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName Id = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	bool bResolvesRoom = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString PayloadText;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_CombatResultDef
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName Id = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString DebugDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	bool bResolvesRoom = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FString PayloadText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Content")
	FName EventType = NAME_None;
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_ContentRegistry : public UObject
{
	GENERATED_BODY()

public:
	void InitializeDefaultRoomDefinitions();

	UFUNCTION(BlueprintCallable, Category = "Graytail|Content")
	void RegisterContentId(FName ContentId);

	bool RegisterRoomContentDef(const FGT_RoomContentDef& Definition);
	bool RegisterRoomRuleDef(const FGT_RoomRuleDef& Definition);
	bool RegisterEventOptionDef(const FGT_EventOptionDef& Definition);
	bool RegisterCombatResultDef(const FGT_CombatResultDef& Definition);
	bool FindRoomContentDef(FName ContentId, FGT_RoomContentDef& OutDefinition) const;
	bool FindRoomRuleDef(FName RuleId, FGT_RoomRuleDef& OutDefinition) const;
	bool FindEventOptionDef(FName OptionId, FGT_EventOptionDef& OutDefinition) const;
	bool FindCombatResultDef(FName ResultId, FGT_CombatResultDef& OutDefinition) const;
	bool FindRoomDefinitions(
		FName ContentId,
		FName RuleId,
		EGT_RoomBaseType ExpectedRoomBaseType,
		FGT_RoomContentDef& OutContentDefinition,
		FGT_RoomRuleDef& OutRuleDefinition,
		FString& OutFailureReason) const;
	bool FindEventOptionDefForRoom(
		FName ContentId,
		FName RuleId,
		FName OptionId,
		FGT_EventOptionDef& OutDefinition,
		FString& OutFailureReason) const;
	bool FindCombatResultDefForRoom(
		FName ContentId,
		FName RuleId,
		FName ResultId,
		FGT_CombatResultDef& OutDefinition,
		FString& OutFailureReason) const;
	bool GetEventOptionDefsForRoom(
		FName ContentId,
		FName RuleId,
		TArray<FGT_EventOptionDef>& OutDefinitions,
		FString& OutFailureReason) const;
	bool GetCombatResultDefsForRoom(
		FName ContentId,
		FName RuleId,
		TArray<FGT_CombatResultDef>& OutDefinitions,
		FString& OutFailureReason) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Content")
	bool IsContentRegistered(FName ContentId) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Content")
	void ClearRegistry();

private:
	UPROPERTY(Transient)
	TSet<FName> RegisteredContentIds;

	UPROPERTY(Transient)
	TMap<FName, FGT_RoomContentDef> RoomContentDefs;

	UPROPERTY(Transient)
	TMap<FName, FGT_RoomRuleDef> RoomRuleDefs;

	UPROPERTY(Transient)
	TMap<FName, FGT_EventOptionDef> EventOptionDefs;

	UPROPERTY(Transient)
	TMap<FName, FGT_CombatResultDef> CombatResultDefs;
};
