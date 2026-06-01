#include "Debug/GT_RuntimeSmokeRunner.h"

#include "Debug/GT_DebugSubsystem.h"
#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailRuntimeSmoke, Log, All);

namespace
{
void ShutdownSmokeGameInstance(UGameInstance* GameInstance)
{
	if (!GameInstance)
	{
		return;
	}

	if (UWorld* World = GameInstance->GetWorld())
	{
		World->CleanupWorld();
	}

	GameInstance->Shutdown();
}
}

UGT_RuntimeSmokeRunnerCommandlet::UGT_RuntimeSmokeRunnerCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGT_RuntimeSmokeRunnerCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGraytailRuntimeSmoke, Display, TEXT("Graytail runtime smoke validation started."));

	if (!GEngine)
	{
		UE_LOG(LogGraytailRuntimeSmoke, Error, TEXT("GRAYTAIL_SMOKE|Check=EngineValid|Result=Fail|Message=GEngine is not valid."));
		return 1;
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	if (!GameInstance)
	{
		UE_LOG(LogGraytailRuntimeSmoke, Error, TEXT("GRAYTAIL_SMOKE|Check=GameInstanceCreated|Result=Fail|Message=Failed to create transient GameInstance."));
		return 1;
	}

	GameInstance->InitializeStandalone(FName(TEXT("GraytailRuntimeSmoke")));

	UGT_DebugSubsystem* DebugSubsystem = GameInstance->GetSubsystem<UGT_DebugSubsystem>();
	if (!DebugSubsystem)
	{
		UE_LOG(LogGraytailRuntimeSmoke, Error, TEXT("GRAYTAIL_SMOKE|Check=DebugSubsystemValid|Result=Fail|Message=DebugSubsystem is not valid."));
		ShutdownSmokeGameInstance(GameInstance);
		return 1;
	}

	TArray<FGT_RuntimeSmokeCheckResult> Results;
	const bool bPassed = DebugSubsystem->RunMinimalMovementSmokeTest(Results);

	for (const FGT_RuntimeSmokeCheckResult& Result : Results)
	{
		UE_LOG(
			LogGraytailRuntimeSmoke,
			Display,
			TEXT("GRAYTAIL_SMOKE|Check=%s|Result=%s|Message=%s"),
			*Result.CheckName.ToString(),
			Result.bPassed ? TEXT("Pass") : TEXT("Fail"),
			*Result.Message);
	}

	UE_LOG(LogGraytailRuntimeSmoke, Display, TEXT("GRAYTAIL_SMOKE|Overall=%s|Count=%d"), bPassed ? TEXT("Pass") : TEXT("Fail"), Results.Num());

	ShutdownSmokeGameInstance(GameInstance);
	return bPassed ? 0 : 1;
}
