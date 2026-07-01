#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "GT_MetaSaveGame.generated.h"

// 局外元进度存档容器。slot "GraytailMeta", user index 0。
UCLASS(BlueprintType)
class GRAYTAIL_API UGT_MetaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 2;

	UPROPERTY() FGT_MetaProgressState State;
	UPROPERTY() int32 SaveVersion = CurrentSaveVersion;
};
