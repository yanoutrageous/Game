#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GT_CommandBus.generated.h"

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_Command
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Command")
	FGuid CommandId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Command")
	FName CommandType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Command")
	FName SourceActorId = NAME_None;

	FGT_Command()
		: CommandId(FGuid::NewGuid())
	{
	}
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_CommandBus : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Command")
	void SubmitCommand(const FGT_Command& Command);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Command")
	void ClearPendingCommands();

	UFUNCTION(BlueprintPure, Category = "Graytail|Command")
	int32 GetPendingCommandCount() const;

	const TArray<FGT_Command>& GetPendingCommands() const;

private:
	UPROPERTY(Transient)
	TArray<FGT_Command> PendingCommands;
};
