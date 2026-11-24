# Session handoff

Written 2026-11-25 (session 32 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1, cpp-dev x2 (one for a small test-window fix), reviewer x1, qa-verifier x1, release-manager x1, docs-keeper x1.
- Spawn count this session: 7 (resumes not counted).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Recommended: open any script-heavy map saved with an older HiveWE and spot-check that each custom script trigger holds its own code. A map saved that way twice may still be mixed up: compare with a backup. QA found three local copies the old build had mispaired and the new build reads correctly: `Documents\Warcraft III\Maps\HiveWE test\2 hivewe\mapA_dest_fixed_build.w3x`, `...\3 rings fix\mapA_rings_fixed.w3x`, `...\4 sound and rings fix\mapA_sound_rings_fixed.w3x`.
2. Recommended (new): on a copy of a map, rename a unit in the Object Editor, close that window, click the main window's X: Save/Discard/Cancel appears and Cancel keeps the window. With the change still unsaved, File > Open Map Directory on another folder map: the prompt appears and Cancel keeps your map. Then Save and click X: it closes without a prompt.
3. Decision (WarMash direct-exe game launch as the standing method): ask again when the user says the WarMash game tests are finished.
4. Optional: select a unit, left-drag its Selection Properties preview: starts from the front and orbits.
5. Optional: select 2 doodads, press Delete, then Ctrl+Z: they come back.
6. Optional, low priority: copy a terrain selection, run Change Tileset removing a tile: Ctrl+V pastes nothing and Ctrl+Z does nothing; a texture-only change keeps undo and closing prompts to save.
7. Optional: File > Open Map on a normal `.w3x`, and a sandbox copy through Map Protector, both succeed.
8. Optional: Map Protector, protect a copy of humanx03 with Asset Path Obfuscation enabled: it completes.
9. Optional: Browse Models: Models and VFX/SFX sidebar rows work, sizes in MB, tiles in view play Stand (up to 12).
10. Optional: Object Editor, double-click a unit's "model file": Browse Models opens with a Path line; OK updates the cell.
11. Optional: Object Editor > Add Units, right-click a unit, open and cancel the Add dialog.
12. Optional: New object dialog, type `hfö`, `ab'c`, `h000`: OK disabled for the first two, enabled for `h000`.
13. Optional: a map with a literal (non-TRIGSTR) w3i name: Map Description shows it; edit and Save persists.

## State
- This commit: 1220329518098180 shipped. Quit (also Exit), New Map and Open Map ask to save after any change Save would write; no prompt after open, save or browsing. `Map::serialized_state()` compares what Save would write; a new writer in `Map::save` must be added there. Offscreen QA windows now stay off screen. ctest 470/470, 466-map sweep unchanged, review APPROVE, QA PASS. Not tested: Open Map folder/MPQ/recent prompt paths at runtime, Windows logoff.
- a6bba0d reached origin at 23:19:54 on 2026-11-24 without this session pushing it; its CI passed.
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220885519534133 (unsaved-prompt leftovers). Comment added to 1220845243296342 (half-truncated wts resaves with exit 0).
- 1220882874361280: research done and posted as a ticket comment; no code yet.

## Next step
1220882874361280 (risky: cpp-dev on Opus, reviewer, qa-verifier): a map without `war3map.wtg` (or with a wtg but no wct) loses its script on save. The wc3-format-specialist note is the ticket comment, do not rerun it. Default from the note: keep the script bytes, write no wtg/wct/.j/.lua, notice on open, trigger editing disabled, one warning when units, regions, cameras, sounds or players changed. Trigger Editor banner is a layout change: avoid it (disabled actions and a message only) or get design approval. Keep the unsaved-changes comparison consistent with whatever Save skips. Game checks wait.

## Backlog only, not next steps
1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (UI modernization, needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents; open wishlist items are real candidates, e.g. 1220329518098116 backups, 1220329469211176 non-map files in imports, 1220329290981221 used files flagged unused), 1220832845348582 (memory growth), 1220329469370593 (listfile, waits for WarMash), 1220362579055972 (waits for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE runs and say so.
User (2026-11-24): move done tickets to "Done — shipped"; focus on Asana board tickets over small edge cases.
User (2026-11-24): hand off when context is high; offer it after a large ticket ships, before starting the next.
User (2026-11-25): after a denied combined build command, chose "run steps separately" (plain Agent-preset commands).
Orchestrator (2026-11-24): a trigger or script file HiveWE cannot read fully (bad header included) refuses the map.
Orchestrator (2026-11-24): wtg deleted-item lists are written back as read (counts may lag after edits; World Editor behaviour unverified); new ids skip deleted ids.
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs.
Orchestrator (2026-11-24): Open Map (MPQ) and Map Protector refuse archives with unsafe entry names.
Orchestrator (2026-11-24): a `Map::load` failure after the body check opens a blank template map.
Orchestrator (2026-11-24): `--resave` exit 5 means objects were skipped and dropped; exit 3 wins when both apply.
Orchestrator (2026-11-25): a script compile failure still counts as saved (filed, not changed); undoing back to the saved state may still prompt.

## Retro
Session 32: 7 spawns, 1 ticket shipped plus research for the next, about 80 minutes; longest runs final QA with full sweep 25 min, cpp-dev round 1 16 min, QA round 1 14 min; 3 fix rounds. Causes: two untestable dialog paths needed a hook round; the new hook showed three dialogs on the user's screen (extra round of about 15 min); one combined build command was denied by a pattern match. Applied: briefs ask for full dialog-path hooks and window-rectangle sampling in round 1; build steps as separate commands. No review, test or sweep removed.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\9faa72dc-a4a4-4def-88a5-3e941f4cdd13\scratchpad` (may vanish; `sweep\w4\work` is the tenth sweep baseline, `qa` holds QA scripts, `offscreen_shot.ps1` samples window rectangles). Stock map extraction: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock` (may vanish).
