#include "App/GT_PlayerController.h"

#include "UI/GT_GameHudWidget.h"

bool AGT_PlayerController::ShouldCreateHud(bool bIsLocalController, bool bHasExistingHud)
{
	return bIsLocalController && !bHasExistingHud;
}

void AGT_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureGameHud();
}

void AGT_PlayerController::EnsureGameHud()
{
	if (!ShouldCreateHud(IsLocalController(), IsValid(GameHud)))
	{
		return;
	}

	GameHud = CreateWidget<UGT_GameHudWidget>(this, UGT_GameHudWidget::StaticClass());
	if (!GameHud)
	{
		UE_LOG(LogTemp, Error, TEXT("Graytail HUD creation failed."));
		return;
	}

	GameHud->AddToViewport();
	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameHud->TakeWidget());
	SetInputMode(InputMode);
}
