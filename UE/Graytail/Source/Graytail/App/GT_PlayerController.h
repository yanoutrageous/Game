#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GT_PlayerController.generated.h"

class UGT_GameHudWidget;

UCLASS()
class GRAYTAIL_API AGT_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	static bool ShouldCreateHud(bool bIsLocalController, bool bHasExistingHud);
#if !UE_BUILD_SHIPPING
	static bool TryGetIsolatedStartupProbeSlot(
		const TCHAR* CommandLine,
		FString& OutSaveSlot);
#endif

protected:
	virtual void BeginPlay() override;

private:
	void EnsureGameHud();
#if !UE_BUILD_SHIPPING
	void RunStartupProbeIfRequested();
#endif

	UPROPERTY(Transient)
	UGT_GameHudWidget* GameHud = nullptr;
};
