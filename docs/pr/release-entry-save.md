# Summary

This PR turns the current prototype into a packaged, player-facing build and
hardens the complete save/run lifecycle.

It is intentionally based on `codex/json-game-data`. The next gameplay/data PR
will stack on this branch after this PR is reviewed.

## Player-visible changes

- Packaged builds now boot through the real `AGT_GameMode` and
  `AGT_PlayerController`, create one HUD, and open the main menu without console
  commands.
- Main-menu start uses the same production `UGT_RunSubsystem` path as normal
  play.
- Save recovery, unsupported versions, corruption, write failures, and
  interrupted runs now have readable menu/toast feedback.
- A failed settlement stays on the result screen and exposes `Retry Save`;
  starting another run, returning to title, or quitting cannot silently discard
  a pending settlement.
- Graceful return/quit paths persist abandonment before leaving.
- Deployment/loadout save failures are rolled back and shown in the deployment
  summary.
- Cheat UI, GM methods, debug console registration, and the packaged startup
  probe are excluded from Shipping.

## Save and run safety

- `GraytailMeta` remains the authoritative UE SaveGame. Existing version-1 saves
  migrate to version 2 without manual conversion.
- Version 2 adds an active-run escrow containing run identity, difficulty,
  equipped items, consumed loadout items, and start time.
- Starting a standard run first commits escrow and consumes loadout, then creates
  the run context. A failed write creates no run.
- Extraction, failure, and abandonment each produce one candidate state and one
  transactional commit. Failed commits keep memory and disk on the previous
  state and can be retried exactly once.
- An interrupted run is recovered as abandonment on the next startup. Consumed
  items remain consumed and escrowed equipment is lost according to the existing
  failure rule.
- The repository maintains `GraytailMeta` plus `GraytailMetaBackup`. It writes
  the previous valid main state to backup before replacing main, recovers a
  corrupt main from backup, and blocks unsafe mutation when both are unusable.
- A future-version main save is never overwritten by an older backup.
- Destructive reset requires explicit double confirmation and only reports
  success after both fresh backup and main writes succeed.
- Non-Shipping builds write the readable
  `Saved/SaveGames/GraytailMeta.debug.json` mirror after a successful commit.
  The game never reads this mirror.

## Verification

- `GraytailEditor Win64 Development`: pass.
- Headless runtime smoke: `234/234`, `Fail=0`, process exit `0`.
- Win64 Development BuildCookRun: pass.
- Packaged Development startup probe: all pass:
  - `SaveSlotIsolated`
  - `HudCreated`
  - `MainMenuVisible`
  - `RunStarted`
  - `RunAbandoned`
  - `Overall`
- Startup-probe save leftovers: `0`.
- Win64 Shipping BuildCookRun: pass.
- Shipping boundary source check: pass.
- Shipping process remained alive for 8 seconds when given the Development-only
  probe flag, proving the probe is inactive in Shipping.
- Shipping binary contains none of `GRAYTAIL_STARTUP`,
  `GraytailStartupProbe`, or the checked GM command strings.

## Review notes

- The packaged main-menu state is asserted by the Development startup probe.
- A separate automated screenshot attempt was blocked by Windows desktop capture
  permissions; no permission bypass was attempted.
