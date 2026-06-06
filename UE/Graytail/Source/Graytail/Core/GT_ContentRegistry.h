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
	bool FindRoomContentDef(FName ContentId, FGT_RoomContentDef& OutDefinition) const;
	bool FindRoomRuleDef(FName RuleId, FGT_RoomRuleDef& OutDefinition) const;
	bool FindRoomDefinitions(
		FName ContentId,
		FName RuleId,
		EGT_RoomBaseType ExpectedRoomBaseType,
		FGT_RoomContentDef& OutContentDefinition,
		FGT_RoomRuleDef& OutRuleDefinition,
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
};
