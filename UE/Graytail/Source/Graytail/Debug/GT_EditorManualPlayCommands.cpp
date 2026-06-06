#include "Debug/GT_DebugSubsystem.h"

#include "Debug/GT_DebugTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailManualPlay, Log, All);

namespace
{
	constexpr int32 GTDefaultManualSeed = 42345;
	constexpr int32 GTDefaultManualMapWidth = 10;
	constexpr int32 GTDefaultManualMapHeight = 10;

	UGT_DebugSubsystem* GetDebugSubsystemFromWorld(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		return GameInstance ? GameInstance->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
	}

	UGT_DebugSubsystem* FindDebugSubsystem(UWorld* World, FString& OutFailureReason)
	{
		if (UGT_DebugSubsystem* DebugSubsystem = GetDebugSubsystemFromWorld(World))
		{
			return DebugSubsystem;
		}

		if (GEngine)
		{
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (!WorldContext.OwningGameInstance)
				{
					continue;
				}

				if (UGT_DebugSubsystem* DebugSubsystem = WorldContext.OwningGameInstance->GetSubsystem<UGT_DebugSubsystem>())
				{
					return DebugSubsystem;
				}
			}
		}

		OutFailureReason = TEXT("No GameInstance with UGT_DebugSubsystem was found. Start PIE or run the command in an active game world.");
		return nullptr;
	}

	bool TryParseIntArg(const TArray<FString>& Args, int32 Index, const TCHAR* ArgName, int32& OutValue)
	{
		if (!Args.IsValidIndex(Index) || !LexTryParseString(OutValue, *Args[Index]))
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Invalid %s argument. Value='%s'."), ArgName, Args.IsValidIndex(Index) ? *Args[Index] : TEXT("<missing>"));
			return false;
		}

		return true;
	}

	FString BuildEventSummaryText(const TArray<FGT_DebugEventSummary>& EventSummary)
	{
		if (EventSummary.IsEmpty())
		{
			return TEXT("none");
		}

		TArray<FString> Parts;
		Parts.Reserve(EventSummary.Num());
		for (const FGT_DebugEventSummary& Summary : EventSummary)
		{
			Parts.Add(FString::Printf(TEXT("%s=%d"), *Summary.EventType.ToString(), Summary.Count));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	void LogSnapshot(UGT_DebugSubsystem* DebugSubsystem, const TCHAR* Prefix)
	{
		FGT_DebugRunSnapshot Snapshot;
		if (!DebugSubsystem->GetDebugRunSnapshot(Snapshot))
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("%s failed: %s"), Prefix, *Snapshot.Summary);
			return;
		}

		TArray<FGT_DebugEventSummary> EventSummary;
		DebugSubsystem->GetDebugEventSummary(EventSummary);
		UE_LOG(
			LogGraytailManualPlay,
			Display,
			TEXT("%s: RunState=%d PlayerPosition=(%d,%d) Map=%dx%d EventCount=%d Events={%s}"),
			Prefix,
			static_cast<int32>(Snapshot.RunState),
			Snapshot.PlayerX,
			Snapshot.PlayerY,
			Snapshot.MapWidth,
			Snapshot.MapHeight,
			Snapshot.EventCount,
			*BuildEventSummaryText(EventSummary));
	}

	TCHAR GetMiniMapSymbol(const FGT_MiniMapCellViewData& Cell, int32 PlayerX, int32 PlayerY)
	{
		if (Cell.X == PlayerX && Cell.Y == PlayerY)
		{
			return TEXT('P');
		}

		if (Cell.VisibleRoomIcon == FName(TEXT("Exit")))
		{
			return TEXT('E');
		}

		if (Cell.VisibleRoomIcon == FName(TEXT("Mine")))
		{
			return TEXT('M');
		}

		if (Cell.bScanned)
		{
			return static_cast<TCHAR>('0' + FMath::Clamp(Cell.DisplayedNumber, 0, 9));
		}

		if (Cell.bExplored || Cell.bVisible)
		{
			return TEXT('K');
		}

		return TEXT('?');
	}

	bool FindMiniMapCell(const TArray<FGT_MiniMapCellViewData>& Cells, int32 X, int32 Y, FGT_MiniMapCellViewData& OutCell)
	{
		const FGT_MiniMapCellViewData* Cell = Cells.FindByPredicate([X, Y](const FGT_MiniMapCellViewData& Candidate)
		{
			return Candidate.X == X && Candidate.Y == Y;
		});

		if (!Cell)
		{
			return false;
		}

		OutCell = *Cell;
		return true;
	}

	void HandleStartRun(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.StartRun failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() != 0 && Args.Num() != 1 && Args.Num() != 3)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.StartRun [Seed] [Width Height]"));
			return;
		}

		int32 Seed = GTDefaultManualSeed;
		int32 Width = GTDefaultManualMapWidth;
		int32 Height = GTDefaultManualMapHeight;

		if (Args.Num() >= 1 && !TryParseIntArg(Args, 0, TEXT("Seed"), Seed))
		{
			return;
		}

		if (Args.Num() == 3
			&& (!TryParseIntArg(Args, 1, TEXT("Width"), Width) || !TryParseIntArg(Args, 2, TEXT("Height"), Height)))
		{
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bStarted = DebugSubsystem->DebugStartNewRun(Seed, Width, Height, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.StartRun %s: %s"), bStarted ? TEXT("accepted") : TEXT("failed"), *Snapshot.Summary);
	}

	void HandleMove(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Move failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() != 2)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Move X Y"));
			return;
		}

		int32 X = 0;
		int32 Y = 0;
		if (!TryParseIntArg(Args, 0, TEXT("X"), X) || !TryParseIntArg(Args, 1, TEXT("Y"), Y))
		{
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugMoveTo(X, Y, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Move %d %d %s: %s"), X, Y, bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
	}

	void HandleScan(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Scan failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() != 2)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Scan X Y"));
			return;
		}

		int32 X = 0;
		int32 Y = 0;
		if (!TryParseIntArg(Args, 0, TEXT("X"), X) || !TryParseIntArg(Args, 1, TEXT("Y"), Y))
		{
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugScanCell(X, Y, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Scan %d %d %s: %s"), X, Y, bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
	}

	void HandleExtract(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Extract failed: %s"), *FailureReason);
			return;
		}

		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Extract"));
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugExtract(Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Extract %s: %s"), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
	}

	void HandleSnapshot(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Snapshot"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Snapshot failed: %s"), *FailureReason);
			return;
		}

		LogSnapshot(DebugSubsystem, TEXT("gt.Snapshot"));
	}

	void HandleMiniMap(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Minimap"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Minimap failed: %s"), *FailureReason);
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		if (!DebugSubsystem->GetDebugRunSnapshot(Snapshot))
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Minimap failed: %s"), *Snapshot.Summary);
			return;
		}

		TArray<FGT_MiniMapCellViewData> Cells;
		int32 Width = 0;
		int32 Height = 0;
		if (!DebugSubsystem->GetDebugMiniMapViewData(Cells, Width, Height))
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Minimap failed: no minimap view data is available."));
			return;
		}

		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Minimap: size=%dx%d legend P=player K=known ?=unknown 0-9=scanned E/M=visible room icon if available"), Width, Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			FString Row;
			Row.Reserve(Width);
			for (int32 X = 0; X < Width; ++X)
			{
				FGT_MiniMapCellViewData Cell;
				Row.AppendChar(FindMiniMapCell(Cells, X, Y, Cell) ? GetMiniMapSymbol(Cell, Snapshot.PlayerX, Snapshot.PlayerY) : TEXT('!'));
			}

			UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Minimap row %02d: %s"), Y, *Row);
		}
	}

	void HandleEvents(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Events"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Events failed: %s"), *FailureReason);
			return;
		}

		TArray<FGT_DebugEventSummary> EventSummary;
		DebugSubsystem->GetDebugEventSummary(EventSummary);
		if (EventSummary.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Events: no events recorded."));
			return;
		}

		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Events: %d event type(s)."), EventSummary.Num());
		for (const FGT_DebugEventSummary& Summary : EventSummary)
		{
			UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Events: %s=%d"), *Summary.EventType.ToString(), Summary.Count);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GTStartRunCommand(
		TEXT("gt.StartRun"),
		TEXT("Starts a Graytail debug run. Usage: gt.StartRun [Seed] [Width Height]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStartRun));

	FAutoConsoleCommandWithWorldAndArgs GTMoveCommand(
		TEXT("gt.Move"),
		TEXT("Moves the debug player through the existing command path. Usage: gt.Move X Y"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMove));

	FAutoConsoleCommandWithWorldAndArgs GTScanCommand(
		TEXT("gt.Scan"),
		TEXT("Scans a cell through the existing command path. Usage: gt.Scan X Y"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleScan));

	FAutoConsoleCommandWithWorldAndArgs GTExtractCommand(
		TEXT("gt.Extract"),
		TEXT("Attempts extraction through the existing command path. Usage: gt.Extract"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleExtract));

	FAutoConsoleCommandWithWorldAndArgs GTSnapshotCommand(
		TEXT("gt.Snapshot"),
		TEXT("Logs the current debug run snapshot. Usage: gt.Snapshot"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleSnapshot));

	FAutoConsoleCommandWithWorldAndArgs GTMiniMapCommand(
		TEXT("gt.Minimap"),
		TEXT("Logs a text minimap from the debug minimap view data. Usage: gt.Minimap"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMiniMap));

	FAutoConsoleCommandWithWorldAndArgs GTEventsCommand(
		TEXT("gt.Events"),
		TEXT("Logs debug event type counts. Usage: gt.Events"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleEvents));
}
