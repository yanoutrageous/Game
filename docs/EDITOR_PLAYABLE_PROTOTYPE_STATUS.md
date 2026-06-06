# Editor Playable Prototype Status

## Validated Branch

feature/editor-playable-prototype

## Validated Commit

298049be4d7c9c8152a1115effa0cb8c479e762d

## Validation Result

- GraytailEditor target: passed
- Runtime smoke commandlet: passed
- Smoke result: 108/108 pass
- Debug manual entrypoints: implemented
- Remote branch: pushed

## Completed Scope

- DebugStartNewRun
- DebugMoveTo
- DebugScanCell
- DebugExtract
- GetDebugRunSnapshot
- GetDebugMiniMapViewData
- GetDebugEventSummary
- Structured debug snapshot
- Structured event summary
- Debug API smoke validation

## Debug API Boundary

- DebugStartNewRun calls RunSubsystem::StartNewRun
- DebugMoveTo uses SubmitCommand / Move
- DebugScanCell uses SubmitCommand / Scan
- DebugExtract uses SubmitCommand / Extract
- Debug APIs do not directly mutate RunContext, TruthMap, IntelMap, or player coordinates
- MiniMap data comes from QueryFacade / MiniMapViewModel
- Event summary is read-only

## Explicitly Not Implemented

- UMG / player-facing UI
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
- Meta progression

## Tag

editor-debug-entrypoints

## Notes

This milestone represents an Editor-facing debug-operation layer over the validated gameplay logic MVP. It is still not a full player-facing UI prototype.
