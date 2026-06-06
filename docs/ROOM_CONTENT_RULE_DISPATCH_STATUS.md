# Room Content / Rule Dispatch Status

## Stage

Room Content / Rule Dispatch

## Branch

feature/room-content-rule-dispatch

## Implemented

- `BasicDebug` map writes room identity for one Event placeholder and one Combat placeholder.
- Event placeholder: `(4,1)`, `RoomContentId=Event_DebugChoice_01`, `RoomRuleId=Event_PresentOnly`.
- Combat placeholder: `(1,4)`, `RoomContentId=Combat_DebugDummy_01`, `RoomRuleId=Combat_StartOnly`.
- `RoomResolver` dispatches `Normal`, `Mine`, `Exit`, `Event`, and `Combat` room base types through handler methods.
- Event placeholder publishes `EventRoomEntered` and `EventPresented`.
- Combat placeholder publishes `CombatRoomEntered` and `CombatStarted`.
- Event payloads include room coordinates, content id, rule id, source system, actor ids, sequence id, numeric outcome, and success flag.
- `gt.Snapshot` reports current room identity.
- `gt.Minimap` can show visible Event and Combat room icons as `V` and `C`.
- `gt.Events` shows the placeholder event type counts.

## Validation

```text
GraytailEditor Win64 Development: passed
GT_RuntimeSmokeRunner: Overall=Pass, Pass=114, Fail=0, Count=114
```

The smoke count increased from 108 to 114 because six focused checks were added for placeholder cell identity and Event/Combat manual debug paths. The previous 108 checks remain covered.

## Manual Play Notes

Suggested Event placeholder path:

```text
gt.StartRun
gt.Move 1 0
gt.Move 2 0
gt.Move 3 0
gt.Move 4 0
gt.Move 4 1
gt.Snapshot
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
gt.Snapshot
gt.Events
```

## Explicitly Not Implemented

- Formal Event choice selection
- Event rewards or effects
- Combat attacks
- Enemy spawning or monster systems
- Inventory, rewards, save/load, or meta progression
- UMG, Blueprint assets, `.uasset` assets, or formal input bindings
