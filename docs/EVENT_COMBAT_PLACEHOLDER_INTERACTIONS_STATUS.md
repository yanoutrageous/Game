# Event / Combat Placeholder Interactions Status

## Stage

Event / Combat Placeholder Interaction

## Branch

feature/event-combat-placeholder-interactions

## Implemented

- `gt.ChooseEventOption [OptionId]` completes the current Event placeholder room when the player is in an Event room.
- `gt.ResolveCombat [Result]` completes the current Combat placeholder room when the player is in a Combat room.
- Event placeholder completion publishes `EventOptionChosen` and `EventResolved`.
- Combat placeholder completion publishes `CombatResolved`.
- Safe failure paths publish visible failure events and do not crash.
- `gt.Events` includes recent event content id, rule id, sequence id, success flag, and payload text.
- Console commands route through `UGT_DebugSubsystem` and the gameplay command path; they do not directly mutate `RunContext`, `TruthMap`, `IntelMap`, player position, or `RunState`.

## Manual Play Notes

Suggested Event placeholder path:

```text
gt.StartRun
gt.Move 1 0
gt.Move 2 0
gt.Move 3 0
gt.Move 4 0
gt.Move 4 1
gt.Events
gt.ChooseEventOption
gt.Events
```

Suggested Combat placeholder path:

```text
gt.StartRun
gt.Move 0 1
gt.Move 0 2
gt.Move 0 3
gt.Move 0 4
gt.Move 1 4
gt.Events
gt.ResolveCombat
gt.Events
```

## Event Payload Notes

- Event interaction events include `EventType`, `SourceSystem`, room coordinates, `ContentId`, `RuleId`, `SequenceId`, payload text, numeric value, and success state where available.
- `gt.ChooseEventOption` defaults to `Event_DebugOption_Continue`.
- `gt.ResolveCombat` defaults to `Success`.
- `ResolveCombat Fail` is intentionally rejected as an unsupported placeholder result and does not fail the run.

## Validation

```text
GraytailEditor Win64 Development: passed
GT_RuntimeSmokeRunner: Overall=Pass, Pass=122, Fail=0, Count=122
```

The smoke count increased from 114 to 122 because eight focused checks were added for Event/Combat placeholder interaction success and safe failure paths. The previous 114 checks remain covered.

## Explicitly Not Implemented

- Formal Event choice systems
- Event rewards or effects
- Combat attacks
- Enemy spawning, HP, damage, or loot systems
- Inventory, rewards, save/load, or meta progression
- UMG, Blueprint assets, `.uasset` assets, or formal input bindings
