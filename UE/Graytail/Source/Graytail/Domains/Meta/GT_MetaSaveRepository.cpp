#include "Domains/Meta/GT_MetaSaveRepository.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/GT_MetaSaveGame.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	class FGT_EngineMetaSaveBackend final : public IGT_MetaSaveBackend
	{
	public:
		virtual bool Exists(const FString& Slot, int32 UserIndex) const override
		{
			return UGameplayStatics::DoesSaveGameExist(Slot, UserIndex);
		}

		virtual USaveGame* Load(const FString& Slot, int32 UserIndex) const override
		{
			return UGameplayStatics::LoadGameFromSlot(Slot, UserIndex);
		}

		virtual bool Save(USaveGame& Object, const FString& Slot, int32 UserIndex) override
		{
			return UGameplayStatics::SaveGameToSlot(&Object, Slot, UserIndex);
		}
	};

	FText ErrorText(const FString& Message)
	{
		return FText::FromString(Message);
	}
}

FGT_MetaSaveRepository::FGT_MetaSaveRepository(
	TSharedRef<IGT_MetaSaveBackend> InBackend,
	FString InMainSlot,
	int32 InUserIndex)
	: Backend(MoveTemp(InBackend))
	, MainSlotName(MoveTemp(InMainSlot))
	, UserIndex(InUserIndex)
{
}

TUniquePtr<FGT_MetaSaveRepository> FGT_MetaSaveRepository::CreateEngine(
	const FString& MainSlot,
	int32 UserIndex)
{
	return MakeUnique<FGT_MetaSaveRepository>(
		MakeShared<FGT_EngineMetaSaveBackend>(),
		MainSlot,
		UserIndex);
}

FGT_MetaLoadResult FGT_MetaSaveRepository::Load()
{
	FGT_MetaLoadResult Result;
	FText MainMessage;
	bool bMainMigrated = false;
	const EReadResult MainRead = ReadSlot(
		MainSlot(),
		Result.State,
		MainMessage,
		&bMainMigrated);
	if (MainRead == EReadResult::Valid)
	{
		if (bMainMigrated)
		{
			const FGT_MetaOperationResult MigrationWrite = Commit(Result.State);
			if (!MigrationWrite.bSuccess)
			{
				Result.Status = EGT_MetaPersistenceStatus::RecoveryWritePending;
				Result.Message = MigrationWrite.Message;
				return Result;
			}
		}
		Result.Status = EGT_MetaPersistenceStatus::Ready;
		return Result;
	}
	if (MainRead == EReadResult::UnsupportedVersion)
	{
		Result.Status = EGT_MetaPersistenceStatus::UnsupportedVersion;
		Result.Message = MainMessage;
		return Result;
	}

	FText BackupMessage;
	FGT_MetaProgressState BackupState;
	const EReadResult BackupRead = ReadSlot(BackupSlot(), BackupState, BackupMessage);
	if (MainRead == EReadResult::Missing && BackupRead == EReadResult::Missing)
	{
		Result.Status = EGT_MetaPersistenceStatus::Fresh;
		Result.State = FGT_MetaProgressState();
		return Result;
	}

	if (BackupRead == EReadResult::Valid)
	{
		Result.State = BackupState;
		const FGT_MetaOperationResult Rewrite = WriteState(
			MainSlot(),
			BackupState,
			FName(TEXT("recovery_write_failed")));
		if (!Rewrite.bSuccess)
		{
			Result.Status = EGT_MetaPersistenceStatus::RecoveryWritePending;
			Result.Message = Rewrite.Message;
			return Result;
		}
		Result.Status = EGT_MetaPersistenceStatus::RecoveredBackup;
		Result.Message = FText::FromString(TEXT("The main save was restored from backup."));
		WriteDebugMirror(Result.State);
		return Result;
	}

	if (MainRead == EReadResult::UnsupportedVersion
		|| BackupRead == EReadResult::UnsupportedVersion)
	{
		Result.Status = EGT_MetaPersistenceStatus::UnsupportedVersion;
		Result.Message = !MainMessage.IsEmpty() ? MainMessage : BackupMessage;
		return Result;
	}

	Result.Status = EGT_MetaPersistenceStatus::Corrupt;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("Both save slots are unusable. Main: %s Backup: %s"),
		*MainMessage.ToString(),
		*BackupMessage.ToString()));
	return Result;
}

FGT_MetaOperationResult FGT_MetaSaveRepository::Commit(
	const FGT_MetaProgressState& Candidate)
{
	FGT_MetaProgressState Sanitized = Candidate;
	FString ValidationError;
	if (!GT_ValidateAndSanitizeMetaState(Sanitized, ValidationError))
	{
		return FGT_MetaOperationResult::Failure(
			FName(TEXT("save_state_invalid")),
			ErrorText(ValidationError));
	}

	if (Backend->Exists(MainSlot(), UserIndex))
	{
		FGT_MetaProgressState CurrentState;
		FText ReadMessage;
		if (ReadSlot(MainSlot(), CurrentState, ReadMessage) != EReadResult::Valid)
		{
			return FGT_MetaOperationResult::Failure(
				FName(TEXT("main_save_invalid")),
				ReadMessage);
		}

		const FGT_MetaOperationResult BackupWrite = WriteState(
			BackupSlot(),
			CurrentState,
			FName(TEXT("backup_write_failed")));
		if (!BackupWrite.bSuccess)
		{
			return BackupWrite;
		}
	}

	const FGT_MetaOperationResult MainWrite = WriteState(
		MainSlot(),
		Sanitized,
		FName(TEXT("main_write_failed")));
	if (!MainWrite.bSuccess)
	{
		return MainWrite;
	}
	WriteDebugMirror(Sanitized);
	return FGT_MetaOperationResult::Success();
}

FGT_MetaOperationResult FGT_MetaSaveRepository::ResetWithFreshState()
{
	const FGT_MetaProgressState FreshState;
	const FGT_MetaOperationResult BackupWrite = WriteState(
		BackupSlot(),
		FreshState,
		FName(TEXT("reset_backup_write_failed")));
	if (!BackupWrite.bSuccess)
	{
		return BackupWrite;
	}
	const FGT_MetaOperationResult MainWrite = WriteState(
		MainSlot(),
		FreshState,
		FName(TEXT("reset_write_failed")));
	if (!MainWrite.bSuccess)
	{
		return MainWrite;
	}
	WriteDebugMirror(FreshState);
	return FGT_MetaOperationResult::Success();
}

FGT_MetaSaveRepository::EReadResult FGT_MetaSaveRepository::ReadSlot(
	const FString& Slot,
	FGT_MetaProgressState& OutState,
	FText& OutMessage,
	bool* bOutMigrated) const
{
	if (bOutMigrated)
	{
		*bOutMigrated = false;
	}
	if (!Backend->Exists(Slot, UserIndex))
	{
		OutMessage = FText::FromString(TEXT("Slot does not exist."));
		return EReadResult::Missing;
	}

	const UGT_MetaSaveGame* Save = Cast<UGT_MetaSaveGame>(Backend->Load(Slot, UserIndex));
	if (!Save)
	{
		OutMessage = FText::FromString(TEXT("Slot could not be deserialized as Graytail meta data."));
		return EReadResult::Corrupt;
	}

	OutState = Save->State;
	FString Error;
	if (!GT_MigrateMetaSave(Save->SaveVersion, OutState, Error))
	{
		OutMessage = ErrorText(Error);
		return Save->SaveVersion > UGT_MetaSaveGame::CurrentSaveVersion
			? EReadResult::UnsupportedVersion
			: EReadResult::Corrupt;
	}
	if (bOutMigrated)
	{
		*bOutMigrated = Save->SaveVersion != UGT_MetaSaveGame::CurrentSaveVersion;
	}
	if (!GT_ValidateAndSanitizeMetaState(OutState, Error))
	{
		OutMessage = ErrorText(Error);
		return EReadResult::Corrupt;
	}
	return EReadResult::Valid;
}

FGT_MetaOperationResult FGT_MetaSaveRepository::WriteState(
	const FString& Slot,
	const FGT_MetaProgressState& State,
	FName ErrorCode)
{
	UGT_MetaSaveGame* Save = NewObject<UGT_MetaSaveGame>();
	if (!Save)
	{
		return FGT_MetaOperationResult::Failure(
			ErrorCode,
			FText::FromString(TEXT("Could not allocate save object.")));
	}
	Save->SaveVersion = UGT_MetaSaveGame::CurrentSaveVersion;
	Save->State = State;
	if (!Backend->Save(*Save, Slot, UserIndex))
	{
		return FGT_MetaOperationResult::Failure(
			ErrorCode,
			FText::FromString(FString::Printf(TEXT("Failed to write slot '%s'."), *Slot)));
	}
	return FGT_MetaOperationResult::Success();
}

void FGT_MetaSaveRepository::WriteDebugMirror(const FGT_MetaProgressState& State) const
{
#if !UE_BUILD_SHIPPING
	TSharedRef<FJsonObject> StateObject = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(
		FGT_MetaProgressState::StaticStruct(),
		&State,
		StateObject,
		0,
		0))
	{
		return;
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("saveVersion"), UGT_MetaSaveGame::CurrentSaveVersion);
	RootObject->SetObjectField(TEXT("state"), StateObject);

	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		return;
	}

	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FFileHelper::SaveStringToFile(
		Json,
		*FPaths::Combine(Directory, TEXT("GraytailMeta.debug.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
#endif
}
