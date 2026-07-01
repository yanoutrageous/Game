# Graytail Deterministic Combat Simulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move all combat rules out of UMG into a deterministic fixed-step simulation that produces the same gameplay result for the same seed and input timeline at any render frame rate.

**Architecture:** A plain C++ `FGT_CombatSimulation` is the sole authority for active-combat HP, positions, timers, attack phases, projectiles, and its seeded random stream. `UGT_RunSubsystem` owns and advances the simulation through a core ticker, applies completed-step and terminal outputs to `UGT_RunContext`, and exposes a lightweight read-only view snapshot. `UGT_RoomViewWidget` only submits input and renders snapshots.

**Tech Stack:** Unreal Engine 5.7 C++, `FTSTicker`, `FRandomStream`, UMG, commandlet smoke tests.

## Global Constraints

- Create `codex/combat-simulation` from the final `codex/release-entry-save` commit.
- Do not alter permanent save semantics established by PR 1.
- Fixed simulation step is exactly `1.0 / 60.0` seconds.
- Clamp accepted outer-frame delta to `0.25` seconds.
- Paused time is discarded, not accumulated.
- No gameplay branch may read widget focus, animation state, or render delta.
- No gameplay random decision may use `FMath::FRand`, `RandRange`, or global RNG.
- Projectiles use segment sweep from previous to next position.
- Active combat state has one authority: do not independently mutate matching HP, enemy, timer, or projectile state in both the simulation and `UGT_RunContext`.
- UI animation may remain frame-based only when it cannot affect damage, collision, cooldowns, movement, rewards, or outcomes.

---

## File Structure

### New files

- `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulationTypes.h`
- `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatMath.h`
- `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatMath.cpp`
- `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.h`
- `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.h`
- `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

### Modified files

- `UE/Graytail/Source/Graytail/Core/GT_RunContext.h`
- `UE/Graytail/Source/Graytail/Core/GT_RunContext.cpp`
- `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.h`
- `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_RoomViewWidget.h`
- `UE/Graytail/Source/Graytail/UI/GT_RoomViewWidget.cpp`
- `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp`
- `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

## Task 1: Define Simulation Contracts and Collision Math

**Files:**
- Create: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulationTypes.h`
- Create: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatMath.h`
- Create: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatMath.cpp`
- Create: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.h`
- Create: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_RuntimeSmokeValidator.cpp`

- [ ] **Step 1: Write failing segment-circle collision tests**

Cover:

- crossing the circle in one large step hits;
- tangent contact hits once;
- segment ending before the circle misses;
- zero-length segment inside hits;
- zero-length segment outside misses.

Target API:

```cpp
namespace GT_CombatMath
{
	bool SegmentIntersectsCircle(
		const FVector2D& Start,
		const FVector2D& End,
		const FVector2D& Center,
		float Radius,
		float& OutHitAlpha);
}
```

- [ ] **Step 2: Define plain simulation types**

The types file contains no widget references:

```cpp
struct FGT_CombatInputState
{
	FVector2D MoveAxis = FVector2D::ZeroVector;
	bool bAttackPressed = false;
	bool bFleePressed = false;
};

struct FGT_CombatSimulationConfig
{
	float FixedStepSeconds = 1.0f / 60.0f;
	float MaxFrameDeltaSeconds = 0.25f;
	// Copy every gameplay value needed during this encounter.
};

struct FGT_CombatViewSnapshot
{
	bool bActive = false;
	bool bPaused = false;
	FVector2D PlayerPosition = FVector2D::ZeroVector;
	TArray<FGT_CombatEnemyView> Enemies;
	TArray<FGT_CombatProjectileView> Projectiles;
};
```

Use numeric IDs and `FName` catalog IDs in snapshots. Do not construct display strings in the simulation.

- [ ] **Step 3: Implement and run collision tests**

Build and run the commandlet smoke. Expected: all new geometry cases pass.

- [ ] **Step 4: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulationTypes.h UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatMath.* UE/Graytail/Source/Graytail/Debug
git commit -m "test: define combat simulation contracts"
```

## Task 2: Implement Fixed-Step Time, Input, and Pause

**Files:**
- Create: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.h`
- Create: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

- [ ] **Step 1: Write frame-schedule equivalence tests**

Drive the same input timeline through:

- 30 FPS deltas;
- 60 FPS deltas;
- 144 FPS deltas;
- a schedule containing one `0.2` second hitch.

Compare an exact canonical state:

```cpp
struct FGT_CombatCanonicalState
{
	int64 CompletedSteps = 0;
	FVector2D PlayerPosition;
	float AttackCooldownRemaining = 0.0f;
	int32 RandomDrawCount = 0;
};
```

Use tolerances only for floating-point vectors; IDs, phases, HP, event order, and step count must match exactly.

- [ ] **Step 2: Implement the accumulator**

Public surface:

```cpp
class FGT_CombatSimulation
{
public:
	void Initialize(const FGT_CombatSimulationConfig& Config, int32 EncounterSeed);
	void SetInput(const FGT_CombatInputState& Input);
	void SetPaused(bool bPaused);
	void AdvanceFrame(float DeltaSeconds, TArray<FGT_CombatDomainEvent>& OutEvents);
	void BuildViewSnapshot(FGT_CombatViewSnapshot& OutSnapshot) const;
};
```

Algorithm:

```cpp
if (State.bPaused)
{
	AccumulatorSeconds = 0.0;
	return;
}

AccumulatorSeconds += FMath::Min(
	static_cast<double>(DeltaSeconds),
	static_cast<double>(Config.MaxFrameDeltaSeconds));

while (AccumulatorSeconds + UE_DOUBLE_SMALL_NUMBER >= Config.FixedStepSeconds)
{
	Step(Config.FixedStepSeconds, OutEvents);
	AccumulatorSeconds -= Config.FixedStepSeconds;
}
```

Consume edge-triggered attack and flee requests once; movement remains held state.

- [ ] **Step 3: Prove pause discards elapsed time**

Test: run 0.5 seconds, pause, feed 5 seconds, resume, run 0.5 seconds. Expected state equals a simulation fed exactly 1 second with no pause.

- [ ] **Step 4: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.* UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp
git commit -m "feat: add fixed-step combat clock"
```

## Task 3: Migrate Player Movement and Melee Attacks

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulationTypes.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

- [ ] **Step 1: Capture current behavior with tests**

Extract the current values and rules from `GT_RoomViewWidget.cpp` into test fixtures before moving code. Cover:

- acceleration and deceleration;
- arena bounds;
- facing direction;
- attack cooldown;
- wind-up, active, and recovery phases;
- one hit per enemy per swing;
- invulnerability timer.

- [ ] **Step 2: Implement player stepping**

Update velocity and position only in `FGT_CombatSimulation::Step`. Normalize diagonal input and clamp to arena bounds. Keep visual interpolation out of rule state.

- [ ] **Step 3: Implement ordered melee phase transitions**

Represent phase explicitly:

```cpp
enum class EGT_CombatAttackPhase : uint8
{
	Idle,
	WindUp,
	Active,
	Recovery
};
```

When one fixed step crosses multiple phase boundaries, process each boundary in order and evaluate the active hit window. Do not skip a hit window because a timer became negative.

- [ ] **Step 4: Verify equivalence and commit**

Expected: frame schedules produce identical positions, phases, HP changes, and hit IDs.

```powershell
git add UE/Graytail/Source/Graytail/Domains/Combat UE/Graytail/Source/Graytail/Debug
git commit -m "feat: simulate player combat deterministically"
```

## Task 4: Migrate Monster AI and Seeded Randomness

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulationTypes.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

- [ ] **Step 1: Add deterministic encounter-seed tests**

Derive the seed only from stable integers:

```cpp
int32 GT_DeriveEncounterSeed(
	int32 RunSeed,
	const FIntPoint& RoomCoord,
	int32 EncounterOrdinal);
```

Same tuple must match; changing any tuple member must change the derived seed in the test corpus.

- [ ] **Step 2: Move all gameplay random draws to `FRandomStream`**

Replace monster wander, attack jitter, ranged spread, and split offsets with the simulation-owned stream. Track draw order in tests to prevent accidental result drift.

- [ ] **Step 3: Move AI state machines**

Implement approach, retreat, wind-up, active, recovery, ranged preparation, and split behavior as explicit states. Configuration comes from copied `FGT_CombatSimulationConfig`, not from widget constants.

- [ ] **Step 4: Add split-monster and multi-enemy tests**

Assert child IDs, spawn positions, HP, target selection, and event order are identical across all frame schedules.

- [ ] **Step 5: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Domains/Combat UE/Graytail/Source/Graytail/Debug
git commit -m "feat: move monster ai into seeded simulation"
```

## Task 5: Migrate Projectiles, Lasers, Damage, and Outputs

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulationTypes.h`
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatSimulation.cpp`
- Modify: `UE/Graytail/Source/Graytail/Domains/Combat/GT_CombatMath.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

- [ ] **Step 1: Add failing tunneling and timing tests**

Cover:

- a fast projectile crossing the player between endpoints;
- a near-edge miss;
- single projectile destruction after hit;
- shotgun pellet independence;
- laser warm-up, active ticks, and cooldown;
- damage immunity spanning exact fixed steps;
- enemy death and combat-complete event order.

- [ ] **Step 2: Implement swept projectile collision**

For every projectile:

1. store previous position;
2. compute next position;
3. sweep the segment against the player circle;
4. select the earliest hit when multiple targets are possible;
5. emit one damage event and retire the projectile.

- [ ] **Step 3: Implement domain outputs**

Use value events:

```cpp
enum class EGT_CombatDomainEventType : uint8
{
	PlayerDamaged,
	EnemyDamaged,
	EnemyDefeated,
	FleeRequested,
	CombatWon,
	CombatLost
};
```

Events contain IDs and numeric values, never widget pointers or localized text.

- [ ] **Step 4: Verify and commit**

Expected: all tunneling and laser timing tests pass at 30/60/144 FPS and with the hitch schedule.

```powershell
git add UE/Graytail/Source/Graytail/Domains/Combat UE/Graytail/Source/Graytail/Debug
git commit -m "fix: make combat hits frame-rate independent"
```

## Task 6: Integrate Simulation Ownership into RunSubsystem

**Files:**
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunContext.h`
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunContext.cpp`
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.h`
- Modify: `UE/Graytail/Source/Graytail/Core/GT_RunSubsystem.cpp`
- Modify: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

- [ ] **Step 1: Add a no-widget integration test**

Create a standalone game instance, start a combat room, submit an input timeline directly to `UGT_RunSubsystem`, and resolve combat without constructing any widget.

- [ ] **Step 2: Make RunSubsystem own and tick the simulation**

Add:

```cpp
TUniquePtr<FGT_CombatSimulation> CombatSimulation;
FTSTicker::FDelegateHandle CombatTickerHandle;

bool TickCombat(float DeltaSeconds);
void SubmitCombatInput(const FGT_CombatInputState& Input);
void SetCombatPaused(bool bPaused);
bool GetCombatViewSnapshot(FGT_CombatViewSnapshot& OutSnapshot) const;
```

Register the ticker in `Initialize` and always remove it in `Deinitialize`. Expose `AdvanceCombatFrame(float)` as a normal C++ method used by the ticker and deterministic tests.

- [ ] **Step 3: Consume outputs into RunContext**

`FGT_CombatSimulation` is authoritative for active player/enemy HP and all combat timing. `UGT_RunContext` remains authoritative for inventory, room completion, rewards, and run lifecycle. Copy initial effective HP into the simulation at encounter start; after each completed fixed step, write one read-only combat summary to the run context for non-combat queries. Do not let that mirror feed decisions back into the simulation.

Consumable use is routed through `UGT_RunSubsystem`: validate and decrement inventory once in the run context, then submit the resulting heal/effect as a simulation command. Terminal outputs commit final HP and room/run outcome exactly once.

- [ ] **Step 4: Build config from the game-data snapshot once per encounter**

Copy all needed values at combat start. Runtime talents and equipment modify that encounter config before initialization; changing JSON requires restart, while in-run bonuses apply immediately through the copied effective values.

- [ ] **Step 5: Verify lifecycle cleanup**

Test:

- ending a run destroys simulation state;
- starting another encounter derives a new ordinal seed;
- pause menu stops combat without relying on focus;
- widget destruction does not stop or reset combat.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Source/Graytail/Core UE/Graytail/Source/Graytail/Domains/Combat UE/Graytail/Source/Graytail/Debug
git commit -m "refactor: let run subsystem own combat"
```

## Task 7: Reduce RoomViewWidget to Input and Presentation

**Files:**
- Modify: `UE/Graytail/Source/Graytail/UI/GT_RoomViewWidget.h`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_RoomViewWidget.cpp`
- Modify: `UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp`
- Test: `UE/Graytail/Source/Graytail/Debug/GT_CombatSimulationSmokeValidator.cpp`

- [ ] **Step 1: Add a widget-rebuild regression test**

Start combat, advance to a known state, destroy and recreate the room view, then compare simulation canonical state before and after. Expected: unchanged.

- [ ] **Step 2: Replace widget rule state**

Remove from the widget:

- enemy AI timers and decisions;
- damage and hit decisions;
- projectile gameplay positions;
- attack cooldown decisions;
- combat random draws;
- focus-derived pause rules;
- per-frame `FGT_DebugRunSnapshot` construction.

Keep held input collection and submit `FGT_CombatInputState` to the run subsystem.

- [ ] **Step 3: Render the lightweight view snapshot**

On `NativeTick`, request only `FGT_CombatViewSnapshot` and update visuals. Interpolate between previous/current snapshot for smooth rendering, but never feed interpolated positions back into gameplay.

- [ ] **Step 4: Connect explicit pause**

Pause-menu open calls `SetCombatPaused(true)`; close calls `SetCombatPaused(false)`. Modal overlays declare whether they pause combat instead of relying on keyboard focus.

- [ ] **Step 5: Audit for leaked gameplay rules**

Run:

```powershell
rg -n "FRand|RandRange|Apply.*Damage|EnemyAttack|Projectile.*Hit|AttackCooldown" UE/Graytail/Source/Graytail/UI/GT_RoomViewWidget.*
```

Expected: only presentation names remain; no rule mutation or gameplay RNG remains in UMG.

- [ ] **Step 6: Commit**

```powershell
git add UE/Graytail/Source/Graytail/UI UE/Graytail/Source/Graytail/Debug
git commit -m "refactor: make room view presentation only"
```

## Task 8: Full Verification and Draft PR

- [ ] **Step 1: Run editor build and full smoke**

Expected: `Fail=0`; deterministic combat cases pass for every frame schedule.

- [ ] **Step 2: Run packaged Development startup probe**

Expected: the PR 1 boot and persistence probe still passes.

- [ ] **Step 3: Perform visible combat checks**

Verify:

- movement and melee feel unchanged;
- ranged, shotgun, laser, and split monsters work;
- pause freezes combat immediately;
- rebuilding/opening overlays does not reset combat;
- no obvious projectile tunneling at low frame rate.

- [ ] **Step 4: Check code boundaries**

```powershell
git diff --check
rg -n "FMath::(FRand|RandRange)" UE/Graytail/Source/Graytail/Domains/Combat UE/Graytail/Source/Graytail/UI
rg -n "GetDebugRunSnapshot" UE/Graytail/Source/Graytail/UI
```

Expected: no gameplay global RNG and no per-frame debug snapshot in UI.

- [ ] **Step 5: Create the PR description**

Create `docs/pr/combat-simulation.md` with architecture, behavior preserved, deterministic schedules, smoke totals, packaged probe result, and residual manual feel-check notes.

- [ ] **Step 6: Push and open a draft stacked PR**

```powershell
git push -u origin codex/combat-simulation
gh pr create --draft --base codex/release-entry-save --head codex/combat-simulation --title "[codex] decouple deterministic combat simulation" --body-file docs/pr/combat-simulation.md
```
