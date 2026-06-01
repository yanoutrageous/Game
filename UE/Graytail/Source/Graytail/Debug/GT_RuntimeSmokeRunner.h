#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GT_RuntimeSmokeRunner.generated.h"

UCLASS()
class GRAYTAIL_API UGT_RuntimeSmokeRunnerCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGT_RuntimeSmokeRunnerCommandlet();

	virtual int32 Main(const FString& Params) override;
};
