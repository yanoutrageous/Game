# Game / Graytail

Graytail is a minesweeper-style extraction game prototype. The repository now preserves the original Lua / UrhoX prototype and carries the Unreal Engine implementation under `UE/Graytail`.

## Current Active Branch

`feature/editor-manual-play-validation`

## Current Milestones

| Milestone | Tag | Status |
|---|---|---|
| Lua / UrhoX Prototype Baseline | `lua-prototype-baseline` | Preserved |
| UE Foundation | `ue-foundation-validated` | Validated |
| Gameplay Logic MVP | `gameplay-logic-mvp` | Validated |
| Editor Debug Entry Points | `editor-debug-entrypoints` | Validated |
| Map / Room Rule Boundary Preparation | `map-room-rule-boundary` | Validated |
| Editor Manual Play Validation | `editor-manual-play-validation` | Validated |

## Current Validated Scope

- Lua / UrhoX prototype is preserved under `scripts/`, `assets/`, `.project/`, `.cli/`, and `game_material/`.
- Unreal Engine project exists under `UE/Graytail`.
- Gameplay Logic MVP is implemented.
- Editor-facing debug manual entrypoints are implemented.
- Map / Room Rule Boundary Preparation is implemented without expanding formal gameplay.
- Editor Manual Play Validation console commands are implemented.
- Runtime smoke baseline is `108/108 pass`.
- Latest implementation commit: `bc6c8678617169bc8c6d3d270994705c56fa7c2f`.
- Latest repository tracking document: `docs/PROJECT_CONTENT_TRACKING.md`.

## Validation Commands

Build:

```powershell
& "D:\UE\UE_5.7\Engine\Binaries\ThirdParty\DotNet\8.0.412\win-x64\dotnet.exe" "D:\UE\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" GraytailEditor Win64 Development -Project="<repo>\UE\Graytail\Graytail.uproject" -WaitMutex -FromMsBuild
```

Smoke:

```powershell
& "D:\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\UE\Graytail\Graytail.uproject" -run=GT_RuntimeSmokeRunner -unattended -nop4 -nosplash -NoShaderCompile -log
```

Expected smoke result:

```text
Overall=Pass
Pass=108
Fail=0
Count=108
```

## Important Documents

Start with `docs/DOCUMENT_INDEX.md`.

Current status documents:

- `docs/PROJECT_CONTENT_TRACKING.md`
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

## Explicitly Not Implemented Yet

- Player-facing UI / UMG
- Blueprint assets
- Input binding
- Map rendering
- Loot / inventory gameplay
- Combat gameplay
- Event room effects
- Reward settlement
- Save / Load disk flow
- Effect interpreter
- ModifierSystem
- Random map generation
- Formal Combat / Event room gameplay
- Meta progression

## Editor Manual Console Commands

- `gt.StartRun [Seed] [Width Height]`
- `gt.Move X Y`
- `gt.Scan X Y`
- `gt.Extract`
- `gt.Snapshot`
- `gt.Minimap`
- `gt.Events`

## Next Suggested Stage

Prepare Room Content / Rule Dispatch and Event / Combat placeholder manual play behind the existing boundaries.
