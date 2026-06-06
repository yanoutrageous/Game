# Document Index

## Current Status Documents

| Document | Role |
|---|---|
| `PROJECT_CONTENT_TRACKING.md` | Repository content and milestone tracking |
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

## Active Branch

`feature/map-room-rule-boundary`

## Current Validated Milestone

Map / Room Rule Boundary Preparation.

Latest known validation result:

```text
GraytailEditor Win64 Development: passed
GT_RuntimeSmokeRunner: 108/108 pass
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
Pass=108
Fail=0
Count=108
```

## Recommended Reading Order

1. `README.md`
2. `PROJECT_CONTENT_TRACKING.md`
3. `MAP_ROOM_RULE_BOUNDARY_STATUS.md`
4. `EDITOR_PLAYABLE_PROTOTYPE_STATUS.md`
5. `GAMEPLAY_LOGIC_MVP_STATUS.md`
6. `UE_FOUNDATION_STATUS.md`
7. `REFACTOR_ARCHITECTURE.md`
8. `UE_REFACTOR_IMPLEMENTATION.md`
9. `可行性判断.md` and `难度判断.md` as design references

## Current Implementation Boundary

Implemented and validated:

- Lua / UrhoX prototype baseline is preserved.
- UE project shell exists under `UE/Graytail`.
- Gameplay Logic MVP is implemented.
- Editor-facing debug manual entrypoints are implemented.
- Map / Room Rule Boundary Preparation is implemented.
- Runtime smoke baseline is `108/108 pass`.

Not implemented yet:

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
- Real map mode expansion
- Meta progression
