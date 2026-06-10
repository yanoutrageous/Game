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

	FName NormalizeCombatResultArg(const FString& Arg)
	{
		if (Arg.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Combat_DebugResult_Success"));
		}

		if (Arg.Equals(TEXT("Retreat"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Combat_DebugResult_Retreat"));
		}

		if (Arg.Equals(TEXT("Fail"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Fail"));
		}

		return FName(*Arg);
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
			TEXT("%s: RunState=%d PlayerPosition=(%d,%d) Map=%dx%d EventCount=%d RoomBaseType=%d RoomContentId=%s RoomRuleId=%s ContentName=%s RuleName=%s EventOptions=%s CombatResults=%s CombatActive=%s DummyEnemyHp=%d CombatResolved=%s LastCombatResult=%s SummaryAvailable=%s SummaryOutcome=%s SummaryFinalPlayer=(%d,%d) SummaryTotalEventCount=%d RoomTriggered=%s RoomResolved=%s Events={%s}"),
			Prefix,
			static_cast<int32>(Snapshot.RunState),
			Snapshot.PlayerX,
			Snapshot.PlayerY,
			Snapshot.MapWidth,
			Snapshot.MapHeight,
			Snapshot.EventCount,
			static_cast<int32>(Snapshot.CurrentRoomBaseType),
			*Snapshot.CurrentRoomContentId.ToString(),
			*Snapshot.CurrentRoomRuleId.ToString(),
			*Snapshot.CurrentRoomContentDisplayName,
			*Snapshot.CurrentRoomRuleDisplayName,
			Snapshot.CurrentRoomAvailableEventOptions.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableEventOptions,
			Snapshot.CurrentRoomAvailableCombatResults.IsEmpty() ? TEXT("none") : *Snapshot.CurrentRoomAvailableCombatResults,
			Snapshot.bCombatActive ? TEXT("true") : TEXT("false"),
			Snapshot.DummyEnemyHp,
			Snapshot.bCombatResolved ? TEXT("true") : TEXT("false"),
			*Snapshot.LastCombatResultId.ToString(),
			Snapshot.bRunSummaryAvailable ? TEXT("true") : TEXT("false"),
			*Snapshot.RunSummaryOutcome.ToString(),
			Snapshot.RunSummaryFinalPlayerX,
			Snapshot.RunSummaryFinalPlayerY,
			Snapshot.RunSummaryTotalEventCount,
			Snapshot.bCurrentRoomTriggered ? TEXT("true") : TEXT("false"),
			Snapshot.bCurrentRoomResolved ? TEXT("true") : TEXT("false"),
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

		if (Cell.VisibleRoomIcon == FName(TEXT("Event")))
		{
			return TEXT('V');
		}

		if (Cell.VisibleRoomIcon == FName(TEXT("Combat")))
		{
			return TEXT('C');
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

	void LogManualPlayLines(const TArray<FString>& Lines)
	{
		for (const FString& Line : Lines)
		{
			UE_LOG(LogGraytailManualPlay, Display, TEXT("%s"), *Line);
		}
	}

	void HandleHelp(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Help"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Help failed: %s"), *FailureReason);
			return;
		}

		TArray<FString> HelpLines;
		DebugSubsystem->GetDebugCommandHelpLines(HelpLines);
		LogManualPlayLines(HelpLines);
	}

	void HandleCommands(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Commands"));
			return;
		}

		HandleHelp(Args, World);
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

	EGT_Difficulty ParseDifficultyArg(const FString& Arg)
	{
		if (Arg.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Tutorial;
		if (Arg.Equals(TEXT("Easy"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Easy;
		if (Arg.Equals(TEXT("Standard"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Standard;
		if (Arg.Equals(TEXT("Hard"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Hard;
		if (Arg.Equals(TEXT("Veteran"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Veteran;
		if (Arg.Equals(TEXT("Elite"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Elite;
		if (Arg.Equals(TEXT("Nightmare"), ESearchCase::IgnoreCase)) return EGT_Difficulty::Nightmare;
		return EGT_Difficulty::Standard;
	}

	void HandleStartStd(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.StartStd failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() > 2)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.StartStd [Tutorial|Easy|Standard|Hard|Veteran|Elite|Nightmare] [Seed]"));
			return;
		}

		const EGT_Difficulty Difficulty = Args.IsValidIndex(0) ? ParseDifficultyArg(Args[0]) : EGT_Difficulty::Standard;

		int32 Seed = GTDefaultManualSeed;
		if (Args.IsValidIndex(1) && !TryParseIntArg(Args, 1, TEXT("Seed"), Seed))
		{
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bStarted = DebugSubsystem->DebugStartStandardRun(Seed, Difficulty, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.StartStd %s: %s"), bStarted ? TEXT("accepted") : TEXT("failed"), *Snapshot.Summary);
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

	void HandleTeleport(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Teleport failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() != 2)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Teleport X Y"));
			return;
		}

		int32 X = 0;
		int32 Y = 0;
		if (!TryParseIntArg(Args, 0, TEXT("X"), X) || !TryParseIntArg(Args, 1, TEXT("Y"), Y))
		{
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugTeleport(X, Y, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Teleport %d %d %s: %s"), X, Y, bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
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

	void HandleSearch(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Search failed: %s"), *FailureReason);
			return;
		}

		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Search"));
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugSearch(Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Search %s: %s"), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);

		// 搜索成功后顺手打印背包, 省一次 gt.Bag。
		if (bAccepted)
		{
			FString InventoryText;
			DebugSubsystem->GetDebugInventoryText(InventoryText);
			UE_LOG(LogGraytailManualPlay, Display, TEXT("%s"), *InventoryText);
		}
	}

	void HandleBag(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Bag failed: %s"), *FailureReason);
			return;
		}

		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Bag"));
			return;
		}

		FString InventoryText;
		DebugSubsystem->GetDebugInventoryText(InventoryText);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("%s"), *InventoryText);
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

	void HandleChooseEventOption(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.ChooseEventOption failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() > 1)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.ChooseEventOption [OptionId]"));
			return;
		}

		const FName OptionId = Args.IsValidIndex(0) ? FName(*Args[0]) : NAME_None;
		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugChooseEventOption(OptionId, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.ChooseEventOption %s: %s"), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
	}

	void HandleResolveCombat(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.ResolveCombat failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() > 1)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.ResolveCombat [Result]"));
			return;
		}

		const FName ResultId = Args.IsValidIndex(0) ? NormalizeCombatResultArg(Args[0]) : NAME_None;
		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugResolveCombat(ResultId, Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.ResolveCombat %s: %s"), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
	}

	void HandleAttack(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Attack failed: %s"), *FailureReason);
			return;
		}

		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Attack"));
			return;
		}

		FGT_DebugRunSnapshot Snapshot;
		const bool bAccepted = DebugSubsystem->DebugAttack(Snapshot);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Attack %s: %s"), bAccepted ? TEXT("accepted") : TEXT("rejected"), *Snapshot.Summary);
	}

	void HandleSummary(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Summary"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Summary failed: %s"), *FailureReason);
			return;
		}

		FString SummaryText;
		DebugSubsystem->GetDebugRunSummaryText(SummaryText);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("%s"), *SummaryText);
	}

	void HandleStatus(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Status"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Status failed: %s"), *FailureReason);
			return;
		}

		FString StatusText;
		DebugSubsystem->GetDebugStatusText(StatusText);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("%s"), *StatusText);
	}

	void HandleRoom(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.Room"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.Room failed: %s"), *FailureReason);
			return;
		}

		FString RoomText;
		DebugSubsystem->GetDebugRoomText(RoomText);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("%s"), *RoomText);
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

		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.Minimap: size=%dx%d legend P=player K=known ?=unknown 0-9=scanned E/M/V/C=visible exit/mine/event/combat icon if available"), Width, Height);
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
			UE_LOG(
				LogGraytailManualPlay,
				Display,
				TEXT("gt.Events: %s=%d LastSuccess=%s LastSeq=%d LastContentId=%s LastRuleId=%s LastContentName=%s LastRuleName=%s LastDescription=%s LastPayload=%s"),
				*Summary.EventType.ToString(),
				Summary.Count,
				Summary.bLastSuccess ? TEXT("true") : TEXT("false"),
				Summary.LastSequenceId,
				*Summary.LastContentId.ToString(),
				*Summary.LastRuleId.ToString(),
				*Summary.LastContentDisplayName,
				*Summary.LastRuleDisplayName,
				*Summary.LastDebugDescription,
				*Summary.LastPayloadText);
		}
	}

	void HandleRunDemo(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.RunDemo"));
			return;
		}

		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.RunDemo failed: %s"), *FailureReason);
			return;
		}

		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.RunDemo: running a deterministic path through Event (4,1), Combat (1,4), Attack, Exit (9,9), Extract, and Summary."));
		TArray<FString> DemoLogLines;
		FGT_DebugRunSnapshot Snapshot;
		const bool bCompleted = DebugSubsystem->DebugRunDemo(DemoLogLines, Snapshot);
		LogManualPlayLines(DemoLogLines);
		UE_LOG(LogGraytailManualPlay, Display, TEXT("gt.RunDemo %s: %s"), bCompleted ? TEXT("completed") : TEXT("failed"), *Snapshot.Summary);
	}

	void HandleGenMap(const TArray<FString>& Args, UWorld* World)
	{
		FString FailureReason;
		UGT_DebugSubsystem* DebugSubsystem = FindDebugSubsystem(World, FailureReason);
		if (!DebugSubsystem)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("gt.GenMap failed: %s"), *FailureReason);
			return;
		}

		if (Args.Num() != 0 && Args.Num() != 1 && Args.Num() != 3)
		{
			UE_LOG(LogGraytailManualPlay, Warning, TEXT("Usage: gt.GenMap [Seed] [Width Height]"));
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

		TArray<FString> Lines;
		DebugSubsystem->BuildStandardMapPreviewLines(Seed, Width, Height, Lines);
		LogManualPlayLines(Lines);
	}

	FAutoConsoleCommandWithWorldAndArgs GTHelpCommand(
		TEXT("gt.Help"),
		TEXT("Shows Graytail manual play command help. Usage: gt.Help"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleHelp));

	FAutoConsoleCommandWithWorldAndArgs GTCommandsCommand(
		TEXT("gt.Commands"),
		TEXT("Shows Graytail manual play command help. Usage: gt.Commands"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleCommands));

	FAutoConsoleCommandWithWorldAndArgs GTStartRunCommand(
		TEXT("gt.StartRun"),
		TEXT("Starts a Graytail debug run. Usage: gt.StartRun [Seed] [Width Height]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStartRun));

	FAutoConsoleCommandWithWorldAndArgs GTStatusCommand(
		TEXT("gt.Status"),
		TEXT("Logs the current manual play status. Usage: gt.Status"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStatus));

	FAutoConsoleCommandWithWorldAndArgs GTRoomCommand(
		TEXT("gt.Room"),
		TEXT("Logs the current room details and placeholder action hints. Usage: gt.Room"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleRoom));

	FAutoConsoleCommandWithWorldAndArgs GTMoveCommand(
		TEXT("gt.Move"),
		TEXT("Moves the debug player through the existing command path. Usage: gt.Move X Y"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMove));

	FAutoConsoleCommandWithWorldAndArgs GTScanCommand(
		TEXT("gt.Scan"),
		TEXT("Scans a cell through the existing command path. Usage: gt.Scan X Y"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleScan));

	FAutoConsoleCommandWithWorldAndArgs GTTeleportCommand(
		TEXT("gt.Teleport"),
		TEXT("DEBUG godmode teleport, no mine/room triggers. Usage: gt.Teleport X Y"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleTeleport));

	FAutoConsoleCommandWithWorldAndArgs GTSearchCommand(
		TEXT("gt.Search"),
		TEXT("Searches the current room for gold and loot. Usage: gt.Search"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleSearch));

	FAutoConsoleCommandWithWorldAndArgs GTBagCommand(
		TEXT("gt.Bag"),
		TEXT("Shows the current run inventory. Usage: gt.Bag"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleBag));

	FAutoConsoleCommandWithWorldAndArgs GTExtractCommand(
		TEXT("gt.Extract"),
		TEXT("Attempts extraction through the existing command path. Usage: gt.Extract"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleExtract));

	FAutoConsoleCommandWithWorldAndArgs GTChooseEventOptionCommand(
		TEXT("gt.ChooseEventOption"),
		TEXT("Chooses a placeholder event option through the existing command path. Usage: gt.ChooseEventOption [Event_DebugOption_Continue|Event_DebugOption_Scout]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleChooseEventOption));

	FAutoConsoleCommandWithWorldAndArgs GTResolveCombatCommand(
		TEXT("gt.ResolveCombat"),
		TEXT("Resolves placeholder combat through the existing command path. Usage: gt.ResolveCombat [Combat_DebugResult_Success|Combat_DebugResult_Retreat]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleResolveCombat));

	FAutoConsoleCommandWithWorldAndArgs GTAttackCommand(
		TEXT("gt.Attack"),
		TEXT("Attacks active dummy combat through the existing command path. Usage: gt.Attack"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleAttack));

	FAutoConsoleCommandWithWorldAndArgs GTSummaryCommand(
		TEXT("gt.Summary"),
		TEXT("Logs the latest successful extract summary. Usage: gt.Summary"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleSummary));

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

	FAutoConsoleCommandWithWorldAndArgs GTRunDemoCommand(
		TEXT("gt.RunDemo"),
		TEXT("Runs a deterministic Event and Combat placeholder demo path. Usage: gt.RunDemo"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleRunDemo));

	FAutoConsoleCommandWithWorldAndArgs GTGenMapCommand(
		TEXT("gt.GenMap"),
		TEXT("Previews a Standard random map without affecting the active run. Usage: gt.GenMap [Seed] [Width Height]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleGenMap));

	FAutoConsoleCommandWithWorldAndArgs GTStartStdCommand(
		TEXT("gt.StartStd"),
		TEXT("Starts a Standard random run at a difficulty. Usage: gt.StartStd [Tutorial|Easy|Standard|Hard|Veteran|Elite|Nightmare] [Seed]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStartStd));
}
