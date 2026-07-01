#pragma once

#include "CoreMinimal.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "GT_MetaPersistenceTypes.generated.h"

UENUM(BlueprintType)
enum class EGT_MetaPersistenceStatus : uint8
{
	Ready,
	Fresh,
	RecoveredBackup,
	RecoveryWritePending,
	Corrupt,
	UnsupportedVersion,
	WriteFailed
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_MetaOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Meta")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Meta")
	FName ErrorCode = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Graytail|Meta")
	FText Message;

	static FGT_MetaOperationResult Success();
	static FGT_MetaOperationResult Failure(FName ErrorCode, const FText& Message);
};

USTRUCT()
struct GRAYTAIL_API FGT_MetaLoadResult
{
	GENERATED_BODY()

	UPROPERTY() EGT_MetaPersistenceStatus Status = EGT_MetaPersistenceStatus::Corrupt;
	UPROPERTY() FGT_MetaProgressState State;
	UPROPERTY() FText Message;

	bool IsUsable() const
	{
		return Status == EGT_MetaPersistenceStatus::Ready
			|| Status == EGT_MetaPersistenceStatus::Fresh
			|| Status == EGT_MetaPersistenceStatus::RecoveredBackup;
	}
};

GRAYTAIL_API bool GT_MigrateMetaSave(
	int32 SourceVersion,
	FGT_MetaProgressState& InOutState,
	FString& OutError);

GRAYTAIL_API bool GT_ValidateAndSanitizeMetaState(
	FGT_MetaProgressState& InOutState,
	FString& OutError);
