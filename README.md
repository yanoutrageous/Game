# Game / Graytail

Graytail is a minesweeper-style extraction game prototype. The repository now preserves the original Lua / UrhoX prototype and carries the Unreal Engine implementation under `UE/Graytail`.

## Current Active Branch

`main`

## Current Milestones

| Milestone | Tag | Status |
|---|---|---|
| Lua / UrhoX Prototype Baseline | `lua-prototype-baseline` | Preserved |
| UE Foundation | `ue-foundation-validated` | Validated |
| Gameplay Logic MVP | `gameplay-logic-mvp` | Validated |
| Editor Debug Entry Points | `editor-debug-entrypoints` | Validated |
| Map / Room Rule Boundary Preparation | `map-room-rule-boundary` | Validated |
| Editor Manual Play Validation | `editor-manual-play-validation` | Validated |
| Room Content / Rule Dispatch | `room-content-rule-dispatch` | Validated |
| Event / Combat Placeholder Interactions | `event-combat-placeholder-interactions` | Validated |
| Editor Playable Prototype V1 | `editor-playable-prototype-v1` | Validated |
| Prototype V1 Playability Polish | `prototype-v1-playability-polish` | Validated |
| Room Content / Rule Registry Minimal | `room-content-rule-registry-minimal` | Validated |
| Event Option / Combat Result Data Minimal | `event-option-combat-result-data-minimal` | Validated |
| Minimal Combat Dummy State | `minimal-combat-dummy-state` | Validated |
| Run Summary / Extract Summary Minimal | `run-extract-summary-minimal` | Validated |

## Current Validated Scope

- Lua / UrhoX prototype is preserved under `scripts/`, `assets/`, `.project/`, `.cli/`, and `game_material/`.
- Unreal Engine project exists under `UE/Graytail`.
- Gameplay Logic MVP is implemented.
- Editor-facing debug manual entrypoints are implemented.
- Map / Room Rule Boundary Preparation is implemented without expanding formal gameplay.
- Editor Manual Play Validation console commands are implemented.
- Room Content / Rule Dispatch placeholder rooms are wired into resolve and manual play observation paths.
- Event / Combat placeholder interactions can be completed through console commands without formal gameplay systems.
- Prototype V1 Playability Polish adds help, status, room detail, and one-shot demo console commands.
- Room Content / Rule Registry Minimal adds lightweight C++ definitions for placeholder room content and rules.
- Event Option / Combat Result Data Minimal adds lightweight C++ Event option and Combat result definitions.
- `gt.ChooseEventOption` and `gt.ResolveCombat` validate through the registry before resolving placeholder rooms.
- Minimal Combat Dummy State adds `gt.Attack`, starts dummy combat with HP 1, and resolves combat when dummy HP reaches 0.
- Run Summary / Extract Summary Minimal adds `gt.Summary` and successful Extract summary output.
- The Unreal implementation now includes a C++ player-facing UI, random Standard maps, inventory and loot, combat and event rooms, extraction settlement, persistent meta progression, settings, and tutorial flow.
- Runtime smoke is `168/168 pass`; the original MVP and placeholder behavior remains covered.
- Latest repository tracking document: `docs/PROJECT_CONTENT_TRACKING.md`.

## Validation Commands

Build:

```powershell
& "G:\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat" GraytailEditor Win64 Development "-Project=<repo>\UE\Graytail\Graytail.uproject" -WaitMutex
```

Smoke:

```powershell
& "G:\Epic\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\UE\Graytail\Graytail.uproject" -run=GT_RuntimeSmokeRunner -unattended -nopause -nop4 -nosplash -NoShaderCompile -log
```

Expected smoke result:

```text
Overall=Pass
Pass=168
Fail=0
Count=168
```

## Important Documents

Start with `docs/DOCUMENT_INDEX.md`.

Current status documents:

- `docs/editor-playable-prototype-v2.md`
- `docs/PROJECT_CONTENT_TRACKING.md`
- `docs/EVENT_OPTION_COMBAT_RESULT_DATA_STATUS.md`
- `docs/ROOM_CONTENT_RULE_REGISTRY_STATUS.md`
- `docs/PROTOTYPE_V1_PLAYABILITY_POLISH_STATUS.md`
- `docs/EVENT_COMBAT_PLACEHOLDER_INTERACTIONS_STATUS.md`
- `docs/ROOM_CONTENT_RULE_DISPATCH_STATUS.md`
- `docs/EDITOR_MANUAL_PLAY_VALIDATION_STATUS.md`
- `docs/MAP_ROOM_RULE_BOUNDARY_STATUS.md`
- `docs/EDITOR_PLAYABLE_PROTOTYPE_STATUS.md`
- `docs/GAMEPLAY_LOGIC_MVP_STATUS.md`
- `docs/UE_FOUNDATION_STATUS.md`

Architecture and design references:

- `docs/REFACTOR_ARCHITECTURE.md`
- `docs/UE_REFACTOR_IMPLEMENTATION.md`
- `docs/可行性判断.md`
- `docs/难度判断.md`

## Known Remaining Architecture Work

- Blueprint-authored UI/content assets are not tracked; the current player-facing interface is implemented in C++.
- `EffectSystem` is still a minimal boundary rather than a general data-driven effect interpreter or modifier pipeline.
- `RunContext` and the largest UI widgets still carry multiple responsibilities and should be split incrementally when those areas next change.
- Some presentation code still reads `RunContext` directly; new UI work should prefer `QueryFacade` or focused ViewModels.
- Automated coverage is centered on the commandlet smoke suite; packaging and long-session playtesting remain release checks.

## Editor Manual Console Commands

- `gt.StartRun [Seed] [Width Height]`
- `gt.Help`
- `gt.Commands`
- `gt.Status`
- `gt.Room`
- `gt.Move X Y`
- `gt.Scan X Y`
- `gt.Extract`
- `gt.Snapshot`
- `gt.Minimap`
- `gt.Events`
- `gt.ChooseEventOption [OptionId]`
- `gt.ResolveCombat [Result]`
- `gt.Attack`
- `gt.Summary`
- `gt.RunDemo`

Current placeholder data ids:

- `gt.ChooseEventOption Event_DebugOption_Continue`
- `gt.ChooseEventOption Event_DebugOption_Scout`
- `gt.ResolveCombat Combat_DebugResult_Success`
- `gt.ResolveCombat Combat_DebugResult_Retreat`

## Next Suggested Stage

Run a packaged-build validation and a focused manual playtest pass, then address only release-blocking issues. Keep larger `RunContext`/UI decomposition as incremental follow-up work rather than a last-minute rewrite.
