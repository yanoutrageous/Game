# Graytail Release Entry and Transactional Save Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make packaged builds enter the real main menu automatically, remove player-accessible Shipping cheats, and guarantee that run start and settlement either persist completely or leave both memory and disk unchanged.

**Architecture:** `AGT_GameMode` and `AGT_PlayerController` become the single production boot path. `UGT_MetaProgressSubsystem` owns an immutable-on-failure persistent state and delegates slot I/O to a replaceable repository. Run start and settlement build one candidate state, persist it through main/backup slots, then publish it to memory. An active-run escrow persisted before `UGT_RunContext` creation makes abnormal exits obey abandonment rules on the next launch.

**Tech Stack:** Unreal Engine 5.7 C++, UMG, `USaveGame`, `UGameInstanceSubsystem`, commandlet smoke tests, PowerShell, BuildCookRun.

## Global Constraints

- Work on `codex/release-entry-save`, based on commit `15d344c`.
- Do not change combat timing, input remapping, localization, or additional JSON fields in this PR.
- Keep `GraytailMeta` as the official slot and `GraytailMeta.debug.json` as a non-Shipping write-only mirror.
- Never load state from the debug JSON mirror.
- A failed write must not replace in-memory state or report success to UI.
- Backup-write failure must stop the commit before the official slot is overwritten.
- Unknown future save versions and two corrupt slots must block progression instead of silently creating a fresh save.
- Run the focused smoke after every task and the full release checks before publishing the draft PR.

---

## File Structure

### New files

- `UE/Graytail/Source/Graytail/App/GT_GameMode.h`
- `UE/Graytail/Source/Graytail/App/GT_GameMode.cpp`
- `UE/Graytail/Source/Graytail/App/GT_PlayerController.h`
- `UE/Graytail/Source/Graytail/App/GT_PlayerController.cpp`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaPersistenceTypes.h`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSaveRepository.h`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSaveRepository.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_MetaPersistenceSmokeValidator.h`
- `UE/Graytail/Source/Graytail/Debug/GT_MetaPersistenceSmokeValidator.cpp`
- `docs/pr/release-entry-save.md`

### Modified files

- `UE/Graytail/Config/DefaultEngine.ini`
- `UE/Graytail/Source/Graytail/Save/GT_MetaSaveGame.h`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaTypes.h`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSettlement.h`
- `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSettlement.cpp`
- `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.h`
- `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.h`
- `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_MainMenuWidget.h`
- `UE/Graytail/Source/Graytail/UI/GT_MainMenuWidget.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.h`
- `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_PauseMenuWidget.h`
- `UE/Graytail/Source/Graytail/UI/GT_PauseMenuWidget.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_DebugSubsystem.h`
- `UE/Graytail/Source/Graytail/Debug/GT_DebugSubsystem.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_EditorManualPlayCommands.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_MetaDebugCommands.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeRunner.cpp`

## Task 1: Add the Production Boot Path

**Files:**
- Create: `UE/Graytail/Source/Graytail/App/GT_GameMode.h`
- Create: `UE/Graytail/Source/Graytail/App/GT_GameMode.cpp`
- Create: `UE/Graytail/Source/Graytail/App/GT_PlayerController.h`
- Create: `UE/Graytail/Source/Graytail/App/GT_PlayerController.cpp`
- Modify: `UE/Graytail/Config/DefaultEngine.ini`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Write a failing boot-contract smoke check**

Add checks that assert:

```cpp
AGT_GameMode::StaticClass()->GetDefaultObject<AGT_GameMode>()
	->PlayerControllerClass == AGT_PlayerController::StaticClass();
```

Add a second check around a pure helper on the controller:

```cpp
bool AGT_PlayerController::ShouldCreateHud(
	bool bIsLocalController,
	bool bHasExistingHud);
```

Expected cases are `(true, false) -> true`, `(true, true) -> false`, and `(false, false) -> false`.

- [ ] **Step 2: Run the smoke and confirm the new contract fails**

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat" GraytailEditor Win64 Development "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: compilation fails because the new app classes do not exist yet.

- [ ] **Step 3: Implement the GameMode and idempotent controller**

`AGT_GameMode` sets `PlayerControllerClass` in its constructor. `AGT_PlayerController::BeginPlay` calls one idempotent method:

```cpp
void AGT_PlayerController::EnsureGameHud()
{
	if (!ShouldCreateHud(IsLocalController(), IsValid(GameHud)))
	{
		return;
	}

	GameHud = CreateWidget<UGT_GameHudWidget>(this, UGT_GameHudWidget::StaticClass());
	GameHud->AddToViewport();
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly().SetWidgetToFocus(GameHud->TakeWidget()));
}
```

Store `GameHud` as `UPROPERTY(Transient)`. Configure:

```ini
[/Script/EngineSettings.GameMapsSettings]
GlobalDefaultGameMode=/Script/Graytail.GT_GameMode
```

Keep `gt.HUD` only as a non-Shipping developer convenience.

- [ ] **Step 4: Add stable HUD queries used by the startup probe**

Expose non-debug production queries:

```cpp
bool UGT_GameHudWidget::IsMainMenuVisible() const;
bool UGT_GameHudWidget::TryStartRunFromMenu(EGT_Difficulty Difficulty);
```

Route both real menu selection and the later startup probe through `TryStartRunFromMenu`; do not duplicate run-start logic in a test hook.

- [ ] **Step 5: Build and run the focused smoke**

Run the editor build, then:

```powershell
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -run=GT_RuntimeSmokeRunner -unattended -nop4 -nosplash -nullrhi -log
```

Expected: all prior checks and the new boot-contract checks pass.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Config/DefaultEngine.ini UE/Graytail/Source/Graytail/App UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.* UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp
git commit -m "feat: add packaged game entry point"
```

## Task 2: Compile Cheats Out of Shipping

**Files:**
- Modify: `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.cpp`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_PauseMenuWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_PauseMenuWidget.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_DebugSubsystem.h`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_DebugSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_EditorManualPlayCommands.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_MetaDebugCommands.cpp`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Add a static Shipping-boundary test**

Add a repository script later reused by CI:

```powershell
$forbidden = @(
  'HandleToggleCheat',
  'GMGrant',
  'GMReset',
  'gt.HUD',
  'FAutoConsoleCommand'
)
```

For this task, first add a smoke check for:

```cpp
static constexpr bool GT_CheatsCompiled =
#if UE_BUILD_SHIPPING
	false;
#else
	true;
#endif
```

Expected: Development reports compiled; Shipping must report absent through a packaged probe, not merely disabled.

- [ ] **Step 2: Put declarations and registrations behind compile guards**

Guard cheat-only members, widget construction blocks, debug mutations, and command registration with:

```cpp
#if !UE_BUILD_SHIPPING
// Developer-only implementation.
#endif
```

Do not guard normal read-only run queries needed by UI. Move any such query out of `UGT_DebugSubsystem` before removing the Shipping dependency.

- [ ] **Step 3: Verify both configurations compile**

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat" GraytailEditor Win64 Development "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -WaitMutex -NoHotReloadFromIDE
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat" Graytail Win64 Shipping "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -WaitMutex
```

Expected: both exit `0`; Shipping compilation has no references from settings or pause UI to cheat-only symbols.

- [ ] **Step 4: Commit**

```powershell
git add UE/Graytail/Source/Graytail/UI UE/Graytail/Source/Graytail/Debug
git commit -m "fix: exclude developer cheats from shipping"
```

## Task 3: Introduce Save Version 2 and Explicit Persistence Results

**Files:**
- Create: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaPersistenceTypes.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaTypes.h`
- Modify: `UE/Graytail/Source/Graytail/Save/GT_MetaSaveGame.h`
- Create: `UE/Graytail/Source/Graytail/Debug/GT_MetaPersistenceSmokeValidator.h`
- Create: `UE/Graytail/Source/Graytail/Debug/GT_MetaPersistenceSmokeValidator.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Define failure-first tests**

Add focused cases for:

- no slot creates a valid fresh state;
- version `1` migrates to version `2` with inactive escrow;
- version `2` round-trips all fields;
- version `3` returns `UnsupportedVersion`;
- negative counts, duplicate IDs, unknown IDs, and equipped-but-not-owned IDs are sanitized or rejected according to the design;
- unsafe corruption never becomes a fresh state.

Use unique temporary slot names and delete them in a scope guard.

- [ ] **Step 2: Define the persistent types**

Use:

```cpp
USTRUCT()
struct FGT_ActiveRunEscrow
{
	GENERATED_BODY()

	UPROPERTY() bool bActive = false;
	UPROPERTY() FGuid RunId;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() EGT_Difficulty Difficulty = EGT_Difficulty::Standard;
	UPROPERTY() TArray<FName> EquippedItemIds;
	UPROPERTY() TMap<FName, int32> ConsumedConsumables;
	UPROPERTY() FDateTime StartedAtUtc;
};

UENUM()
enum class EGT_MetaPersistenceStatus : uint8
{
	Ready,
	Fresh,
	RecoveredBackup,
	RecoveryWritePending,
	Corrupt,
	UnsupportedVersion,
	WriteFailed
};

USTRUCT(BlueprintType)
struct FGT_MetaOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly) FName ErrorCode = NAME_None;
	UPROPERTY(BlueprintReadOnly) FText Message;
};
```

Add `FGT_ActiveRunEscrow ActiveRun` to `FGT_MetaProgressState`. Set:

```cpp
static constexpr int32 CurrentSaveVersion = 2;
UPROPERTY() int32 SaveVersion = CurrentSaveVersion;
```

- [ ] **Step 3: Implement version migration and strict validation as pure functions**

Add pure helpers:

```cpp
bool GT_MigrateMetaSave(
	int32 SourceVersion,
	FGT_MetaProgressState& InOutState,
	FString& OutError);

bool GT_ValidateAndSanitizeMetaState(
	FGT_MetaProgressState& InOutState,
	FString& OutError);
```

Migration accepts only versions `1` and `2`. Validation must use loaded game-data catalogs for cross references and deterministic ordering after duplicate removal.

- [ ] **Step 4: Run focused smoke**

Expected: all migration and validation cases pass without invoking UI or creating a run.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaPersistenceTypes.h UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaTypes.h UE/Graytail/Source/Graytail/Save/GT_MetaSaveGame.h UE/Graytail/Source/Graytail/Debug
git commit -m "feat: version and validate meta saves"
```

## Task 4: Build a Main/Backup Save Repository

**Files:**
- Create: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSaveRepository.h`
- Create: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSaveRepository.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_MetaPersistenceSmokeValidator.cpp`

- [ ] **Step 1: Write repository failure-injection tests**

Define:

```cpp
class IGT_MetaSaveBackend
{
public:
	virtual ~IGT_MetaSaveBackend() = default;
	virtual bool Exists(const FString& Slot, int32 UserIndex) const = 0;
	virtual USaveGame* Load(const FString& Slot, int32 UserIndex) const = 0;
	virtual bool Save(USaveGame& Object, const FString& Slot, int32 UserIndex) = 0;
};
```

Use an in-memory fake backend to prove:

- backup failure prevents main write;
- main failure preserves the previously loaded state;
- corrupt main loads valid backup;
- corrupt main plus corrupt backup returns `Corrupt`;
- backup recovery that cannot rewrite main returns `RecoveryWritePending`.

- [ ] **Step 2: Implement the production backend and repository**

`FGT_EngineMetaSaveBackend` wraps `UGameplayStatics`. `FGT_MetaSaveRepository` owns slot naming:

```cpp
FString MainSlot() const;    // GraytailMeta or command-line override
FString BackupSlot() const;  // MainSlot + "Backup"

FGT_MetaLoadResult Load();
FGT_MetaOperationResult Commit(const FGT_MetaProgressState& Candidate);
```

Commit order:

1. If a valid main save exists, write that state to backup.
2. Abort immediately if backup write fails.
3. Write candidate to main.
4. Write the debug JSON mirror only after main succeeds and only outside Shipping.

- [ ] **Step 3: Make subsystem state replacement transactional**

Replace public `Save()`/`Load()` behavior with:

```cpp
FGT_MetaOperationResult CommitCandidate(FGT_MetaProgressState Candidate);
const FGT_MetaOperationResult& GetLastPersistenceResult() const;
bool CanMutateProgress() const;
FGT_MetaOperationResult ResetCorruptSaveAndCreateFresh();
```

`CommitCandidate` validates, persists, and only then moves `Candidate` into `State`. Expose a backend override only under `WITH_DEV_AUTOMATION_TESTS` so the commandlet can inject failures; it must not exist in Shipping.

`ResetCorruptSaveAndCreateFresh` is valid only after an explicit UI confirmation while status is `Corrupt` or `UnsupportedVersion`. It writes a valid empty version-2 state to the main slot before changing memory; a failed reset leaves the blocking status and old in-memory state intact.

- [ ] **Step 4: Convert each public mutation**

For each mutation, follow:

```cpp
FGT_MetaProgressState Candidate = State;
// Validate request and mutate Candidate only.
return CommitCandidate(MoveTemp(Candidate));
```

Update call sites so success UI is shown only when `bSuccess` is true. Preserve existing business error codes such as `not_enough_gold`; add persistence errors such as `save_write_failed`.

- [ ] **Step 5: Verify failure leaves memory unchanged**

For every user-facing category, test at least one injected write failure:

- gold purchase;
- equipment toggle;
- talent unlock;
- consumable loadout;
- warehouse sale;
- GM reset in Development.

Expected: state before and after the failed call compares equal.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Meta UE/Graytail/Source/Graytail/Debug UE/Graytail/Source/Graytail/UI
git commit -m "refactor: make meta persistence transactional"
```

## Task 5: Make Run Escrow and Settlement Atomic

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSettlement.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSettlement.cpp`
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.h`
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.cpp`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_MetaPersistenceSmokeValidator.cpp`

- [ ] **Step 1: Add failing lifecycle tests**

Test these state transitions:

```text
Ready -> BeginRun commit -> EscrowActive -> Create RunContext
EscrowActive -> Extraction candidate -> Commit -> Ready
EscrowActive -> Failure candidate -> Commit -> Ready
EscrowActive -> Startup recovery -> Abandon candidate -> Commit -> Ready
```

Also prove:

- begin-run write failure creates no `UGT_RunContext`;
- settlement write failure keeps the run and escrow active;
- repeated settlement retry grants rewards exactly once;
- abnormal-exit recovery removes escrow equipment and does not refund consumed items.

- [ ] **Step 2: Add aggregate meta operations**

Use explicit APIs:

```cpp
FGT_MetaOperationResult BeginRunEscrow(
	int32 Seed,
	EGT_Difficulty Difficulty,
	FGuid& OutRunId,
	TMap<FName, int32>& OutConsumedConsumables);

FGT_MetaOperationResult CommitExtraction(
	const FGT_ExtractionReward& Reward,
	const TMap<FName, int32>& ReturnedConsumables);

FGT_MetaOperationResult CommitFailure(
	const FGT_FailureSettlement& Settlement);

FGT_MetaOperationResult RecoverInterruptedRun();
```

These methods perform one candidate mutation and one disk commit each. Remove nested saving from `AddConsumable`, `AddGold`, `LoseEquippedItemsOnFailure`, and settlement helpers.

`BeginRunEscrow` also increments `TotalRuns` in the same candidate; remove the separate saving behavior from `RecordRun`. Abandonment must be represented as one settlement reason and must not publish a normal failure that triggers a second commit.

- [ ] **Step 3: Reorder `StartNewRunStandard`**

Required order:

1. Verify game data and persistence readiness.
2. Ask meta subsystem to persist escrow and consume loadout.
3. Return `nullptr` if escrow commit fails.
4. Create and initialize `UGT_RunContext`.
5. Apply the already committed consumed items and bonuses.
6. Publish `RunStarted`.

If context initialization fails after escrow commit, immediately run the persisted abandonment recovery path; never silently clear escrow in memory.

- [ ] **Step 4: Make settlement retryable**

Replace `bRunSettled` with:

```cpp
enum class EGT_RunSettlementState : uint8
{
	NotStarted,
	PendingCommit,
	Committed
};
```

Cache the immutable settlement input while `PendingCommit`. Only transition to `Committed` and clear the run after `CommitExtraction` or `CommitFailure` succeeds. Add:

```cpp
FGT_MetaOperationResult RetryPendingSettlement();
```

- [ ] **Step 5: Recover interrupted runs during meta initialization**

In `Initialize`, call `Collection.InitializeDependency<UGT_GameDataSubsystem>()` before loading or validating meta state. After a valid save loads, call `RecoverInterruptedRun()` when `State.ActiveRun.bActive`. Store a one-shot notification:

```cpp
FText ConsumeStartupNotice();
```

If recovery cannot be persisted, set persistence status to blocking and do not allow a new run.

- [ ] **Step 6: Run lifecycle smoke**

Expected: all lifecycle cases pass and the existing smoke count increases without regressions.

- [ ] **Step 7: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Meta UE/Graytail/Source/Graytail/Core UE/Graytail/Source/Graytail/Debug
git commit -m "fix: persist run lifecycle atomically"
```

## Task 6: Surface Save Recovery and Retry in UI

**Files:**
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_MainMenuWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_MainMenuWidget.cpp`

- [ ] **Step 1: Add a UI-state smoke contract**

Build a pure presentation mapper:

```cpp
FGT_MetaUiState GT_BuildMetaUiState(
	EGT_MetaPersistenceStatus Status,
	const FGT_MetaOperationResult& LastResult,
	const FText& StartupNotice);
```

Test blocking versus recoverable states without rendering Slate.

- [ ] **Step 2: Add main-menu error presentation**

Show:

- recovered-from-backup notice;
- interrupted-run abandonment notice;
- unsupported-version blocker;
- dual-corruption blocker with explicit reset confirmation;
- recovery-write-pending blocker with retry;
- begin-run save failure.

Do not show a playable difficulty button while `CanMutateProgress()` is false.

- [ ] **Step 3: Add settlement-save retry presentation**

When settlement is `PendingCommit`, keep the result panel open and disable:

- return to title;
- start new run;
- quit without warning.

Show a retry button wired to `RetryPendingSettlement()`. Only update permanent reward wording after commit succeeds.

- [ ] **Step 4: Route Quit through abandonment**

Before `QuitGame`, if a standard run is active:

```cpp
const FGT_MetaOperationResult Result = RunSubsystem->AbandonRun();
if (!Result.bSuccess)
{
	ShowPersistenceError(Result);
	return;
}
```

Window-close and crashes still rely on escrow recovery; this callback is only the graceful path.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/UI
git commit -m "feat: surface save recovery and retry states"
```

## Task 7: Add a Real Packaged Startup Probe

**Files:**
- Modify: `UE/Graytail/Source/Graytail/App/GT_PlayerController.h`
- Modify: `UE/Graytail/Source/Graytail/App/GT_PlayerController.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeRunner.cpp`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp`

- [ ] **Step 1: Define structured probe output**

Development-only probe logs:

```text
GRAYTAIL_STARTUP|Check=HudCreated|Result=Pass
GRAYTAIL_STARTUP|Check=MainMenuVisible|Result=Pass
GRAYTAIL_STARTUP|Check=RunStarted|Result=Pass
GRAYTAIL_STARTUP|Overall=Pass
```

Any failure logs `Overall=Fail` and exits with a non-zero process code.

- [ ] **Step 2: Implement `-GraytailStartupProbe`**

In non-Shipping builds, after `EnsureGameHud()`:

1. verify the HUD instance exists;
2. verify the main menu is visible;
3. call the same `TryStartRunFromMenu` path used by real UI;
4. verify `UGT_RunSubsystem::GetCurrentRunContext()` is valid;
5. abandon the temporary run through the normal persisted path;
6. request process exit.

Use a unique `-GraytailSaveSlot=` supplied by the caller. Do not hard-code or touch `GraytailMeta`.

- [ ] **Step 3: Package Development and run the probe**

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="D:\CODE\NetEase Training\.worktrees\Game\json-game-data\Artifacts\PR1-Development"
```

Launch the packaged executable with `-GraytailStartupProbe -GraytailSaveSlot=GraytailStartupProbe_<guid> -unattended -log`.

Expected: `Overall=Pass`, exit code `0`, and no official `GraytailMeta` slot modification.

- [ ] **Step 4: Launch the packaged build visibly**

Run without probe and verify the main menu appears without opening the console or typing `gt.HUD`.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/App UE/Graytail/Source/Graytail/UI UE/Graytail/Source/Graytail/Debug
git commit -m "test: probe packaged startup flow"
```

## Task 8: Final Verification and Draft PR

- [ ] **Step 1: Run the complete editor smoke**

Run the editor build and commandlet smoke. Expected: exit `0`, `Fail=0`, and every persistence failure-injection check passes.

- [ ] **Step 2: Package and probe Development**

Expected: startup probe passes and a normal visible launch reaches the main menu.

- [ ] **Step 3: Package and inspect Shipping**

Run BuildCookRun with `-clientconfig=Shipping`. Expected:

- process remains alive through startup;
- main menu is visible;
- settings and pause menus contain no cheat controls;
- startup-probe and cheat console strings are unavailable to the player.

- [ ] **Step 4: Verify repository cleanliness**

```powershell
git status --short
git diff --check
rg -n "TODO|TBD|PLACEHOLDER" UE/Graytail/Source/Graytail UE/Graytail/Config
```

Expected: clean worktree after final commit, no whitespace errors, and no newly introduced placeholders.

- [ ] **Step 5: Push and open a draft PR**

Create `docs/pr/release-entry-save.md` with user-visible behavior, save migration/recovery semantics, exact test totals, packaged probe evidence, and the fact that PR 2 stacks on this branch. Commit the description before pushing.

```powershell
git add docs/pr/release-entry-save.md
git commit -m "docs: describe release entry and save changes"
git push -u origin codex/release-entry-save
gh pr create --draft --base codex/json-game-data --head codex/release-entry-save --title "[codex] secure packaged entry and run persistence" --body-file docs/pr/release-entry-save.md
```

The PR body must include user-visible behavior, save migration/recovery semantics, exact test results, packaged probe evidence, and the fact that PR 2 stacks on this branch.
