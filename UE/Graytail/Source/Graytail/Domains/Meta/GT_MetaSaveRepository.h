#pragma once

#include "CoreMinimal.h"
#include "Domains/Meta/GT_MetaPersistenceTypes.h"

class USaveGame;

class GRAYTAIL_API IGT_MetaSaveBackend
{
public:
	virtual ~IGT_MetaSaveBackend() = default;

	virtual bool Exists(const FString& Slot, int32 UserIndex) const = 0;
	virtual USaveGame* Load(const FString& Slot, int32 UserIndex) const = 0;
	virtual bool Save(USaveGame& Object, const FString& Slot, int32 UserIndex) = 0;
};

class GRAYTAIL_API FGT_MetaSaveRepository
{
public:
	FGT_MetaSaveRepository(
		TSharedRef<IGT_MetaSaveBackend> InBackend,
		FString InMainSlot,
		int32 InUserIndex);

	static TUniquePtr<FGT_MetaSaveRepository> CreateEngine(
		const FString& MainSlot,
		int32 UserIndex);

	const FString& MainSlot() const { return MainSlotName; }
	FString BackupSlot() const { return MainSlotName + TEXT("Backup"); }

	FGT_MetaLoadResult Load();
	FGT_MetaOperationResult Commit(const FGT_MetaProgressState& Candidate);
	FGT_MetaOperationResult ResetWithFreshState();

private:
	enum class EReadResult : uint8
	{
		Valid,
		Missing,
		Corrupt,
		UnsupportedVersion
	};

	EReadResult ReadSlot(
		const FString& Slot,
		FGT_MetaProgressState& OutState,
		FText& OutMessage,
		bool* bOutMigrated = nullptr) const;
	FGT_MetaOperationResult WriteState(
		const FString& Slot,
		const FGT_MetaProgressState& State,
		FName ErrorCode);
	void WriteDebugMirror(const FGT_MetaProgressState& State) const;

	TSharedRef<IGT_MetaSaveBackend> Backend;
	FString MainSlotName;
	int32 UserIndex = 0;
};
