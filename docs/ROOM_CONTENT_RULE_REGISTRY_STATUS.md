# Room Content / Rule Registry Status

## Stage

Room Content / Rule Registry Minimal

## Branch

feature/room-content-rule-registry-minimal

## Implemented

- `FGT_RoomContentDef` defines lightweight C++ room content metadata.
- `FGT_RoomRuleDef` defines lightweight C++ room rule metadata.
- `UGT_ContentRegistry` registers and queries room content/rule definitions.
- `RoomResolver` validates Event/Combat placeholder rooms through `UGT_ContentRegistry`.
- Event/Combat interaction defaults are read from registry definitions.
- `gt.Status`, `gt.Room`, and `gt.Events` can show room definition display names and debug descriptions.

## Registered Room Content

- `Event_DebugChoice_01`
  - RoomBaseType: `Event`
  - DisplayName: `Debug Event Choice`
  - DefaultRuleId: `Event_PresentOnly`
  - DefaultOptionId: `Event_DebugOption_Continue`
- `Combat_DebugDummy_01`
  - RoomBaseType: `Combat`
  - DisplayName: `Debug Dummy Combat`
  - DefaultRuleId: `Combat_StartOnly`
  - DefaultResultId: `Success`

## Registered Room Rules

- `Event_PresentOnly`
  - RoomBaseType: `Event`
  - DisplayName: `Present Only Event`
  - DefaultOptionId: `Event_DebugOption_Continue`
- `Combat_StartOnly`
  - RoomBaseType: `Combat`
  - DisplayName: `Start Only Combat`
  - DefaultResultId: `Success`

## Validation

```text
GraytailEditor Win64 Development: passed
GT_RuntimeSmokeRunner: Overall=Pass, Pass=134, Fail=0, Count=134
```

The smoke count increased from 128 to 134 because six checks were added for registry lookup and Event/Combat placeholder definition usage. The previous 128 checks remain covered.

## Boundaries

- No DataAsset, DataTable, Blueprint, `.uasset`, JSON, or external data loading is used.
- `MapGenerator` still writes debug/basic room ids into the truth map.
- `RoomResolver` uses registry definitions when resolving or interacting with Event/Combat placeholder rooms.
- Console code does not directly mutate `RunContext`, `TruthMap`, `IntelMap`, player position, or `RunState`.

## Explicitly Not Implemented

- Formal Event option systems
- Formal combat systems, enemies, HP, damage, or loot
- Inventory, rewards, save/load, or meta progression
- Random map generation
- UMG, Blueprint assets, `.uasset` assets, Content changes, or Config changes
