# Project Content Tracking

## Repository State

- Branch: feature/editor-playable-prototype
- HEAD: df4702b3fcb295e2e99c0103d2203498427704fa
- Remote: https://github.com/yanoutrageous/Game.git
- Validation tags: lua-prototype-baseline, ue-foundation-validated, gameplay-logic-mvp, editor-debug-entrypoints
- Remote branches: origin/main, origin/refactor/ue-foundation, origin/feature/gameplay-vertical-slice, origin/feature/editor-playable-prototype

## Milestone Timeline

| Milestone | Tag / Commit | Status | Primary Docs |
|---|---|---|---|
| Lua / UrhoX prototype baseline | lua-prototype-baseline / a20752d | Preserved baseline | README.md, docs/game-design.md, docs/dev-plan.md |
| UE foundation | ue-foundation-validated / b278b8b | Validated foundation recorded; validation commit was d5a4077 | docs/UE_FOUNDATION_STATUS.md |
| Gameplay logic MVP | gameplay-logic-mvp / 004e746 | Logic-level playable MVP recorded; validation commit was acffc62 | docs/GAMEPLAY_LOGIC_MVP_STATUS.md |
| Editor-facing debug entrypoints | editor-debug-entrypoints / df4702b | Editor-facing debug-operation layer recorded; validation commit was 298049b | docs/EDITOR_PLAYABLE_PROTOTYPE_STATUS.md |

## Current Validated Scope

- Lua / UrhoX prototype baseline remains tracked under `scripts/`, `assets/`, `.project/`, `.cli/`, and `game_material/`.
- UE foundation exists under `UE/Graytail` with project shell, config, source module, and placeholder content folders.
- Gameplay logic MVP covers run start, movement, scan, room resolution, mine failure, exit extraction, event recording, and commandlet smoke validation.
- Editor-facing debug entrypoints expose debug start, move, scan, extract, snapshot, minimap view data, and event summary through `UGT_DebugSubsystem`.

## Project Content Inventory

| Area | Path | Status | Notes |
|---|---|---|---|
| Lua / UrhoX prototype | `scripts/` | Tracked | 49 tracked Lua prototype files and metadata; includes `scripts/main.lua`, systems, scenes, UI, and tests. |
| UE project shell | `UE/Graytail` | Tracked | `Graytail.uproject`, config files, source module, and empty content folder markers are tracked. |
| UE Core runtime | `UE/Graytail/Source/Graytail/Core` | Tracked | 20 tracked core runtime files including command bus, command processor, event bus, query facade, room resolver, run context, and run subsystem. |
| UE Map / Intel | `UE/Graytail/Source/Graytail/Domains/Map` | Tracked | `GT_MapTypes.h` defines map, truth, and intel structures. |
| UE Data definitions | `UE/Graytail/Source/Graytail/Data` | Tracked | 12 tracked DataAsset and effect/type definition files. |
| UE UI ViewModels | `UE/Graytail/Source/Graytail/UI/ViewModels` | Tracked | MiniMap ViewModel source/header plus `.gitkeep`; no UMG assets are tracked. |
| UE Save / Debug | `UE/Graytail/Source/Graytail/Save`, `UE/Graytail/Source/Graytail/Debug` | Tracked | 12 tracked save/debug files; `GT_DebugTypes.h`, `GT_RuntimeSmokeRunner`, and `GT_RuntimeSmokeValidator` are present. |
| Docs | `docs/`, `README.md` | Tracked | 16 tracked docs/readme files before this document; status docs and design/planning docs coexist. |
| Tools | `tools/`, `.cli/`, `.claude/skills` | Tracked | Local tooling, CLI helpers, Lua LSP helpers, and skill install scripts are tracked. |
| Assets / game_material | `assets/`, `game_material/`, `.project/game_material/` | Tracked | 410 tracked art/audio/video/material files and metadata. |
| Config / project metadata | `.project/`, `.gitignore` | Tracked | Project metadata and ignore rules are tracked. |

## Runtime Validation

- GraytailEditor: latest known validation passed during the editor debug entrypoints milestone.
- Runtime smoke: latest known commandlet validation passed during the editor debug entrypoints milestone.
- Latest known smoke result: `Overall=Pass`, `Pass=108`, `Fail=0`, `Count=108`.
- Current tracking check: no `.uasset` files are tracked.
- Current tracking check: `Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` are not tracked by Git.

## Documents Review

| Document | Role | Current / Historical | Notes |
|---|---|---|---|
| `docs/UE_FOUNDATION_STATUS.md` | UE foundation status | Current milestone status | Tag points to status commit `b278b8b`; validation commit recorded as `d5a4077`. |
| `docs/GAMEPLAY_LOGIC_MVP_STATUS.md` | Gameplay logic MVP status | Current milestone status | Tag points to status commit `004e746`; validation commit recorded as `acffc62`. |
| `docs/EDITOR_PLAYABLE_PROTOTYPE_STATUS.md` | Editor-facing debug entrypoints status | Current milestone status | Tag points to status commit `df4702b`; validation commit recorded as `298049b`. |
| `docs/可行性判断.md` | Feasibility and balance analysis | Current design analysis | Added with `298049b`; supports design feasibility and risk judgment. |
| `docs/难度判断.md` | Difficulty and balance analysis | Current design analysis | Added with `298049b`; supports difficulty scaling and demo positioning. |
| `docs/CODEX_TASKS.md` | Codex implementation plan | Historical planning | Useful for original UE refactor scope; not a current status source for completed debug entrypoints. |
| `docs/REFACTOR_ARCHITECTURE.md` | Architecture guide | Current reference | Defines command/event/query/viewmodel boundaries and state ownership rules. |
| `docs/UE_REFACTOR_IMPLEMENTATION.md` | UE implementation guide | Current reference with historical scope notes | Maps architecture into UE classes and notes UI/Blueprint boundaries. |
| `README.md` | Prototype overview | Historical / likely stale | Current terminal rendering shows encoding mojibake, and content still points mainly at Lua prototype docs and scripts. |

## Explicitly Not Implemented

- UMG / player-facing UI
- Blueprint assets
- Full map generation
- Loot / inventory gameplay
- Combat gameplay
- Event room effects
- Reward settlement
- Save / Load disk flow
- Effect interpreter
- ModifierSystem
- Meta progression

## Tracking Notes

This document tracks repository content and milestone alignment. It does not replace detailed design documents.

Status documents are treated as milestone records. Some tags point to the status-recording commit, while each status document may separately name the validation commit that was verified before the status document was added.
