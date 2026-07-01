#include "Debug/GT_RuntimeSmokeValidator.h"

#include "App/GT_GameMode.h"
#include "App/GT_PlayerController.h"
#include "Core/GT_CommandBus.h"
#include "Core/GT_CommandProcessor.h"
#include "Core/GT_ContentRegistry.h"
#include "Core/GT_EventBus.h"
#include "Core/GT_QueryFacade.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Debug/GT_DebugTypes.h"
#include "Debug/GT_MetaPersistenceSmokeValidator.h"
#include "Data/GT_GameDataSubsystem.h"
#include "Dom/JsonObject.h"
#include "Domains/Combat/GT_MonsterCatalog.h"
#include "Domains/Events/GT_EventRules.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Inventory/GT_LootRules.h"
#include "Domains/Meta/GT_MetaCatalog.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaSettlement.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Save/GT_MetaSaveGame.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectHash.h"

namespace
{
	const FName GTCheck_DefaultGameModeUsesGraytailController(TEXT("DefaultGameModeUsesGraytailController"));
	const FName GTCheck_LocalControllerCreatesHudOnce(TEXT("LocalControllerCreatesHudOnce"));
	const FName GTCheck_ExistingHudIsNotDuplicated(TEXT("ExistingHudIsNotDuplicated"));
	const FName GTCheck_RemoteControllerDoesNotCreateHud(TEXT("RemoteControllerDoesNotCreateHud"));
	const FName GTCheck_RunSubsystemValid(TEXT("RunSubsystemValid"));
	const FName GTCheck_PlayerExists(TEXT("PlayerExists"));
	const FName GTCheck_InitialPlayerPosition(TEXT("InitialPlayerPosition"));
	const FName GTCheck_InitialIntelCell(TEXT("InitialIntelCell"));
	const FName GTCheck_LegalMoveAccepted(TEXT("LegalMoveAccepted"));
	const FName GTCheck_MovedPlayerPosition(TEXT("MovedPlayerPosition"));
	const FName GTCheck_MovedIntelCell(TEXT("MovedIntelCell"));
	const FName GTCheck_RoomNotResolvedBeforeMove(TEXT("RoomNotResolvedBeforeMove"));
	const FName GTCheck_MoveResolvesTargetRoom(TEXT("MoveResolvesTargetRoom"));
	const FName GTCheck_MoveTriggersTargetRoom(TEXT("MoveTriggersTargetRoom"));
	const FName GTCheck_RoomEnteredEvent(TEXT("RoomEnteredEvent"));
	const FName GTCheck_RoomResolvedEvent(TEXT("RoomResolvedEvent"));
	const FName GTCheck_InvalidMoveDoesNotResolveRoom(TEXT("InvalidMoveDoesNotResolveRoom"));
	const FName GTCheck_OutOfBoundsMoveRejected(TEXT("OutOfBoundsMoveRejected"));
	const FName GTCheck_RejectedMovePreservesPosition(TEXT("RejectedMovePreservesPosition"));
	const FName GTCheck_EventsRecorded(TEXT("EventsRecorded"));
	const FName GTCheck_QueryFacadePlayerPosition(TEXT("QueryFacadePlayerPosition"));
	const FName GTCheck_TruthMapSize(TEXT("TruthMapSize"));
	const FName GTCheck_TruthMapCellCount(TEXT("TruthMapCellCount"));
	const FName GTCheck_TruthCellOrigin(TEXT("TruthCellOrigin"));
	const FName GTCheck_TruthCellCorner(TEXT("TruthCellCorner"));
	const FName GTCheck_Neighbors4Corner(TEXT("Neighbors4Corner"));
	const FName GTCheck_Neighbors4Center(TEXT("Neighbors4Center"));
	const FName GTCheck_Neighbors8Corner(TEXT("Neighbors8Corner"));
	const FName GTCheck_Neighbors8Center(TEXT("Neighbors8Center"));
	const FName GTCheck_ExitCellDebug(TEXT("ExitCellDebug"));
	const FName GTCheck_MineCellDebug(TEXT("MineCellDebug"));
	const FName GTCheck_EventPlaceholderCellDebug(TEXT("EventPlaceholderCellDebug"));
	const FName GTCheck_CombatPlaceholderCellDebug(TEXT("CombatPlaceholderCellDebug"));
	const FName GTCheck_AdjacentMineCountNearMine(TEXT("AdjacentMineCountNearMine"));
	const FName GTCheck_AdjacentMineCountFarFromMine(TEXT("AdjacentMineCountFarFromMine"));
	const FName GTCheck_AdjacentMineCountMineCellSelfExcluded(TEXT("AdjacentMineCountMineCellSelfExcluded"));
	const FName GTCheck_AdjacentMineCountInvalidCoord(TEXT("AdjacentMineCountInvalidCoord"));
	const FName GTCheck_ScanCommandAccepted(TEXT("ScanCommandAccepted"));
	const FName GTCheck_ScannedIntelCellMarked(TEXT("ScannedIntelCellMarked"));
	const FName GTCheck_ScannedDisplayedNumber(TEXT("ScannedDisplayedNumber"));
	const FName GTCheck_CellScannedEvent(TEXT("CellScannedEvent"));
	const FName GTCheck_ScanDoesNotResolveRoom(TEXT("ScanDoesNotResolveRoom"));
	const FName GTCheck_InvalidScanRejected(TEXT("InvalidScanRejected"));
	const FName GTCheck_InvalidScanDoesNotWriteIntel(TEXT("InvalidScanDoesNotWriteIntel"));
	const FName GTCheck_InvalidScanCommandFailedEvent(TEXT("InvalidScanCommandFailedEvent"));
	const FName GTCheck_MiniMapViewModelBuild(TEXT("MiniMapViewModelBuild"));
	const FName GTCheck_MiniMapViewModelSize(TEXT("MiniMapViewModelSize"));
	const FName GTCheck_MiniMapViewModelScannedCell(TEXT("MiniMapViewModelScannedCell"));
	const FName GTCheck_MiniMapViewModelDisplayedNumber(TEXT("MiniMapViewModelDisplayedNumber"));
	const FName GTCheck_MiniMapViewModelCellVisibleExplored(TEXT("MiniMapViewModelCellVisibleExplored"));
	const FName GTCheck_MiniMapViewModelReliability(TEXT("MiniMapViewModelReliability"));
	const FName GTCheck_QueryFacadeReusesMiniMapViewModel(TEXT("QueryFacadeReusesMiniMapViewModel"));
	const FName GTCheck_NormalRoomResolveOutcome(TEXT("NormalRoomResolveOutcome"));
	const FName GTCheck_NormalRoomEvents(TEXT("NormalRoomEvents"));
	const FName GTCheck_MineRoomResolveOutcome(TEXT("MineRoomResolveOutcome"));
	const FName GTCheck_MineEncounteredEvent(TEXT("MineEncounteredEvent"));
	const FName GTCheck_MineDoesNotFailRunYet(TEXT("MineDoesNotFailRunYet"));
	const FName GTCheck_ExitRoomResolveOutcome(TEXT("ExitRoomResolveOutcome"));
	const FName GTCheck_ExitFoundEvent(TEXT("ExitFoundEvent"));
	const FName GTCheck_ExitDoesNotWinRunYet(TEXT("ExitDoesNotWinRunYet"));
	const FName GTCheck_ExtractRejectedAwayFromExit(TEXT("ExtractRejectedAwayFromExit"));
	const FName GTCheck_ExtractAwayFromExitCommandFailed(TEXT("ExtractAwayFromExitCommandFailed"));
	const FName GTCheck_ExtractAwayFromExitNoRunSummary(TEXT("ExtractAwayFromExitNoRunSummary"));
	const FName GTCheck_MoveToExitAccepted(TEXT("MoveToExitAccepted"));
	const FName GTCheck_ExitFoundBeforeExtract(TEXT("ExitFoundBeforeExtract"));
	const FName GTCheck_RunStillActiveAtExitBeforeExtract(TEXT("RunStillActiveAtExitBeforeExtract"));
	const FName GTCheck_ExtractAcceptedAtExit(TEXT("ExtractAcceptedAtExit"));
	const FName GTCheck_RunSucceededAfterExtract(TEXT("RunSucceededAfterExtract"));
	const FName GTCheck_RunSucceededEvent(TEXT("RunSucceededEvent"));
	const FName GTCheck_RunSummaryAvailableAfterExtract(TEXT("RunSummaryAvailableAfterExtract"));
	const FName GTCheck_RunSummaryOutcomeAfterExtract(TEXT("RunSummaryOutcomeAfterExtract"));
	const FName GTCheck_RunSummaryFinalPositionAfterExtract(TEXT("RunSummaryFinalPositionAfterExtract"));
	const FName GTCheck_RunSummaryEventCountAfterExtract(TEXT("RunSummaryEventCountAfterExtract"));
	const FName GTCheck_RunSummaryPreservedAfterRejectedExtract(TEXT("RunSummaryPreservedAfterRejectedExtract"));
	const FName GTCheck_RunSummaryClearedAfterStart(TEXT("RunSummaryClearedAfterStart"));
	const FName GTCheck_MoveRejectedAfterRunSucceeded(TEXT("MoveRejectedAfterRunSucceeded"));
	const FName GTCheck_ScanRejectedAfterRunSucceeded(TEXT("ScanRejectedAfterRunSucceeded"));
	const FName GTCheck_ExtractRejectedAfterRunSucceeded(TEXT("ExtractRejectedAfterRunSucceeded"));
	const FName GTCheck_ScanDoesNotTriggerRoomResolver(TEXT("ScanDoesNotTriggerRoomResolver"));
	const FName GTCheck_RunStateAfterStart(TEXT("RunStateAfterStart"));
	const FName GTCheck_MineMoveAccepted(TEXT("MineMoveAccepted"));
	const FName GTCheck_MineEncounteredBeforeFail(TEXT("MineEncounteredBeforeFail"));
	const FName GTCheck_RunFailedAfterMine(TEXT("RunFailedAfterMine"));
	const FName GTCheck_RunFailedEvent(TEXT("RunFailedEvent"));
	const FName GTCheck_MoveRejectedAfterRunFailed(TEXT("MoveRejectedAfterRunFailed"));
	const FName GTCheck_ScanRejectedAfterRunFailed(TEXT("ScanRejectedAfterRunFailed"));
	const FName GTCheck_PositionPreservedAfterFailedMove(TEXT("PositionPreservedAfterFailedMove"));
	const FName GTCheck_IntelPreservedAfterFailedScan(TEXT("IntelPreservedAfterFailedScan"));
	const FName GTCheck_ScenarioFailureStartRun(TEXT("ScenarioFailureStartRun"));
	const FName GTCheck_ScenarioFailureInitialPosition(TEXT("ScenarioFailureInitialPosition"));
	const FName GTCheck_ScenarioFailureScanBeforeMine(TEXT("ScenarioFailureScanBeforeMine"));
	const FName GTCheck_ScenarioFailureMoveToMinePath(TEXT("ScenarioFailureMoveToMinePath"));
	const FName GTCheck_ScenarioFailureMineEncountered(TEXT("ScenarioFailureMineEncountered"));
	const FName GTCheck_ScenarioFailureRunFailed(TEXT("ScenarioFailureRunFailed"));
	const FName GTCheck_ScenarioFailureRunFailedEvent(TEXT("ScenarioFailureRunFailedEvent"));
	const FName GTCheck_ScenarioFailurePostFailMoveRejected(TEXT("ScenarioFailurePostFailMoveRejected"));
	const FName GTCheck_ScenarioFailurePostFailScanRejected(TEXT("ScenarioFailurePostFailScanRejected"));
	const FName GTCheck_ScenarioFailurePostFailExtractRejected(TEXT("ScenarioFailurePostFailExtractRejected"));
	const FName GTCheck_ScenarioSuccessStartRun(TEXT("ScenarioSuccessStartRun"));
	const FName GTCheck_ScenarioSuccessInitialPosition(TEXT("ScenarioSuccessInitialPosition"));
	const FName GTCheck_ScenarioSuccessScanBeforeExit(TEXT("ScenarioSuccessScanBeforeExit"));
	const FName GTCheck_ScenarioSuccessMoveToExitPath(TEXT("ScenarioSuccessMoveToExitPath"));
	const FName GTCheck_ScenarioSuccessExitFound(TEXT("ScenarioSuccessExitFound"));
	const FName GTCheck_ScenarioSuccessStillRunningAtExit(TEXT("ScenarioSuccessStillRunningAtExit"));
	const FName GTCheck_ScenarioSuccessExtractAccepted(TEXT("ScenarioSuccessExtractAccepted"));
	const FName GTCheck_ScenarioSuccessRunSucceeded(TEXT("ScenarioSuccessRunSucceeded"));
	const FName GTCheck_ScenarioSuccessRunSucceededEvent(TEXT("ScenarioSuccessRunSucceededEvent"));
	const FName GTCheck_ScenarioSuccessPostSuccessMoveRejected(TEXT("ScenarioSuccessPostSuccessMoveRejected"));
	const FName GTCheck_ScenarioSuccessPostSuccessScanRejected(TEXT("ScenarioSuccessPostSuccessScanRejected"));
	const FName GTCheck_ScenarioSuccessPostSuccessExtractRejected(TEXT("ScenarioSuccessPostSuccessExtractRejected"));
	const FName GTCheck_DebugStartNewRunAccepted(TEXT("DebugStartNewRunAccepted"));
	const FName GTCheck_DebugSummaryNoRunSafe(TEXT("DebugSummaryNoRunSafe"));
	const FName GTCheck_DebugSnapshotAfterStart(TEXT("DebugSnapshotAfterStart"));
	const FName GTCheck_DebugSummaryAfterStartUnavailable(TEXT("DebugSummaryAfterStartUnavailable"));
	const FName GTCheck_DebugMoveAccepted(TEXT("DebugMoveAccepted"));
	const FName GTCheck_DebugSnapshotAfterMove(TEXT("DebugSnapshotAfterMove"));
	const FName GTCheck_DebugScanAccepted(TEXT("DebugScanAccepted"));
	const FName GTCheck_DebugMiniMapAfterScan(TEXT("DebugMiniMapAfterScan"));
	const FName GTCheck_DebugExtractRejectedAwayFromExit(TEXT("DebugExtractRejectedAwayFromExit"));
	const FName GTCheck_DebugMoveToExitPathAccepted(TEXT("DebugMoveToExitPathAccepted"));
	const FName GTCheck_DebugExtractAcceptedAtExit(TEXT("DebugExtractAcceptedAtExit"));
	const FName GTCheck_DebugSnapshotAfterExtract(TEXT("DebugSnapshotAfterExtract"));
	const FName GTCheck_DebugSummaryAfterExtract(TEXT("DebugSummaryAfterExtract"));
	const FName GTCheck_DebugStatusShowsSummaryAfterExtract(TEXT("DebugStatusShowsSummaryAfterExtract"));
	const FName GTCheck_DebugMoveRejectedAfterSuccess(TEXT("DebugMoveRejectedAfterSuccess"));
	const FName GTCheck_DebugEventSummaryAvailable(TEXT("DebugEventSummaryAvailable"));
	const FName GTCheck_DebugEventPlaceholderMoveAccepted(TEXT("DebugEventPlaceholderMoveAccepted"));
	const FName GTCheck_DebugEventPlaceholderEvents(TEXT("DebugEventPlaceholderEvents"));
	const FName GTCheck_DebugCombatPlaceholderMoveAccepted(TEXT("DebugCombatPlaceholderMoveAccepted"));
	const FName GTCheck_DebugCombatPlaceholderEvents(TEXT("DebugCombatPlaceholderEvents"));
	const FName GTCheck_DebugCombatStateStarted(TEXT("DebugCombatStateStarted"));
	const FName GTCheck_DebugCombatStateInitialHp(TEXT("DebugCombatStateInitialHp"));
	const FName GTCheck_DebugAttackAccepted(TEXT("DebugAttackAccepted"));
	const FName GTCheck_DebugAttackReducesDummyHp(TEXT("DebugAttackReducesDummyHp"));
	const FName GTCheck_DebugAttackCombatResolvedEvent(TEXT("DebugAttackCombatResolvedEvent"));
	const FName GTCheck_DebugChooseEventOptionAccepted(TEXT("DebugChooseEventOptionAccepted"));
	const FName GTCheck_DebugChooseEventOptionEvents(TEXT("DebugChooseEventOptionEvents"));
	const FName GTCheck_DebugResolveCombatAccepted(TEXT("DebugResolveCombatAccepted"));
	const FName GTCheck_DebugResolveCombatEvents(TEXT("DebugResolveCombatEvents"));
	const FName GTCheck_DebugChooseEventOptionRejectedOutsideEvent(TEXT("DebugChooseEventOptionRejectedOutsideEvent"));
	const FName GTCheck_DebugChooseEventOptionFailureEvent(TEXT("DebugChooseEventOptionFailureEvent"));
	const FName GTCheck_DebugResolveCombatRejectedOutsideCombat(TEXT("DebugResolveCombatRejectedOutsideCombat"));
	const FName GTCheck_DebugResolveCombatFailureEvent(TEXT("DebugResolveCombatFailureEvent"));
	const FName GTCheck_DebugAttackRejectedOutsideCombat(TEXT("DebugAttackRejectedOutsideCombat"));
	const FName GTCheck_DebugAttackFailureEvent(TEXT("DebugAttackFailureEvent"));
	const FName GTCheck_DebugManualPlayHelpAvailable(TEXT("DebugManualPlayHelpAvailable"));
	const FName GTCheck_DebugManualPlayStatusNoRunSafe(TEXT("DebugManualPlayStatusNoRunSafe"));
	const FName GTCheck_DebugManualPlayStatusAfterStart(TEXT("DebugManualPlayStatusAfterStart"));
	const FName GTCheck_DebugManualPlayRoomAfterStart(TEXT("DebugManualPlayRoomAfterStart"));
	const FName GTCheck_DebugManualPlayRunDemoCompleted(TEXT("DebugManualPlayRunDemoCompleted"));
	const FName GTCheck_DebugManualPlayRunDemoEvents(TEXT("DebugManualPlayRunDemoEvents"));
	const FName GTCheck_DebugManualPlayRunDemoSummary(TEXT("DebugManualPlayRunDemoSummary"));
	const FName GTCheck_RoomRegistryEventContent(TEXT("RoomRegistryEventContent"));
	const FName GTCheck_RoomRegistryEventRule(TEXT("RoomRegistryEventRule"));
	const FName GTCheck_RoomRegistryCombatContent(TEXT("RoomRegistryCombatContent"));
	const FName GTCheck_RoomRegistryCombatRule(TEXT("RoomRegistryCombatRule"));
	const FName GTCheck_EventOptionRegistryContinue(TEXT("EventOptionRegistryContinue"));
	const FName GTCheck_EventOptionRegistryScout(TEXT("EventOptionRegistryScout"));
	const FName GTCheck_CombatResultRegistrySuccess(TEXT("CombatResultRegistrySuccess"));
	const FName GTCheck_CombatResultRegistryRetreat(TEXT("CombatResultRegistryRetreat"));
	const FName GTCheck_DebugEventRoomUsesRegistryDefinition(TEXT("DebugEventRoomUsesRegistryDefinition"));
	const FName GTCheck_DebugCombatRoomUsesRegistryDefinition(TEXT("DebugCombatRoomUsesRegistryDefinition"));
	const FName GTCheck_DebugChooseEventOptionDefaultAccepted(TEXT("DebugChooseEventOptionDefaultAccepted"));
	const FName GTCheck_DebugChooseEventOptionScoutAccepted(TEXT("DebugChooseEventOptionScoutAccepted"));
	const FName GTCheck_DebugChooseEventOptionInvalidRejected(TEXT("DebugChooseEventOptionInvalidRejected"));
	const FName GTCheck_DebugResolveCombatDefaultAccepted(TEXT("DebugResolveCombatDefaultAccepted"));
	const FName GTCheck_DebugResolveCombatRetreatAccepted(TEXT("DebugResolveCombatRetreatAccepted"));
	const FName GTCheck_DebugResolveCombatInvalidRejected(TEXT("DebugResolveCombatInvalidRejected"));
	const FName GTCheck_NewRunStartsWithFreshEventHistory(TEXT("NewRunStartsWithFreshEventHistory"));
	const FName GTCheck_ResolvedCombatRoomDoesNotRestart(TEXT("ResolvedCombatRoomDoesNotRestart"));
	const FName GTCheck_ProtocolDrainStopsRoomResolution(TEXT("ProtocolDrainStopsRoomResolution"));
	const FName GTCheck_AbandonRunClearsRuntimeState(TEXT("AbandonRunClearsRuntimeState"));
	const FName GTCheck_GameDataDefaultLoads(TEXT("GameDataDefaultLoads"));
	const FName GTCheck_GameDataMissingDirectoryRejected(TEXT("GameDataMissingDirectoryRejected"));
	const FName GTCheck_GameDataMalformedJsonRejected(TEXT("GameDataMalformedJsonRejected"));
	const FName GTCheck_GameDataMisspelledFieldRejected(TEXT("GameDataMisspelledFieldRejected"));
	const FName GTCheck_GameDataWrongScalarTypeRejected(TEXT("GameDataWrongScalarTypeRejected"));
	const FName GTCheck_GameDataUnknownFieldRejected(TEXT("GameDataUnknownFieldRejected"));
	const FName GTCheck_GameDataVersionRejected(TEXT("GameDataVersionRejected"));
	const FName GTCheck_GameDataDuplicateIdRejected(TEXT("GameDataDuplicateIdRejected"));
	const FName GTCheck_GameDataNegativeValueRejected(TEXT("GameDataNegativeValueRejected"));
	const FName GTCheck_GameDataProbabilityRejected(TEXT("GameDataProbabilityRejected"));
	const FName GTCheck_GameDataInvalidReferenceRejected(TEXT("GameDataInvalidReferenceRejected"));
	const FName GTCheck_GameDataMissingRequiredIdRejected(TEXT("GameDataMissingRequiredIdRejected"));
	const FName GTCheck_GameDataUnknownEventRejected(TEXT("GameDataUnknownEventRejected"));
	const FName GTCheck_GameDataUnknownTriggerRejected(TEXT("GameDataUnknownTriggerRejected"));
	const FName GTCheck_GameDataProtocolOrderRejected(TEXT("GameDataProtocolOrderRejected"));
	const FName GTCheck_GameDataInvalidManualLayoutRejected(TEXT("GameDataInvalidManualLayoutRejected"));
	const FName GTCheck_GameDataNoRandomExitRejected(TEXT("GameDataNoRandomExitRejected"));
	const FName GTCheck_GameDataAssetlessItemRejected(TEXT("GameDataAssetlessItemRejected"));
	const FName GTCheck_GameDataMissingQualityPoolRejected(TEXT("GameDataMissingQualityPoolRejected"));
	const FName GTCheck_GameDataNegativeMetaEffectRejected(TEXT("GameDataNegativeMetaEffectRejected"));
	const FName GTCheck_GameDataMineFloorRejected(TEXT("GameDataMineFloorRejected"));
	const FName GTCheck_GameDataSlimelingSpawnRejected(TEXT("GameDataSlimelingSpawnRejected"));
	const FName GTCheck_GameDataMissingPersistentMetaIdRejected(TEXT("GameDataMissingPersistentMetaIdRejected"));
	const FName GTCheck_GameDataExternalValueReloaded(TEXT("GameDataExternalValueReloaded"));
	const FName GTCheck_GameDataLootFacadeReloaded(TEXT("GameDataLootFacadeReloaded"));
	const FName GTCheck_GameDataEventFacadeReloaded(TEXT("GameDataEventFacadeReloaded"));
	const FName GTCheck_GameDataMetaFacadeReloaded(TEXT("GameDataMetaFacadeReloaded"));
	const FName GTCheck_GameDataItemFacadeReloaded(TEXT("GameDataItemFacadeReloaded"));
	const FName GTCheck_GameDataMonsterFacadeReloaded(TEXT("GameDataMonsterFacadeReloaded"));
	const FName GTCheck_GameDataInvalidReloadKeepsSnapshot(TEXT("GameDataInvalidReloadKeepsSnapshot"));
	const FName GTCheck_LuckyCoinPreservesLegacySeed(TEXT("LuckyCoinPreservesLegacySeed"));
	const FName GTCheck_ConfiguredSettleGoldBonusApplied(TEXT("ConfiguredSettleGoldBonusApplied"));
	const FName GTCheck_ConfiguredChestBonusApplied(TEXT("ConfiguredChestBonusApplied"));
	const FName GTCheck_SmokeUsesIsolatedSaveSlot(TEXT("SmokeUsesIsolatedSaveSlot"));
	const FName GTCheck_MetaSaveRemovesOrphanedEquip(TEXT("MetaSaveRemovesOrphanedEquip"));
	const FName GTCheck_MetaSaveDebugMirrorWritten(TEXT("MetaSaveDebugMirrorWritten"));
	const FName GTCheck_MetaSaveDebugMirrorMatches(TEXT("MetaSaveDebugMirrorMatches"));
	const FName GTCheck_MetaSaveIgnoresDebugMirror(TEXT("MetaSaveIgnoresDebugMirror"));

	const TCHAR* GTGameDataFileNames[] = {
		TEXT("core.json"),
		TEXT("difficulties.json"),
		TEXT("monsters.json"),
		TEXT("items.json"),
		TEXT("loot_events.json"),
		TEXT("meta_catalog.json")
	};

	FString MakeGameDataTestDirectory(const FString& Tag)
	{
		const FString Directory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("GameDataSmoke"),
			Tag + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*Directory, true);
		for (const TCHAR* FileName : GTGameDataFileNames)
		{
			IFileManager::Get().Copy(
				*FPaths::Combine(Directory, FileName),
				*FPaths::Combine(UGT_GameDataSubsystem::GetDefaultDataDirectory(), FileName));
		}
		return Directory;
	}

	bool ReplaceGameDataText(
		const FString& Directory,
		const TCHAR* FileName,
		const FString& From,
		const FString& To)
	{
		const FString Path = FPaths::Combine(Directory, FileName);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !Text.Contains(From))
		{
			return false;
		}
		Text.ReplaceInline(*From, *To, ESearchCase::CaseSensitive);
		return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	const FName GTCommandType_Move(TEXT("Move"));
	const FName GTCommandType_Scan(TEXT("Scan"));
	const FName GTCommandType_Extract(TEXT("Extract"));
	const FName GTEventType_ActorMoved(TEXT("ActorMoved"));
	const FName GTEventType_RoomEntered(TEXT("RoomEntered"));
	const FName GTEventType_RoomResolved(TEXT("RoomResolved"));
	const FName GTEventType_MineEncountered(TEXT("MineEncountered"));
	const FName GTEventType_ExitFound(TEXT("ExitFound"));
	const FName GTEventType_RunFailed(TEXT("RunFailed"));
	const FName GTEventType_RunSucceeded(TEXT("RunSucceeded"));
	const FName GTEventType_CellScanned(TEXT("CellScanned"));
	const FName GTEventType_CommandFailed(TEXT("CommandFailed"));
	const FName GTEventType_EventRoomEntered(TEXT("EventRoomEntered"));
	const FName GTEventType_EventPresented(TEXT("EventPresented"));
	const FName GTEventType_CombatRoomEntered(TEXT("CombatRoomEntered"));
	const FName GTEventType_CombatStarted(TEXT("CombatStarted"));
	const FName GTEventType_CombatAttack(TEXT("CombatAttack"));
	const FName GTEventType_CombatAttackFailed(TEXT("CombatAttackFailed"));
	const FName GTEventType_EventOptionChosen(TEXT("EventOptionChosen"));
	const FName GTEventType_EventResolved(TEXT("EventResolved"));
	const FName GTEventType_EventOptionChooseFailed(TEXT("EventOptionChooseFailed"));
	const FName GTEventType_CombatResolved(TEXT("CombatResolved"));
	const FName GTEventType_CombatRetreated(TEXT("CombatRetreated"));
	const FName GTEventType_CombatResolveFailed(TEXT("CombatResolveFailed"));
	const FName GTRoomContent_EventDebugChoice01(TEXT("Event_DebugChoice_01"));
	const FName GTRoomRule_EventPresentOnly(TEXT("Event_PresentOnly"));
	const FName GTRoomContent_CombatDebugDummy01(TEXT("Combat_DebugDummy_01"));
	const FName GTRoomRule_CombatStartOnly(TEXT("Combat_StartOnly"));
	const FString GTEventContentDisplayName(TEXT("Debug Event Choice"));
	const FString GTEventRuleDisplayName(TEXT("Present Only Event"));
	const FString GTCombatContentDisplayName(TEXT("Debug Dummy Combat"));
	const FString GTCombatRuleDisplayName(TEXT("Start Only Combat"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTEventOption_Scout(TEXT("Event_DebugOption_Scout"));
	const FName GTEventOption_Invalid(TEXT("Event_DebugOption_Invalid"));
	const FName GTCombatResult_Success(TEXT("Combat_DebugResult_Success"));
	const FName GTCombatResult_Retreat(TEXT("Combat_DebugResult_Retreat"));
	const FName GTCombatResult_Invalid(TEXT("Combat_DebugResult_Invalid"));
	const FName GTRunSummaryOutcome_Extracted(TEXT("Extracted"));
	const FName GTActorId_Player(TEXT("Player"));
}

void UGT_RuntimeSmokeValidator::Initialize(UGT_RunSubsystem* InRunSubsystem)
{
	RunSubsystem = InRunSubsystem;
}

void UGT_RuntimeSmokeValidator::SetDebugSubsystem(UGT_DebugSubsystem* InDebugSubsystem)
{
	DebugSubsystem = InDebugSubsystem;
}

bool UGT_RuntimeSmokeValidator::RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults)
{
	OutResults.Reset();

	const AGT_GameMode* DefaultGameMode = GetDefault<AGT_GameMode>();
	AddCheck(
		OutResults,
		GTCheck_DefaultGameModeUsesGraytailController,
		DefaultGameMode && DefaultGameMode->PlayerControllerClass == AGT_PlayerController::StaticClass(),
		TEXT("Graytail game mode must use the Graytail player controller."));
	AddCheck(
		OutResults,
		GTCheck_LocalControllerCreatesHudOnce,
		AGT_PlayerController::ShouldCreateHud(true, false),
		TEXT("A local controller without a HUD should create one."));
	AddCheck(
		OutResults,
		GTCheck_ExistingHudIsNotDuplicated,
		!AGT_PlayerController::ShouldCreateHud(true, true),
		TEXT("A local controller with a HUD must not create a duplicate."));
	AddCheck(
		OutResults,
		GTCheck_RemoteControllerDoesNotCreateHud,
		!AGT_PlayerController::ShouldCreateHud(false, false),
		TEXT("A remote controller must not create a local HUD."));
	UGT_MetaProgressSubsystem* PersistenceMeta = DebugSubsystem && DebugSubsystem->GetGameInstance()
		? DebugSubsystem->GetGameInstance()->GetSubsystem<UGT_MetaProgressSubsystem>()
		: nullptr;
	GT_MetaPersistenceSmokeValidator::AppendChecks(OutResults, PersistenceMeta);

	FGT_GameDataSnapshot DefaultGameData;
	TArray<FString> DefaultGameDataErrors;
	const bool bDefaultGameDataLoads = FGT_GameDataLoader::LoadFromDirectory(
		UGT_GameDataSubsystem::GetDefaultDataDirectory(),
		DefaultGameData,
		DefaultGameDataErrors);
	AddCheck(
		OutResults,
		GTCheck_GameDataDefaultLoads,
		bDefaultGameDataLoads,
		bDefaultGameDataLoads ? TEXT("Default JSON game data loaded.") : FString::Join(DefaultGameDataErrors, TEXT(" | ")));

	FGT_GameDataSnapshot MissingGameData;
	TArray<FString> MissingGameDataErrors;
	const bool bMissingDirectoryRejected = !FGT_GameDataLoader::LoadFromDirectory(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MissingGameDataDirectory")),
		MissingGameData,
		MissingGameDataErrors)
		&& MissingGameDataErrors.Num() > 0;
	AddCheck(
		OutResults,
		GTCheck_GameDataMissingDirectoryRejected,
		bMissingDirectoryRejected,
		FString::Printf(TEXT("Missing directory produced %d error(s)."), MissingGameDataErrors.Num()));

	auto AddRejectedGameDataCheck = [&OutResults](
		FName CheckName,
		const FString& Directory,
		const FString& SetupDescription)
	{
		FGT_GameDataSnapshot Snapshot;
		TArray<FString> Errors;
		const bool bRejected = !FGT_GameDataLoader::LoadFromDirectory(Directory, Snapshot, Errors)
			&& Errors.Num() > 0;
		AddCheck(
			OutResults,
			CheckName,
			bRejected,
			FString::Printf(TEXT("%s produced %d error(s): %s"), *SetupDescription, Errors.Num(), *FString::Join(Errors, TEXT(" | "))));
	};

	const FString MalformedDirectory = MakeGameDataTestDirectory(TEXT("Malformed"));
	FFileHelper::SaveStringToFile(
		TEXT("{"),
		*FPaths::Combine(MalformedDirectory, TEXT("core.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	AddRejectedGameDataCheck(GTCheck_GameDataMalformedJsonRejected, MalformedDirectory, TEXT("Malformed JSON"));

	const FString MisspelledFieldDirectory = MakeGameDataTestDirectory(TEXT("MisspelledField"));
	ReplaceGameDataText(
		MisspelledFieldDirectory,
		TEXT("monsters.json"),
		TEXT("\"moveSpeed\": 0.18"),
		TEXT("\"moveSpeeed\": 0.18"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataMisspelledFieldRejected,
		MisspelledFieldDirectory,
		TEXT("Misspelled field"));

	const FString WrongScalarTypeDirectory = MakeGameDataTestDirectory(TEXT("WrongScalarType"));
	ReplaceGameDataText(
		WrongScalarTypeDirectory,
		TEXT("core.json"),
		TEXT("\"schemaVersion\": 1"),
		TEXT("\"schemaVersion\": \"1\""));
	AddRejectedGameDataCheck(
		GTCheck_GameDataWrongScalarTypeRejected,
		WrongScalarTypeDirectory,
		TEXT("Wrong scalar type"));

	const FString UnknownFieldDirectory = MakeGameDataTestDirectory(TEXT("UnknownField"));
	ReplaceGameDataText(
		UnknownFieldDirectory,
		TEXT("core.json"),
		TEXT("\"baseHp\": 100"),
		TEXT("\"baseHp\": 100, \"unknownHp\": 1"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataUnknownFieldRejected,
		UnknownFieldDirectory,
		TEXT("Unknown field"));

	const FString VersionDirectory = MakeGameDataTestDirectory(TEXT("Version"));
	ReplaceGameDataText(VersionDirectory, TEXT("core.json"), TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": 2"));
	AddRejectedGameDataCheck(GTCheck_GameDataVersionRejected, VersionDirectory, TEXT("Unsupported schemaVersion"));

	const FString DuplicateDirectory = MakeGameDataTestDirectory(TEXT("Duplicate"));
	ReplaceGameDataText(DuplicateDirectory, TEXT("difficulties.json"), TEXT("\"id\": \"easy\""), TEXT("\"id\": \"standard\""));
	AddRejectedGameDataCheck(GTCheck_GameDataDuplicateIdRejected, DuplicateDirectory, TEXT("Duplicate id"));

	const FString NegativeDirectory = MakeGameDataTestDirectory(TEXT("Negative"));
	ReplaceGameDataText(NegativeDirectory, TEXT("core.json"), TEXT("\"mineDamage\": 30"), TEXT("\"mineDamage\": -30"));
	AddRejectedGameDataCheck(GTCheck_GameDataNegativeValueRejected, NegativeDirectory, TEXT("Negative value"));

	const FString ProbabilityDirectory = MakeGameDataTestDirectory(TEXT("Probability"));
	ReplaceGameDataText(ProbabilityDirectory, TEXT("difficulties.json"), TEXT("\"mineDensity\": 0.20"), TEXT("\"mineDensity\": 1.20"));
	AddRejectedGameDataCheck(GTCheck_GameDataProbabilityRejected, ProbabilityDirectory, TEXT("Out-of-range probability"));

	const FString ReferenceDirectory = MakeGameDataTestDirectory(TEXT("Reference"));
	ReplaceGameDataText(
		ReferenceDirectory,
		TEXT("items.json"),
		TEXT("\"itemIds\": [\"broken_copper_wire\""),
		TEXT("\"itemIds\": [\"missing_item\""));
	AddRejectedGameDataCheck(GTCheck_GameDataInvalidReferenceRejected, ReferenceDirectory, TEXT("Invalid item reference"));

	const FString MissingRequiredIdDirectory = MakeGameDataTestDirectory(TEXT("MissingRequiredId"));
	ReplaceGameDataText(
		MissingRequiredIdDirectory,
		TEXT("monsters.json"),
		TEXT("\"id\": \"slime\""),
		TEXT("\"id\": \"removed_slime\""));
	AddRejectedGameDataCheck(
		GTCheck_GameDataMissingRequiredIdRejected,
		MissingRequiredIdDirectory,
		TEXT("Missing required monster id"));

	const FString UnknownEventDirectory = MakeGameDataTestDirectory(TEXT("UnknownEvent"));
	ReplaceGameDataText(
		UnknownEventDirectory,
		TEXT("loot_events.json"),
		TEXT("\"id\": \"trader\", \"weight\": 30"),
		TEXT("\"id\": \"unknown\", \"weight\": 30"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataUnknownEventRejected,
		UnknownEventDirectory,
		TEXT("Unknown event id"));

	const FString UnknownTriggerDirectory = MakeGameDataTestDirectory(TEXT("UnknownTrigger"));
	ReplaceGameDataText(
		UnknownTriggerDirectory,
		TEXT("meta_catalog.json"),
		TEXT("\"trigger\": \"luckyCoin\""),
		TEXT("\"trigger\": \"unknown\""));
	AddRejectedGameDataCheck(
		GTCheck_GameDataUnknownTriggerRejected,
		UnknownTriggerDirectory,
		TEXT("Unknown trigger"));

	const FString ProtocolOrderDirectory = MakeGameDataTestDirectory(TEXT("ProtocolOrder"));
	ReplaceGameDataText(
		ProtocolOrderDirectory,
		TEXT("core.json"),
		TEXT("{ \"pressure\": 80, \"level\": 1 }"),
		TEXT("{ \"pressure\": 0, \"level\": 1 }"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataProtocolOrderRejected,
		ProtocolOrderDirectory,
		TEXT("Misordered protocol thresholds"));

	const FString InvalidManualLayoutDirectory = MakeGameDataTestDirectory(TEXT("InvalidManualLayout"));
	ReplaceGameDataText(
		InvalidManualLayoutDirectory,
		TEXT("difficulties.json"),
		TEXT("\"spawn\": { \"x\": 0, \"y\": 0 }"),
		TEXT("\"spawn\": { \"x\": 99, \"y\": 99 }"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataInvalidManualLayoutRejected,
		InvalidManualLayoutDirectory,
		TEXT("Out-of-bounds manual layout"));

	const FString NoRandomExitDirectory = MakeGameDataTestDirectory(TEXT("NoRandomExit"));
	ReplaceGameDataText(
		NoRandomExitDirectory,
		TEXT("difficulties.json"),
		TEXT("\"randomExitCount\": 3"),
		TEXT("\"randomExitCount\": 0"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataNoRandomExitRejected,
		NoRandomExitDirectory,
		TEXT("Random map without exits"));

	const FString AssetlessItemDirectory = MakeGameDataTestDirectory(TEXT("AssetlessItem"));
	ReplaceGameDataText(
		AssetlessItemDirectory,
		TEXT("items.json"),
		TEXT("\"id\": \"broken_copper_wire\""),
		TEXT("\"id\": \"missing_asset\""));
	ReplaceGameDataText(
		AssetlessItemDirectory,
		TEXT("items.json"),
		TEXT("\"broken_copper_wire\", \"dim_capacitor\""),
		TEXT("\"missing_asset\", \"dim_capacitor\""));
	AddRejectedGameDataCheck(
		GTCheck_GameDataAssetlessItemRejected,
		AssetlessItemDirectory,
		TEXT("Item without runtime asset"));

	const FString MissingQualityPoolDirectory = MakeGameDataTestDirectory(TEXT("MissingQualityPool"));
	ReplaceGameDataText(
		MissingQualityPoolDirectory,
		TEXT("items.json"),
		TEXT("{ \"quality\": \"rare\", \"itemIds\": [\"static_lens\", \"blackbox_tag\", \"data_disk\"] },"),
		TEXT(""));
	AddRejectedGameDataCheck(
		GTCheck_GameDataMissingQualityPoolRejected,
		MissingQualityPoolDirectory,
		TEXT("Missing quality pool"));

	const FString NegativeMetaEffectDirectory = MakeGameDataTestDirectory(TEXT("NegativeMetaEffect"));
	ReplaceGameDataText(
		NegativeMetaEffectDirectory,
		TEXT("meta_catalog.json"),
		TEXT("\"bonusHp\": 20"),
		TEXT("\"bonusHp\": -101"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataNegativeMetaEffectRejected,
		NegativeMetaEffectDirectory,
		TEXT("Negative meta effect"));

	const FString MineFloorDirectory = MakeGameDataTestDirectory(TEXT("MineFloor"));
	ReplaceGameDataText(
		MineFloorDirectory,
		TEXT("core.json"),
		TEXT("\"mineDamageFloor\": 5"),
		TEXT("\"mineDamageFloor\": 31"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataMineFloorRejected,
		MineFloorDirectory,
		TEXT("Mine damage floor above base damage"));

	const FString SlimelingSpawnDirectory = MakeGameDataTestDirectory(TEXT("SlimelingSpawn"));
	ReplaceGameDataText(
		SlimelingSpawnDirectory,
		TEXT("monsters.json"),
		TEXT("\"bChaseWhenFar\": false, \"bDashAwayOnHit\": false, \"spawnWeight\": 0"),
		TEXT("\"bChaseWhenFar\": false, \"bDashAwayOnHit\": false, \"spawnWeight\": 1"));
	AddRejectedGameDataCheck(
		GTCheck_GameDataSlimelingSpawnRejected,
		SlimelingSpawnDirectory,
		TEXT("Slimeling direct spawn weight"));

	const FString MissingPersistentMetaIdDirectory = MakeGameDataTestDirectory(TEXT("MissingPersistentMetaId"));
	ReplaceGameDataText(
		MissingPersistentMetaIdDirectory,
		TEXT("meta_catalog.json"),
		TEXT("\"id\": \"armor\""),
		TEXT("\"id\": \"renamed_armor\""));
	AddRejectedGameDataCheck(
		GTCheck_GameDataMissingPersistentMetaIdRejected,
		MissingPersistentMetaIdDirectory,
		TEXT("Missing persistent meta id"));

	const FString ExternalValueDirectory = MakeGameDataTestDirectory(TEXT("ExternalValue"));
	const bool bExternalValueWritten = ReplaceGameDataText(
		ExternalValueDirectory,
		TEXT("core.json"),
		TEXT("\"mineDamage\": 30"),
		TEXT("\"mineDamage\": 31"));
	FGT_GameDataSnapshot ExternalValueSnapshot;
	TArray<FString> ExternalValueErrors;
	const bool bExternalValueReloaded = bExternalValueWritten
		&& FGT_GameDataLoader::LoadFromDirectory(ExternalValueDirectory, ExternalValueSnapshot, ExternalValueErrors)
		&& ExternalValueSnapshot.Core.Combat.MineDamage == 31;
	AddCheck(
		OutResults,
		GTCheck_GameDataExternalValueReloaded,
		bExternalValueReloaded,
		FString::Printf(
			TEXT("External mineDamage=%d errors=%s."),
			ExternalValueSnapshot.Core.Combat.MineDamage,
			*FString::Join(ExternalValueErrors, TEXT(" | "))));

	UGT_GameDataSubsystem* GameDataSubsystem = GEngine
		? GEngine->GetEngineSubsystem<UGT_GameDataSubsystem>()
		: nullptr;

	const int32 DefaultWireValue = GT_ItemCatalog::GetItemValue(FName(TEXT("broken_copper_wire")));
	const int32 DefaultSlimeHp = GT_MonsterCatalog::GetArchetype(EGT_MonsterType::Slime).HpBase;
	const FString FacadeDirectory = MakeGameDataTestDirectory(TEXT("Facade"));
	const bool bFacadeValuesWritten =
		ReplaceGameDataText(FacadeDirectory, TEXT("loot_events.json"), TEXT("\"baseMin\": 2"), TEXT("\"baseMin\": 37"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("loot_events.json"), TEXT("\"baseMax\": 5"), TEXT("\"baseMax\": 37"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("loot_events.json"), TEXT("\"goldCap\": 10"), TEXT("\"goldCap\": 37"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("loot_events.json"), TEXT("\"baseSalePercent\": 75"), TEXT("\"baseSalePercent\": 50"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("loot_events.json"), TEXT("\"goldDropPercent\": 10"), TEXT("\"goldDropPercent\": 11"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("meta_catalog.json"), TEXT("\"maxEquipped\": 2"), TEXT("\"maxEquipped\": 3"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("meta_catalog.json"), TEXT("\"id\": \"armor\", \"price\": 110"), TEXT("\"id\": \"armor\", \"price\": 111"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("items.json"), TEXT("\"id\": \"broken_copper_wire\", \"value\": 8"), TEXT("\"id\": \"broken_copper_wire\", \"value\": 9"))
		&& ReplaceGameDataText(FacadeDirectory, TEXT("monsters.json"), TEXT("\"id\": \"slime\", \"hpBase\": 24"), TEXT("\"id\": \"slime\", \"hpBase\": 25"));
	const bool bFacadeDataLoaded = GameDataSubsystem
		&& bFacadeValuesWritten
		&& GameDataSubsystem->ReloadFromDirectory(FacadeDirectory, false);
	const FGT_SearchReward FacadeSearchReward = bFacadeDataLoaded
		? GT_LootRules::ComputeSearchReward(123, 0, 0, 0, false)
		: FGT_SearchReward();
	AddCheck(
		OutResults,
		GTCheck_GameDataLootFacadeReloaded,
		bFacadeDataLoaded
			&& FacadeSearchReward.Gold == 37
			&& GT_LootRules::GetFleeGoldDropPercent() == 11,
		FString::Printf(
			TEXT("Search gold=%d flee gold percent=%d."),
			FacadeSearchReward.Gold,
			GT_LootRules::GetFleeGoldDropPercent()));
	AddCheck(
		OutResults,
		GTCheck_GameDataEventFacadeReloaded,
		bFacadeDataLoaded && GT_EventRules::GetTraderSaleValue(100) == 50,
		FString::Printf(TEXT("Trader sale value=%d."), GT_EventRules::GetTraderSaleValue(100)));
	const FGT_EquipDef* FacadeArmor = bFacadeDataLoaded
		? GT_MetaCatalog::FindEquip(FName(TEXT("armor")))
		: nullptr;
	AddCheck(
		OutResults,
		GTCheck_GameDataMetaFacadeReloaded,
		FacadeArmor
			&& FacadeArmor->Price == 111
			&& GT_MetaCatalog::GetMaxEquipped() == 3,
		FString::Printf(
			TEXT("Armor price=%d max equipped=%d."),
			FacadeArmor ? FacadeArmor->Price : INDEX_NONE,
			GT_MetaCatalog::GetMaxEquipped()));

	AddCheck(
		OutResults,
		GTCheck_GameDataItemFacadeReloaded,
		bFacadeDataLoaded
			&& DefaultWireValue == 8
			&& GT_ItemCatalog::GetItemValue(FName(TEXT("broken_copper_wire"))) == 9,
		FString::Printf(
			TEXT("Wire value default=%d reloaded=%d."),
			DefaultWireValue,
			GT_ItemCatalog::GetItemValue(FName(TEXT("broken_copper_wire")))));
	AddCheck(
		OutResults,
		GTCheck_GameDataMonsterFacadeReloaded,
		bFacadeDataLoaded
			&& DefaultSlimeHp == 24
			&& GT_MonsterCatalog::GetArchetype(EGT_MonsterType::Slime).HpBase == 25,
		FString::Printf(
			TEXT("Slime HP default=%d reloaded=%d."),
			DefaultSlimeHp,
			GT_MonsterCatalog::GetArchetype(EGT_MonsterType::Slime).HpBase));

	const uint64 RevisionBeforeInvalidReload = GameDataSubsystem
		? GameDataSubsystem->GetRevision()
		: 0;
	const bool bInvalidRejected = GameDataSubsystem
		&& !GameDataSubsystem->ReloadFromDirectory(MalformedDirectory, false);
	const FGT_GameDataSnapshot* SnapshotAfterInvalidReload = GameDataSubsystem
		? GameDataSubsystem->GetSnapshot()
		: nullptr;
	const bool bInvalidReloadKeepsSnapshot = bInvalidRejected
		&& GameDataSubsystem->IsReady()
		&& GameDataSubsystem->GetRevision() == RevisionBeforeInvalidReload
		&& SnapshotAfterInvalidReload
		&& SnapshotAfterInvalidReload->Core.Combat.MineDamage == 30;
	AddCheck(
		OutResults,
		GTCheck_GameDataInvalidReloadKeepsSnapshot,
		bInvalidReloadKeepsSnapshot,
		FString::Printf(
			TEXT("Rejected=%s ready=%s revision=%llu->%llu snapshot=%s."),
			bInvalidRejected ? TEXT("true") : TEXT("false"),
			GameDataSubsystem && GameDataSubsystem->IsReady() ? TEXT("true") : TEXT("false"),
			RevisionBeforeInvalidReload,
			GameDataSubsystem ? GameDataSubsystem->GetRevision() : 0,
			SnapshotAfterInvalidReload ? TEXT("valid") : TEXT("null")));
	const bool bDefaultReloaded = GameDataSubsystem
		&& GameDataSubsystem->ReloadFromDirectory(UGT_GameDataSubsystem::GetDefaultDataDirectory(), false);
	if (!bDefaultReloaded)
	{
		AddCheck(
			OutResults,
			FName(TEXT("GameDataDefaultRestored")),
			false,
			TEXT("Default game data could not be restored after reload tests."));
	}

	UGT_MetaProgressSubsystem* MetaProgress = DebugSubsystem && DebugSubsystem->GetGameInstance()
		? DebugSubsystem->GetGameInstance()->GetSubsystem<UGT_MetaProgressSubsystem>()
		: nullptr;

	UGT_RunContext* LuckyCoinRun = RunSubsystem
		? RunSubsystem->StartNewRun(1, 10, 10)
		: nullptr;
	FGT_ConsumableOutcome LuckyCoinOutcome;
	if (LuckyCoinRun)
	{
		LuckyCoinRun->CheatGiveItem(FName(TEXT("lucky_coin")), 1);
		LuckyCoinRun->UseConsumableAtPlayer(FName(TEXT("lucky_coin")), LuckyCoinOutcome);
	}
	const bool bLuckyCoinPreservesLegacySeed = LuckyCoinRun
		&& LuckyCoinOutcome.Status == FName(TEXT("lucky_gold"))
		&& LuckyCoinRun->GetRunInventory().SafeGold == 30;
	AddCheck(
		OutResults,
		GTCheck_LuckyCoinPreservesLegacySeed,
		bLuckyCoinPreservesLegacySeed,
		FString::Printf(
			TEXT("Lucky coin status=%s safeGold=%d."),
			*LuckyCoinOutcome.Status.ToString(),
			LuckyCoinRun ? LuckyCoinRun->GetRunInventory().SafeGold : INDEX_NONE));
	if (RunSubsystem)
	{
		RunSubsystem->EndCurrentRun();
	}

	const FString TriggerDirectory = MakeGameDataTestDirectory(TEXT("TriggerValues"));
	const bool bTriggerValuesWritten =
		ReplaceGameDataText(
			TriggerDirectory,
			TEXT("meta_catalog.json"),
			TEXT("\"trigger\": \"settleGoldBonus\", \"triggerCap\": 0, \"triggerAmount\": 15"),
			TEXT("\"trigger\": \"settleGoldBonus\", \"triggerCap\": 0, \"triggerAmount\": 20"))
		&& ReplaceGameDataText(
			TriggerDirectory,
			TEXT("meta_catalog.json"),
			TEXT("\"trigger\": \"chestBonusLoot\", \"triggerCap\": 0, \"triggerAmount\": 1"),
			TEXT("\"trigger\": \"chestBonusLoot\", \"triggerCap\": 0, \"triggerAmount\": 2"));
	const bool bTriggerDataLoaded = GameDataSubsystem
		&& bTriggerValuesWritten
		&& GameDataSubsystem->ReloadFromDirectory(TriggerDirectory, false);

#if !UE_BUILD_SHIPPING
	if (MetaProgress)
	{
		MetaProgress->GMReset();
		MetaProgress->GMGrantItem(FName(TEXT("company_badge")));
		FName EquipError;
		MetaProgress->ToggleEquip(FName(TEXT("company_badge")), EquipError);
	}
#endif
	UGT_RunContext* SettlementRun = bTriggerDataLoaded && RunSubsystem
		? RunSubsystem->StartNewRun(1001, 10, 10)
		: nullptr;
	if (SettlementRun)
	{
		SettlementRun->CheatAddPendingGold(100);
		GT_MetaSettlement::SettleExtraction(*SettlementRun, *MetaProgress);
	}
	AddCheck(
		OutResults,
		GTCheck_ConfiguredSettleGoldBonusApplied,
		SettlementRun && MetaProgress && MetaProgress->GetGold() == 120,
		FString::Printf(
			TEXT("Configured settlement gold=%d."),
			MetaProgress ? MetaProgress->GetGold() : INDEX_NONE));
	if (RunSubsystem)
	{
		RunSubsystem->EndCurrentRun();
	}

#if !UE_BUILD_SHIPPING
	if (MetaProgress)
	{
		MetaProgress->GMReset();
	}
#endif
	UGT_RunContext* ChestBonusRun = bTriggerDataLoaded && RunSubsystem
		? RunSubsystem->StartNewRunStandard(1002, EGT_Difficulty::Easy)
		: nullptr;
	if (ChestBonusRun)
	{
		ChestBonusRun->ApplyMetaLoadout(
			FGT_EquipBonus(),
			FGT_TalentEffects(),
			TMap<FName, int32>(),
			{ FName(TEXT("salvage_magnet")) });
	}
	FIntPoint ChestCoord(INDEX_NONE, INDEX_NONE);
	if (ChestBonusRun)
	{
		for (int32 Y = 0; Y < 10 && ChestCoord.X == INDEX_NONE; ++Y)
		{
			for (int32 X = 0; X < 10; ++X)
			{
				FGT_TruthCell Cell;
				if (ChestBonusRun->GetTruthCellSnapshot(X, Y, Cell)
					&& Cell.RoomBaseType == EGT_RoomBaseType::Chest)
				{
					ChestCoord = FIntPoint(X, Y);
					break;
				}
			}
		}
	}
	const int32 ChestItemsBefore = ChestBonusRun
		? ChestBonusRun->GetRunInventory().GetCarriedItemCount()
		: INDEX_NONE;
	const bool bChestBonusGranted = ChestBonusRun
		&& ChestCoord.X != INDEX_NONE
		&& ChestBonusRun->TryGrantChestMagnetLoot(ChestCoord.X, ChestCoord.Y);
	const int32 ChestItemsAfter = ChestBonusRun
		? ChestBonusRun->GetRunInventory().GetCarriedItemCount()
		: INDEX_NONE;
	AddCheck(
		OutResults,
		GTCheck_ConfiguredChestBonusApplied,
		bChestBonusGranted && ChestItemsAfter - ChestItemsBefore == 2,
		FString::Printf(
			TEXT("Chest bonus granted=%s item delta=%d."),
			bChestBonusGranted ? TEXT("true") : TEXT("false"),
			ChestItemsAfter - ChestItemsBefore));
	if (RunSubsystem)
	{
		RunSubsystem->EndCurrentRun();
	}
#if !UE_BUILD_SHIPPING
	if (MetaProgress)
	{
		MetaProgress->GMReset();
	}
#endif
	const bool bDefaultRestoredAfterTriggerTests = GameDataSubsystem
		&& GameDataSubsystem->ReloadFromDirectory(UGT_GameDataSubsystem::GetDefaultDataDirectory(), false);
	if (!bDefaultRestoredAfterTriggerTests)
	{
		AddCheck(
			OutResults,
			FName(TEXT("GameDataDefaultRestoredAfterTriggerTests")),
			false,
			TEXT("Default game data could not be restored after trigger tests."));
	}

	FString SmokeSaveSlot;
	const bool bUsesIsolatedSaveSlot = FParse::Value(
		FCommandLine::Get(),
		TEXT("GraytailSaveSlot="),
		SmokeSaveSlot)
		&& SmokeSaveSlot.StartsWith(TEXT("GraytailMetaSmoke_"));
	AddCheck(
		OutResults,
		GTCheck_SmokeUsesIsolatedSaveSlot,
		bUsesIsolatedSaveSlot,
		FString::Printf(TEXT("Active smoke save slot=%s."), *SmokeSaveSlot));

	UGT_MetaSaveGame* OrphanedEquipSave = Cast<UGT_MetaSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UGT_MetaSaveGame::StaticClass()));
	const FName OrphanedEquipId(TEXT("removed_equipment"));
	bool bOrphanedSaveWritten = false;
	if (OrphanedEquipSave && bUsesIsolatedSaveSlot)
	{
		OrphanedEquipSave->State.OwnedItems.Add(OrphanedEquipId);
		OrphanedEquipSave->State.EquippedItems.Add(OrphanedEquipId);
		bOrphanedSaveWritten = UGameplayStatics::SaveGameToSlot(
			OrphanedEquipSave,
			SmokeSaveSlot,
			0);
	}
	if (MetaProgress && bOrphanedSaveWritten)
	{
		MetaProgress->Load();
	}
	const bool bOrphanedEquipRemoved = MetaProgress
		&& bOrphanedSaveWritten
		&& MetaProgress->GetState().OwnedItems.Contains(OrphanedEquipId)
		&& !MetaProgress->GetState().EquippedItems.Contains(OrphanedEquipId);
	AddCheck(
		OutResults,
		GTCheck_MetaSaveRemovesOrphanedEquip,
		bOrphanedEquipRemoved,
		FString::Printf(
			TEXT("Orphan owned=%s equipped=%s."),
			MetaProgress && MetaProgress->GetState().OwnedItems.Contains(OrphanedEquipId)
				? TEXT("true")
				: TEXT("false"),
			MetaProgress && MetaProgress->GetState().EquippedItems.Contains(OrphanedEquipId)
				? TEXT("true")
				: TEXT("false")));
#if !UE_BUILD_SHIPPING
	if (MetaProgress)
	{
		MetaProgress->GMReset();
	}
#endif

	const FString DebugMirrorPath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SaveGames/GraytailMeta.debug.json"));
	if (MetaProgress && bUsesIsolatedSaveSlot)
	{
		MetaProgress->Save();
	}
	FString DebugMirrorJson;
	const bool bMirrorWritten = MetaProgress
		&& bUsesIsolatedSaveSlot
		&& FFileHelper::LoadFileToString(DebugMirrorJson, *DebugMirrorPath);
	AddCheck(
		OutResults,
		GTCheck_MetaSaveDebugMirrorWritten,
		bMirrorWritten,
		FString::Printf(TEXT("Debug mirror path=%s."), *DebugMirrorPath));

	TSharedPtr<FJsonObject> DebugMirrorObject;
	const TSharedRef<TJsonReader<>> DebugMirrorReader = TJsonReaderFactory<>::Create(DebugMirrorJson);
	const bool bMirrorParsed = bMirrorWritten
		&& FJsonSerializer::Deserialize(DebugMirrorReader, DebugMirrorObject)
		&& DebugMirrorObject.IsValid();
	const TSharedPtr<FJsonObject>* DebugStateObject = nullptr;
	const bool bMirrorMatches = bMirrorParsed
		&& DebugMirrorObject->GetIntegerField(TEXT("saveVersion")) == UGT_MetaSaveGame::CurrentSaveVersion
		&& DebugMirrorObject->TryGetObjectField(TEXT("state"), DebugStateObject)
		&& DebugStateObject
		&& (*DebugStateObject)->GetIntegerField(TEXT("gold")) == MetaProgress->GetGold();
	AddCheck(
		OutResults,
		GTCheck_MetaSaveDebugMirrorMatches,
		bMirrorMatches,
		FString::Printf(
			TEXT("Mirror parsed=%s gold=%d."),
			bMirrorParsed ? TEXT("true") : TEXT("false"),
			MetaProgress ? MetaProgress->GetGold() : INDEX_NONE));

	const int32 CanonicalGold = MetaProgress ? MetaProgress->GetGold() : INDEX_NONE;
	const bool bMirrorCorrupted = FFileHelper::SaveStringToFile(
		TEXT("{\"saveVersion\":999,\"state\":{\"gold\":2147480000}}"),
		*DebugMirrorPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (MetaProgress && bUsesIsolatedSaveSlot)
	{
		MetaProgress->Load();
	}
	const bool bMirrorIgnored = MetaProgress
		&& bUsesIsolatedSaveSlot
		&& bMirrorCorrupted
		&& MetaProgress->GetGold() == CanonicalGold;
	AddCheck(
		OutResults,
		GTCheck_MetaSaveIgnoresDebugMirror,
		bMirrorIgnored,
		FString::Printf(
			TEXT("Canonical gold=%d loaded gold=%d."),
			CanonicalGold,
			MetaProgress ? MetaProgress->GetGold() : INDEX_NONE));
	if (MetaProgress && bUsesIsolatedSaveSlot)
	{
		MetaProgress->Save();
	}

	if (!IsValid(RunSubsystem))
	{
		AddCheck(OutResults, GTCheck_RunSubsystemValid, false, TEXT("RunSubsystem is not valid."));
		return false;
	}

	AddCheck(OutResults, GTCheck_RunSubsystemValid, true, TEXT("RunSubsystem is valid."));

	UGT_ContentRegistry* ContentRegistry = RunSubsystem->GetContentRegistry();
	FGT_RoomContentDef EventContentDefinition;
	const bool bEventContentRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindRoomContentDef(GTRoomContent_EventDebugChoice01, EventContentDefinition)
		&& EventContentDefinition.RoomBaseType == EGT_RoomBaseType::Event
		&& EventContentDefinition.DefaultRuleId == GTRoomRule_EventPresentOnly
		&& EventContentDefinition.DefaultOptionId == GTEventOption_DefaultContinue
		&& EventContentDefinition.DisplayName == GTEventContentDisplayName;
	AddCheck(
		OutResults,
		GTCheck_RoomRegistryEventContent,
		bEventContentRegistered,
		FString::Printf(TEXT("Event content def display=%s defaultRule=%s."),
			*EventContentDefinition.DisplayName,
			*EventContentDefinition.DefaultRuleId.ToString()));

	FGT_RoomRuleDef EventRuleDefinition;
	const bool bEventRuleRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindRoomRuleDef(GTRoomRule_EventPresentOnly, EventRuleDefinition)
		&& EventRuleDefinition.RoomBaseType == EGT_RoomBaseType::Event
		&& EventRuleDefinition.DefaultOptionId == GTEventOption_DefaultContinue
		&& EventRuleDefinition.DisplayName == GTEventRuleDisplayName;
	AddCheck(
		OutResults,
		GTCheck_RoomRegistryEventRule,
		bEventRuleRegistered,
		FString::Printf(TEXT("Event rule def display=%s defaultOption=%s."),
			*EventRuleDefinition.DisplayName,
			*EventRuleDefinition.DefaultOptionId.ToString()));

	FGT_RoomContentDef CombatContentDefinition;
	const bool bCombatContentRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindRoomContentDef(GTRoomContent_CombatDebugDummy01, CombatContentDefinition)
		&& CombatContentDefinition.RoomBaseType == EGT_RoomBaseType::Combat
		&& CombatContentDefinition.DefaultRuleId == GTRoomRule_CombatStartOnly
		&& CombatContentDefinition.DefaultResultId == GTCombatResult_Success
		&& CombatContentDefinition.DisplayName == GTCombatContentDisplayName;
	AddCheck(
		OutResults,
		GTCheck_RoomRegistryCombatContent,
		bCombatContentRegistered,
		FString::Printf(TEXT("Combat content def display=%s defaultRule=%s."),
			*CombatContentDefinition.DisplayName,
			*CombatContentDefinition.DefaultRuleId.ToString()));

	FGT_RoomRuleDef CombatRuleDefinition;
	const bool bCombatRuleRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindRoomRuleDef(GTRoomRule_CombatStartOnly, CombatRuleDefinition)
		&& CombatRuleDefinition.RoomBaseType == EGT_RoomBaseType::Combat
		&& CombatRuleDefinition.DefaultResultId == GTCombatResult_Success
		&& CombatRuleDefinition.DisplayName == GTCombatRuleDisplayName;
	AddCheck(
		OutResults,
		GTCheck_RoomRegistryCombatRule,
		bCombatRuleRegistered,
		FString::Printf(TEXT("Combat rule def display=%s defaultResult=%s."),
			*CombatRuleDefinition.DisplayName,
			*CombatRuleDefinition.DefaultResultId.ToString()));

	FGT_EventOptionDef ContinueOptionDefinition;
	const bool bContinueOptionRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindEventOptionDef(GTEventOption_DefaultContinue, ContinueOptionDefinition)
		&& ContinueOptionDefinition.Id == GTEventOption_DefaultContinue
		&& ContinueOptionDefinition.bResolvesRoom
		&& !ContinueOptionDefinition.DisplayName.IsEmpty()
		&& !ContinueOptionDefinition.PayloadText.IsEmpty();
	AddCheck(
		OutResults,
		GTCheck_EventOptionRegistryContinue,
		bContinueOptionRegistered,
		FString::Printf(TEXT("Continue option display=%s payload=%s."),
			*ContinueOptionDefinition.DisplayName,
			*ContinueOptionDefinition.PayloadText));

	FGT_EventOptionDef ScoutOptionDefinition;
	const bool bScoutOptionRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindEventOptionDef(GTEventOption_Scout, ScoutOptionDefinition)
		&& ScoutOptionDefinition.Id == GTEventOption_Scout
		&& ScoutOptionDefinition.bResolvesRoom
		&& !ScoutOptionDefinition.DisplayName.IsEmpty()
		&& ScoutOptionDefinition.PayloadText.Contains(TEXT("Scout"));
	AddCheck(
		OutResults,
		GTCheck_EventOptionRegistryScout,
		bScoutOptionRegistered,
		FString::Printf(TEXT("Scout option display=%s payload=%s."),
			*ScoutOptionDefinition.DisplayName,
			*ScoutOptionDefinition.PayloadText));

	FGT_CombatResultDef SuccessResultDefinition;
	const bool bSuccessResultRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindCombatResultDef(GTCombatResult_Success, SuccessResultDefinition)
		&& SuccessResultDefinition.Id == GTCombatResult_Success
		&& SuccessResultDefinition.bResolvesRoom
		&& SuccessResultDefinition.EventType == GTEventType_CombatResolved
		&& !SuccessResultDefinition.PayloadText.IsEmpty();
	AddCheck(
		OutResults,
		GTCheck_CombatResultRegistrySuccess,
		bSuccessResultRegistered,
		FString::Printf(TEXT("Success result display=%s payload=%s."),
			*SuccessResultDefinition.DisplayName,
			*SuccessResultDefinition.PayloadText));

	FGT_CombatResultDef RetreatResultDefinition;
	const bool bRetreatResultRegistered = IsValid(ContentRegistry)
		&& ContentRegistry->FindCombatResultDef(GTCombatResult_Retreat, RetreatResultDefinition)
		&& RetreatResultDefinition.Id == GTCombatResult_Retreat
		&& RetreatResultDefinition.bResolvesRoom
		&& RetreatResultDefinition.EventType == GTEventType_CombatRetreated
		&& RetreatResultDefinition.PayloadText.Contains(TEXT("Retreat"));
	AddCheck(
		OutResults,
		GTCheck_CombatResultRegistryRetreat,
		bRetreatResultRegistered,
		FString::Printf(TEXT("Retreat result display=%s payload=%s."),
			*RetreatResultDefinition.DisplayName,
			*RetreatResultDefinition.PayloadText));

	TArray<FString> ManualPlayHelpLines;
	if (IsValid(DebugSubsystem))
	{
		DebugSubsystem->GetDebugCommandHelpLines(ManualPlayHelpLines);
	}

	auto HasHelpLineContaining = [&ManualPlayHelpLines](const TCHAR* Needle) -> bool
	{
		return ManualPlayHelpLines.ContainsByPredicate([Needle](const FString& Line)
		{
			return Line.Contains(Needle);
		});
	};

	const bool bManualPlayHelpAvailable = IsValid(DebugSubsystem)
		&& HasHelpLineContaining(TEXT("gt.Help"))
		&& HasHelpLineContaining(TEXT("gt.Commands"))
		&& HasHelpLineContaining(TEXT("gt.Status"))
		&& HasHelpLineContaining(TEXT("gt.Room"))
		&& HasHelpLineContaining(TEXT("gt.Summary"))
		&& HasHelpLineContaining(TEXT("gt.Attack"))
		&& HasHelpLineContaining(TEXT("gt.RunDemo"));
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayHelpAvailable,
		bManualPlayHelpAvailable,
		FString::Printf(TEXT("Manual play help line count is %d."), ManualPlayHelpLines.Num()));

	FString ManualPlayStatusTextBeforeRun;
	const bool bManualPlayStatusBeforeRunActive = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugStatusText(ManualPlayStatusTextBeforeRun);
	const bool bManualPlayStatusNoRunSafe = IsValid(DebugSubsystem)
		&& !bManualPlayStatusBeforeRunActive
		&& ManualPlayStatusTextBeforeRun.Contains(TEXT("gt.StartRun"));
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayStatusNoRunSafe,
		bManualPlayStatusNoRunSafe,
		ManualPlayStatusTextBeforeRun);

	FString ManualPlaySummaryTextBeforeRun;
	const bool bManualPlaySummaryBeforeRunAvailable = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugRunSummaryText(ManualPlaySummaryTextBeforeRun);
	const bool bManualPlaySummaryNoRunSafe = IsValid(DebugSubsystem)
		&& !bManualPlaySummaryBeforeRunAvailable
		&& ManualPlaySummaryTextBeforeRun.Contains(TEXT("SummaryAvailable=false"))
		&& ManualPlaySummaryTextBeforeRun.Contains(TEXT("NoRunSummary"));
	AddCheck(
		OutResults,
		GTCheck_DebugSummaryNoRunSafe,
		bManualPlaySummaryNoRunSafe,
		ManualPlaySummaryTextBeforeRun);

	RunSubsystem->StartNewRun(12345, 10, 10);

	UGT_QueryFacade* QueryFacade = RunSubsystem->GetQueryFacade();
	const UGT_RunContext* RunContext = RunSubsystem->GetCurrentRunContext();
	const FGT_TruthMap* TruthMap = RunContext ? &RunContext->GetTruthMapForDebugOnly() : nullptr;
	UGT_EventBus* EventBus = RunSubsystem->GetEventBus();
	if (EventBus)
	{
		EventBus->ClearEventHistory();
	}

	const bool bRunStateAfterStartOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Running
		&& QueryFacade->IsRunActive();
	AddCheck(
		OutResults,
		GTCheck_RunStateAfterStart,
		bRunStateAfterStartOk,
		FString::Printf(TEXT("RunState after StartNewRun is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	TArray<FGT_ActorRuntimeState> ActorStates;
	const bool bGotActors = QueryFacade && QueryFacade->GetActorStates(ActorStates);
	const bool bPlayerExists = bGotActors && ActorStates.ContainsByPredicate([](const FGT_ActorRuntimeState& ActorState)
	{
		return ActorState.ActorId.ToName() == GTActorId_Player;
	});
	AddCheck(OutResults, GTCheck_PlayerExists, bPlayerExists, bPlayerExists ? TEXT("Player actor exists.") : TEXT("Player actor was not found."));

	int32 PlayerX = INDEX_NONE;
	int32 PlayerY = INDEX_NONE;
	const bool bGotInitialPosition = QueryFacade && QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bInitialPositionOk = bGotInitialPosition && PlayerX == 0 && PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_InitialPlayerPosition,
		bInitialPositionOk,
		FString::Printf(TEXT("Initial player position is (%d,%d)."), PlayerX, PlayerY));

	const bool bInitialIntelOk = QueryFacade
		&& QueryFacade->IsIntelCellVisible(0, 0)
		&& QueryFacade->IsIntelCellExplored(0, 0);
	AddCheck(OutResults, GTCheck_InitialIntelCell, bInitialIntelOk, bInitialIntelOk ? TEXT("Initial cell is visible and explored.") : TEXT("Initial cell is not visible/explored."));

	const bool bTruthMapSizeOk = TruthMap && TruthMap->Width == 10 && TruthMap->Height == 10;
	AddCheck(
		OutResults,
		GTCheck_TruthMapSize,
		bTruthMapSizeOk,
		FString::Printf(TEXT("TruthMap size is %dx%d."), TruthMap ? TruthMap->Width : 0, TruthMap ? TruthMap->Height : 0));

	const int32 TruthCellCount = TruthMap ? TruthMap->Cells.Num() : 0;
	const bool bTruthMapCellCountOk = TruthCellCount == 100;
	AddCheck(
		OutResults,
		GTCheck_TruthMapCellCount,
		bTruthMapCellCountOk,
		FString::Printf(TEXT("TruthMap cell count is %d."), TruthCellCount));

	FGT_TruthCell TruthCell;
	const bool bTruthCellOriginOk = QueryFacade
		&& QueryFacade->GetTruthCellDebugOnly(0, 0, TruthCell)
		&& TruthCell.X == 0
		&& TruthCell.Y == 0;
	AddCheck(
		OutResults,
		GTCheck_TruthCellOrigin,
		bTruthCellOriginOk,
		FString::Printf(TEXT("Truth origin cell is (%d,%d)."), TruthCell.X, TruthCell.Y));

	TruthCell = FGT_TruthCell();
	const bool bTruthCellCornerOk = QueryFacade
		&& QueryFacade->GetTruthCellDebugOnly(9, 9, TruthCell)
		&& TruthCell.X == 9
		&& TruthCell.Y == 9;
	AddCheck(
		OutResults,
		GTCheck_TruthCellCorner,
		bTruthCellCornerOk,
		FString::Printf(TEXT("Truth corner cell is (%d,%d)."), TruthCell.X, TruthCell.Y));

	TArray<FIntPoint> AdjacentCoords;
	const bool bNeighbors4CornerOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords4DebugOnly(0, 0, AdjacentCoords)
		&& AdjacentCoords.Num() == 2;
	AddCheck(
		OutResults,
		GTCheck_Neighbors4Corner,
		bNeighbors4CornerOk,
		FString::Printf(TEXT("4-neighbor count at (0,0) is %d."), AdjacentCoords.Num()));

	AdjacentCoords.Reset();
	const bool bNeighbors4CenterOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords4DebugOnly(1, 1, AdjacentCoords)
		&& AdjacentCoords.Num() == 4;
	AddCheck(
		OutResults,
		GTCheck_Neighbors4Center,
		bNeighbors4CenterOk,
		FString::Printf(TEXT("4-neighbor count at (1,1) is %d."), AdjacentCoords.Num()));

	AdjacentCoords.Reset();
	const bool bNeighbors8CornerOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords8DebugOnly(0, 0, AdjacentCoords)
		&& AdjacentCoords.Num() == 3;
	AddCheck(
		OutResults,
		GTCheck_Neighbors8Corner,
		bNeighbors8CornerOk,
		FString::Printf(TEXT("8-neighbor count at (0,0) is %d."), AdjacentCoords.Num()));

	AdjacentCoords.Reset();
	const bool bNeighbors8CenterOk = QueryFacade
		&& QueryFacade->GetTruthAdjacentCoords8DebugOnly(1, 1, AdjacentCoords)
		&& AdjacentCoords.Num() == 8;
	AddCheck(
		OutResults,
		GTCheck_Neighbors8Center,
		bNeighbors8CenterOk,
		FString::Printf(TEXT("8-neighbor count at (1,1) is %d."), AdjacentCoords.Num()));

	const bool bExitCellDebugOk = QueryFacade && QueryFacade->IsTruthExitDebugOnly(9, 9);
	AddCheck(OutResults, GTCheck_ExitCellDebug, bExitCellDebugOk, bExitCellDebugOk ? TEXT("Truth cell (9,9) is exit.") : TEXT("Truth cell (9,9) is not exit."));

	const bool bMineCellDebugOk = QueryFacade && QueryFacade->IsTruthMineDebugOnly(2, 2);
	AddCheck(OutResults, GTCheck_MineCellDebug, bMineCellDebugOk, bMineCellDebugOk ? TEXT("Truth cell (2,2) is mine.") : TEXT("Truth cell (2,2) is not mine."));

	FGT_TruthCell EventPlaceholderTruthCell;
	const bool bEventPlaceholderCellDebugOk = QueryFacade
		&& QueryFacade->GetTruthCellDebugOnly(4, 1, EventPlaceholderTruthCell)
		&& EventPlaceholderTruthCell.RoomBaseType == EGT_RoomBaseType::Event
		&& EventPlaceholderTruthCell.RoomContentId == GTRoomContent_EventDebugChoice01
		&& EventPlaceholderTruthCell.RoomRuleId == GTRoomRule_EventPresentOnly;
	AddCheck(
		OutResults,
		GTCheck_EventPlaceholderCellDebug,
		bEventPlaceholderCellDebugOk,
		FString::Printf(TEXT("Event placeholder cell (4,1): type=%d content=%s rule=%s."),
			static_cast<int32>(EventPlaceholderTruthCell.RoomBaseType),
			*EventPlaceholderTruthCell.RoomContentId.ToString(),
			*EventPlaceholderTruthCell.RoomRuleId.ToString()));

	FGT_TruthCell CombatPlaceholderTruthCell;
	const bool bCombatPlaceholderCellDebugOk = QueryFacade
		&& QueryFacade->GetTruthCellDebugOnly(1, 4, CombatPlaceholderTruthCell)
		&& CombatPlaceholderTruthCell.RoomBaseType == EGT_RoomBaseType::Combat
		&& CombatPlaceholderTruthCell.RoomContentId == GTRoomContent_CombatDebugDummy01
		&& CombatPlaceholderTruthCell.RoomRuleId == GTRoomRule_CombatStartOnly;
	AddCheck(
		OutResults,
		GTCheck_CombatPlaceholderCellDebug,
		bCombatPlaceholderCellDebugOk,
		FString::Printf(TEXT("Combat placeholder cell (1,4): type=%d content=%s rule=%s."),
			static_cast<int32>(CombatPlaceholderTruthCell.RoomBaseType),
			*CombatPlaceholderTruthCell.RoomContentId.ToString(),
			*CombatPlaceholderTruthCell.RoomRuleId.ToString()));

	int32 AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountNearMineReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(1, 1, AdjacentMineCount);
	const bool bAdjacentMineCountNearMineOk = bAdjacentMineCountNearMineReturned && AdjacentMineCount == 1;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountNearMine,
		bAdjacentMineCountNearMineOk,
		FString::Printf(TEXT("Adjacent mine count at (1,1) is %d."), AdjacentMineCount));

	AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountFarReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(0, 0, AdjacentMineCount);
	const bool bAdjacentMineCountFarOk = bAdjacentMineCountFarReturned && AdjacentMineCount == 0;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountFarFromMine,
		bAdjacentMineCountFarOk,
		FString::Printf(TEXT("Adjacent mine count at (0,0) is %d."), AdjacentMineCount));

	AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountSelfReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(2, 2, AdjacentMineCount);
	const bool bAdjacentMineCountSelfOk = bAdjacentMineCountSelfReturned && AdjacentMineCount == 0;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountMineCellSelfExcluded,
		bAdjacentMineCountSelfOk,
		FString::Printf(TEXT("Adjacent mine count at mine cell (2,2) is %d."), AdjacentMineCount));

	AdjacentMineCount = INDEX_NONE;
	const bool bAdjacentMineCountInvalidReturned = QueryFacade && QueryFacade->CountAdjacentMinesDebugOnly(-1, 0, AdjacentMineCount);
	const bool bAdjacentMineCountInvalidOk = !bAdjacentMineCountInvalidReturned && AdjacentMineCount == 0;
	AddCheck(
		OutResults,
		GTCheck_AdjacentMineCountInvalidCoord,
		bAdjacentMineCountInvalidOk,
		FString::Printf(TEXT("Invalid adjacent mine count query returned %s with count %d."), bAdjacentMineCountInvalidReturned ? TEXT("true") : TEXT("false"), AdjacentMineCount));

	FGT_Command ScanCommand;
	ScanCommand.CommandType = GTCommandType_Scan;
	ScanCommand.SourceActorId = GTActorId_Player;
	ScanCommand.TargetActorId = GTActorId_Player;
	ScanCommand.TargetX = 1;
	ScanCommand.TargetY = 1;

	const int32 RoomEnteredCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountBeforeScan = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const bool bScanAccepted = RunSubsystem->SubmitCommand(ScanCommand);
	AddCheck(OutResults, GTCheck_ScanCommandAccepted, bScanAccepted, bScanAccepted ? TEXT("Scan command at (1,1) accepted.") : TEXT("Scan command at (1,1) was rejected."));

	FGT_MiniMapCellViewData ScannedCell;
	const bool bGotScannedCell = QueryFacade && QueryFacade->GetIntelCellViewData(1, 1, ScannedCell);
	const bool bScannedIntelCellMarked = bGotScannedCell
		&& ScannedCell.bScanned
		&& ScannedCell.bVisible
		&& ScannedCell.bExplored;
	AddCheck(
		OutResults,
		GTCheck_ScannedIntelCellMarked,
		bScannedIntelCellMarked,
		FString::Printf(TEXT("Scanned intel cell flags are scanned=%s visible=%s explored=%s."),
			ScannedCell.bScanned ? TEXT("true") : TEXT("false"),
			ScannedCell.bVisible ? TEXT("true") : TEXT("false"),
			ScannedCell.bExplored ? TEXT("true") : TEXT("false")));

	const bool bScannedDisplayedNumberOk = bGotScannedCell && ScannedCell.DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_ScannedDisplayedNumber,
		bScannedDisplayedNumberOk,
		FString::Printf(TEXT("Scanned displayed number at (1,1) is %d."), ScannedCell.DisplayedNumber));

	const bool bCellScannedEventOk = EventBus && EventBus->HasEventOfType(GTEventType_CellScanned);
	AddCheck(OutResults, GTCheck_CellScannedEvent, bCellScannedEventOk, bCellScannedEventOk ? TEXT("CellScanned event recorded.") : TEXT("CellScanned event was not recorded."));

	const int32 RoomEnteredCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountAfterScan = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const bool bScanDoesNotTriggerRoomResolverOk = RoomEnteredCountAfterScan == RoomEnteredCountBeforeScan
		&& RoomResolvedCountAfterScan == RoomResolvedCountBeforeScan
		&& MineEncounteredCountAfterScan == MineEncounteredCountBeforeScan
		&& ExitFoundCountAfterScan == ExitFoundCountBeforeScan;
	AddCheck(
		OutResults,
		GTCheck_ScanDoesNotTriggerRoomResolver,
		bScanDoesNotTriggerRoomResolverOk,
		FString::Printf(TEXT("Resolver event counts after scan: entered %d->%d, resolved %d->%d, mine %d->%d, exit %d->%d."),
			RoomEnteredCountBeforeScan,
			RoomEnteredCountAfterScan,
			RoomResolvedCountBeforeScan,
			RoomResolvedCountAfterScan,
			MineEncounteredCountBeforeScan,
			MineEncounteredCountAfterScan,
			ExitFoundCountBeforeScan,
			ExitFoundCountAfterScan));

	FGT_TruthCell ScannedTruthCell;
	const bool bGotScannedTruthCell = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 1, ScannedTruthCell);
	const bool bScanDoesNotResolveRoomOk = bGotScannedTruthCell
		&& !ScannedTruthCell.bResolved
		&& !ScannedTruthCell.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_ScanDoesNotResolveRoom,
		bScanDoesNotResolveRoomOk,
		FString::Printf(TEXT("Scanned truth cell (1,1) resolved=%s triggered=%s."),
			ScannedTruthCell.bResolved ? TEXT("true") : TEXT("false"),
			ScannedTruthCell.bTriggered ? TEXT("true") : TEXT("false")));

	UGT_MiniMapViewModel* MiniMapViewModel = NewObject<UGT_MiniMapViewModel>(this);
	const bool bMiniMapViewModelBuildOk = MiniMapViewModel && RunContext;
	if (bMiniMapViewModelBuildOk)
	{
		MiniMapViewModel->BuildFromIntelMap(RunContext->GetPlayerIntelMap());
	}
	AddCheck(OutResults, GTCheck_MiniMapViewModelBuild, bMiniMapViewModelBuildOk, bMiniMapViewModelBuildOk ? TEXT("MiniMapViewModel built from IntelMap.") : TEXT("MiniMapViewModel could not be built."));

	TArray<FGT_MiniMapCellViewData> MiniMapCells;
	int32 MiniMapWidth = 0;
	int32 MiniMapHeight = 0;
	if (MiniMapViewModel)
	{
		MiniMapCells = MiniMapViewModel->GetCells();
		MiniMapWidth = MiniMapViewModel->GetWidth();
		MiniMapHeight = MiniMapViewModel->GetHeight();
	}

	const bool bMiniMapViewModelSizeOk = MiniMapWidth == 10
		&& MiniMapHeight == 10
		&& MiniMapCells.Num() == 100;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelSize,
		bMiniMapViewModelSizeOk,
		FString::Printf(TEXT("MiniMapViewModel size is %dx%d with %d cells."), MiniMapWidth, MiniMapHeight, MiniMapCells.Num()));

	auto CountQueryMiniMapViewModels = [QueryFacade]() -> int32
	{
		if (!QueryFacade)
		{
			return 0;
		}

		TArray<UObject*> QueryChildren;
		GetObjectsWithOuter(QueryFacade, QueryChildren, false);
		int32 MiniMapViewModelCount = 0;
		for (const UObject* Child : QueryChildren)
		{
			if (IsValid(Child) && Child->IsA<UGT_MiniMapViewModel>())
			{
				++MiniMapViewModelCount;
			}
		}
		return MiniMapViewModelCount;
	};

	TArray<FGT_MiniMapCellViewData> QueryMiniMapCells;
	int32 QueryMiniMapWidth = 0;
	int32 QueryMiniMapHeight = 0;
	const int32 QueryMiniMapModelsBeforeBuild = CountQueryMiniMapViewModels();
	if (QueryFacade)
	{
		QueryFacade->BuildMiniMapViewData(QueryMiniMapCells, QueryMiniMapWidth, QueryMiniMapHeight);
	}
	const int32 QueryMiniMapModelsAfterFirstBuild = CountQueryMiniMapViewModels();
	if (QueryFacade)
	{
		QueryFacade->BuildMiniMapViewData(QueryMiniMapCells, QueryMiniMapWidth, QueryMiniMapHeight);
	}
	const int32 QueryMiniMapModelsAfterSecondBuild = CountQueryMiniMapViewModels();
	const bool bQueryFacadeReusesMiniMapViewModel = QueryFacade
		&& QueryMiniMapModelsBeforeBuild == 1
		&& QueryMiniMapModelsAfterFirstBuild == QueryMiniMapModelsBeforeBuild
		&& QueryMiniMapModelsAfterSecondBuild == QueryMiniMapModelsAfterFirstBuild;
	AddCheck(
		OutResults,
		GTCheck_QueryFacadeReusesMiniMapViewModel,
		bQueryFacadeReusesMiniMapViewModel,
		FString::Printf(TEXT("QueryFacade MiniMapViewModel children %d->%d->%d."),
			QueryMiniMapModelsBeforeBuild,
			QueryMiniMapModelsAfterFirstBuild,
			QueryMiniMapModelsAfterSecondBuild));

	FGT_MiniMapCellViewData MiniMapScannedCell;
	bool bFoundMiniMapScannedCell = false;
	for (const FGT_MiniMapCellViewData& Cell : MiniMapCells)
	{
		if (Cell.X == 1 && Cell.Y == 1)
		{
			MiniMapScannedCell = Cell;
			bFoundMiniMapScannedCell = true;
			break;
		}
	}

	const bool bMiniMapViewModelScannedCellOk = bFoundMiniMapScannedCell && MiniMapScannedCell.bScanned;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelScannedCell,
		bMiniMapViewModelScannedCellOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) scanned=%s."), MiniMapScannedCell.bScanned ? TEXT("true") : TEXT("false")));

	const bool bMiniMapViewModelDisplayedNumberOk = bFoundMiniMapScannedCell && MiniMapScannedCell.DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelDisplayedNumber,
		bMiniMapViewModelDisplayedNumberOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) displayed number is %d."), MiniMapScannedCell.DisplayedNumber));

	const bool bMiniMapViewModelCellVisibleExploredOk = bFoundMiniMapScannedCell
		&& MiniMapScannedCell.bVisible
		&& MiniMapScannedCell.bExplored;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelCellVisibleExplored,
		bMiniMapViewModelCellVisibleExploredOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) visible=%s explored=%s."),
			MiniMapScannedCell.bVisible ? TEXT("true") : TEXT("false"),
			MiniMapScannedCell.bExplored ? TEXT("true") : TEXT("false")));

	const bool bMiniMapViewModelReliabilityOk = bFoundMiniMapScannedCell
		&& MiniMapScannedCell.ReliabilityState == EGT_IntelReliabilityState::Accurate;
	AddCheck(
		OutResults,
		GTCheck_MiniMapViewModelReliability,
		bMiniMapViewModelReliabilityOk,
		FString::Printf(TEXT("MiniMapViewModel cell (1,1) reliability is %d."), static_cast<int32>(MiniMapScannedCell.ReliabilityState)));

	const int32 CommandFailedCountBeforeInvalidScan = EventBus ? EventBus->CountEventsOfType(GTEventType_CommandFailed) : 0;

	FGT_Command InvalidScanCommand;
	InvalidScanCommand.CommandType = GTCommandType_Scan;
	InvalidScanCommand.SourceActorId = GTActorId_Player;
	InvalidScanCommand.TargetActorId = GTActorId_Player;
	InvalidScanCommand.TargetX = -1;
	InvalidScanCommand.TargetY = 0;

	const bool bInvalidScanAccepted = RunSubsystem->SubmitCommand(InvalidScanCommand);
	AddCheck(OutResults, GTCheck_InvalidScanRejected, !bInvalidScanAccepted, !bInvalidScanAccepted ? TEXT("Invalid scan rejected.") : TEXT("Invalid scan was accepted."));

	FGT_MiniMapCellViewData UntouchedCell;
	const bool bGotUntouchedCell = QueryFacade && QueryFacade->GetIntelCellViewData(0, 1, UntouchedCell);
	const bool bInvalidScanDoesNotWriteIntel = bGotUntouchedCell
		&& !UntouchedCell.bScanned
		&& UntouchedCell.DisplayedNumber == 0;
	AddCheck(
		OutResults,
		GTCheck_InvalidScanDoesNotWriteIntel,
		bInvalidScanDoesNotWriteIntel,
		FString::Printf(TEXT("Untouched intel cell (0,1) scanned=%s displayed=%d."),
			UntouchedCell.bScanned ? TEXT("true") : TEXT("false"),
			UntouchedCell.DisplayedNumber));

	const int32 CommandFailedCountAfterInvalidScan = EventBus ? EventBus->CountEventsOfType(GTEventType_CommandFailed) : 0;
	const bool bInvalidScanCommandFailedEventOk = CommandFailedCountAfterInvalidScan == CommandFailedCountBeforeInvalidScan + 1;
	AddCheck(
		OutResults,
		GTCheck_InvalidScanCommandFailedEvent,
		bInvalidScanCommandFailedEventOk,
		FString::Printf(TEXT("CommandFailed count before invalid scan was %d, after was %d."),
			CommandFailedCountBeforeInvalidScan,
			CommandFailedCountAfterInvalidScan));

	FGT_TruthCell MoveTargetTruthCellBeforeMove;
	const bool bGotMoveTargetBeforeMove = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 0, MoveTargetTruthCellBeforeMove);
	const bool bRoomNotResolvedBeforeMoveOk = bGotMoveTargetBeforeMove
		&& !MoveTargetTruthCellBeforeMove.bResolved
		&& !MoveTargetTruthCellBeforeMove.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_RoomNotResolvedBeforeMove,
		bRoomNotResolvedBeforeMoveOk,
		FString::Printf(TEXT("Move target room (1,0) before move resolved=%s triggered=%s."),
			MoveTargetTruthCellBeforeMove.bResolved ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellBeforeMove.bTriggered ? TEXT("true") : TEXT("false")));

	const int32 RoomEnteredCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountBeforeNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;

	FGT_Command MoveCommand;
	MoveCommand.CommandType = GTCommandType_Move;
	MoveCommand.SourceActorId = GTActorId_Player;
	MoveCommand.TargetActorId = GTActorId_Player;
	MoveCommand.TargetX = 1;
	MoveCommand.TargetY = 0;

	const bool bMoveAccepted = RunSubsystem->SubmitCommand(MoveCommand);
	AddCheck(OutResults, GTCheck_LegalMoveAccepted, bMoveAccepted, bMoveAccepted ? TEXT("Legal move to (1,0) accepted.") : TEXT("Legal move to (1,0) was rejected."));

	PlayerX = INDEX_NONE;
	PlayerY = INDEX_NONE;
	const bool bGotMovedPosition = QueryFacade && QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bMovedPositionOk = bGotMovedPosition && PlayerX == 1 && PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_MovedPlayerPosition,
		bMovedPositionOk,
		FString::Printf(TEXT("Player position after legal move is (%d,%d)."), PlayerX, PlayerY));

	const bool bMovedIntelOk = QueryFacade
		&& QueryFacade->IsIntelCellVisible(1, 0)
		&& QueryFacade->IsIntelCellExplored(1, 0);
	AddCheck(OutResults, GTCheck_MovedIntelCell, bMovedIntelOk, bMovedIntelOk ? TEXT("Moved cell is visible and explored.") : TEXT("Moved cell is not visible/explored."));

	FGT_TruthCell MoveTargetTruthCellAfterMove;
	const bool bGotMoveTargetAfterMove = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 0, MoveTargetTruthCellAfterMove);
	const bool bMoveResolvesTargetRoomOk = bGotMoveTargetAfterMove && MoveTargetTruthCellAfterMove.bResolved;
	AddCheck(
		OutResults,
		GTCheck_MoveResolvesTargetRoom,
		bMoveResolvesTargetRoomOk,
		FString::Printf(TEXT("Move target room (1,0) resolved=%s."),
			MoveTargetTruthCellAfterMove.bResolved ? TEXT("true") : TEXT("false")));

	const bool bMoveTriggersTargetRoomOk = bGotMoveTargetAfterMove && MoveTargetTruthCellAfterMove.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_MoveTriggersTargetRoom,
		bMoveTriggersTargetRoomOk,
		FString::Printf(TEXT("Move target room (1,0) triggered=%s."),
			MoveTargetTruthCellAfterMove.bTriggered ? TEXT("true") : TEXT("false")));

	const bool bRoomEnteredEventOk = EventBus && EventBus->HasEventOfType(GTEventType_RoomEntered);
	AddCheck(OutResults, GTCheck_RoomEnteredEvent, bRoomEnteredEventOk, bRoomEnteredEventOk ? TEXT("RoomEntered event recorded.") : TEXT("RoomEntered event was not recorded."));

	const bool bRoomResolvedEventOk = EventBus && EventBus->HasEventOfType(GTEventType_RoomResolved);
	AddCheck(OutResults, GTCheck_RoomResolvedEvent, bRoomResolvedEventOk, bRoomResolvedEventOk ? TEXT("RoomResolved event recorded.") : TEXT("RoomResolved event was not recorded."));

	const int32 RoomEnteredCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomEntered) : 0;
	const int32 RoomResolvedCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RoomResolved) : 0;
	const int32 MineEncounteredCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountAfterNormalMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const bool bNormalRoomResolveOutcomeOk = bGotMoveTargetAfterMove
		&& MoveTargetTruthCellAfterMove.RoomBaseType == EGT_RoomBaseType::Normal
		&& !MoveTargetTruthCellAfterMove.bHasMine
		&& !MoveTargetTruthCellAfterMove.bIsExit
		&& RoomResolvedCountAfterNormalMove == RoomResolvedCountBeforeNormalMove + 1
		&& MineEncounteredCountAfterNormalMove == MineEncounteredCountBeforeNormalMove
		&& ExitFoundCountAfterNormalMove == ExitFoundCountBeforeNormalMove;
	AddCheck(
		OutResults,
		GTCheck_NormalRoomResolveOutcome,
		bNormalRoomResolveOutcomeOk,
		FString::Printf(TEXT("Normal room (1,0) type=%d resolved events %d->%d, mine %d->%d, exit %d->%d."),
			static_cast<int32>(MoveTargetTruthCellAfterMove.RoomBaseType),
			RoomResolvedCountBeforeNormalMove,
			RoomResolvedCountAfterNormalMove,
			MineEncounteredCountBeforeNormalMove,
			MineEncounteredCountAfterNormalMove,
			ExitFoundCountBeforeNormalMove,
			ExitFoundCountAfterNormalMove));

	const bool bNormalRoomEventsOk = RoomEnteredCountAfterNormalMove == RoomEnteredCountBeforeNormalMove + 1
		&& RoomResolvedCountAfterNormalMove == RoomResolvedCountBeforeNormalMove + 1;
	AddCheck(
		OutResults,
		GTCheck_NormalRoomEvents,
		bNormalRoomEventsOk,
		FString::Printf(TEXT("Normal room events entered %d->%d, resolved %d->%d."),
			RoomEnteredCountBeforeNormalMove,
			RoomEnteredCountAfterNormalMove,
			RoomResolvedCountBeforeNormalMove,
			RoomResolvedCountAfterNormalMove));

	FGT_Command OutOfBoundsCommand;
	OutOfBoundsCommand.CommandType = GTCommandType_Move;
	OutOfBoundsCommand.SourceActorId = GTActorId_Player;
	OutOfBoundsCommand.TargetActorId = GTActorId_Player;
	OutOfBoundsCommand.TargetX = -1;
	OutOfBoundsCommand.TargetY = 0;

	const bool bOutOfBoundsAccepted = RunSubsystem->SubmitCommand(OutOfBoundsCommand);
	AddCheck(OutResults, GTCheck_OutOfBoundsMoveRejected, !bOutOfBoundsAccepted, !bOutOfBoundsAccepted ? TEXT("Out-of-bounds move rejected.") : TEXT("Out-of-bounds move was accepted."));

	FGT_TruthCell InvalidMoveTargetTruthCell;
	const bool bInvalidMoveTargetExists = QueryFacade && QueryFacade->GetTruthCellDebugOnly(-1, 0, InvalidMoveTargetTruthCell);
	FGT_TruthCell MoveTargetTruthCellAfterInvalidMove;
	const bool bGotMoveTargetAfterInvalidMove = QueryFacade && QueryFacade->GetTruthCellDebugOnly(1, 0, MoveTargetTruthCellAfterInvalidMove);
	const bool bInvalidMoveDoesNotResolveRoomOk = !bInvalidMoveTargetExists
		&& bGotMoveTargetAfterInvalidMove
		&& bGotMoveTargetAfterMove
		&& MoveTargetTruthCellAfterInvalidMove.bResolved == MoveTargetTruthCellAfterMove.bResolved
		&& MoveTargetTruthCellAfterInvalidMove.bTriggered == MoveTargetTruthCellAfterMove.bTriggered;
	AddCheck(
		OutResults,
		GTCheck_InvalidMoveDoesNotResolveRoom,
		bInvalidMoveDoesNotResolveRoomOk,
		FString::Printf(TEXT("Invalid move target exists=%s; current room resolved %s->%s triggered %s->%s."),
			bInvalidMoveTargetExists ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterMove.bResolved ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterInvalidMove.bResolved ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterMove.bTriggered ? TEXT("true") : TEXT("false"),
			MoveTargetTruthCellAfterInvalidMove.bTriggered ? TEXT("true") : TEXT("false")));

	PlayerX = INDEX_NONE;
	PlayerY = INDEX_NONE;
	const bool bGotRejectedPosition = QueryFacade && QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY);
	const bool bRejectedPositionOk = bGotRejectedPosition && PlayerX == 1 && PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_RejectedMovePreservesPosition,
		bRejectedPositionOk,
		FString::Printf(TEXT("Player position after rejected move is (%d,%d)."), PlayerX, PlayerY));

	const bool bEventsRecorded = EventBus
		&& EventBus->HasEventOfType(GTEventType_ActorMoved)
		&& EventBus->HasEventOfType(GTEventType_CommandFailed);
	AddCheck(OutResults, GTCheck_EventsRecorded, bEventsRecorded, bEventsRecorded ? TEXT("ActorMoved and CommandFailed events recorded.") : TEXT("Expected movement events were not recorded."));

	PlayerX = INDEX_NONE;
	PlayerY = INDEX_NONE;
	const bool bQueryPositionOk = QueryFacade
		&& QueryFacade->TryGetPlayerPosition(PlayerX, PlayerY)
		&& PlayerX == 1
		&& PlayerY == 0;
	AddCheck(OutResults, GTCheck_QueryFacadePlayerPosition, bQueryPositionOk, bQueryPositionOk ? TEXT("QueryFacade reads final player position.") : TEXT("QueryFacade failed to read final player position."));

	const int32 CommandFailedCountBeforeAwayExtract = EventBus ? EventBus->CountEventsOfType(GTEventType_CommandFailed) : 0;
	FGT_Command ExtractAwayCommand;
	ExtractAwayCommand.CommandType = GTCommandType_Extract;
	ExtractAwayCommand.SourceActorId = GTActorId_Player;
	ExtractAwayCommand.TargetActorId = GTActorId_Player;
	const bool bExtractAwayAccepted = RunSubsystem->SubmitCommand(ExtractAwayCommand);
	AddCheck(
		OutResults,
		GTCheck_ExtractRejectedAwayFromExit,
		!bExtractAwayAccepted,
		!bExtractAwayAccepted ? TEXT("Extract away from exit was rejected.") : TEXT("Extract away from exit was accepted."));

	const int32 CommandFailedCountAfterAwayExtract = EventBus ? EventBus->CountEventsOfType(GTEventType_CommandFailed) : 0;
	const bool bExtractAwayCommandFailedOk = CommandFailedCountAfterAwayExtract == CommandFailedCountBeforeAwayExtract + 1;
	AddCheck(
		OutResults,
		GTCheck_ExtractAwayFromExitCommandFailed,
		bExtractAwayCommandFailedOk,
		FString::Printf(TEXT("CommandFailed count before away extract was %d, after was %d."),
			CommandFailedCountBeforeAwayExtract,
			CommandFailedCountAfterAwayExtract));

	FGT_RunSummary AwayExtractRunSummary;
	const bool bAwayExtractSummaryAvailable = RunContext && RunContext->GetRunSummarySnapshot(AwayExtractRunSummary);
	AddCheck(
		OutResults,
		GTCheck_ExtractAwayFromExitNoRunSummary,
		!bAwayExtractSummaryAvailable,
		FString::Printf(TEXT("Away extract summary available=%s outcome=%s."),
			bAwayExtractSummaryAvailable ? TEXT("true") : TEXT("false"),
			*AwayExtractRunSummary.Outcome.ToString()));

	UGT_RunSubsystem* ActiveRunSubsystem = RunSubsystem;
	auto SubmitPlayerMoveTo = [ActiveRunSubsystem](int32 TargetX, int32 TargetY) -> bool
	{
		if (!ActiveRunSubsystem)
		{
			return false;
		}

		FGT_Command Command;
		Command.CommandType = GTCommandType_Move;
		Command.SourceActorId = GTActorId_Player;
		Command.TargetActorId = GTActorId_Player;
		Command.TargetX = TargetX;
		Command.TargetY = TargetY;
		return ActiveRunSubsystem->SubmitCommand(Command);
	};

	auto SubmitPlayerScanAt = [ActiveRunSubsystem](int32 TargetX, int32 TargetY) -> bool
	{
		if (!ActiveRunSubsystem)
		{
			return false;
		}

		FGT_Command Command;
		Command.CommandType = GTCommandType_Scan;
		Command.SourceActorId = GTActorId_Player;
		Command.TargetActorId = GTActorId_Player;
		Command.TargetX = TargetX;
		Command.TargetY = TargetY;
		return ActiveRunSubsystem->SubmitCommand(Command);
	};

	auto SubmitPlayerExtract = [ActiveRunSubsystem]() -> bool
	{
		if (!ActiveRunSubsystem)
		{
			return false;
		}

		FGT_Command Command;
		Command.CommandType = GTCommandType_Extract;
		Command.SourceActorId = GTActorId_Player;
		Command.TargetActorId = GTActorId_Player;
		return ActiveRunSubsystem->SubmitCommand(Command);
	};

	const FIntPoint ExitApproachPath[] = {
		FIntPoint(2, 0),
		FIntPoint(3, 0),
		FIntPoint(4, 0),
		FIntPoint(5, 0),
		FIntPoint(6, 0),
		FIntPoint(7, 0),
		FIntPoint(8, 0),
		FIntPoint(9, 0),
		FIntPoint(9, 1),
		FIntPoint(9, 2),
		FIntPoint(9, 3),
		FIntPoint(9, 4),
		FIntPoint(9, 5),
		FIntPoint(9, 6),
		FIntPoint(9, 7),
		FIntPoint(9, 8)
	};

	bool bPathToExitOk = true;
	for (const FIntPoint& Coord : ExitApproachPath)
	{
		if (!SubmitPlayerMoveTo(Coord.X, Coord.Y))
		{
			bPathToExitOk = false;
			break;
		}
	}

	const int32 ExitFoundCountBeforeExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const int32 MineEncounteredCountBeforeExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const bool bExitMoveAccepted = bPathToExitOk && SubmitPlayerMoveTo(9, 9);
	const int32 ExitFoundCountAfterExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const int32 MineEncounteredCountAfterExitMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;

	AddCheck(
		OutResults,
		GTCheck_MoveToExitAccepted,
		bExitMoveAccepted,
		bExitMoveAccepted ? TEXT("Legal move sequence reached exit (9,9).") : TEXT("Move sequence to exit failed."));

	FGT_TruthCell ExitTruthCell;
	const bool bGotExitTruthCell = QueryFacade && QueryFacade->GetTruthCellDebugOnly(9, 9, ExitTruthCell);
	const bool bExitRoomResolveOutcomeOk = bPathToExitOk
		&& bExitMoveAccepted
		&& bGotExitTruthCell
		&& (ExitTruthCell.bIsExit || ExitTruthCell.RoomBaseType == EGT_RoomBaseType::Exit)
		&& ExitTruthCell.bResolved
		&& ExitTruthCell.bTriggered
		&& ExitFoundCountAfterExitMove == ExitFoundCountBeforeExitMove + 1
		&& MineEncounteredCountAfterExitMove == MineEncounteredCountBeforeExitMove;
	AddCheck(
		OutResults,
		GTCheck_ExitRoomResolveOutcome,
		bExitRoomResolveOutcomeOk,
		FString::Printf(TEXT("Exit room (9,9) path=%s accepted=%s exit=%s resolved=%s triggered=%s exit events %d->%d."),
			bPathToExitOk ? TEXT("true") : TEXT("false"),
			bExitMoveAccepted ? TEXT("true") : TEXT("false"),
			ExitTruthCell.bIsExit ? TEXT("true") : TEXT("false"),
			ExitTruthCell.bResolved ? TEXT("true") : TEXT("false"),
			ExitTruthCell.bTriggered ? TEXT("true") : TEXT("false"),
			ExitFoundCountBeforeExitMove,
			ExitFoundCountAfterExitMove));

	const bool bExitFoundEventOk = ExitFoundCountAfterExitMove == ExitFoundCountBeforeExitMove + 1;
	AddCheck(
		OutResults,
		GTCheck_ExitFoundEvent,
		bExitFoundEventOk,
		FString::Printf(TEXT("ExitFound events %d->%d."), ExitFoundCountBeforeExitMove, ExitFoundCountAfterExitMove));

	AddCheck(
		OutResults,
		GTCheck_ExitFoundBeforeExtract,
		bExitFoundEventOk,
		bExitFoundEventOk ? TEXT("ExitFound event was recorded before Extract.") : TEXT("ExitFound event was not recorded before Extract."));

	int32 ExitPositionX = INDEX_NONE;
	int32 ExitPositionY = INDEX_NONE;
	const bool bExitPositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(ExitPositionX, ExitPositionY);
	const bool bRunStillActiveAtExitBeforeExtractOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Running
		&& QueryFacade->IsRunActive()
		&& !QueryFacade->IsRunSucceeded();
	AddCheck(
		OutResults,
		GTCheck_RunStillActiveAtExitBeforeExtract,
		bRunStillActiveAtExitBeforeExtractOk,
		FString::Printf(TEXT("RunState at exit before Extract is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	const bool bExitDoesNotWinRunYetOk = RunSubsystem->GetCurrentRunContext() != nullptr
		&& bExitPositionReadable
		&& QueryFacade
		&& !QueryFacade->IsRunSucceeded()
		&& ExitPositionX == 9
		&& ExitPositionY == 9;
	AddCheck(
		OutResults,
		GTCheck_ExitDoesNotWinRunYet,
		bExitDoesNotWinRunYetOk,
		FString::Printf(TEXT("Run context after exit is %s; player position is (%d,%d)."),
			RunSubsystem->GetCurrentRunContext() ? TEXT("valid") : TEXT("invalid"),
			ExitPositionX,
			ExitPositionY));

	const int32 RunSucceededCountBeforeExtract = EventBus ? EventBus->CountEventsOfType(GTEventType_RunSucceeded) : 0;
	FGT_Command ExtractAtExitCommand;
	ExtractAtExitCommand.CommandType = GTCommandType_Extract;
	ExtractAtExitCommand.SourceActorId = GTActorId_Player;
	ExtractAtExitCommand.TargetActorId = GTActorId_Player;
	const bool bExtractAtExitAccepted = RunSubsystem->SubmitCommand(ExtractAtExitCommand);
	const int32 RunSucceededCountAfterExtract = EventBus ? EventBus->CountEventsOfType(GTEventType_RunSucceeded) : 0;
	AddCheck(
		OutResults,
		GTCheck_ExtractAcceptedAtExit,
		bExtractAtExitAccepted,
		bExtractAtExitAccepted ? TEXT("Extract at exit was accepted.") : TEXT("Extract at exit was rejected."));

	const bool bRunSucceededAfterExtractOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Succeeded
		&& QueryFacade->IsRunSucceeded()
		&& !QueryFacade->IsRunActive();
	AddCheck(
		OutResults,
		GTCheck_RunSucceededAfterExtract,
		bRunSucceededAfterExtractOk,
		FString::Printf(TEXT("RunState after Extract is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	const bool bRunSucceededEventOk = RunSucceededCountAfterExtract == RunSucceededCountBeforeExtract + 1;
	AddCheck(
		OutResults,
		GTCheck_RunSucceededEvent,
		bRunSucceededEventOk,
		FString::Printf(TEXT("RunSucceeded events %d->%d."), RunSucceededCountBeforeExtract, RunSucceededCountAfterExtract));

	FGT_RunSummary ExtractRunSummary;
	const bool bExtractRunSummaryAvailable = RunContext && RunContext->GetRunSummarySnapshot(ExtractRunSummary);
	AddCheck(
		OutResults,
		GTCheck_RunSummaryAvailableAfterExtract,
		bExtractRunSummaryAvailable,
		FString::Printf(TEXT("Extract summary available=%s outcome=%s."),
			bExtractRunSummaryAvailable ? TEXT("true") : TEXT("false"),
			*ExtractRunSummary.Outcome.ToString()));

	const bool bExtractRunSummaryOutcomeOk = bExtractRunSummaryAvailable
		&& ExtractRunSummary.bExtracted
		&& ExtractRunSummary.Outcome == GTRunSummaryOutcome_Extracted;
	AddCheck(
		OutResults,
		GTCheck_RunSummaryOutcomeAfterExtract,
		bExtractRunSummaryOutcomeOk,
		FString::Printf(TEXT("Extract summary outcome=%s extracted=%s."),
			*ExtractRunSummary.Outcome.ToString(),
			ExtractRunSummary.bExtracted ? TEXT("true") : TEXT("false")));

	const bool bExtractRunSummaryFinalPositionOk = bExtractRunSummaryAvailable
		&& ExtractRunSummary.FinalPlayerX == 9
		&& ExtractRunSummary.FinalPlayerY == 9;
	AddCheck(
		OutResults,
		GTCheck_RunSummaryFinalPositionAfterExtract,
		bExtractRunSummaryFinalPositionOk,
		FString::Printf(TEXT("Extract summary final player=(%d,%d)."),
			ExtractRunSummary.FinalPlayerX,
			ExtractRunSummary.FinalPlayerY));

	const bool bExtractRunSummaryEventCountOk = bExtractRunSummaryAvailable
		&& EventBus
		&& ExtractRunSummary.TotalEventCount == EventBus->GetEventCount()
		&& ExtractRunSummary.TotalEventCount > 0;
	AddCheck(
		OutResults,
		GTCheck_RunSummaryEventCountAfterExtract,
		bExtractRunSummaryEventCountOk,
		FString::Printf(TEXT("Extract summary event count=%d current event count=%d."),
			ExtractRunSummary.TotalEventCount,
			EventBus ? EventBus->GetEventCount() : 0));

	FGT_Command MoveAfterSucceededCommand;
	MoveAfterSucceededCommand.CommandType = GTCommandType_Move;
	MoveAfterSucceededCommand.SourceActorId = GTActorId_Player;
	MoveAfterSucceededCommand.TargetActorId = GTActorId_Player;
	MoveAfterSucceededCommand.TargetX = 9;
	MoveAfterSucceededCommand.TargetY = 8;
	const bool bMoveAfterSucceededAccepted = RunSubsystem->SubmitCommand(MoveAfterSucceededCommand);
	AddCheck(
		OutResults,
		GTCheck_MoveRejectedAfterRunSucceeded,
		!bMoveAfterSucceededAccepted,
		!bMoveAfterSucceededAccepted ? TEXT("Move after RunSucceeded was rejected.") : TEXT("Move after RunSucceeded was accepted."));

	FGT_Command ScanAfterSucceededCommand;
	ScanAfterSucceededCommand.CommandType = GTCommandType_Scan;
	ScanAfterSucceededCommand.SourceActorId = GTActorId_Player;
	ScanAfterSucceededCommand.TargetActorId = GTActorId_Player;
	ScanAfterSucceededCommand.TargetX = 8;
	ScanAfterSucceededCommand.TargetY = 8;
	const bool bScanAfterSucceededAccepted = RunSubsystem->SubmitCommand(ScanAfterSucceededCommand);
	AddCheck(
		OutResults,
		GTCheck_ScanRejectedAfterRunSucceeded,
		!bScanAfterSucceededAccepted,
		!bScanAfterSucceededAccepted ? TEXT("Scan after RunSucceeded was rejected.") : TEXT("Scan after RunSucceeded was accepted."));

	FGT_Command ExtractAfterSucceededCommand;
	ExtractAfterSucceededCommand.CommandType = GTCommandType_Extract;
	ExtractAfterSucceededCommand.SourceActorId = GTActorId_Player;
	ExtractAfterSucceededCommand.TargetActorId = GTActorId_Player;
	const bool bExtractAfterSucceededAccepted = RunSubsystem->SubmitCommand(ExtractAfterSucceededCommand);
	AddCheck(
		OutResults,
		GTCheck_ExtractRejectedAfterRunSucceeded,
		!bExtractAfterSucceededAccepted,
		!bExtractAfterSucceededAccepted ? TEXT("Extract after RunSucceeded was rejected.") : TEXT("Extract after RunSucceeded was accepted."));

	FGT_RunSummary SummaryAfterRejectedExtract;
	const bool bSummaryAfterRejectedExtractAvailable = RunContext && RunContext->GetRunSummarySnapshot(SummaryAfterRejectedExtract);
	const bool bSummaryPreservedAfterRejectedExtract = bSummaryAfterRejectedExtractAvailable
		&& SummaryAfterRejectedExtract.Outcome == ExtractRunSummary.Outcome
		&& SummaryAfterRejectedExtract.FinalPlayerX == ExtractRunSummary.FinalPlayerX
		&& SummaryAfterRejectedExtract.FinalPlayerY == ExtractRunSummary.FinalPlayerY
		&& SummaryAfterRejectedExtract.TotalEventCount == ExtractRunSummary.TotalEventCount;
	AddCheck(
		OutResults,
		GTCheck_RunSummaryPreservedAfterRejectedExtract,
		bSummaryPreservedAfterRejectedExtract,
		FString::Printf(TEXT("Summary after rejected extract available=%s outcome=%s final=(%d,%d) events=%d."),
			bSummaryAfterRejectedExtractAvailable ? TEXT("true") : TEXT("false"),
			*SummaryAfterRejectedExtract.Outcome.ToString(),
			SummaryAfterRejectedExtract.FinalPlayerX,
			SummaryAfterRejectedExtract.FinalPlayerY,
			SummaryAfterRejectedExtract.TotalEventCount));

	RunSubsystem->StartNewRun(12345, 10, 10);
	QueryFacade = RunSubsystem->GetQueryFacade();
	RunContext = RunSubsystem->GetCurrentRunContext();
	TruthMap = RunContext ? &RunContext->GetTruthMapForDebugOnly() : nullptr;
	EventBus = RunSubsystem->GetEventBus();
	FGT_RunSummary ClearedRunSummary;
	const bool bRunSummaryAvailableAfterNewStart = RunContext && RunContext->GetRunSummarySnapshot(ClearedRunSummary);
	AddCheck(
		OutResults,
		GTCheck_RunSummaryClearedAfterStart,
		!bRunSummaryAvailableAfterNewStart,
		FString::Printf(TEXT("New StartRun summary available=%s outcome=%s."),
			bRunSummaryAvailableAfterNewStart ? TEXT("true") : TEXT("false"),
			*ClearedRunSummary.Outcome.ToString()));
	const int32 NewRunInitialEventCount = EventBus ? EventBus->GetEventCount() : 0;
	AddCheck(
		OutResults,
		GTCheck_NewRunStartsWithFreshEventHistory,
		NewRunInitialEventCount == 1
			&& EventBus
			&& EventBus->CountEventsOfType(FName(TEXT("RunStarted"))) == 1,
		FString::Printf(TEXT("New run starts with %d event(s); expected only RunStarted."), NewRunInitialEventCount));
	if (EventBus)
	{
		EventBus->ClearEventHistory();
	}

	bool bPathToMineOk = SubmitPlayerMoveTo(1, 0);
	bPathToMineOk = bPathToMineOk && SubmitPlayerMoveTo(2, 0);
	bPathToMineOk = bPathToMineOk && SubmitPlayerMoveTo(2, 1);
	const int32 MineEncounteredCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const int32 RunFailedCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RunFailed) : 0;
	const bool bMineMoveAccepted = bPathToMineOk && SubmitPlayerMoveTo(2, 2);
	const int32 MineEncounteredCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ExitFoundCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	const int32 RunFailedCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RunFailed) : 0;

	AddCheck(OutResults, GTCheck_MineMoveAccepted, bMineMoveAccepted, bMineMoveAccepted ? TEXT("Final move onto mine was accepted.") : TEXT("Final move onto mine was rejected."));

	FGT_TruthCell MineTruthCell;
	const bool bGotMineTruthCell = QueryFacade && QueryFacade->GetTruthCellDebugOnly(2, 2, MineTruthCell);
	const bool bMineRoomResolveOutcomeOk = bPathToMineOk
		&& bMineMoveAccepted
		&& bGotMineTruthCell
		&& (MineTruthCell.bHasMine || MineTruthCell.RoomBaseType == EGT_RoomBaseType::Mine)
		&& MineTruthCell.bResolved
		&& MineTruthCell.bTriggered
		&& MineEncounteredCountAfterMineMove == MineEncounteredCountBeforeMineMove + 1
		&& ExitFoundCountAfterMineMove == ExitFoundCountBeforeMineMove;
	AddCheck(
		OutResults,
		GTCheck_MineRoomResolveOutcome,
		bMineRoomResolveOutcomeOk,
		FString::Printf(TEXT("Mine room (2,2) path=%s accepted=%s mine=%s resolved=%s triggered=%s mine events %d->%d."),
			bPathToMineOk ? TEXT("true") : TEXT("false"),
			bMineMoveAccepted ? TEXT("true") : TEXT("false"),
			MineTruthCell.bHasMine ? TEXT("true") : TEXT("false"),
			MineTruthCell.bResolved ? TEXT("true") : TEXT("false"),
			MineTruthCell.bTriggered ? TEXT("true") : TEXT("false"),
			MineEncounteredCountBeforeMineMove,
			MineEncounteredCountAfterMineMove));

	const bool bMineEncounteredEventOk = MineEncounteredCountAfterMineMove == MineEncounteredCountBeforeMineMove + 1;
	AddCheck(
		OutResults,
		GTCheck_MineEncounteredEvent,
		bMineEncounteredEventOk,
		FString::Printf(TEXT("MineEncountered events %d->%d."), MineEncounteredCountBeforeMineMove, MineEncounteredCountAfterMineMove));

	AddCheck(
		OutResults,
		GTCheck_MineEncounteredBeforeFail,
		bMineEncounteredEventOk,
		bMineEncounteredEventOk ? TEXT("MineEncountered event was recorded before failure checks.") : TEXT("MineEncountered event was not recorded."));

	int32 MinePositionX = INDEX_NONE;
	int32 MinePositionY = INDEX_NONE;
	const bool bMinePositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(MinePositionX, MinePositionY);
	const bool bMineDoesNotFailRunYetOk = RunSubsystem->GetCurrentRunContext() != nullptr
		&& bMinePositionReadable
		&& MinePositionX == 2
		&& MinePositionY == 2;
	AddCheck(
		OutResults,
		GTCheck_MineDoesNotFailRunYet,
		bMineDoesNotFailRunYetOk,
		FString::Printf(TEXT("Run context after mine is %s; player position is (%d,%d)."),
			RunSubsystem->GetCurrentRunContext() ? TEXT("valid") : TEXT("invalid"),
			MinePositionX,
			MinePositionY));

	const bool bRunFailedAfterMineOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Failed
		&& QueryFacade->IsRunFailed();
	AddCheck(
		OutResults,
		GTCheck_RunFailedAfterMine,
		bRunFailedAfterMineOk,
		FString::Printf(TEXT("RunState after mine is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	const bool bRunFailedEventOk = RunFailedCountAfterMineMove == RunFailedCountBeforeMineMove + 1;
	AddCheck(
		OutResults,
		GTCheck_RunFailedEvent,
		bRunFailedEventOk,
		FString::Printf(TEXT("RunFailed events %d->%d."), RunFailedCountBeforeMineMove, RunFailedCountAfterMineMove));

	FGT_Command MoveAfterFailedCommand;
	MoveAfterFailedCommand.CommandType = GTCommandType_Move;
	MoveAfterFailedCommand.SourceActorId = GTActorId_Player;
	MoveAfterFailedCommand.TargetActorId = GTActorId_Player;
	MoveAfterFailedCommand.TargetX = 2;
	MoveAfterFailedCommand.TargetY = 3;
	const bool bMoveAfterFailedAccepted = RunSubsystem->SubmitCommand(MoveAfterFailedCommand);
	AddCheck(
		OutResults,
		GTCheck_MoveRejectedAfterRunFailed,
		!bMoveAfterFailedAccepted,
		!bMoveAfterFailedAccepted ? TEXT("Move after RunFailed was rejected.") : TEXT("Move after RunFailed was accepted."));

	int32 PositionAfterFailedMoveX = INDEX_NONE;
	int32 PositionAfterFailedMoveY = INDEX_NONE;
	const bool bPositionAfterFailedMoveReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(PositionAfterFailedMoveX, PositionAfterFailedMoveY);
	const bool bPositionPreservedAfterFailedMoveOk = bPositionAfterFailedMoveReadable
		&& PositionAfterFailedMoveX == MinePositionX
		&& PositionAfterFailedMoveY == MinePositionY;
	AddCheck(
		OutResults,
		GTCheck_PositionPreservedAfterFailedMove,
		bPositionPreservedAfterFailedMoveOk,
		FString::Printf(TEXT("Position after failed move command is (%d,%d), before was (%d,%d)."),
			PositionAfterFailedMoveX,
			PositionAfterFailedMoveY,
			MinePositionX,
			MinePositionY));

	FGT_MiniMapCellViewData IntelBeforeFailedScan;
	const bool bGotIntelBeforeFailedScan = QueryFacade && QueryFacade->GetIntelCellViewData(0, 2, IntelBeforeFailedScan);
	FGT_Command ScanAfterFailedCommand;
	ScanAfterFailedCommand.CommandType = GTCommandType_Scan;
	ScanAfterFailedCommand.SourceActorId = GTActorId_Player;
	ScanAfterFailedCommand.TargetActorId = GTActorId_Player;
	ScanAfterFailedCommand.TargetX = 0;
	ScanAfterFailedCommand.TargetY = 2;
	const bool bScanAfterFailedAccepted = RunSubsystem->SubmitCommand(ScanAfterFailedCommand);
	AddCheck(
		OutResults,
		GTCheck_ScanRejectedAfterRunFailed,
		!bScanAfterFailedAccepted,
		!bScanAfterFailedAccepted ? TEXT("Scan after RunFailed was rejected.") : TEXT("Scan after RunFailed was accepted."));

	FGT_MiniMapCellViewData IntelAfterFailedScan;
	const bool bGotIntelAfterFailedScan = QueryFacade && QueryFacade->GetIntelCellViewData(0, 2, IntelAfterFailedScan);
	const bool bIntelPreservedAfterFailedScanOk = bGotIntelBeforeFailedScan
		&& bGotIntelAfterFailedScan
		&& IntelAfterFailedScan.bScanned == IntelBeforeFailedScan.bScanned
		&& IntelAfterFailedScan.bVisible == IntelBeforeFailedScan.bVisible
		&& IntelAfterFailedScan.bExplored == IntelBeforeFailedScan.bExplored
		&& IntelAfterFailedScan.DisplayedNumber == IntelBeforeFailedScan.DisplayedNumber;
	AddCheck(
		OutResults,
		GTCheck_IntelPreservedAfterFailedScan,
		bIntelPreservedAfterFailedScanOk,
		FString::Printf(TEXT("Intel (0,2) after failed scan scanned %s->%s visible %s->%s explored %s->%s displayed %d->%d."),
			IntelBeforeFailedScan.bScanned ? TEXT("true") : TEXT("false"),
			IntelAfterFailedScan.bScanned ? TEXT("true") : TEXT("false"),
			IntelBeforeFailedScan.bVisible ? TEXT("true") : TEXT("false"),
			IntelAfterFailedScan.bVisible ? TEXT("true") : TEXT("false"),
			IntelBeforeFailedScan.bExplored ? TEXT("true") : TEXT("false"),
			IntelAfterFailedScan.bExplored ? TEXT("true") : TEXT("false"),
			IntelBeforeFailedScan.DisplayedNumber,
			IntelAfterFailedScan.DisplayedNumber));

	RunSubsystem->StartNewRun(22345, 10, 10);
	QueryFacade = RunSubsystem->GetQueryFacade();
	RunContext = RunSubsystem->GetCurrentRunContext();
	TruthMap = RunContext ? &RunContext->GetTruthMapForDebugOnly() : nullptr;
	EventBus = RunSubsystem->GetEventBus();
	if (EventBus)
	{
		EventBus->ClearEventHistory();
	}

	const bool bScenarioFailureStartRunOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Running
		&& QueryFacade->IsRunActive();
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureStartRun,
		bScenarioFailureStartRunOk,
		FString::Printf(TEXT("Failure scenario RunState after StartNewRun is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	int32 ScenarioFailurePlayerX = INDEX_NONE;
	int32 ScenarioFailurePlayerY = INDEX_NONE;
	const bool bScenarioFailureInitialPositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(ScenarioFailurePlayerX, ScenarioFailurePlayerY);
	const bool bScenarioFailureInitialPositionOk = bScenarioFailureInitialPositionReadable
		&& ScenarioFailurePlayerX == 0
		&& ScenarioFailurePlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureInitialPosition,
		bScenarioFailureInitialPositionOk,
		FString::Printf(TEXT("Failure scenario initial player position is (%d,%d)."), ScenarioFailurePlayerX, ScenarioFailurePlayerY));

	const bool bScenarioFailureScanAccepted = SubmitPlayerScanAt(1, 1);
	FGT_MiniMapCellViewData ScenarioFailureScanCell;
	const bool bScenarioFailureScanCellReadable = QueryFacade && QueryFacade->GetIntelCellViewData(1, 1, ScenarioFailureScanCell);
	const bool bScenarioFailureScanBeforeMineOk = bScenarioFailureScanAccepted
		&& bScenarioFailureScanCellReadable
		&& ScenarioFailureScanCell.bScanned
		&& ScenarioFailureScanCell.DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureScanBeforeMine,
		bScenarioFailureScanBeforeMineOk,
		FString::Printf(TEXT("Failure scenario scan accepted=%s, scanned=%s, displayed=%d."),
			bScenarioFailureScanAccepted ? TEXT("true") : TEXT("false"),
			ScenarioFailureScanCell.bScanned ? TEXT("true") : TEXT("false"),
			ScenarioFailureScanCell.DisplayedNumber));

	bool bScenarioFailureMovePathOk = SubmitPlayerMoveTo(1, 0);
	bScenarioFailureMovePathOk = bScenarioFailureMovePathOk && SubmitPlayerMoveTo(2, 0);
	bScenarioFailureMovePathOk = bScenarioFailureMovePathOk && SubmitPlayerMoveTo(2, 1);
	const int32 ScenarioFailureMineEncounteredCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ScenarioFailureRunFailedCountBeforeMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RunFailed) : 0;
	const bool bScenarioFailureMineMoveAccepted = bScenarioFailureMovePathOk && SubmitPlayerMoveTo(2, 2);
	const int32 ScenarioFailureMineEncounteredCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_MineEncountered) : 0;
	const int32 ScenarioFailureRunFailedCountAfterMineMove = EventBus ? EventBus->CountEventsOfType(GTEventType_RunFailed) : 0;

	ScenarioFailurePlayerX = INDEX_NONE;
	ScenarioFailurePlayerY = INDEX_NONE;
	const bool bScenarioFailureMinePositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(ScenarioFailurePlayerX, ScenarioFailurePlayerY);
	const bool bScenarioFailureMoveToMinePathOk = bScenarioFailureMovePathOk
		&& bScenarioFailureMineMoveAccepted
		&& bScenarioFailureMinePositionReadable
		&& ScenarioFailurePlayerX == 2
		&& ScenarioFailurePlayerY == 2;
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureMoveToMinePath,
		bScenarioFailureMoveToMinePathOk,
		FString::Printf(TEXT("Failure scenario path=%s final move=%s, player position=(%d,%d)."),
			bScenarioFailureMovePathOk ? TEXT("true") : TEXT("false"),
			bScenarioFailureMineMoveAccepted ? TEXT("true") : TEXT("false"),
			ScenarioFailurePlayerX,
			ScenarioFailurePlayerY));

	const bool bScenarioFailureMineEncounteredOk = ScenarioFailureMineEncounteredCountAfterMineMove == ScenarioFailureMineEncounteredCountBeforeMineMove + 1;
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureMineEncountered,
		bScenarioFailureMineEncounteredOk,
		FString::Printf(TEXT("Failure scenario MineEncountered events %d->%d."),
			ScenarioFailureMineEncounteredCountBeforeMineMove,
			ScenarioFailureMineEncounteredCountAfterMineMove));

	const bool bScenarioFailureRunFailedOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Failed
		&& QueryFacade->IsRunFailed();
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureRunFailed,
		bScenarioFailureRunFailedOk,
		FString::Printf(TEXT("Failure scenario RunState after mine is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	const bool bScenarioFailureRunFailedEventOk = ScenarioFailureRunFailedCountAfterMineMove == ScenarioFailureRunFailedCountBeforeMineMove + 1;
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailureRunFailedEvent,
		bScenarioFailureRunFailedEventOk,
		FString::Printf(TEXT("Failure scenario RunFailed events %d->%d."),
			ScenarioFailureRunFailedCountBeforeMineMove,
			ScenarioFailureRunFailedCountAfterMineMove));

	const bool bScenarioFailurePostFailMoveAccepted = SubmitPlayerMoveTo(2, 3);
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailurePostFailMoveRejected,
		!bScenarioFailurePostFailMoveAccepted,
		!bScenarioFailurePostFailMoveAccepted ? TEXT("Failure scenario move after RunFailed was rejected.") : TEXT("Failure scenario move after RunFailed was accepted."));

	const bool bScenarioFailurePostFailScanAccepted = SubmitPlayerScanAt(0, 2);
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailurePostFailScanRejected,
		!bScenarioFailurePostFailScanAccepted,
		!bScenarioFailurePostFailScanAccepted ? TEXT("Failure scenario scan after RunFailed was rejected.") : TEXT("Failure scenario scan after RunFailed was accepted."));

	const bool bScenarioFailurePostFailExtractAccepted = SubmitPlayerExtract();
	AddCheck(
		OutResults,
		GTCheck_ScenarioFailurePostFailExtractRejected,
		!bScenarioFailurePostFailExtractAccepted,
		!bScenarioFailurePostFailExtractAccepted ? TEXT("Failure scenario extract after RunFailed was rejected.") : TEXT("Failure scenario extract after RunFailed was accepted."));

	RunSubsystem->StartNewRun(32345, 10, 10);
	QueryFacade = RunSubsystem->GetQueryFacade();
	RunContext = RunSubsystem->GetCurrentRunContext();
	TruthMap = RunContext ? &RunContext->GetTruthMapForDebugOnly() : nullptr;
	EventBus = RunSubsystem->GetEventBus();
	if (EventBus)
	{
		EventBus->ClearEventHistory();
	}

	const bool bScenarioSuccessStartRunOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Running
		&& QueryFacade->IsRunActive();
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessStartRun,
		bScenarioSuccessStartRunOk,
		FString::Printf(TEXT("Success scenario RunState after StartNewRun is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	int32 ScenarioSuccessPlayerX = INDEX_NONE;
	int32 ScenarioSuccessPlayerY = INDEX_NONE;
	const bool bScenarioSuccessInitialPositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(ScenarioSuccessPlayerX, ScenarioSuccessPlayerY);
	const bool bScenarioSuccessInitialPositionOk = bScenarioSuccessInitialPositionReadable
		&& ScenarioSuccessPlayerX == 0
		&& ScenarioSuccessPlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessInitialPosition,
		bScenarioSuccessInitialPositionOk,
		FString::Printf(TEXT("Success scenario initial player position is (%d,%d)."), ScenarioSuccessPlayerX, ScenarioSuccessPlayerY));

	const bool bScenarioSuccessScanAccepted = SubmitPlayerScanAt(1, 1);
	FGT_MiniMapCellViewData ScenarioSuccessScanCell;
	const bool bScenarioSuccessScanCellReadable = QueryFacade && QueryFacade->GetIntelCellViewData(1, 1, ScenarioSuccessScanCell);
	const bool bScenarioSuccessScanBeforeExitOk = bScenarioSuccessScanAccepted
		&& bScenarioSuccessScanCellReadable
		&& ScenarioSuccessScanCell.bScanned
		&& ScenarioSuccessScanCell.DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessScanBeforeExit,
		bScenarioSuccessScanBeforeExitOk,
		FString::Printf(TEXT("Success scenario scan accepted=%s, scanned=%s, displayed=%d."),
			bScenarioSuccessScanAccepted ? TEXT("true") : TEXT("false"),
			ScenarioSuccessScanCell.bScanned ? TEXT("true") : TEXT("false"),
			ScenarioSuccessScanCell.DisplayedNumber));

	const FIntPoint ScenarioSuccessExitPath[] = {
		FIntPoint(1, 0),
		FIntPoint(2, 0),
		FIntPoint(3, 0),
		FIntPoint(4, 0),
		FIntPoint(5, 0),
		FIntPoint(6, 0),
		FIntPoint(7, 0),
		FIntPoint(8, 0),
		FIntPoint(9, 0),
		FIntPoint(9, 1),
		FIntPoint(9, 2),
		FIntPoint(9, 3),
		FIntPoint(9, 4),
		FIntPoint(9, 5),
		FIntPoint(9, 6),
		FIntPoint(9, 7),
		FIntPoint(9, 8),
		FIntPoint(9, 9)
	};

	const int32 ScenarioSuccessExitFoundCountBeforePath = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;
	bool bScenarioSuccessMovePathOk = true;
	for (const FIntPoint& Coord : ScenarioSuccessExitPath)
	{
		if (!SubmitPlayerMoveTo(Coord.X, Coord.Y))
		{
			bScenarioSuccessMovePathOk = false;
			break;
		}
	}
	const int32 ScenarioSuccessExitFoundCountAfterPath = EventBus ? EventBus->CountEventsOfType(GTEventType_ExitFound) : 0;

	ScenarioSuccessPlayerX = INDEX_NONE;
	ScenarioSuccessPlayerY = INDEX_NONE;
	const bool bScenarioSuccessExitPositionReadable = QueryFacade && QueryFacade->TryGetPlayerPosition(ScenarioSuccessPlayerX, ScenarioSuccessPlayerY);
	const bool bScenarioSuccessMoveToExitPathOk = bScenarioSuccessMovePathOk
		&& bScenarioSuccessExitPositionReadable
		&& ScenarioSuccessPlayerX == 9
		&& ScenarioSuccessPlayerY == 9;
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessMoveToExitPath,
		bScenarioSuccessMoveToExitPathOk,
		FString::Printf(TEXT("Success scenario path=%s, player position=(%d,%d)."),
			bScenarioSuccessMovePathOk ? TEXT("true") : TEXT("false"),
			ScenarioSuccessPlayerX,
			ScenarioSuccessPlayerY));

	const bool bScenarioSuccessExitFoundOk = ScenarioSuccessExitFoundCountAfterPath == ScenarioSuccessExitFoundCountBeforePath + 1;
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessExitFound,
		bScenarioSuccessExitFoundOk,
		FString::Printf(TEXT("Success scenario ExitFound events %d->%d."),
			ScenarioSuccessExitFoundCountBeforePath,
			ScenarioSuccessExitFoundCountAfterPath));

	const bool bScenarioSuccessStillRunningAtExitOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Running
		&& QueryFacade->IsRunActive()
		&& !QueryFacade->IsRunSucceeded();
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessStillRunningAtExit,
		bScenarioSuccessStillRunningAtExitOk,
		FString::Printf(TEXT("Success scenario RunState at exit before Extract is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	const int32 ScenarioSuccessRunSucceededCountBeforeExtract = EventBus ? EventBus->CountEventsOfType(GTEventType_RunSucceeded) : 0;
	const bool bScenarioSuccessExtractAccepted = SubmitPlayerExtract();
	const int32 ScenarioSuccessRunSucceededCountAfterExtract = EventBus ? EventBus->CountEventsOfType(GTEventType_RunSucceeded) : 0;
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessExtractAccepted,
		bScenarioSuccessExtractAccepted,
		bScenarioSuccessExtractAccepted ? TEXT("Success scenario Extract at exit was accepted.") : TEXT("Success scenario Extract at exit was rejected."));

	const bool bScenarioSuccessRunSucceededOk = QueryFacade
		&& QueryFacade->GetRunState() == EGT_RunState::Succeeded
		&& QueryFacade->IsRunSucceeded()
		&& !QueryFacade->IsRunActive();
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessRunSucceeded,
		bScenarioSuccessRunSucceededOk,
		FString::Printf(TEXT("Success scenario RunState after Extract is %d."), QueryFacade ? static_cast<int32>(QueryFacade->GetRunState()) : static_cast<int32>(EGT_RunState::NotStarted)));

	const bool bScenarioSuccessRunSucceededEventOk = ScenarioSuccessRunSucceededCountAfterExtract == ScenarioSuccessRunSucceededCountBeforeExtract + 1;
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessRunSucceededEvent,
		bScenarioSuccessRunSucceededEventOk,
		FString::Printf(TEXT("Success scenario RunSucceeded events %d->%d."),
			ScenarioSuccessRunSucceededCountBeforeExtract,
			ScenarioSuccessRunSucceededCountAfterExtract));

	const bool bScenarioSuccessPostSuccessMoveAccepted = SubmitPlayerMoveTo(9, 8);
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessPostSuccessMoveRejected,
		!bScenarioSuccessPostSuccessMoveAccepted,
		!bScenarioSuccessPostSuccessMoveAccepted ? TEXT("Success scenario move after RunSucceeded was rejected.") : TEXT("Success scenario move after RunSucceeded was accepted."));

	const bool bScenarioSuccessPostSuccessScanAccepted = SubmitPlayerScanAt(8, 8);
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessPostSuccessScanRejected,
		!bScenarioSuccessPostSuccessScanAccepted,
		!bScenarioSuccessPostSuccessScanAccepted ? TEXT("Success scenario scan after RunSucceeded was rejected.") : TEXT("Success scenario scan after RunSucceeded was accepted."));

	const bool bScenarioSuccessPostSuccessExtractAccepted = SubmitPlayerExtract();
	AddCheck(
		OutResults,
		GTCheck_ScenarioSuccessPostSuccessExtractRejected,
		!bScenarioSuccessPostSuccessExtractAccepted,
		!bScenarioSuccessPostSuccessExtractAccepted ? TEXT("Success scenario extract after RunSucceeded was rejected.") : TEXT("Success scenario extract after RunSucceeded was accepted."));

	FGT_DebugRunSnapshot DebugSnapshot;
	const bool bDebugStartNewRunAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugStartNewRun(42345, 10, 10, DebugSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugStartNewRunAccepted,
		bDebugStartNewRunAccepted,
		bDebugStartNewRunAccepted ? TEXT("DebugStartNewRun accepted.") : TEXT("DebugStartNewRun was rejected."));

	const bool bDebugSnapshotAfterStartOk = bDebugStartNewRunAccepted
		&& DebugSnapshot.RunState == EGT_RunState::Running
		&& DebugSnapshot.PlayerX == 0
		&& DebugSnapshot.PlayerY == 0
		&& DebugSnapshot.MapWidth == 10
		&& DebugSnapshot.MapHeight == 10;
	AddCheck(
		OutResults,
		GTCheck_DebugSnapshotAfterStart,
		bDebugSnapshotAfterStartOk,
		FString::Printf(TEXT("Debug snapshot after start: state=%d player=(%d,%d) size=%dx%d events=%d."),
			static_cast<int32>(DebugSnapshot.RunState),
			DebugSnapshot.PlayerX,
			DebugSnapshot.PlayerY,
			DebugSnapshot.MapWidth,
			DebugSnapshot.MapHeight,
			DebugSnapshot.EventCount));

	FString DebugSummaryTextAfterStart;
	const bool bDebugSummaryAfterStartAvailable = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugRunSummaryText(DebugSummaryTextAfterStart);
	const bool bDebugSummaryAfterStartUnavailable = IsValid(DebugSubsystem)
		&& !bDebugSummaryAfterStartAvailable
		&& !DebugSnapshot.bRunSummaryAvailable
		&& DebugSnapshot.Summary.Contains(TEXT("SummaryAvailable=false"))
		&& DebugSummaryTextAfterStart.Contains(TEXT("SummaryAvailable=false"))
		&& DebugSummaryTextAfterStart.Contains(TEXT("NoRunSummary"));
	AddCheck(
		OutResults,
		GTCheck_DebugSummaryAfterStartUnavailable,
		bDebugSummaryAfterStartUnavailable,
		FString::Printf(TEXT("Snapshot=%s SummaryText=%s."), *DebugSnapshot.Summary, *DebugSummaryTextAfterStart));

	FString ManualPlayStatusTextAfterStart;
	const bool bManualPlayStatusAfterStartActive = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugStatusText(ManualPlayStatusTextAfterStart);
	const bool bManualPlayStatusAfterStartOk = bManualPlayStatusAfterStartActive
		&& ManualPlayStatusTextAfterStart.Contains(TEXT("RunState=Running"))
		&& ManualPlayStatusTextAfterStart.Contains(TEXT("PlayerPosition=(0,0)"))
		&& ManualPlayStatusTextAfterStart.Contains(TEXT("RoomBaseType=Normal"))
		&& ManualPlayStatusTextAfterStart.Contains(TEXT("SummaryAvailable=false"));
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayStatusAfterStart,
		bManualPlayStatusAfterStartOk,
		ManualPlayStatusTextAfterStart);

	FString ManualPlayRoomTextAfterStart;
	const bool bManualPlayRoomAfterStartReadable = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugRoomText(ManualPlayRoomTextAfterStart);
	const bool bManualPlayRoomAfterStartOk = bManualPlayRoomAfterStartReadable
		&& ManualPlayRoomTextAfterStart.Contains(TEXT("RoomBaseType=Normal"))
		&& ManualPlayRoomTextAfterStart.Contains(TEXT("Resolved=false"));
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayRoomAfterStart,
		bManualPlayRoomAfterStartOk,
		ManualPlayRoomTextAfterStart);

	const bool bDebugMoveAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugMoveTo(1, 0, DebugSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugMoveAccepted,
		bDebugMoveAccepted,
		bDebugMoveAccepted ? TEXT("DebugMoveTo(1,0) accepted.") : TEXT("DebugMoveTo(1,0) was rejected."));

	const bool bDebugSnapshotAfterMoveOk = bDebugMoveAccepted
		&& DebugSnapshot.RunState == EGT_RunState::Running
		&& DebugSnapshot.PlayerX == 1
		&& DebugSnapshot.PlayerY == 0;
	AddCheck(
		OutResults,
		GTCheck_DebugSnapshotAfterMove,
		bDebugSnapshotAfterMoveOk,
		FString::Printf(TEXT("Debug snapshot after move: state=%d player=(%d,%d)."),
			static_cast<int32>(DebugSnapshot.RunState),
			DebugSnapshot.PlayerX,
			DebugSnapshot.PlayerY));

	const bool bDebugScanAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugScanCell(1, 1, DebugSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugScanAccepted,
		bDebugScanAccepted,
		bDebugScanAccepted ? TEXT("DebugScanCell(1,1) accepted.") : TEXT("DebugScanCell(1,1) was rejected."));

	TArray<FGT_MiniMapCellViewData> DebugMiniMapCells;
	int32 DebugMiniMapWidth = 0;
	int32 DebugMiniMapHeight = 0;
	const bool bDebugMiniMapReadable = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugMiniMapViewData(DebugMiniMapCells, DebugMiniMapWidth, DebugMiniMapHeight);
	const FGT_MiniMapCellViewData* DebugScannedCell = DebugMiniMapCells.FindByPredicate([](const FGT_MiniMapCellViewData& Cell)
	{
		return Cell.X == 1 && Cell.Y == 1;
	});
	const bool bDebugMiniMapAfterScanOk = bDebugMiniMapReadable
		&& DebugMiniMapWidth == 10
		&& DebugMiniMapHeight == 10
		&& DebugScannedCell
		&& DebugScannedCell->bScanned
		&& DebugScannedCell->DisplayedNumber == 1;
	AddCheck(
		OutResults,
		GTCheck_DebugMiniMapAfterScan,
		bDebugMiniMapAfterScanOk,
		FString::Printf(TEXT("Debug minimap after scan: readable=%s size=%dx%d scanned=%s displayed=%d."),
			bDebugMiniMapReadable ? TEXT("true") : TEXT("false"),
			DebugMiniMapWidth,
			DebugMiniMapHeight,
			DebugScannedCell && DebugScannedCell->bScanned ? TEXT("true") : TEXT("false"),
			DebugScannedCell ? DebugScannedCell->DisplayedNumber : INDEX_NONE));

	const bool bDebugExtractAwayFromExitAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugExtract(DebugSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugExtractRejectedAwayFromExit,
		!bDebugExtractAwayFromExitAccepted,
		!bDebugExtractAwayFromExitAccepted ? TEXT("DebugExtract away from exit was rejected.") : TEXT("DebugExtract away from exit was accepted."));

	const FIntPoint DebugExitPath[] = {
		FIntPoint(2, 0),
		FIntPoint(3, 0),
		FIntPoint(4, 0),
		FIntPoint(5, 0),
		FIntPoint(6, 0),
		FIntPoint(7, 0),
		FIntPoint(8, 0),
		FIntPoint(9, 0),
		FIntPoint(9, 1),
		FIntPoint(9, 2),
		FIntPoint(9, 3),
		FIntPoint(9, 4),
		FIntPoint(9, 5),
		FIntPoint(9, 6),
		FIntPoint(9, 7),
		FIntPoint(9, 8),
		FIntPoint(9, 9)
	};

	bool bDebugMoveToExitPathAccepted = IsValid(DebugSubsystem);
	for (const FIntPoint& Coord : DebugExitPath)
	{
		if (!DebugSubsystem->DebugMoveTo(Coord.X, Coord.Y, DebugSnapshot))
		{
			bDebugMoveToExitPathAccepted = false;
			break;
		}
	}
	const bool bDebugMoveToExitPathOk = bDebugMoveToExitPathAccepted
		&& DebugSnapshot.RunState == EGT_RunState::Running
		&& DebugSnapshot.PlayerX == 9
		&& DebugSnapshot.PlayerY == 9;
	AddCheck(
		OutResults,
		GTCheck_DebugMoveToExitPathAccepted,
		bDebugMoveToExitPathOk,
		FString::Printf(TEXT("Debug path to exit accepted=%s player=(%d,%d) state=%d."),
			bDebugMoveToExitPathAccepted ? TEXT("true") : TEXT("false"),
			DebugSnapshot.PlayerX,
			DebugSnapshot.PlayerY,
			static_cast<int32>(DebugSnapshot.RunState)));

	const bool bDebugExtractAcceptedAtExit = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugExtract(DebugSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugExtractAcceptedAtExit,
		bDebugExtractAcceptedAtExit,
		bDebugExtractAcceptedAtExit ? TEXT("DebugExtract at exit accepted.") : TEXT("DebugExtract at exit was rejected."));

	const bool bDebugSnapshotAfterExtractOk = bDebugExtractAcceptedAtExit
		&& DebugSnapshot.RunState == EGT_RunState::Succeeded;
	AddCheck(
		OutResults,
		GTCheck_DebugSnapshotAfterExtract,
		bDebugSnapshotAfterExtractOk,
		FString::Printf(TEXT("Debug snapshot after extract: state=%d player=(%d,%d)."),
			static_cast<int32>(DebugSnapshot.RunState),
			DebugSnapshot.PlayerX,
			DebugSnapshot.PlayerY));

	FString DebugSummaryTextAfterExtract;
	const bool bDebugSummaryAfterExtractAvailable = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugRunSummaryText(DebugSummaryTextAfterExtract);
	const bool bDebugSummaryAfterExtractOk = bDebugSummaryAfterExtractAvailable
		&& DebugSnapshot.bRunSummaryAvailable
		&& DebugSnapshot.RunSummaryOutcome == GTRunSummaryOutcome_Extracted
		&& DebugSnapshot.RunSummaryFinalPlayerX == 9
		&& DebugSnapshot.RunSummaryFinalPlayerY == 9
		&& DebugSnapshot.RunSummaryTotalEventCount > 0
		&& DebugSummaryTextAfterExtract.Contains(TEXT("SummaryAvailable=true"))
		&& DebugSummaryTextAfterExtract.Contains(TEXT("Outcome=Extracted"));
	AddCheck(
		OutResults,
		GTCheck_DebugSummaryAfterExtract,
		bDebugSummaryAfterExtractOk,
		FString::Printf(TEXT("Snapshot summary available=%s outcome=%s final=(%d,%d) events=%d text=%s."),
			DebugSnapshot.bRunSummaryAvailable ? TEXT("true") : TEXT("false"),
			*DebugSnapshot.RunSummaryOutcome.ToString(),
			DebugSnapshot.RunSummaryFinalPlayerX,
			DebugSnapshot.RunSummaryFinalPlayerY,
			DebugSnapshot.RunSummaryTotalEventCount,
			*DebugSummaryTextAfterExtract));

	FString ManualPlayStatusTextAfterExtract;
	const bool bManualPlayStatusAfterExtractReadable = IsValid(DebugSubsystem)
		&& DebugSubsystem->GetDebugStatusText(ManualPlayStatusTextAfterExtract);
	const bool bManualPlayStatusAfterExtractOk = bManualPlayStatusAfterExtractReadable
		&& ManualPlayStatusTextAfterExtract.Contains(TEXT("RunState=Succeeded"))
		&& ManualPlayStatusTextAfterExtract.Contains(TEXT("SummaryAvailable=true"))
		&& ManualPlayStatusTextAfterExtract.Contains(TEXT("SummaryOutcome=Extracted"));
	AddCheck(
		OutResults,
		GTCheck_DebugStatusShowsSummaryAfterExtract,
		bManualPlayStatusAfterExtractOk,
		ManualPlayStatusTextAfterExtract);

	const bool bDebugMoveAfterSuccessAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugMoveTo(9, 8, DebugSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugMoveRejectedAfterSuccess,
		!bDebugMoveAfterSuccessAccepted,
		!bDebugMoveAfterSuccessAccepted ? TEXT("DebugMoveTo after success was rejected.") : TEXT("DebugMoveTo after success was accepted."));

	TArray<FGT_DebugEventSummary> DebugEventSummary;
	if (IsValid(DebugSubsystem))
	{
		DebugSubsystem->GetDebugEventSummary(DebugEventSummary);
	}

	auto HasDebugEventType = [&DebugEventSummary](FName EventType) -> bool
	{
		return DebugEventSummary.ContainsByPredicate([EventType](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == EventType && Summary.Count > 0;
		});
	};

	const bool bDebugEventSummaryAvailable = HasDebugEventType(GTEventType_ActorMoved)
		&& HasDebugEventType(GTEventType_CellScanned)
		&& HasDebugEventType(GTEventType_ExitFound)
		&& HasDebugEventType(GTEventType_RunSucceeded);
	AddCheck(
		OutResults,
		GTCheck_DebugEventSummaryAvailable,
		bDebugEventSummaryAvailable,
		FString::Printf(TEXT("Debug event summary contains %d event types."), DebugEventSummary.Num()));

	FGT_DebugRunSnapshot PlaceholderSnapshot;
	const bool bDebugEventPlaceholderStartAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugStartNewRun(52345, 10, 10, PlaceholderSnapshot);
	const int32 EventRoomEnteredCountBeforePlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_EventRoomEntered) : 0;
	const int32 EventPresentedCountBeforePlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_EventPresented) : 0;
	const FIntPoint DebugEventPlaceholderPath[] = {
		FIntPoint(1, 0),
		FIntPoint(2, 0),
		FIntPoint(3, 0),
		FIntPoint(4, 0),
		FIntPoint(4, 1)
	};

	bool bDebugEventPlaceholderPathAccepted = bDebugEventPlaceholderStartAccepted;
	for (const FIntPoint& Coord : DebugEventPlaceholderPath)
	{
		if (!bDebugEventPlaceholderPathAccepted || !IsValid(DebugSubsystem))
		{
			bDebugEventPlaceholderPathAccepted = false;
			break;
		}

		if (!DebugSubsystem->DebugMoveTo(Coord.X, Coord.Y, PlaceholderSnapshot))
		{
			bDebugEventPlaceholderPathAccepted = false;
			break;
		}
	}

	const int32 EventRoomEnteredCountAfterPlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_EventRoomEntered) : 0;
	const int32 EventPresentedCountAfterPlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_EventPresented) : 0;
	const bool bDebugEventPlaceholderMoveOk = bDebugEventPlaceholderPathAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.PlayerX == 4
		&& PlaceholderSnapshot.PlayerY == 1
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event
		&& PlaceholderSnapshot.CurrentRoomContentId == GTRoomContent_EventDebugChoice01
		&& PlaceholderSnapshot.CurrentRoomRuleId == GTRoomRule_EventPresentOnly
		&& PlaceholderSnapshot.bCurrentRoomTriggered
		&& PlaceholderSnapshot.bCurrentRoomResolved;
	AddCheck(
		OutResults,
		GTCheck_DebugEventPlaceholderMoveAccepted,
		bDebugEventPlaceholderMoveOk,
		FString::Printf(TEXT("Event placeholder path accepted=%s player=(%d,%d) content=%s rule=%s."),
			bDebugEventPlaceholderPathAccepted ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.PlayerX,
			PlaceholderSnapshot.PlayerY,
			*PlaceholderSnapshot.CurrentRoomContentId.ToString(),
			*PlaceholderSnapshot.CurrentRoomRuleId.ToString()));

	const bool bDebugEventRoomUsesRegistryDefinition = bDebugEventPlaceholderMoveOk
		&& PlaceholderSnapshot.CurrentRoomContentDisplayName == GTEventContentDisplayName
		&& PlaceholderSnapshot.CurrentRoomRuleDisplayName == GTEventRuleDisplayName
		&& !PlaceholderSnapshot.CurrentRoomContentDebugDescription.IsEmpty()
		&& !PlaceholderSnapshot.CurrentRoomRuleDebugDescription.IsEmpty();
	AddCheck(
		OutResults,
		GTCheck_DebugEventRoomUsesRegistryDefinition,
		bDebugEventRoomUsesRegistryDefinition,
		FString::Printf(TEXT("Event registry display content=%s rule=%s."),
			*PlaceholderSnapshot.CurrentRoomContentDisplayName,
			*PlaceholderSnapshot.CurrentRoomRuleDisplayName));

	const bool bDebugEventPlaceholderEventsOk = EventRoomEnteredCountAfterPlaceholderMove == EventRoomEnteredCountBeforePlaceholderMove + 1
		&& EventPresentedCountAfterPlaceholderMove == EventPresentedCountBeforePlaceholderMove + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugEventPlaceholderEvents,
		bDebugEventPlaceholderEventsOk,
		FString::Printf(TEXT("Event placeholder events EventRoomEntered %d->%d EventPresented %d->%d."),
			EventRoomEnteredCountBeforePlaceholderMove,
			EventRoomEnteredCountAfterPlaceholderMove,
			EventPresentedCountBeforePlaceholderMove,
			EventPresentedCountAfterPlaceholderMove));

	const int32 EventOptionChosenCountBeforeChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChosen) : 0;
	const int32 EventResolvedCountBeforeChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventResolved) : 0;
	const bool bDebugChooseEventOptionAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugChooseEventOption(GTEventOption_DefaultContinue, PlaceholderSnapshot);
	const int32 EventOptionChosenCountAfterChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChosen) : 0;
	const int32 EventResolvedCountAfterChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventResolved) : 0;
	const bool bDebugChooseEventOptionOk = bDebugChooseEventOptionAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.PlayerX == 4
		&& PlaceholderSnapshot.PlayerY == 1
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event
		&& PlaceholderSnapshot.CurrentRoomContentId == GTRoomContent_EventDebugChoice01
		&& PlaceholderSnapshot.CurrentRoomRuleId == GTRoomRule_EventPresentOnly
		&& PlaceholderSnapshot.bCurrentRoomResolved;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionAccepted,
		bDebugChooseEventOptionOk,
		FString::Printf(TEXT("DebugChooseEventOption accepted=%s player=(%d,%d) content=%s rule=%s."),
			bDebugChooseEventOptionAccepted ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.PlayerX,
			PlaceholderSnapshot.PlayerY,
			*PlaceholderSnapshot.CurrentRoomContentId.ToString(),
			*PlaceholderSnapshot.CurrentRoomRuleId.ToString()));

	const bool bDebugChooseEventOptionEventsOk = EventOptionChosenCountAfterChoose == EventOptionChosenCountBeforeChoose + 1
		&& EventResolvedCountAfterChoose == EventResolvedCountBeforeChoose + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionEvents,
		bDebugChooseEventOptionEventsOk,
		FString::Printf(TEXT("Event interaction events EventOptionChosen %d->%d EventResolved %d->%d."),
			EventOptionChosenCountBeforeChoose,
			EventOptionChosenCountAfterChoose,
			EventResolvedCountBeforeChoose,
			EventResolvedCountAfterChoose));

	const int32 EventOptionChosenCountBeforeDefaultChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChosen) : 0;
	const int32 EventResolvedCountBeforeDefaultChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventResolved) : 0;
	const bool bDebugChooseEventOptionDefaultAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugChooseEventOption(NAME_None, PlaceholderSnapshot);
	const int32 EventOptionChosenCountAfterDefaultChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChosen) : 0;
	const int32 EventResolvedCountAfterDefaultChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventResolved) : 0;
	const bool bDebugChooseEventOptionDefaultOk = bDebugChooseEventOptionDefaultAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event
		&& EventOptionChosenCountAfterDefaultChoose == EventOptionChosenCountBeforeDefaultChoose + 1
		&& EventResolvedCountAfterDefaultChoose == EventResolvedCountBeforeDefaultChoose + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionDefaultAccepted,
		bDebugChooseEventOptionDefaultOk,
		FString::Printf(TEXT("DebugChooseEventOption default accepted=%s EventOptionChosen %d->%d EventResolved %d->%d."),
			bDebugChooseEventOptionDefaultAccepted ? TEXT("true") : TEXT("false"),
			EventOptionChosenCountBeforeDefaultChoose,
			EventOptionChosenCountAfterDefaultChoose,
			EventResolvedCountBeforeDefaultChoose,
			EventResolvedCountAfterDefaultChoose));

	const int32 EventOptionChosenCountBeforeScoutChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChosen) : 0;
	const int32 EventResolvedCountBeforeScoutChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventResolved) : 0;
	const bool bDebugChooseEventOptionScoutAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugChooseEventOption(GTEventOption_Scout, PlaceholderSnapshot);
	const int32 EventOptionChosenCountAfterScoutChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChosen) : 0;
	const int32 EventResolvedCountAfterScoutChoose = EventBus ? EventBus->CountEventsOfType(GTEventType_EventResolved) : 0;
	const bool bDebugChooseEventOptionScoutOk = bDebugChooseEventOptionScoutAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event
		&& EventOptionChosenCountAfterScoutChoose == EventOptionChosenCountBeforeScoutChoose + 1
		&& EventResolvedCountAfterScoutChoose == EventResolvedCountBeforeScoutChoose + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionScoutAccepted,
		bDebugChooseEventOptionScoutOk,
		FString::Printf(TEXT("DebugChooseEventOption Scout accepted=%s EventOptionChosen %d->%d EventResolved %d->%d."),
			bDebugChooseEventOptionScoutAccepted ? TEXT("true") : TEXT("false"),
			EventOptionChosenCountBeforeScoutChoose,
			EventOptionChosenCountAfterScoutChoose,
			EventResolvedCountBeforeScoutChoose,
			EventResolvedCountAfterScoutChoose));

	const int32 EventOptionChooseFailedCountBeforeInvalidOption = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChooseFailed) : 0;
	const bool bDebugChooseEventOptionInvalidAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugChooseEventOption(GTEventOption_Invalid, PlaceholderSnapshot);
	const int32 EventOptionChooseFailedCountAfterInvalidOption = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChooseFailed) : 0;
	const bool bDebugChooseEventOptionInvalidRejectedOk = !bDebugChooseEventOptionInvalidAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event
		&& EventOptionChooseFailedCountAfterInvalidOption == EventOptionChooseFailedCountBeforeInvalidOption + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionInvalidRejected,
		bDebugChooseEventOptionInvalidRejectedOk,
		FString::Printf(TEXT("DebugChooseEventOption invalid accepted=%s EventOptionChooseFailed %d->%d summary=%s."),
			bDebugChooseEventOptionInvalidAccepted ? TEXT("true") : TEXT("false"),
			EventOptionChooseFailedCountBeforeInvalidOption,
			EventOptionChooseFailedCountAfterInvalidOption,
			*PlaceholderSnapshot.Summary));

	const bool bDebugCombatPlaceholderStartAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugStartNewRun(62345, 10, 10, PlaceholderSnapshot);
	const int32 CombatRoomEnteredCountBeforePlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatRoomEntered) : 0;
	const int32 CombatStartedCountBeforePlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatStarted) : 0;
	const FIntPoint DebugCombatPlaceholderPath[] = {
		FIntPoint(0, 1),
		FIntPoint(0, 2),
		FIntPoint(0, 3),
		FIntPoint(0, 4),
		FIntPoint(1, 4)
	};

	bool bDebugCombatPlaceholderPathAccepted = bDebugCombatPlaceholderStartAccepted;
	for (const FIntPoint& Coord : DebugCombatPlaceholderPath)
	{
		if (!bDebugCombatPlaceholderPathAccepted || !IsValid(DebugSubsystem))
		{
			bDebugCombatPlaceholderPathAccepted = false;
			break;
		}

		if (!DebugSubsystem->DebugMoveTo(Coord.X, Coord.Y, PlaceholderSnapshot))
		{
			bDebugCombatPlaceholderPathAccepted = false;
			break;
		}
	}

	const int32 CombatRoomEnteredCountAfterPlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatRoomEntered) : 0;
	const int32 CombatStartedCountAfterPlaceholderMove = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatStarted) : 0;
	const bool bDebugCombatPlaceholderMoveOk = bDebugCombatPlaceholderPathAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.PlayerX == 1
		&& PlaceholderSnapshot.PlayerY == 4
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& PlaceholderSnapshot.CurrentRoomContentId == GTRoomContent_CombatDebugDummy01
		&& PlaceholderSnapshot.CurrentRoomRuleId == GTRoomRule_CombatStartOnly
		&& PlaceholderSnapshot.bCurrentRoomTriggered
		&& PlaceholderSnapshot.bCurrentRoomResolved;
	AddCheck(
		OutResults,
		GTCheck_DebugCombatPlaceholderMoveAccepted,
		bDebugCombatPlaceholderMoveOk,
		FString::Printf(TEXT("Combat placeholder path accepted=%s player=(%d,%d) content=%s rule=%s."),
			bDebugCombatPlaceholderPathAccepted ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.PlayerX,
			PlaceholderSnapshot.PlayerY,
			*PlaceholderSnapshot.CurrentRoomContentId.ToString(),
			*PlaceholderSnapshot.CurrentRoomRuleId.ToString()));

	const bool bDebugCombatRoomUsesRegistryDefinition = bDebugCombatPlaceholderMoveOk
		&& PlaceholderSnapshot.CurrentRoomContentDisplayName == GTCombatContentDisplayName
		&& PlaceholderSnapshot.CurrentRoomRuleDisplayName == GTCombatRuleDisplayName
		&& !PlaceholderSnapshot.CurrentRoomContentDebugDescription.IsEmpty()
		&& !PlaceholderSnapshot.CurrentRoomRuleDebugDescription.IsEmpty();
	AddCheck(
		OutResults,
		GTCheck_DebugCombatRoomUsesRegistryDefinition,
		bDebugCombatRoomUsesRegistryDefinition,
		FString::Printf(TEXT("Combat registry display content=%s rule=%s."),
			*PlaceholderSnapshot.CurrentRoomContentDisplayName,
			*PlaceholderSnapshot.CurrentRoomRuleDisplayName));

	const bool bDebugCombatStateStarted = bDebugCombatPlaceholderMoveOk
		&& PlaceholderSnapshot.bCombatActive
		&& !PlaceholderSnapshot.bCombatResolved;
	AddCheck(
		OutResults,
		GTCheck_DebugCombatStateStarted,
		bDebugCombatStateStarted,
		FString::Printf(TEXT("Combat state active=%s resolved=%s hp=%d lastResult=%s."),
			PlaceholderSnapshot.bCombatActive ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.bCombatResolved ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.DummyEnemyHp,
			*PlaceholderSnapshot.LastCombatResultId.ToString()));

	const bool bDebugCombatStateInitialHp = bDebugCombatPlaceholderMoveOk
		&& PlaceholderSnapshot.bCombatActive
		&& PlaceholderSnapshot.DummyEnemyHp == 1;
	AddCheck(
		OutResults,
		GTCheck_DebugCombatStateInitialHp,
		bDebugCombatStateInitialHp,
		FString::Printf(TEXT("Combat state initial dummy HP is %d."), PlaceholderSnapshot.DummyEnemyHp));

	const bool bDebugCombatPlaceholderEventsOk = CombatRoomEnteredCountAfterPlaceholderMove == CombatRoomEnteredCountBeforePlaceholderMove + 1
		&& CombatStartedCountAfterPlaceholderMove == CombatStartedCountBeforePlaceholderMove + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugCombatPlaceholderEvents,
		bDebugCombatPlaceholderEventsOk,
		FString::Printf(TEXT("Combat placeholder events CombatRoomEntered %d->%d CombatStarted %d->%d."),
			CombatRoomEnteredCountBeforePlaceholderMove,
			CombatRoomEnteredCountAfterPlaceholderMove,
			CombatStartedCountBeforePlaceholderMove,
			CombatStartedCountAfterPlaceholderMove));

	const int32 CombatAttackCountBeforeAttack = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatAttack) : 0;
	const int32 CombatResolvedCountBeforeAttack = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolved) : 0;
	const bool bDebugAttackAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugAttack(PlaceholderSnapshot);
	const int32 CombatAttackCountAfterAttack = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatAttack) : 0;
	const int32 CombatResolvedCountAfterAttack = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolved) : 0;
	const bool bDebugAttackAcceptedOk = bDebugAttackAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat;
	AddCheck(
		OutResults,
		GTCheck_DebugAttackAccepted,
		bDebugAttackAcceptedOk,
		FString::Printf(TEXT("DebugAttack accepted=%s player=(%d,%d) type=%d."),
			bDebugAttackAccepted ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.PlayerX,
			PlaceholderSnapshot.PlayerY,
			static_cast<int32>(PlaceholderSnapshot.CurrentRoomBaseType)));

	const bool bDebugAttackReducesDummyHp = bDebugAttackAccepted
		&& !PlaceholderSnapshot.bCombatActive
		&& PlaceholderSnapshot.bCombatResolved
		&& PlaceholderSnapshot.DummyEnemyHp == 0;
	AddCheck(
		OutResults,
		GTCheck_DebugAttackReducesDummyHp,
		bDebugAttackReducesDummyHp,
		FString::Printf(TEXT("After DebugAttack active=%s resolved=%s hp=%d lastResult=%s."),
			PlaceholderSnapshot.bCombatActive ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.bCombatResolved ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.DummyEnemyHp,
			*PlaceholderSnapshot.LastCombatResultId.ToString()));

	const bool bDebugAttackCombatResolvedEvent = CombatAttackCountAfterAttack == CombatAttackCountBeforeAttack + 1
		&& CombatResolvedCountAfterAttack == CombatResolvedCountBeforeAttack + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugAttackCombatResolvedEvent,
		bDebugAttackCombatResolvedEvent,
		FString::Printf(TEXT("CombatAttack %d->%d CombatResolved %d->%d."),
			CombatAttackCountBeforeAttack,
			CombatAttackCountAfterAttack,
			CombatResolvedCountBeforeAttack,
			CombatResolvedCountAfterAttack));

	const int32 CombatStartedCountBeforeResolvedReentry = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatStarted) : 0;
	const bool bDebugMoveAwayFromResolvedCombat = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugMoveTo(0, 4, PlaceholderSnapshot);
	const bool bDebugReenterResolvedCombat = bDebugMoveAwayFromResolvedCombat
		&& DebugSubsystem->DebugMoveTo(1, 4, PlaceholderSnapshot);
	const int32 CombatStartedCountAfterResolvedReentry = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatStarted) : 0;
	const bool bResolvedCombatRoomDoesNotRestart = bDebugReenterResolvedCombat
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& !PlaceholderSnapshot.bCombatActive
		&& PlaceholderSnapshot.bCombatResolved
		&& CombatStartedCountAfterResolvedReentry == CombatStartedCountBeforeResolvedReentry;
	AddCheck(
		OutResults,
		GTCheck_ResolvedCombatRoomDoesNotRestart,
		bResolvedCombatRoomDoesNotRestart,
		FString::Printf(TEXT("Resolved combat reentry accepted=%s CombatStarted %d->%d active=%s resolved=%s."),
			bDebugReenterResolvedCombat ? TEXT("true") : TEXT("false"),
			CombatStartedCountBeforeResolvedReentry,
			CombatStartedCountAfterResolvedReentry,
			PlaceholderSnapshot.bCombatActive ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.bCombatResolved ? TEXT("true") : TEXT("false")));

	const int32 CombatResolvedCountBeforeResolve = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolved) : 0;
	const bool bDebugResolveCombatAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugResolveCombat(GTCombatResult_Success, PlaceholderSnapshot);
	const int32 CombatResolvedCountAfterResolve = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolved) : 0;
	const bool bDebugResolveCombatOk = bDebugResolveCombatAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.PlayerX == 1
		&& PlaceholderSnapshot.PlayerY == 4
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& PlaceholderSnapshot.CurrentRoomContentId == GTRoomContent_CombatDebugDummy01
		&& PlaceholderSnapshot.CurrentRoomRuleId == GTRoomRule_CombatStartOnly
		&& PlaceholderSnapshot.bCurrentRoomResolved;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatAccepted,
		bDebugResolveCombatOk,
		FString::Printf(TEXT("DebugResolveCombat accepted=%s player=(%d,%d) content=%s rule=%s."),
			bDebugResolveCombatAccepted ? TEXT("true") : TEXT("false"),
			PlaceholderSnapshot.PlayerX,
			PlaceholderSnapshot.PlayerY,
			*PlaceholderSnapshot.CurrentRoomContentId.ToString(),
			*PlaceholderSnapshot.CurrentRoomRuleId.ToString()));

	const bool bDebugResolveCombatEventsOk = CombatResolvedCountAfterResolve == CombatResolvedCountBeforeResolve + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatEvents,
		bDebugResolveCombatEventsOk,
		FString::Printf(TEXT("CombatResolved events %d->%d."), CombatResolvedCountBeforeResolve, CombatResolvedCountAfterResolve));

	const int32 CombatResolvedCountBeforeDefaultResolve = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolved) : 0;
	const bool bDebugResolveCombatDefaultAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugResolveCombat(NAME_None, PlaceholderSnapshot);
	const int32 CombatResolvedCountAfterDefaultResolve = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolved) : 0;
	const bool bDebugResolveCombatDefaultOk = bDebugResolveCombatDefaultAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& CombatResolvedCountAfterDefaultResolve == CombatResolvedCountBeforeDefaultResolve + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatDefaultAccepted,
		bDebugResolveCombatDefaultOk,
		FString::Printf(TEXT("DebugResolveCombat default accepted=%s CombatResolved %d->%d."),
			bDebugResolveCombatDefaultAccepted ? TEXT("true") : TEXT("false"),
			CombatResolvedCountBeforeDefaultResolve,
			CombatResolvedCountAfterDefaultResolve));

	const int32 CombatRetreatedCountBeforeRetreatResolve = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatRetreated) : 0;
	const bool bDebugResolveCombatRetreatAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugResolveCombat(GTCombatResult_Retreat, PlaceholderSnapshot);
	const int32 CombatRetreatedCountAfterRetreatResolve = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatRetreated) : 0;
	const bool bDebugResolveCombatRetreatOk = bDebugResolveCombatRetreatAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& CombatRetreatedCountAfterRetreatResolve == CombatRetreatedCountBeforeRetreatResolve + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatRetreatAccepted,
		bDebugResolveCombatRetreatOk,
		FString::Printf(TEXT("DebugResolveCombat Retreat accepted=%s CombatRetreated %d->%d."),
			bDebugResolveCombatRetreatAccepted ? TEXT("true") : TEXT("false"),
			CombatRetreatedCountBeforeRetreatResolve,
			CombatRetreatedCountAfterRetreatResolve));

	const int32 CombatResolveFailedCountBeforeInvalidResult = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolveFailed) : 0;
	const bool bDebugResolveCombatInvalidAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugResolveCombat(GTCombatResult_Invalid, PlaceholderSnapshot);
	const int32 CombatResolveFailedCountAfterInvalidResult = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolveFailed) : 0;
	const bool bDebugResolveCombatInvalidRejectedOk = !bDebugResolveCombatInvalidAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& CombatResolveFailedCountAfterInvalidResult == CombatResolveFailedCountBeforeInvalidResult + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatInvalidRejected,
		bDebugResolveCombatInvalidRejectedOk,
		FString::Printf(TEXT("DebugResolveCombat invalid accepted=%s CombatResolveFailed %d->%d summary=%s."),
			bDebugResolveCombatInvalidAccepted ? TEXT("true") : TEXT("false"),
			CombatResolveFailedCountBeforeInvalidResult,
			CombatResolveFailedCountAfterInvalidResult,
			*PlaceholderSnapshot.Summary));

	const bool bDebugNegativeStartAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugStartNewRun(72345, 10, 10, PlaceholderSnapshot);
	const int32 EventOptionChooseFailedCountBeforeNegative = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChooseFailed) : 0;
	const bool bDebugChooseEventOutsideAccepted = bDebugNegativeStartAccepted
		&& DebugSubsystem->DebugChooseEventOption(GTEventOption_DefaultContinue, PlaceholderSnapshot);
	const int32 EventOptionChooseFailedCountAfterNegative = EventBus ? EventBus->CountEventsOfType(GTEventType_EventOptionChooseFailed) : 0;
	const bool bDebugChooseEventOutsideRejectedOk = bDebugNegativeStartAccepted
		&& !bDebugChooseEventOutsideAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType != EGT_RoomBaseType::Event;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionRejectedOutsideEvent,
		bDebugChooseEventOutsideRejectedOk,
		FString::Printf(TEXT("ChooseEventOption outside Event accepted=%s current type=%d summary=%s."),
			bDebugChooseEventOutsideAccepted ? TEXT("true") : TEXT("false"),
			static_cast<int32>(PlaceholderSnapshot.CurrentRoomBaseType),
			*PlaceholderSnapshot.Summary));

	const bool bDebugChooseEventFailureEventOk = EventOptionChooseFailedCountAfterNegative == EventOptionChooseFailedCountBeforeNegative + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugChooseEventOptionFailureEvent,
		bDebugChooseEventFailureEventOk,
		FString::Printf(TEXT("EventOptionChooseFailed events %d->%d."), EventOptionChooseFailedCountBeforeNegative, EventOptionChooseFailedCountAfterNegative));

	const int32 CombatResolveFailedCountBeforeNegative = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolveFailed) : 0;
	const bool bDebugResolveCombatOutsideAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugResolveCombat(GTCombatResult_Success, PlaceholderSnapshot);
	const int32 CombatResolveFailedCountAfterNegative = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatResolveFailed) : 0;
	const bool bDebugResolveCombatOutsideRejectedOk = !bDebugResolveCombatOutsideAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType != EGT_RoomBaseType::Combat;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatRejectedOutsideCombat,
		bDebugResolveCombatOutsideRejectedOk,
		FString::Printf(TEXT("ResolveCombat outside Combat accepted=%s current type=%d summary=%s."),
			bDebugResolveCombatOutsideAccepted ? TEXT("true") : TEXT("false"),
			static_cast<int32>(PlaceholderSnapshot.CurrentRoomBaseType),
			*PlaceholderSnapshot.Summary));

	const bool bDebugResolveCombatFailureEventOk = CombatResolveFailedCountAfterNegative == CombatResolveFailedCountBeforeNegative + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugResolveCombatFailureEvent,
		bDebugResolveCombatFailureEventOk,
		FString::Printf(TEXT("CombatResolveFailed events %d->%d."), CombatResolveFailedCountBeforeNegative, CombatResolveFailedCountAfterNegative));

	const int32 CombatAttackFailedCountBeforeNegative = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatAttackFailed) : 0;
	const bool bDebugAttackOutsideAccepted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugAttack(PlaceholderSnapshot);
	const int32 CombatAttackFailedCountAfterNegative = EventBus ? EventBus->CountEventsOfType(GTEventType_CombatAttackFailed) : 0;
	const bool bDebugAttackOutsideRejectedOk = !bDebugAttackOutsideAccepted
		&& PlaceholderSnapshot.RunState == EGT_RunState::Running
		&& PlaceholderSnapshot.CurrentRoomBaseType != EGT_RoomBaseType::Combat;
	AddCheck(
		OutResults,
		GTCheck_DebugAttackRejectedOutsideCombat,
		bDebugAttackOutsideRejectedOk,
		FString::Printf(TEXT("Attack outside Combat accepted=%s current type=%d summary=%s."),
			bDebugAttackOutsideAccepted ? TEXT("true") : TEXT("false"),
			static_cast<int32>(PlaceholderSnapshot.CurrentRoomBaseType),
			*PlaceholderSnapshot.Summary));

	const bool bDebugAttackFailureEventOk = CombatAttackFailedCountAfterNegative == CombatAttackFailedCountBeforeNegative + 1;
	AddCheck(
		OutResults,
		GTCheck_DebugAttackFailureEvent,
		bDebugAttackFailureEventOk,
		FString::Printf(TEXT("CombatAttackFailed events %d->%d."), CombatAttackFailedCountBeforeNegative, CombatAttackFailedCountAfterNegative));

	TArray<FString> ManualPlayDemoLogLines;
	FGT_DebugRunSnapshot ManualPlayDemoSnapshot;
	const bool bManualPlayRunDemoCompleted = IsValid(DebugSubsystem)
		&& DebugSubsystem->DebugRunDemo(ManualPlayDemoLogLines, ManualPlayDemoSnapshot);
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayRunDemoCompleted,
		bManualPlayRunDemoCompleted,
		FString::Printf(TEXT("RunDemo completed=%s logLines=%d summary=%s."),
			bManualPlayRunDemoCompleted ? TEXT("true") : TEXT("false"),
			ManualPlayDemoLogLines.Num(),
			*ManualPlayDemoSnapshot.Summary));

	const bool bManualPlayRunDemoSummaryOk = bManualPlayRunDemoCompleted
		&& ManualPlayDemoSnapshot.RunState == EGT_RunState::Succeeded
		&& ManualPlayDemoSnapshot.bRunSummaryAvailable
		&& ManualPlayDemoSnapshot.RunSummaryOutcome == GTRunSummaryOutcome_Extracted
		&& ManualPlayDemoLogLines.ContainsByPredicate([](const FString& Line)
		{
			return Line.Contains(TEXT("gt.Summary: SummaryAvailable=true"))
				&& Line.Contains(TEXT("Outcome=Extracted"));
		});
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayRunDemoSummary,
		bManualPlayRunDemoSummaryOk,
		FString::Printf(TEXT("RunDemo summary available=%s outcome=%s logLines=%d."),
			ManualPlayDemoSnapshot.bRunSummaryAvailable ? TEXT("true") : TEXT("false"),
			*ManualPlayDemoSnapshot.RunSummaryOutcome.ToString(),
			ManualPlayDemoLogLines.Num()));

	TArray<FGT_DebugEventSummary> ManualPlayDemoEventSummary;
	if (IsValid(DebugSubsystem))
	{
		DebugSubsystem->GetDebugEventSummary(ManualPlayDemoEventSummary);
	}

	const bool bManualPlayRunDemoEventsOk = ManualPlayDemoEventSummary.ContainsByPredicate([](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == GTEventType_EventResolved && Summary.Count > 0;
		})
		&& ManualPlayDemoEventSummary.ContainsByPredicate([](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == GTEventType_CombatResolved && Summary.Count > 0;
		})
		&& ManualPlayDemoEventSummary.ContainsByPredicate([](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == GTEventType_CombatAttack && Summary.Count > 0;
		})
		&& ManualPlayDemoEventSummary.ContainsByPredicate([](const FGT_DebugEventSummary& Summary)
		{
			return Summary.EventType == GTEventType_RunSucceeded && Summary.Count > 0;
		});
	AddCheck(
		OutResults,
		GTCheck_DebugManualPlayRunDemoEvents,
		bManualPlayRunDemoEventsOk,
		FString::Printf(TEXT("RunDemo event summary contains %d event types."), ManualPlayDemoEventSummary.Num()));

	UGT_RunContext* ProtocolDrainRunContext = NewObject<UGT_RunContext>(this);
	UGT_EventBus* ProtocolDrainEventBus = NewObject<UGT_EventBus>(this);
	UGT_CommandProcessor* ProtocolDrainProcessor = NewObject<UGT_CommandProcessor>(this);
	ProtocolDrainRunContext->InitializeRunStandard(82345, EGT_Difficulty::Easy);
	ProtocolDrainProcessor->Initialize(ProtocolDrainRunContext, ProtocolDrainEventBus, ContentRegistry);
	ProtocolDrainRunContext->CheatSetPlayerHp(1);
	ProtocolDrainRunContext->AddProtocolPressure(ProtocolDrainRunContext->GetProtocolMaxPressure());

	int32 ProtocolDrainStartX = 0;
	int32 ProtocolDrainStartY = 0;
	ProtocolDrainRunContext->TryGetPlayerPosition(ProtocolDrainStartX, ProtocolDrainStartY);
	const TArray<FIntPoint> ProtocolDrainMoveCandidates = {
		FIntPoint(ProtocolDrainStartX + 1, ProtocolDrainStartY),
		FIntPoint(ProtocolDrainStartX - 1, ProtocolDrainStartY),
		FIntPoint(ProtocolDrainStartX, ProtocolDrainStartY + 1),
		FIntPoint(ProtocolDrainStartX, ProtocolDrainStartY - 1)
	};
	FIntPoint ProtocolDrainTarget = FIntPoint::ZeroValue;
	bool bFoundProtocolDrainTarget = false;
	for (const FIntPoint& Candidate : ProtocolDrainMoveCandidates)
	{
		if (ProtocolDrainRunContext->IsValidMapCoord(Candidate.X, Candidate.Y))
		{
			ProtocolDrainTarget = Candidate;
			bFoundProtocolDrainTarget = true;
			break;
		}
	}

	FGT_Command ProtocolDrainMove;
	ProtocolDrainMove.CommandType = GTCommandType_Move;
	ProtocolDrainMove.SourceActorId = GTActorId_Player;
	ProtocolDrainMove.TargetActorId = GTActorId_Player;
	ProtocolDrainMove.TargetX = ProtocolDrainTarget.X;
	ProtocolDrainMove.TargetY = ProtocolDrainTarget.Y;
	const bool bProtocolDrainMoveAccepted = bFoundProtocolDrainTarget
		&& ProtocolDrainProcessor->ProcessCommand(ProtocolDrainMove);
	const int32 ProtocolDrainActorMovedCount = ProtocolDrainEventBus->CountEventsOfType(GTEventType_ActorMoved);
	const int32 ProtocolDrainRoomEnteredCount = ProtocolDrainEventBus->CountEventsOfType(GTEventType_RoomEntered);
	const int32 ProtocolDrainRunFailedCount = ProtocolDrainEventBus->CountEventsOfType(GTEventType_RunFailed);
	const bool bProtocolDrainStopsRoomResolution = bProtocolDrainMoveAccepted
		&& ProtocolDrainRunContext->GetRunState() == EGT_RunState::Failed
		&& ProtocolDrainRunFailedCount == 1
		&& ProtocolDrainActorMovedCount == 0
		&& ProtocolDrainRoomEnteredCount == 0;
	AddCheck(
		OutResults,
		GTCheck_ProtocolDrainStopsRoomResolution,
		bProtocolDrainStopsRoomResolution,
		FString::Printf(TEXT("Protocol drain move accepted=%s state=%d RunFailed=%d ActorMoved=%d RoomEntered=%d."),
			bProtocolDrainMoveAccepted ? TEXT("true") : TEXT("false"),
			static_cast<int32>(ProtocolDrainRunContext->GetRunState()),
			ProtocolDrainRunFailedCount,
			ProtocolDrainActorMovedCount,
			ProtocolDrainRoomEnteredCount));

	RunSubsystem->StartNewRun(92345, 10, 10);
	FGT_Command AbandonSetupMove;
	AbandonSetupMove.CommandType = GTCommandType_Move;
	AbandonSetupMove.SourceActorId = GTActorId_Player;
	AbandonSetupMove.TargetActorId = GTActorId_Player;
	AbandonSetupMove.TargetX = 1;
	AbandonSetupMove.TargetY = 0;
	RunSubsystem->SubmitCommand(AbandonSetupMove);
	RunSubsystem->AbandonRun();
	const UGT_QueryFacade* AbandonQueryFacade = RunSubsystem->GetQueryFacade();
	const UGT_EventBus* AbandonEventBus = RunSubsystem->GetEventBus();
	const UGT_CommandBus* AbandonCommandBus = RunSubsystem->GetCommandBus();
	const bool bAbandonRunClearsRuntimeState = RunSubsystem->GetCurrentRunContext() == nullptr
		&& AbandonQueryFacade
		&& AbandonQueryFacade->GetRunState() == EGT_RunState::NotStarted
		&& AbandonEventBus
		&& AbandonEventBus->GetEventCount() == 0
		&& AbandonCommandBus
		&& AbandonCommandBus->GetPendingCommandCount() == 0;
	AddCheck(
		OutResults,
		GTCheck_AbandonRunClearsRuntimeState,
		bAbandonRunClearsRuntimeState,
		FString::Printf(TEXT("After abandon context=%s state=%d events=%d commands=%d."),
			RunSubsystem->GetCurrentRunContext() ? TEXT("valid") : TEXT("null"),
			AbandonQueryFacade ? static_cast<int32>(AbandonQueryFacade->GetRunState()) : INDEX_NONE,
			AbandonEventBus ? AbandonEventBus->GetEventCount() : INDEX_NONE,
			AbandonCommandBus ? AbandonCommandBus->GetPendingCommandCount() : INDEX_NONE));

	for (const FGT_RuntimeSmokeCheckResult& Result : OutResults)
	{
		if (!Result.bPassed)
		{
			return false;
		}
	}

	return true;
}

void UGT_RuntimeSmokeValidator::AddCheck(TArray<FGT_RuntimeSmokeCheckResult>& OutResults, FName CheckName, bool bPassed, const FString& Message)
{
	FGT_RuntimeSmokeCheckResult Result;
	Result.bPassed = bPassed;
	Result.CheckName = CheckName;
	Result.Message = Message;
	OutResults.Add(Result);
}
