#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GT_RuntimeSmokeValidator.generated.h"

class UGT_RunSubsystem;
class UGT_DebugSubsystem;

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RuntimeSmokeCheckResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Smoke")
	bool bPassed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Smoke")
	FName CheckName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Smoke")
	FString Message;
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_RuntimeSmokeValidator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Smoke")
	void Initialize(UGT_RunSubsystem* InRunSubsystem);

	void SetDebugSubsystem(UGT_DebugSubsystem* InDebugSubsystem);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Smoke")
	bool RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults);

private:
	static void AddCheck(TArray<FGT_RuntimeSmokeCheckResult>& OutResults, FName CheckName, bool bPassed, const FString& Message);

	UPROPERTY(Transient)
	UGT_RunSubsystem* RunSubsystem = nullptr;

	UPROPERTY(Transient)
	UGT_DebugSubsystem* DebugSubsystem = nullptr;
};
