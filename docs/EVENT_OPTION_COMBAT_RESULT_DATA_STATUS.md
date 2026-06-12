# Event Option / Combat Result Data Minimal Status

## Stage

Event Option / Combat Result Data Minimal

## Branch

`feature/event-option-combat-result-data-minimal`

## Scope

This stage extends the lightweight C++ content registry with placeholder Event option and Combat result data.

No DataAsset, DataTable, Blueprint, `.uasset`, JSON, Content, or Config data source was added.

## Added Definitions

Event options:

- `Event_DebugOption_Continue`
  - DisplayName: `Continue`
  - Resolves room: yes
  - Payload: resolves the Event room without rewards.
- `Event_DebugOption_Scout`
  - DisplayName: `Scout`
  - Resolves room: yes
  - Payload: records a Scout placeholder choice without scan rewards, reveals, items, or resources.

Combat results:

- `Combat_DebugResult_Success`
  - DisplayName: `Success`
  - Resolves room: yes
  - Event: `CombatResolved`
  - Payload: resolves Combat without HP, damage, loot, rewards, or RunState effects.
- `Combat_DebugResult_Retreat`
  - DisplayName: `Retreat`
  - Resolves room: yes
  - Event: `CombatRetreated`
  - Payload: resolves Combat without RunFailed, HP, damage, loot, or rewards.

## Runtime Behavior

- `gt.ChooseEventOption [OptionId]` validates the selected option through `UGT_ContentRegistry`.
- No `OptionId` uses the room content/rule default option.
- Invalid options publish `EventOptionChooseFailed` and leave run success/failure state unchanged.
- `gt.ResolveCombat [Result]` validates the selected result through `UGT_ContentRegistry`.
- No `Result` uses the room content/rule default result.
- Invalid results publish `CombatResolveFailed` and leave run success/failure state unchanged.
- `gt.Room`, `gt.Status`, `gt.Snapshot`, and `gt.Events` expose available option/result IDs and registry display/debug text where available.
- `gt.RunDemo` uses explicit registry IDs: `Event_DebugOption_Continue` and `Combat_DebugResult_Success`.

## Validation

Build:

```text
GraytailEditor Win64 Development: passed
```

Smoke:

```text
Overall=Pass
Pass=144
Fail=0
Count=144
```

The previous `134/134` smoke behavior remains covered.

## Not Implemented

- Formal event option effect execution
- Event rewards
- Formal combat system
- Attack, enemy actors, HP, damage, loot, or drops
- Inventory
- Save / Load
- Meta progression
- UMG
- Blueprint
- `.uasset`
- Content or Config data
