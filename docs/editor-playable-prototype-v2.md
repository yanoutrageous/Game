# Graytail Editor Playable Prototype V2

## Project State

- Project: Graytail / Game UE Editor console playable prototype.
- UE project directory: `UE/Graytail`.
- UE project file: `UE/Graytail/Graytail.uproject`.
- Current primary branch: `feature/editor-playable-prototype`.
- Current baseline commit: `05d0fea5f6d6d4c91c032ba080f999d51e1a60b2`.
- Current latest tag: `run-extract-summary-minimal`.
- Current smoke baseline: `Overall=Pass`, `Pass=163`, `Fail=0`, `Count=163`.

## V2 Completed Scope

- Room Content / Rule Registry Minimal.
- Event Option / Combat Result Data Minimal.
- Minimal Combat Dummy State + `gt.Attack`.
- Run Summary / Extract Summary Minimal.

These V2 slices keep the prototype C++ only. They add data boundaries, console-observable placeholder interaction, dummy combat state, and extraction summary output without adding formal UI, assets, rewards, inventory, save/load, or full combat/event systems.

## Current Console Commands

```text
gt.Help
gt.Commands
gt.StartRun [Seed] [Width Height]
gt.Status
gt.Room
gt.Move X Y
gt.Scan X Y
gt.Extract
gt.Snapshot
gt.Minimap
gt.Events
gt.ChooseEventOption [OptionId]
gt.ResolveCombat [Result]
gt.Attack
gt.Summary
gt.RunDemo
```

Useful data-driven examples:

```text
gt.ChooseEventOption Event_DebugOption_Continue
gt.ChooseEventOption Event_DebugOption_Scout
gt.ResolveCombat Combat_DebugResult_Success
gt.ResolveCombat Combat_DebugResult_Retreat
```

## Current Playable Loop

- `gt.StartRun` starts the debug/basic run.
- `gt.Move X Y` moves through the command path.
- `gt.Scan X Y` records scan behavior.
- Entering the debug mine still fails the run.
- Reaching the exit and running `gt.Extract` succeeds.
- Event room flow supports entered, presented, option chosen, and resolved events.
- Combat room flow supports entered and started events.
- Entering the Combat placeholder starts dummy combat with `DummyEnemyHp=1`.
- `gt.Attack` reduces dummy HP to `0` and triggers `CombatResolved`.
- `gt.ResolveCombat Combat_DebugResult_Success` and `gt.ResolveCombat Combat_DebugResult_Retreat` remain available as registry-validated fallback paths.
- Event option and Combat result IDs are validated through the C++ content registry.
- Successful `gt.Extract` creates a run summary.
- `gt.Summary` prints the current run summary or a clear no-summary state.
- `gt.Status`, `gt.Room`, `gt.Snapshot`, `gt.Minimap`, and `gt.Events` expose current run, room, map, event, combat, and summary state.
- `gt.RunDemo` runs a deterministic one-command path through Event, Combat, `gt.Attack`, Exit, Extract, and Summary.

The current `gt.RunDemo` path starts `gt.StartRun 42345 10 10`, scans `(1,1)`, moves to Event `(4,1)`, chooses `Event_DebugOption_Continue`, moves to Combat `(1,4)`, attacks the dummy combat, moves to Exit `(9,9)`, extracts, and prints `gt.Summary`.

## Current Registry Data

Event options:

- `Event_DebugOption_Continue`
- `Event_DebugOption_Scout`

Combat results:

- `Combat_DebugResult_Success`
- `Combat_DebugResult_Retreat`

Related room content/rule IDs remain:

- Event room content: `Event_DebugChoice_01`
- Event room rule: `Event_PresentOnly`
- Combat room content: `Combat_DebugDummy_01`
- Combat room rule: `Combat_StartOnly`

## Current Run Summary Fields

`FGT_RunSummary` currently records:

- `bSummaryAvailable`
- `Outcome`
- `bExtracted`
- `FinalPlayerX`
- `FinalPlayerY`
- `TotalEventCount`
- `Seed`
- `MapWidth`
- `MapHeight`
- `bCombatActive`
- `bCombatResolved`
- `LastCombatResultId`

Summary generation happens only on successful Extract. Failed Extract does not create or overwrite a valid summary, and `StartRun` clears the previous run summary.

## Explicitly Not Implemented

- UMG.
- Blueprint assets.
- `.uasset`.
- Formal input binding.
- Random map generation.
- Formal combat system.
- Enemy system expansion.
- Multiple enemies.
- AI.
- Skills.
- Equipment.
- Damage formula.
- Loot.
- Reward settlement.
- Score.
- Inventory.
- Save / Load.
- Meta progression.
- DataAsset / DataTable.
- External JSON configuration.
- Full effect interpreter.
- Formal UI version.

## Architecture Boundaries

- `RunSubsystem`: run lifecycle and coordination entrypoint.
- `RunContext`: current run state container, including combat runtime state and run summary.
- `CommandProcessor`: command execution boundary.
- `RoomResolver`: room entry and room interaction dispatch.
- `EventBus`: event recording and event statistics.
- `QueryFacade`: read-only query boundary.
- `MiniMapViewModel`: `IntelMap` to minimap view data projection.
- `DebugSubsystem`: Editor/debug manual entrypoint.
- `ContentRegistry`: lightweight C++ content, rule, option, and result registry.
- `EditorManualPlayCommands`: console command layer.

Boundary rules for the next stage:

- Console commands must not directly mutate `RunContext`, `TruthMap`, `IntelMap`, player coordinates, `RunState`, `CombatRuntimeState`, or `RunSummary`.
- `StartRun`, `Move`, `Scan`, `Extract`, and `Attack` must go through `DebugSubsystem`, `RunSubsystem`, `CommandProcessor`, and `RoomResolver` where applicable.
- `ChooseEventOption` and `ResolveCombat` must continue through gameplay/core layer code and registry validation.
- `Status`, `Room`, `Snapshot`, `Minimap`, `Events`, and `Summary` are read-only output commands.

## Verification

Build:

```powershell
& "D:\UE\UE_5.7\Engine\Binaries\ThirdParty\DotNet\8.0.412\win-x64\dotnet.exe" "D:\UE\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" GraytailEditor Win64 Development -Project="D:\AGAME1\UE\Graytail\Graytail.uproject" -WaitMutex -FromMsBuild
```

Smoke:

```powershell
& "D:\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\AGAME1\UE\Graytail\Graytail.uproject" -run=GT_RuntimeSmokeRunner -unattended -nop4 -nosplash -NoShaderCompile -log
```

Expected:

```text
Overall=Pass
Pass=163
Fail=0
Count=163
```

## Recommended Next Closeout

After this V2 document branch is reviewed and validated, the next safe closeout step is:

- Create tag `editor-playable-prototype-v2`.
- Fast-forward `feature/editor-playable-prototype` to the V2 documentation commit.
- Push `feature/editor-playable-prototype`.
- Delete `feature/editor-playable-prototype-v2-docs`.

Do not perform those closeout actions as part of this documentation branch.
