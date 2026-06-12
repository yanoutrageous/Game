# Prototype V1 Playability Polish Status

## Stage

Prototype V1 Playability Polish

## Branch

feature/prototype-v1-playability-polish

## Implemented

- `gt.Help` prints all manual play commands, parameter formats, and recommended demo flows.
- `gt.Commands` is an alias for `gt.Help`.
- `gt.Status` prints run state, player position, current room identity, trigger/resolve flags, total event count, and event type counts.
- `gt.Room` prints current room identity and hints for Event/Combat placeholder commands.
- `gt.RunDemo` runs a deterministic single-run path through the Event placeholder and Combat placeholder.
- Existing commands remain available: `gt.StartRun`, `gt.Move`, `gt.Scan`, `gt.Extract`, `gt.Snapshot`, `gt.Minimap`, `gt.Events`, `gt.ChooseEventOption`, and `gt.ResolveCombat`.

## RunDemo Path

`gt.RunDemo` uses one continuous run:

```text
gt.StartRun 42345 10 10
gt.Scan 1 1
gt.Move 1 0
gt.Move 2 0
gt.Move 3 0
gt.Move 4 0
gt.Move 4 1
gt.ChooseEventOption
gt.Move 3 1
gt.Move 2 1
gt.Move 1 1
gt.Move 1 2
gt.Move 1 3
gt.Move 1 4
gt.ResolveCombat
gt.Events
gt.Status
```

The path avoids the debug mine at `(2,2)`, reaches the Event placeholder at `(4,1)`, then reaches the Combat placeholder at `(1,4)` without restarting.

## Validation

```text
GraytailEditor Win64 Development: passed
GT_RuntimeSmokeRunner: Overall=Pass, Pass=128, Fail=0, Count=128
```

The smoke count increased from 122 to 128 because six checks were added for manual play help/status/room output and `gt.RunDemo` completion. The previous 122 checks remain covered.

## Boundaries

- Console commands continue to call `UGT_DebugSubsystem` and existing debug/gameplay/query boundaries.
- `gt.RunDemo` composes existing debug commands and does not bypass `CommandProcessor`.
- Console code does not directly mutate `RunContext`, `TruthMap`, `IntelMap`, player position, or `RunState`.

## Explicitly Not Implemented

- UMG or player-facing UI
- Blueprint assets or `.uasset` assets
- Content or Config changes
- Formal input bindings
- Random map generation
- Full Event choice systems
- Full combat systems, enemies, HP, damage, or loot
- Inventory, rewards, save/load, or meta progression
