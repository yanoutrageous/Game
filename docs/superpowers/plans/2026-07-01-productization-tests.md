# Graytail Productization and Release Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish gameplay-value data ownership, add keyboard and mouse rebinding with truthful UI prompts, move player-visible Chinese into Unreal localization, and provide repeatable local and CI release validation.

**Architecture:** The startup game-data snapshot owns every value that changes gameplay outcomes; the combat simulator receives copied effective values and remains immutable during an encounter. `UGT_InputSettingsSubsystem` owns action-to-key bindings persisted in user settings, while `AGT_PlayerController` translates physical keys into semantic actions. Player-visible source text uses `FText` and Unreal gatherable localization macros. One PowerShell release entry point composes build, smoke, packaging, startup, Pak-content, and save-isolation checks; hosted CI runs the engine-independent subset.

**Tech Stack:** Unreal Engine 5.7 C++, JSON/JsonUtilities, InputCore, UMG, Unreal localization commandlets, PowerShell 7, GitHub Actions.

## Global Constraints

- Create `codex/productization-tests` from the final `codex/combat-simulation` commit.
- Keep JSON `schemaVersion` at `1`; add required fields to the current schema and fail old incomplete files with field-specific errors.
- Runtime talents, equipment, and consumables modify effective player values without reloading JSON.
- Editing JSON takes effect after restarting the game; no hot reload is required.
- Keep pure color, layout, pixel size, easing, and cosmetic animation duration in presentation code.
- Move any literal that changes HP, damage, movement, timing windows, probabilities, rewards, limits, or outcomes to JSON.
- First input-remapping release supports keyboard and mouse only; retain current gamepad defaults but do not advertise full gamepad rebinding.
- Chinese is the only shipped language in this PR, but all player-visible text must become gatherable.
- Hosted CI must not pretend to compile Unreal without an Unreal-capable runner.

---

## File Structure

### New files

- `UE/Graytail/Source/Graytail/Core/GT_InputTypes.h`
- `UE/Graytail/Source/Graytail/Core/GT_InputSettingsSubsystem.h`
- `UE/Graytail/Source/Graytail/Core/GT_InputSettingsSubsystem.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_LocalizedTextCatalog.h`
- `UE/Graytail/Source/Graytail/UI/GT_LocalizedTextCatalog.cpp`
- `UE/Graytail/Config/Localization/Graytail_Gather.ini`
- `Tools/Invoke-GraytailRelease.ps1`
- `Tools/Test-GraytailStatic.ps1`
- `Tools/Test-GraytailPackage.ps1`
- `.github/workflows/static-validation.yml`
- `docs/architecture.md`
- `docs/game-data.md`
- `docs/save-recovery.md`
- `docs/release-validation.md`
- `docs/player-controls.md`

### Modified files

- `UE/Graytail/Content/Graytail/Data/Balance/core.json`
- `UE/Graytail/Content/Graytail/Data/Balance/difficulties.json`
- `UE/Graytail/Content/Graytail/Data/Balance/monsters.json`
- `UE/Graytail/Content/Graytail/Data/Balance/items.json`
- `UE/Graytail/Content/Graytail/Data/Balance/loot_events.json`
- `UE/Graytail/Content/Graytail/Data/Balance/meta_catalog.json`
- `UE/Graytail/Source/Graytail/Data/GT_GameDataTypes.h`
- `UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.h`
- `UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp`
- `UE/Graytail/Source/Graytail/Graytail.Build.cs`
- `UE/Graytail/Source/Graytail/App/GT_PlayerController.h`
- `UE/Graytail/Source/Graytail/App/GT_PlayerController.cpp`
- `UE/Graytail/Source/Graytail/Core/GT_SettingsSubsystem.h`
- `UE/Graytail/Source/Graytail/Core/GT_SettingsSubsystem.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.h`
- `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.cpp`
- All player-facing `UE/Graytail/Source/Graytail/UI/GT_*.cpp`
- `UE/Graytail/Config/DefaultGame.ini`
- `README.md`

## Task 1: Inventory and Migrate Remaining Gameplay Literals

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Data/GT_GameDataTypes.h`
- Modify: all six files under `UE/Graytail/Content/Graytail/Data/Balance`
- Modify: gameplay callers under `Core`, `Domains`, and `UI`
- Create: `Tools/Test-GraytailStatic.ps1`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Generate a reviewed gameplay-literal inventory**

Search:

```powershell
rg -n "(Damage|Hp|Health|Speed|Cooldown|Duration|Radius|Range|Chance|Percent|Reward|Gold|Pressure|Limit|Max|Min).*[=<>].*[0-9]" UE/Graytail/Source/Graytail/Core UE/Graytail/Source/Graytail/Domains UE/Graytail/Source/Graytail/UI
```

Classify every hit as:

- gameplay data;
- algorithmic invariant such as `0`, `1`, fixed-step `60`, or percentage divisor `100`;
- presentation-only value.

Record the approved non-gameplay cases in `Tools/Test-GraytailStatic.ps1` by file and symbolic context, not by broad directory exclusion.

- [ ] **Step 2: Write failing default-equivalence checks**

Before changing callers, assert every current behavior-affecting default by named field. At minimum cover:

- movement speed, acceleration, deceleration, arena bounds;
- player attack range, cooldown, phase durations, invulnerability;
- projectile speed and collision radii;
- difficulty-specific full-pressure damage;
- monster-specific attack and split settings;
- current map, loot, event, and meta values already in JSON.

The expected values must be listed in the six JSON fixtures, not duplicated as fallback constants in production code.

- [ ] **Step 3: Extend typed JSON schemas**

Add fields to the appropriate structures. Example:

```cpp
USTRUCT()
struct FGT_PlayerCombatMotionConfig
{
	GENERATED_BODY()

	UPROPERTY() float MoveSpeed = 0.0f;
	UPROPERTY() float Acceleration = 0.0f;
	UPROPERTY() float Deceleration = 0.0f;
	UPROPERTY() float AttackRange = 0.0f;
	UPROPERTY() float AttackCooldown = 0.0f;
	UPROPERTY() float InvulnerabilitySeconds = 0.0f;
	UPROPERTY() float ProjectileCollisionRadius = 0.0f;
};
```

Place shared player/combat values in `core.json`, per-difficulty pressure damage in `difficulties.json`, and per-monster values in `monsters.json`.

- [ ] **Step 4: Extend strict validation**

Validate:

- required fields are present;
- finite positive movement and timing values;
- non-negative damage and rewards;
- probabilities in `[0, 100]` or ratios in `[0, 1]` according to field semantics;
- active attack duration is non-zero;
- phase sums and projectile radii are safe;
- cross references still resolve.

An omitted new field must produce a path such as `core.playerCombat.attackCooldown is required`; do not substitute a C++ default.

- [ ] **Step 5: Replace gameplay literals with snapshot reads**

Construct `FGT_CombatSimulationConfig` from `UGT_GameDataSubsystem` at encounter start. Apply equipment/talent effects afterward to build effective values. Remove the old literals from `GT_RoomViewWidget`, `GT_RunContext`, combat rules, and settlement logic.

- [ ] **Step 6: Add mutation-without-recompile test**

Copy the six files to a temporary balance directory, change one field such as player movement speed, initialize a fresh loader against that directory, and assert the new typed value is used. Restore the directory through scope cleanup.

- [ ] **Step 7: Run equivalence smoke**

Expected: unchanged JSON produces the same seeded map, monster selection, combat outcome, loot, events, and settlement as the pre-migration fixtures.

- [ ] **Step 8: Commit**

```powershell
git add UE/Graytail/Content/Graytail/Data/Balance UE/Graytail/Source/Graytail/Data UE/Graytail/Source/Graytail/Core UE/Graytail/Source/Graytail/Domains UE/Graytail/Source/Graytail/UI/GT_RoomViewWidget.* UE/Graytail/Source/Graytail/Debug Tools/Test-GraytailStatic.ps1
git commit -m "refactor: complete gameplay balance data migration"
```

## Task 2: Add Semantic Input Actions and Persistent Bindings

**Files:**
- Create: `UE/Graytail/Source/Graytail/Core/GT_InputTypes.h`
- Create: `UE/Graytail/Source/Graytail/Core/GT_InputSettingsSubsystem.h`
- Create: `UE/Graytail/Source/Graytail/Core/GT_InputSettingsSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/App/GT_PlayerController.h`
- Modify: `UE/Graytail/Source/Graytail/App/GT_PlayerController.cpp`
- Modify: `UE/Graytail/Source/Graytail/Graytail.Build.cs`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Write binding validation tests**

Define actions:

```cpp
UENUM()
enum class EGT_InputAction : uint8
{
	MoveUp,
	MoveDown,
	MoveLeft,
	MoveRight,
	Attack,
	Interact,
	UseConsumable,
	ToggleMap,
	TogglePause,
	MenuConfirm,
	MenuBack
};
```

Test default bindings, save/load round trip, invalid keys, conflict detection, and conflict replacement.

- [ ] **Step 2: Implement the input settings subsystem**

Public API:

```cpp
FKey GetPrimaryKey(EGT_InputAction Action) const;
FGT_InputBindingResult Rebind(
	EGT_InputAction Action,
	FKey NewKey,
	bool bReplaceConflict);
void ResetToDefaults();
FText GetBindingDisplayText(EGT_InputAction Action) const;
```

Persist under a dedicated `[/Script/Graytail.GT_InputSettings]` section in `GGameUserSettingsIni`. Serialize key names, validate on load, and restore only the invalid entry to its default.

- [ ] **Step 3: Route physical input through PlayerController**

Override the controller input path and translate `FKey` plus press/release into semantic actions. Dispatch:

```cpp
void UGT_GameHudWidget::HandleInputAction(
	EGT_InputAction Action,
	bool bPressed);
```

The room view receives movement and combat actions; modal menus receive confirm/back; the active HUD layer decides consumption. Remove direct gameplay `EKeys::W/A/S/D/F/Q/M/Escape` comparisons from child widgets after migration.

- [ ] **Step 4: Verify rebinding changes behavior**

Bind attack from `F` to `LeftMouseButton`, submit the physical key through the controller resolver, and assert the semantic attack action fires while `F` no longer does. Restart the test game instance and verify persistence.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Core/GT_Input* UE/Graytail/Source/Graytail/App UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.* UE/Graytail/Source/Graytail/Graytail.Build.cs UE/Graytail/Source/Graytail/Debug
git commit -m "feat: add persistent semantic input bindings"
```

## Task 3: Add Rebinding UI and Dynamic Prompts

**Files:**
- Modify: `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.cpp`
- Modify: all widgets containing hard-coded key prompts
- Test: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Add pure prompt-format tests**

Use:

```cpp
FText GT_FormatActionPrompt(
	const UGT_InputSettingsSubsystem& Bindings,
	EGT_InputAction Action,
	const FText& Verb);
```

Assert changing a binding changes combat, menu, tutorial, and footer prompt output without widget-specific key strings.

- [ ] **Step 2: Build the keyboard/mouse controls panel**

For each supported action show:

- localized action name;
- current key;
- rebind button;
- reset-to-default control.

While capturing, accept one keyboard or mouse button. Reject mouse movement, wheel axis, modifier-only keys, Escape as a binding, and all gamepad keys in this release.

- [ ] **Step 3: Handle conflicts explicitly**

If a key is already bound, show the conflicting action and require a second confirmation to replace it. Never silently unbind another action.

- [ ] **Step 4: Replace hard-coded prompts**

Replace strings such as `W/S`, `F`, `Q`, `M`, `Enter`, and `Esc` with action display text. Numeric consumable slots can remain fixed only if they are documented as outside this first remapping set.

- [ ] **Step 5: Verify no player-facing key prompt is stale**

Run:

```powershell
rg -n 'TEXT\\(\"[^\"]*(W/S|Enter|Esc|按 F|按 Q|按 M)' UE/Graytail/Source/Graytail/UI
```

Expected: no player-facing hard-coded prompt remains for remappable actions.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Source/Graytail/UI UE/Graytail/Source/Graytail/Debug
git commit -m "feat: add keyboard and mouse rebinding ui"
```

## Task 4: Move Player-Visible Chinese into Unreal Localization

**Files:**
- Create: `UE/Graytail/Source/Graytail/UI/GT_LocalizedTextCatalog.h`
- Create: `UE/Graytail/Source/Graytail/UI/GT_LocalizedTextCatalog.cpp`
- Create: `UE/Graytail/Config/Localization/Graytail_Gather.ini`
- Modify: player-facing `UE/Graytail/Source/Graytail/UI/GT_*.cpp`
- Modify: catalogs that currently return player-visible `FString`
- Modify: `UE/Graytail/Config/DefaultGame.ini`

- [ ] **Step 1: Add a static localization guard**

Extend `Tools/Test-GraytailStatic.ps1` to fail on new player-visible:

```cpp
FText::FromString(TEXT("中文"))
```

Allow `FromString` only for runtime numbers, user-generated data, log-derived diagnostics, or text already returned by a localized catalog.

- [ ] **Step 2: Convert static UI text**

At each source file:

```cpp
#define LOCTEXT_NAMESPACE "Graytail.Settings"
TitleText->SetText(LOCTEXT("Title", "设置"));
#undef LOCTEXT_NAMESPACE
```

Use stable, descriptive keys. Format variable text with `FText::Format` and named arguments instead of `FString::Printf`.

- [ ] **Step 3: Centralize content text**

`GT_LocalizedTextCatalog` maps stable gameplay IDs to gatherable `NSLOCTEXT` values for:

- item names, descriptions, and effects;
- monster names;
- difficulty labels and hints;
- event titles, options, and outcomes;
- equipment, talent, and consumable descriptions.

Return `FText`, not localized `FString`. Keep logs and internal error codes as `FString`/`FName`.

- [ ] **Step 4: Configure localization gathering**

Set Chinese Simplified (`zh-Hans`) as native culture. The gather config includes source C++ and any gatherable content, then generates manifest/archive/resource files under `Content/Localization/Graytail`.

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -run=GatherText -config="D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Config\Localization\Graytail_Gather.ini" -unattended -nop4
```

Expected: command exits `0` and generated resources contain all converted namespaces.

- [ ] **Step 5: Verify source coverage**

Run:

```powershell
rg -n -P 'FText::FromString\\(TEXT\\(\"[^\"A-Za-z0-9]*[\\p{Han}]' UE/Graytail/Source/Graytail
```

Review every remaining hit and add a narrow justified exception or convert it. Do not silence an entire file.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Source/Graytail/UI UE/Graytail/Source/Graytail/Domains UE/Graytail/Config UE/Graytail/Content/Localization Tools/Test-GraytailStatic.ps1
git commit -m "refactor: localize player-facing chinese text"
```

## Task 5: Build One-Command Local Release Validation

**Files:**
- Create: `Tools/Invoke-GraytailRelease.ps1`
- Create: `Tools/Test-GraytailPackage.ps1`
- Modify: `Tools/Test-GraytailStatic.ps1`
- Modify: `UE/Graytail/Source/Graytail/App/GT_PlayerController.cpp`

- [ ] **Step 1: Write script self-tests for parameter and failure handling**

The release entry point accepts:

```powershell
param(
  [string]$EngineRoot = 'G:\Epic\UE_5.7',
  [ValidateSet('Development','Shipping','All')]
  [string]$Configuration = 'All',
  [string]$ArtifactsRoot = '.\Artifacts\ReleaseValidation',
  [switch]$SkipVisibleLaunch
)
```

It must stop on the first failed stage and print a final stage summary.

- [ ] **Step 2: Implement the ordered pipeline**

Run:

1. static validation;
2. `GraytailEditor` Development build;
3. full commandlet smoke;
4. Development BuildCookRun;
5. Development `-GraytailStartupProbe`;
6. Shipping BuildCookRun;
7. Pak JSON and localization-resource inventory;
8. Shipping survival launch;
9. official-save isolation check.

- [ ] **Step 3: Verify packaged data**

Use `UnrealPak.exe -List` and require exactly these staged balance files:

```text
core.json
difficulties.json
monsters.json
items.json
loot_events.json
meta_catalog.json
```

Also require the Graytail localization resource. Fail on a missing or duplicate domain file.

- [ ] **Step 4: Verify Shipping survival without adding a Shipping test hook**

Launch Shipping with a temporary `-GraytailSaveSlot=` and `-log`, wait 10 seconds, and assert the process remains alive. Then close it normally. Development startup probe remains the behavior-aware test; Shipping only proves stable product startup and absence of development hooks.

- [ ] **Step 5: Verify save isolation**

Hash and timestamp the official `GraytailMeta` save before and after all automated probes. Expected: unchanged. Confirm temporary probe slots are removed in `finally` blocks even after failure.

- [ ] **Step 6: Run the complete command locally**

```powershell
pwsh -File .\Tools\Invoke-GraytailRelease.ps1 -EngineRoot "G:\Epic\UE_5.7" -Configuration All
```

Expected: one final `GRAYTAIL_RELEASE|Overall=Pass` and all stage logs retained under the artifact directory.

- [ ] **Step 7: Commit**

```powershell
git add Tools UE/Graytail/Source/Graytail/App
git commit -m "test: add one-command release validation"
```

## Task 6: Add Honest Hosted CI

**Files:**
- Create: `.github/workflows/static-validation.yml`
- Modify: `Tools/Test-GraytailStatic.ps1`

- [ ] **Step 1: Make static validation platform-independent**

The script checks:

- all six JSON files parse;
- each top-level `schemaVersion` is `1`;
- required file names are exact;
- duplicate JSON IDs and obvious probability/range errors;
- no Shipping UI references cheat-only symbols;
- no new hard-coded remappable key prompts;
- no new non-localized Chinese text;
- docs reference existing scripts and paths.

The script returns non-zero for every violation and emits file/field context.

- [ ] **Step 2: Add GitHub Actions workflow**

Use Windows hosted runner and PowerShell:

```yaml
name: Static validation
on:
  pull_request:
  push:
    branches: [main]
jobs:
  validate:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - shell: pwsh
        run: ./Tools/Test-GraytailStatic.ps1
```

Do not label this job “build” or imply Unreal binaries were compiled.

- [ ] **Step 3: Run locally and commit**

```powershell
pwsh -File .\Tools\Test-GraytailStatic.ps1
git add .github Tools/Test-GraytailStatic.ps1
git commit -m "ci: validate data and release boundaries"
```

## Task 7: Replace Stale Documentation

**Files:**
- Modify: `README.md`
- Create: `docs/architecture.md`
- Create: `docs/game-data.md`
- Create: `docs/save-recovery.md`
- Create: `docs/release-validation.md`
- Create: `docs/player-controls.md`

- [ ] **Step 1: Audit stale claims**

Search for claims that HUD, combat, save, meta progression, packaging, or tests are placeholders or command-only. Record each outdated document before editing.

- [ ] **Step 2: Document the three-layer model**

`docs/architecture.md` describes:

```text
JSON startup snapshot -> domain/run simulation -> UMG presentation
                           |
                           +-> transactional SaveGame persistence
```

Explain that startup snapshot immutability does not prevent runtime item/talent modifiers: JSON provides base rules, while effective runtime state applies bonuses in the simulation.

- [ ] **Step 3: Document operator-facing workflows**

Cover:

- every JSON field and valid range;
- save v2, backup recovery, and interrupted-run behavior;
- default controls and rebinding limits;
- exact local release command and artifact locations;
- what hosted CI does and does not prove.

- [ ] **Step 4: Validate links and paths**

Extend static validation to verify referenced local paths exist.

- [ ] **Step 5: Commit**

```powershell
git add README.md docs Tools/Test-GraytailStatic.ps1
git commit -m "docs: describe release-ready game architecture"
```

## Task 8: Final Release Verification and Draft PR

- [ ] **Step 1: Run static validation**

Expected: exit `0`, no broad suppressions, and all reported counts included in the PR body.

- [ ] **Step 2: Run one-command full release validation**

Expected:

- editor build passes;
- full smoke has `Fail=0`;
- Development startup probe passes;
- Shipping survives startup;
- Pak contains six balance files and localization resources;
- official save hash is unchanged.

- [ ] **Step 3: Perform visible product checks**

Verify on Development and Shipping:

- double-click reaches main menu;
- remapped keyboard/mouse actions work after restart;
- every displayed prompt follows the current binding;
- Chinese text renders correctly;
- settings and pause contain no Shipping cheats;
- a full run can start, settle, return to menu, and reload its progress.

- [ ] **Step 4: Check repository and generated artifacts**

```powershell
git diff --check
git status --short
rg -n "TODO|TBD|PLACEHOLDER" UE/Graytail/Source UE/Graytail/Config Tools docs README.md
```

Keep packaged binaries and transient localization intermediates out of Git; commit only required localization resources and source.

- [ ] **Step 5: Create the PR description**

Create `docs/pr/productization-tests.md` covering data fields migrated, input scope, localization coverage, static CI limits, full local release evidence, and manual checks.

- [ ] **Step 6: Push and open the final draft stacked PR**

```powershell
git push -u origin codex/productization-tests
gh pr create --draft --base codex/combat-simulation --head codex/productization-tests --title "[codex] finish productization and release validation" --body-file docs/pr/productization-tests.md
```
