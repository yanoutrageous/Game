# Graytail JSON Game Data Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make JSON balance edits fail safely, preserve existing saves and seeded behaviour, and ensure every accepted value is used consistently at runtime.

**Architecture:** Keep one validated startup rule snapshot separate from mutable player/run state. Parse JSON strictly before UStruct conversion, validate all semantic and asset boundaries, make test reloads transactional, and make derived facade caches revision-aware.

**Tech Stack:** Unreal Engine 5.7 C++, `Json`, `JsonUtilities`, UE SaveGame, `GT_RuntimeSmokeRunner`, Win64 BuildCookRun.

## Global Constraints

- Production rule changes take effect after restart; runtime player and run state remain mutable.
- UE SaveGame is the only official load source.
- Invalid configuration must leave the main menu usable and block run creation.
- Default JSON values must preserve pre-JSON seeded gameplay results.
- Smoke tests must never write the normal `GraytailMeta` slot.
- No hidden gameplay defaults may replace missing JSON values.

---

### Task 1: Strict JSON Shape Validation

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

**Interfaces:**
- Produces: `LoadJsonFile()` rejects missing, unknown, and wrong-type fields before `FJsonObjectConverter`.
- Consumes: Existing six `FGT_*BalanceFile` UStruct definitions.

- [ ] **Step 1: Add failing smoke cases**

Add checks that copy the default data directory and then:

```cpp
ReplaceGameDataText(Directory, TEXT("monsters.json"),
    TEXT("\"moveSpeed\": 1.0"), TEXT("\"moveSpeeed\": 1.0"));
ReplaceGameDataText(Directory, TEXT("core.json"),
    TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": \"1\""));
ReplaceGameDataText(Directory, TEXT("core.json"),
    TEXT("\"baseHp\": 100"), TEXT("\"baseHp\": 100, \"unknownHp\": 1"));
```

Each directory must make `FGT_GameDataLoader::LoadFromDirectory()` return false
with at least one field-path error.

- [ ] **Step 2: Run smoke and verify RED**

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" `
  -run=GT_RuntimeSmokeRunner -unattended -nopause -nosplash
```

Expected: the three new checks fail because the converter currently ignores
unknown keys and coerces scalar types.

- [ ] **Step 3: Add strict DOM validation**

Before conversion, recursively compare each JSON object with its UStruct:

```cpp
bool ValidateJsonObjectShape(
    const TSharedPtr<FJsonObject>& Object,
    const UStruct* StructType,
    const FString& Path,
    TArray<FString>& OutErrors);
```

For every reflected `FProperty`, require the matching JSON key and exact type.
Recurse into struct properties and array elements. Reject keys not represented
by a property. Integer properties require `EJson::Number` and an integral,
in-range value; booleans and strings require their exact JSON types.

Use the already parsed DOM in `LoadJsonFile()` and call
`FJsonObjectConverter::JsonObjectToUStruct()` only after shape validation passes.

- [ ] **Step 4: Build and rerun smoke**

Expected: all strict-shape cases pass and existing default files still load.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp `
        UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp
git commit -m "fix: validate JSON balance schema strictly"
```

### Task 2: Semantic And Cross-Reference Validation

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

**Interfaces:**
- Produces: accepted snapshots are safe for protocol, map, item, monster, and meta facades.
- Consumes: strict JSON shape validation from Task 1.

- [ ] **Step 1: Add failing semantic cases**

Add negative cases for:

```text
protocol thresholds reordered or missing pressure 0
tutorial spawn or exit outside width/height
randomExitCount == 0 for non-manual difficulties
item ID with no runtime asset definition
drop-table quality with no non-empty pool
negative bonusHp, mineDamageReduce, triggerCap, or triggerAmount
mineDamageFloor > mineDamage
slimeling spawnWeight > 0
missing currently shipped equipment, talent, consumable, or item ID
```

- [ ] **Step 2: Run smoke and verify RED**

Expected: each new invalid configuration is currently accepted.

- [ ] **Step 3: Implement domain invariants**

Add exact supported persistent ID sets and validate them with `RequireIds()`.
Validate item IDs against the runtime item asset registry used by
`GT_ItemCatalog`. Require every produced quality to have a non-empty pool.

Require protocol thresholds to contain levels 1 through 5, strictly descending
pressure, and a final pressure-zero entry. Require:

```cpp
0 < MineDamageFloor && MineDamageFloor <= MineDamage;
RandomExitCount >= 1; // for non-manual standard rows
Slimeling.SpawnWeight == 0;
BonusHp >= 0;
BonusPower >= 0;
MineDamageReduce >= 0;
SearchBonus in [0,100];
TriggerCap >= 0;
TriggerAmount >= 0;
```

For enabled manual layouts, validate every coordinate, at least one exit, one
valid spawn, no duplicate coordinates within a category, and no conflicting
room categories.

- [ ] **Step 4: Build and rerun smoke**

Expected: all semantic negative cases pass and default-equivalence checks remain green.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp `
        UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp
git commit -m "fix: enforce gameplay data invariants"
```

### Task 3: Safe Snapshot Lifecycle And Catalog Caches

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_MonsterCatalog.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Inventory/GT_ItemCatalog.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

**Interfaces:**
- Produces: failed reload keeps the last valid snapshot; derived caches follow `GetRevision()`.
- Produces: `SanitizeAfterLoad()` safely defers catalog work when rules are unavailable.

- [ ] **Step 1: Add failing lifecycle tests**

Cover:

```text
valid snapshot -> invalid reload -> old snapshot and revision remain usable
item value changes after successful test reload
monster HP/speed changes after successful test reload
existing SaveGame state can load while game data is invalid without a crash
```

- [ ] **Step 2: Run smoke and verify RED**

Expected: failed reload makes `GetSnapshot()` null and item/monster caches stay stale.

- [ ] **Step 3: Make reload transactional**

Only update readiness and snapshot after a successful candidate load:

```cpp
const bool bCandidateReady =
    FGT_GameDataLoader::LoadFromDirectory(Directory, Candidate, CandidateErrors);
if (bCandidateReady)
{
    Snapshot = MoveTemp(Candidate);
    bReady = true;
    Errors.Reset();
    ++Revision;
}
else
{
    Errors = MoveTemp(CandidateErrors);
}
```

When no valid snapshot has ever loaded, `bReady` stays false. A failed later
test reload does not discard a valid snapshot.

- [ ] **Step 4: Make item and monster caches revision-aware**

Replace process-lifetime `static const` values with cache structs containing
`uint64 Revision`. Rebuild arrays/archetypes when
`GT_GameData::GetRevision()` changes, matching `GT_MetaCatalog`.

- [ ] **Step 5: Defer save sanitization**

In `Load()`, deserialize `State` regardless of rule readiness. Run
catalog-dependent cleanup only when `GT_GameData::IsReady()`. Call the cleanup
before the first meta operation once valid data exists. Remove orphaned
equipped/loadout references while preserving owned assets for explicit future
migration.

- [ ] **Step 6: Build and rerun smoke**

Expected: lifecycle and cache checks pass without assertions.

- [ ] **Step 7: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Data/GT_GameDataSubsystem.cpp `
        UE/Graytail/Source/Graytail/Domains/Combat/GT_MonsterCatalog.cpp `
        UE/Graytail/Source/Graytail/Domains/Inventory/GT_ItemCatalog.cpp `
        UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp `
        UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp
git commit -m "fix: keep game data snapshots consistent"
```

### Task 4: Isolate Smoke Save State

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

**Interfaces:**
- Produces: non-Shipping test-only save-slot override.
- Produces: smoke always deletes its temporary SaveGame slot.

- [ ] **Step 1: Add a save isolation assertion**

Capture whether the canonical `GraytailMeta` file exists and its timestamp/hash
before mirror tests. After the tests, assert those values are unchanged.

- [ ] **Step 2: Run smoke and verify RED**

Expected: canonical slot timestamp changes at the two current `Save()` calls.

- [ ] **Step 3: Add a scoped test slot**

Add non-Shipping methods:

```cpp
void SetSaveSlotOverrideForTesting(const FString& SlotName);
void ClearSaveSlotOverrideForTesting();
FString GetActiveSaveSlotName() const;
```

All `DoesSaveGameExist`, `LoadGameFromSlot`, and `SaveGameToSlot` calls use
`GetActiveSaveSlotName()`. Smoke assigns
`GraytailMetaSmoke_<guid>`, exercises save/load/mirror behaviour, deletes that
slot on every exit path, then clears the override.

- [ ] **Step 4: Build and rerun smoke**

Expected: mirror tests pass and canonical save metadata remains unchanged.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h `
        UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.cpp `
        UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp
git commit -m "test: isolate runtime smoke save data"
```

### Task 5: Preserve Seed Results And Apply Trigger Values

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunContext.h`
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunContext.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSettlement.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

**Interfaces:**
- Produces: `StableLegacyPercentRoll(uint32 Hash)` preserves old even/odd outcomes at 50 percent.
- Produces: run state stores configured chest bonus amount.
- Produces: settlement derives configured gold bonus from equipped definitions.

- [ ] **Step 1: Add failing gameplay checks**

Assert that the known lucky-coin seed/hash that was gold before JSON remains
gold at the default 50 percent. Change `ChestBonusLoot.triggerAmount` and
`SettleGoldBonus.triggerAmount` in a temporary config and assert the actual
reward changes by those amounts.

- [ ] **Step 2: Run smoke and verify RED**

Expected: lucky coin flips, chest always adds one item, and settlement always adds 15 percent.

- [ ] **Step 3: Implement stable percent mapping**

Use:

```cpp
const uint32 HalfRoll = (Hash >> 1u) % 50u;
const int32 StableRoll = (Hash & 1u) == 0u
    ? static_cast<int32>(HalfRoll)
    : 50 + static_cast<int32>(HalfRoll);
const bool bGold = StableRoll < LuckyCoin.GoldChancePercent;
```

At 50 percent this selects exactly the legacy even hashes.

- [ ] **Step 4: Remove trigger constants**

Store `ChestBonusLootAmount` from `Def->TriggerAmount` and grant that many
deterministic low-quality items subject to inventory capacity.

For settlement, inspect equipped definitions with
`EGT_ItemTrigger::SettleGoldBonus`, sum `TriggerAmount` as a percentage, and
calculate:

```cpp
DirectGold = FMath::RoundToInt(
    static_cast<float>(DirectGold) * (100.0f + BonusPercent) / 100.0f);
```

- [ ] **Step 5: Build and rerun smoke**

Expected: seeded equivalence and configured trigger checks pass.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Core/GT_RunContext.h `
        UE/Graytail/Source/Graytail/Core/GT_RunContext.cpp `
        UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaSettlement.cpp `
        UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp
git commit -m "fix: preserve seeded balance behaviour"
```

### Task 6: Release Verification And PR Update

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`
- Modify: pull request description only after verification.

**Interfaces:**
- Consumes: all prior tasks.
- Produces: verified draft PR with accurate checks and limitations.

- [ ] **Step 1: Run a clean Editor build**

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat" `
  GraytailEditor Win64 Development `
  "-Project=D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" -WaitMutex
```

Expected: exit code 0.

- [ ] **Step 2: Run the full smoke suite**

```powershell
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" `
  -run=GT_RuntimeSmokeRunner -unattended -nopause -nosplash
```

Expected: `Overall=Pass`, zero failures, and a count greater than 187.

- [ ] **Step 3: Package Development and launch headlessly**

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  "-project=D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" `
  -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak `
  -archive "-archivedirectory=D:\CODE\NetEase Training\.artifacts\GraytailJsonDataHardened"
```

Inspect the Pak listing for exactly six balance JSON files, then launch the
packaged executable with `-nullrhi -unattended`.

- [ ] **Step 4: Compile and package Shipping**

Run:

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  "-project=D:\CODE\NetEase Training\.worktrees\Game\json-game-data\UE\Graytail\Graytail.uproject" `
  -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak `
  -archive "-archivedirectory=D:\CODE\NetEase Training\.artifacts\GraytailJsonDataShipping"
```

Debug-mirror checks must be conditionally excluded from Shipping-only
validation. Launch the Shipping executable and verify no configuration,
assertion, or fatal error.

- [ ] **Step 5: Push commits and update draft PR**

```powershell
git push origin codex/json-game-data
$body | gh pr edit 21 --repo yanoutrageous/Game --body-file -
```

Include the new regression count, save-slot isolation, strict-schema coverage,
Development package result, and Shipping package result.
