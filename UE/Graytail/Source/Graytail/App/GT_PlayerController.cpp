#include "App/GT_PlayerController.h"

#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Domains/Map/GT_MapTypes.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/GT_GameHudWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailStartup, Log, All);

bool AGT_PlayerController::ShouldCreateHud(bool bIsLocalController, bool bHasExistingHud)
{
	return bIsLocalController && !bHasExistingHud;
}

#if !UE_BUILD_SHIPPING
bool AGT_PlayerController::TryGetIsolatedStartupProbeSlot(
	const TCHAR* CommandLine,
	FString& OutSaveSlot)
{
	OutSaveSlot.Reset();
	if (!FParse::Param(CommandLine, TEXT("GraytailStartupProbe"))
		|| !FParse::Value(CommandLine, TEXT("GraytailSaveSlot="), OutSaveSlot))
	{
		return false;
	}
	return OutSaveSlot.StartsWith(TEXT("GraytailStartupProbe_"))
		&& OutSaveSlot.Len() > FCString::Strlen(TEXT("GraytailStartupProbe_"));
}
#endif

void AGT_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureGameHud();
#if !UE_BUILD_SHIPPING
	RunStartupProbeIfRequested();
#endif
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

#if !UE_BUILD_SHIPPING
void AGT_PlayerController::RunStartupProbeIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("GraytailStartupProbe")))
	{
		return;
	}

	bool bPassed = true;
	auto Report = [&bPassed](const TCHAR* Check, bool bCheckPassed)
	{
		bPassed = bPassed && bCheckPassed;
		UE_LOG(
			LogGraytailStartup,
			Display,
			TEXT("GRAYTAIL_STARTUP|Check=%s|Result=%s"),
			Check,
			bCheckPassed ? TEXT("Pass") : TEXT("Fail"));
	};

	FString ProbeSaveSlot;
	const bool bIsolatedSlot = TryGetIsolatedStartupProbeSlot(
		FCommandLine::Get(),
		ProbeSaveSlot);
	Report(TEXT("SaveSlotIsolated"), bIsolatedSlot);
	Report(TEXT("HudCreated"), IsValid(GameHud));
	Report(TEXT("MainMenuVisible"), IsValid(GameHud) && GameHud->IsMainMenuVisible());

	bool bRunStarted = false;
	bool bRunAbandoned = false;
	if (bIsolatedSlot && IsValid(GameHud))
	{
		bRunStarted = GameHud->TryStartRunFromMenu(EGT_Difficulty::Standard);
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UGT_RunSubsystem* RunSubsystem =
				GameInstance->GetSubsystem<UGT_RunSubsystem>())
			{
				bRunStarted = bRunStarted
					&& IsValid(RunSubsystem->GetCurrentRunContext());
				if (bRunStarted)
				{
					bRunAbandoned = RunSubsystem->AbandonRun().bSuccess;
				}
			}
		}
	}
	Report(TEXT("RunStarted"), bRunStarted);
	Report(TEXT("RunAbandoned"), bRunAbandoned);

	if (bIsolatedSlot)
	{
		UGameplayStatics::DeleteGameInSlot(ProbeSaveSlot, 0);
		UGameplayStatics::DeleteGameInSlot(ProbeSaveSlot + TEXT("Backup"), 0);
	}

	UE_LOG(
		LogGraytailStartup,
		Display,
		TEXT("GRAYTAIL_STARTUP|Overall=%s"),
		bPassed ? TEXT("Pass") : TEXT("Fail"));
	FPlatformMisc::RequestExitWithStatus(true, bPassed ? 0 : 1);
}
#endif
