#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Data/GT_GameDataTypes.h"
#include "GT_GameDataSubsystem.generated.h"

class GRAYTAIL_API FGT_GameDataLoader
{
public:
	static bool LoadFromDirectory(const FString& Directory, FGT_GameDataSnapshot& OutSnapshot, TArray<FString>& OutErrors);
};

UCLASS()
class GRAYTAIL_API UGT_GameDataSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool ReloadFromDirectory(const FString& Directory, bool bLogErrors = true);
	bool IsReady() const;
	const FGT_GameDataSnapshot* GetSnapshot() const;
	uint64 GetRevision() const;
	FString GetErrorText() const;
	static FString GetDefaultDataDirectory();

private:
	FGT_GameDataSnapshot Snapshot;
	TArray<FString> Errors;
	bool bReady = false;
	uint64 Revision = 0;
};

namespace GT_GameData
{
	GRAYTAIL_API const UGT_GameDataSubsystem* GetSubsystem();
	GRAYTAIL_API const FGT_GameDataSnapshot* GetSnapshot();
	GRAYTAIL_API uint64 GetRevision();
	GRAYTAIL_API bool IsReady();
	GRAYTAIL_API FString GetErrorText();
}
