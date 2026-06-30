# Graytail JSON Game Data Hardening

## Goal

Make JSON balance data safe to edit without turning configuration mistakes into
crashes, corrupted saves, mixed rule versions, or seeded gameplay regressions.

## Rule Data And Runtime State

JSON files define rules such as base HP, item bonuses, mine damage, drop rates,
and talent effects. The engine loads and validates these rules once at startup.
The accepted snapshot is read-only for the lifetime of the process.

Runtime state remains mutable. Equipping armor, unlocking a talent, taking
damage, gaining gold, and consuming an item immediately change the current
player or run state. These operations apply modifiers from the loaded rules and
do not require a restart.

Editing a JSON file on disk requires restarting the game. Production gameplay
does not support live rule replacement.

## Loading And Validation

All six files form one configuration unit. A candidate is accepted only when:

- Every required field exists with the exact JSON type.
- Unknown fields are rejected so spelling mistakes cannot become zero values.
- IDs are unique, supported, and resolve to required runtime assets.
- Numeric values satisfy field-specific and cross-field constraints.
- Protocol thresholds are complete and deterministic regardless of authoring
  mistakes.
- Random maps can place at least one exit.
- Manual layouts use in-bounds, non-conflicting coordinates and contain a valid
  spawn and exit.
- Drop-table qualities resolve to non-empty item pools.
- Persistent meta IDs remain compatible with existing saves.

Loading is transactional. A failed test reload reports candidate errors without
discarding the last valid snapshot. On initial startup, invalid data leaves no
snapshot and blocks starting a run.

## Failure Behaviour

Invalid game data must not crash the process. The main menu remains available
and displays the validation errors. SaveGame loading may deserialize state while
rules are invalid, but catalog-dependent sanitization waits until valid rules
exist.

Catalog facades return explicit failure where data can be unavailable. They do
not rely on a check followed by a null dereference.

## Save Compatibility

UE SaveGame remains the only official load source. The debug JSON mirror is
write-only and never affects gameplay.

Unknown persistent equipment, talent, or consumable IDs are handled by an
explicit compatibility policy. Current shipped IDs are required by validation;
future renames need a SaveVersion migration or alias rather than silent removal.

Smoke tests use a unique temporary save slot and delete it afterward. They never
write the normal `GraytailMeta` slot.

## Gameplay Equivalence

With the default JSON values, seeded map, monster, loot, event, and lucky-coin
results must match the pre-JSON implementation. Configurable probabilities use
a stable mapping that preserves the old lucky-coin even/odd split at 50 percent.

Every configured trigger amount affects gameplay. Settlement and chest bonuses
must not retain ID-specific constants that disagree with JSON.

## Cache Policy

Production uses one startup snapshot. Test-only reloads increment a revision,
and every derived facade cache refreshes from that revision. This prevents tests
and editor tooling from observing a mixture of old item or monster data and new
rule data.

## Verification

Add failing regression checks before each fix for:

- Missing, misspelled, unknown, and wrong-type JSON fields.
- Assetless item IDs and missing quality pools.
- Invalid manual and random map layouts.
- Misordered or incomplete protocol thresholds.
- Invalid meta effect ranges and persistent ID removal.
- Existing-save startup with invalid game data.
- Isolated smoke save slots.
- Item and monster cache revision changes.
- Lucky-coin seed equivalence.
- JSON-backed settlement and chest trigger amounts.

Then run the GraytailEditor build, complete headless smoke suite, Win64
Development packaging, packaged startup, and a Shipping compile/package gate.
