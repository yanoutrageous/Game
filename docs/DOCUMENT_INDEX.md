# Document Index

## Current Status Documents

| Document | Role |
|---|---|
| `PROJECT_CONTENT_TRACKING.md` | Repository content and milestone tracking |
| `EVENT_OPTION_COMBAT_RESULT_DATA_STATUS.md` | Event option and Combat result C++ data status |
| `ROOM_CONTENT_RULE_REGISTRY_STATUS.md` | Room content/rule C++ registry status |
| `PROTOTYPE_V1_PLAYABILITY_POLISH_STATUS.md` | Prototype V1 console playability polish status |
| `EVENT_COMBAT_PLACEHOLDER_INTERACTIONS_STATUS.md` | Event / Combat placeholder console interaction status |
| `ROOM_CONTENT_RULE_DISPATCH_STATUS.md` | Room content/rule dispatch placeholder status |
| `EDITOR_MANUAL_PLAY_VALIDATION_STATUS.md` | Editor manual play console command status |
| `MAP_ROOM_RULE_BOUNDARY_STATUS.md` | Map / Room Rule Boundary Preparation status |
| `EDITOR_PLAYABLE_PROTOTYPE_STATUS.md` | Editor-facing debug entrypoints status |
| `GAMEPLAY_LOGIC_MVP_STATUS.md` | Gameplay logic MVP validation status |
| `UE_FOUNDATION_STATUS.md` | UE foundation validation status |

## Design / Planning References

| Document | Role |
|---|---|
| `REFACTOR_ARCHITECTURE.md` | Architecture target and module boundaries |
| `UE_REFACTOR_IMPLEMENTATION.md` | UE implementation plan |
| `CODEX_TASKS.md` | Historical task plan |
| `可行性判断.md` | Demo feasibility and numeric assumptions |
| `难度判断.md` | Difficulty structure and map configuration analysis |
| `game-design.md` | Original Lua / UrhoX prototype design notes |
| `dev-plan.md` | Original Lua / UrhoX prototype development plan |

## Milestone Tags

| Tag | Meaning |
|---|---|
| `lua-prototype-baseline` | Preserved Lua / UrhoX prototype baseline |
| `ue-foundation-validated` | Validated UE foundation |
| `gameplay-logic-mvp` | Validated logic-level playable MVP |
| `editor-debug-entrypoints` | Validated editor-facing debug manual entrypoints |
| `map-room-rule-boundary` | Validated map / room rule boundary preparation |
| `editor-manual-play-validation` | Validated editor manual play console command entrypoint |

## Active Branch

`feature/event-option-combat-result-data-minimal`

## Current Validated Milestone

Event Option / Combat Result Data Minimal.

Latest known validation result:

```text
GraytailEditor Win64 Development: passed
GT_RuntimeSmokeRunner: 144/144 pass
```

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
Pass=144
Fail=0
Count=144
```

## Recommended Reading Order

1. `README.md`
2. `PROJECT_CONTENT_TRACKING.md`
3. `EVENT_OPTION_COMBAT_RESULT_DATA_STATUS.md`
4. `ROOM_CONTENT_RULE_REGISTRY_STATUS.md`
5. `PROTOTYPE_V1_PLAYABILITY_POLISH_STATUS.md`
6. `EVENT_COMBAT_PLACEHOLDER_INTERACTIONS_STATUS.md`
7. `ROOM_CONTENT_RULE_DISPATCH_STATUS.md`
8. `EDITOR_MANUAL_PLAY_VALIDATION_STATUS.md`
9. `MAP_ROOM_RULE_BOUNDARY_STATUS.md`
10. `EDITOR_PLAYABLE_PROTOTYPE_STATUS.md`
11. `GAMEPLAY_LOGIC_MVP_STATUS.md`
12. `UE_FOUNDATION_STATUS.md`
13. `REFACTOR_ARCHITECTURE.md`
14. `UE_REFACTOR_IMPLEMENTATION.md`
14. `可行性判断.md` and `难度判断.md` as design references

## Current Implementation Boundary

Implemented and validated:

- Lua / UrhoX prototype baseline is preserved.
- UE project shell exists under `UE/Graytail`.
- Gameplay Logic MVP is implemented.
- Editor-facing debug manual entrypoints are implemented.
- Map / Room Rule Boundary Preparation is implemented.
- Editor Manual Play Validation console commands are implemented.
- Manual play commands call `UGT_DebugSubsystem` and do not directly mutate run state or map state.
- Room Content / Rule Dispatch placeholder rooms are implemented.
- Event / Combat placeholder interactions are exposed through `gt.ChooseEventOption` and `gt.ResolveCombat`.
- Prototype V1 playability polish commands are exposed through `gt.Help`, `gt.Commands`, `gt.Status`, `gt.Room`, and `gt.RunDemo`.
- Room Content / Rule Registry Minimal is implemented as lightweight C++ definitions.
- Event Option / Combat Result Data Minimal is implemented as lightweight C++ definitions.
- Runtime smoke is `144/144 pass`.

Not implemented yet:

- Player-facing UI / UMG
- Blueprint assets
- Input binding
- Map rendering
- Loot / inventory gameplay
- Combat gameplay
- Event room effects
- Formal Event option effect execution
- Formal Event option selection
- Combat actor/enemy systems
- Reward settlement
- Save / Load disk flow
- Effect interpreter
- ModifierSystem
- Random map generation
- Real map mode expansion
- Meta progression
