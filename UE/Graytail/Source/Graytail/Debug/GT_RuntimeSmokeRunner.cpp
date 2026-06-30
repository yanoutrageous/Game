#include "Debug/GT_RuntimeSmokeRunner.h"

#include "Debug/GT_DebugSubsystem.h"
#include "Debug/GT_RuntimeSmokeValidator.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

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

void DeleteSmokeSaveSlot(const FString& SlotName)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}
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

	const FString SmokeSaveSlot = FString::Printf(
		TEXT("GraytailMetaSmoke_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FCommandLine::Append(*FString::Printf(TEXT(" -GraytailSaveSlot=%s"), *SmokeSaveSlot));

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	if (!GameInstance)
	{
		UE_LOG(LogGraytailRuntimeSmoke, Error, TEXT("GRAYTAIL_SMOKE|Check=GameInstanceCreated|Result=Fail|Message=Failed to create transient GameInstance."));
		DeleteSmokeSaveSlot(SmokeSaveSlot);
		return 1;
	}

	GameInstance->InitializeStandalone(FName(TEXT("GraytailRuntimeSmoke")));

	UGT_DebugSubsystem* DebugSubsystem = GameInstance->GetSubsystem<UGT_DebugSubsystem>();
	if (!DebugSubsystem)
	{
		UE_LOG(LogGraytailRuntimeSmoke, Error, TEXT("GRAYTAIL_SMOKE|Check=DebugSubsystemValid|Result=Fail|Message=DebugSubsystem is not valid."));
		ShutdownSmokeGameInstance(GameInstance);
		DeleteSmokeSaveSlot(SmokeSaveSlot);
		return 1;
	}

	TArray<FGT_RuntimeSmokeCheckResult> Results;
	const bool bPassed = DebugSubsystem->RunMinimalMovementSmokeTest(Results);

	int32 PassCount = 0;
	int32 FailCount = 0;
	for (const FGT_RuntimeSmokeCheckResult& Result : Results)
	{
		if (Result.bPassed)
		{
			++PassCount;
		}
		else
		{
			++FailCount;
		}

		UE_LOG(
			LogGraytailRuntimeSmoke,
			Display,
			TEXT("GRAYTAIL_SMOKE|Check=%s|Result=%s|Message=%s"),
			*Result.CheckName.ToString(),
			Result.bPassed ? TEXT("Pass") : TEXT("Fail"),
			*Result.Message);
	}

	UE_LOG(
		LogGraytailRuntimeSmoke,
		Display,
		TEXT("GRAYTAIL_SMOKE|Overall=%s|Pass=%d|Fail=%d|Count=%d"),
		bPassed ? TEXT("Pass") : TEXT("Fail"),
		PassCount,
		FailCount,
		Results.Num());

	ShutdownSmokeGameInstance(GameInstance);
	DeleteSmokeSaveSlot(SmokeSaveSlot);
	return bPassed ? 0 : 1;
}
