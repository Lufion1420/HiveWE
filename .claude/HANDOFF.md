# Session handoff

Written 2026-11-25 (session 33 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x1 (Opus), reviewer x1, qa-verifier x1, release-manager x1, docs-keeper x1.
- Spawn count this session: 5 (resumes not counted).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed. The shipped ticket of this session added no user check.
1. Recommended: open any script-heavy map saved with an older HiveWE and spot-check that each custom script trigger holds its own code. A map saved that way twice may still be mixed up: compare with a backup. QA found three local copies the old build had mispaired and the new build reads correctly: `Documents\Warcraft III\Maps\HiveWE test\2 hivewe\mapA_dest_fixed_build.w3x`, `...\3 rings fix\mapA_rings_fixed.w3x`, `...\4 sound and rings fix\mapA_sound_rings_fixed.w3x`.
2. Recommended: on a copy of a map, rename a unit in the Object Editor, close that window, click the main window's X: Save/Discard/Cancel appears and Cancel keeps the window. With the change still unsaved, File > Open Map Directory on another folder map: the prompt appears and Cancel keeps your map. Then Save and click X: it closes without a prompt.
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
- beb21e4: 1220882874361280 shipped. A map with a script and no `war3map.wtg` or no `war3map.wct` keeps its script on every save path; trigger editing is disabled with a notice. Review APPROVE after one REJECT, QA PASS, ctest 478/478, 466-map sweep unchanged (0 maps classed as script-kept). Redeployed to the Tools folder. Ticket moved to Done.
- Not verified: game load, including whether the game reads `scripts\war3map.lua`.
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220895284225944 (Asset Manager and unpack folder ignore `scripts\`), 1220895469636543 (stock 0460 w3q shrinks on the second resave).

## Next step
1220329290981221 (standard, data safety; subtask of the wishlist ticket): Asset Manager flags used files as unused, so bulk delete can remove files the map uses (`src/base/map/unused_files.cpp`: upgrade lookup commented out, fixed field lists, only the root script is scanned; FDF, TOC and `war3mapSkin.txt` references are ignored, and Map Protector already reads those). Include the `get_file_usage` item of 1220895284225944. Verify the claims in code first (ticket evidence is code reading from 2026-11-08). Game checks wait.

## Backlog only, not next steps
1220895469636543, 1220895284225944 (unpack-folder item), 1220329518098244 (Save already reports "Saved, script not updated"; only Test Map still launches the old script), 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (UI modernization, needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents; e.g. 1220329518098116 backups, 1220329469211176 non-map files in imports), 1220832845348582 (memory growth), 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

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
Orchestrator (2026-11-25): the script is kept whenever wtg or wct is missing (wider than the research note); `--resave` still exits 0 for such a map; cameras and sounds are not in the "Script Not Updated" warning because Save never writes them.

## Retro
Session 33: 5 spawns, 1 risky ticket shipped, about 1.5 hours; longest runs final QA with full sweep 43 min (previous sweep 19 min; CPU was loaded by other work), cpp-dev round 1 23 min, fix round 13 min; 1 fix round. Causes: the research note's detection rule was too narrow (review caught it); the round-1 sweep was requested before review and cancelled. Applied: Full sweep only after review APPROVE; format notes list everything stored only in a file a rule depends on; QA staleness check split per binary. One QA rebuild was denied by a permission pattern and not retried. No review, test or sweep removed.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c85be5c8-e658-4d4f-963a-a25e71743bd5\scratchpad` (may vanish; `qav\sweep\w4\work` is the eleventh sweep baseline, `qav\sweep\code` its scripts, `qav\sweeprun.ps1` the runner, `kshot.ps1` runs the offscreen hooks). Stock map extraction: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock` (may vanish).
