# Session handoff

Written 2026-11-25 (session 34 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x1 (Sonnet), reviewer x1 (Opus), qa-verifier x1, release-manager x1, docs-keeper x1.
- Spawn count this session: 5 (resumes not counted).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Map defect found by QA in both NCOW maps (NCOW Arena 0.3 and Test\release): the script names `Sound/Spells/NCOW_Sound_Seinsashu.mp3`, but the file is in `Sound/Spells/Sasuke/`. Fix the path in the trigger or move the file; the Asset Manager lists that file as unused.
2. Recommended before a bulk delete: open the Asset Manager on a copy of an NCOW map, "Select all unused", delete, then test the map. (Paths a script builds at runtime are not detected.) The game test itself waits like all game tests.
3. Recommended: open any script-heavy map saved with an older HiveWE and spot-check that each custom script trigger holds its own code. A map saved that way twice may still be mixed up: compare with a backup. QA found three local copies the old build had mispaired and the new build reads correctly: `Documents\Warcraft III\Maps\HiveWE test\2 hivewe\mapA_dest_fixed_build.w3x`, `...\3 rings fix\mapA_rings_fixed.w3x`, `...\4 sound and rings fix\mapA_sound_rings_fixed.w3x`.
4. Recommended: on a copy of a map, rename a unit in the Object Editor, close that window, click the main window's X: Save/Discard/Cancel appears and Cancel keeps the window. With the change still unsaved, File > Open Map Directory on another folder map: the prompt appears and Cancel keeps your map. Then Save and click X: it closes without a prompt.
5. Decision (WarMash direct-exe game launch as the standing method): ask again when the user says the WarMash game tests are finished.
6. Optional: select a unit, left-drag its Selection Properties preview: starts from the front and orbits.
7. Optional: select 2 doodads, press Delete, then Ctrl+Z: they come back.
8. Optional, low priority: copy a terrain selection, run Change Tileset removing a tile: Ctrl+V pastes nothing and Ctrl+Z does nothing; a texture-only change keeps undo and closing prompts to save.
9. Optional: File > Open Map on a normal `.w3x`, and a sandbox copy through Map Protector, both succeed.
10. Optional: Map Protector, protect a copy of humanx03 with Asset Path Obfuscation enabled: it completes.
11. Optional: Browse Models: Models and VFX/SFX sidebar rows work, sizes in MB, tiles in view play Stand (up to 12).
12. Optional: Object Editor, double-click a unit's "model file": Browse Models opens with a Path line; OK updates the cell.
13. Optional: Object Editor > Add Units, right-click a unit, open and cancel the Add dialog.
14. Optional: New object dialog, type `hfö`, `ab'c`, `h000`: OK disabled for the first two, enabled for `h000`.
15. Optional: a map with a literal (non-TRIGSTR) w3i name: Map Description shows it; edit and Save persists.

## State
- 1220329290981221 shipped in this commit: the Asset Manager no longer lists used files as unused (object fields, variations, portraits, all scripts and text files, open editor text, audio/texture/model extensions), hides dot files, and refuses to delete from an outdated list. Review REJECT then APPROVE, QA FAIL then PASS, ctest 502/502, offscreen delete and unsaved hooks green, checked on copies of three real maps. Includes item 1 of 1220895284225944; item 2 (unpack folder and `scripts\`) is open.
- Not verified: game load; right-click "Delete file" path; non-ASCII letter case.
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220898118343928 (Asset Manager leftovers). A stray "Placeholder check" comment was added by mistake on 1220844868394880.

## Next step
1220329518098116 (standard or risky, data safety; "[Wish] Add safe writes and automatic versioned backups"). Read the ticket and verify its claims in code first; part of it may overlap 1220767791486098. Game checks wait.

## Backlog only, not next steps
1220329469211176 (non-map files in imports), 1220898118343928, 1220895469636543, 1220895284225944 (unpack-folder item), 1220329518098244, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (UI modernization, needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582 (memory growth), 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE runs and say so.
User (2026-11-24): move done tickets to "Done — shipped"; focus on Asana board tickets over small edge cases.
User (2026-11-24): hand off when context is high; offer it after a large ticket ships, before starting the next.
User (2026-11-25, twice): after a denied combined build command, chose "run steps separately" (plain Agent-preset commands). Not a standing approval: ask again if it happens.
Orchestrator (2026-11-24): a trigger or script file HiveWE cannot read fully (bad header included) refuses the map.
Orchestrator (2026-11-24): wtg deleted-item lists are written back as read (counts may lag after edits; World Editor behaviour unverified); new ids skip deleted ids.
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs.
Orchestrator (2026-11-24): Open Map (MPQ) and Map Protector refuse archives with unsafe entry names.
Orchestrator (2026-11-24): a `Map::load` failure after the body check opens a blank template map.
Orchestrator (2026-11-24): `--resave` exit 5 means objects were skipped and dropped; exit 3 wins when both apply.
Orchestrator (2026-11-25): a script compile failure still counts as saved (filed, not changed); undoing back to the saved state may still prompt.
Orchestrator (2026-11-25): the script is kept whenever wtg or wct is missing (wider than the research note); `--resave` still exits 0 for such a map; cameras and sounds are not in the "Script Not Updated" warning because Save never writes them.
Orchestrator (2026-11-25): in the Asset Manager an uncertain reference counts as used; dot files and folders are never listed; no map-diff sweep for the Asset Manager change (no writer or script generator changed).

## Retro
Session 34: 5 spawns, 1 standard ticket, 2 rounds, about 1.5 hours; cpp-dev 21 + 5 + 11 min, reviewer 4.5 + 1.3 min, QA 9 + 9 min. Causes: the first brief missed unsaved editor text and audio extensions (found by review and real-map QA); cpp-dev chained a build with the test exe, was denied and needed a user answer; cpp-dev ended at 450k tokens, over its 350k guide. Applied: use ctest filters rather than the test exe; QA runs map-analysing features on copies of real maps. No review or test removed.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\b5ca324a-17bb-45c1-a5e3-e2651a687f66\scratchpad` (may vanish; `qa_am\scripts` and `qa_am\out` hold the Asset Manager QA tools and reports). Session 33 scratch with the eleventh sweep baseline: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c85be5c8-e658-4d4f-963a-a25e71743bd5\scratchpad` (`qav\sweep\w4\work`, `qav\sweep\code`, `qav\sweeprun.ps1`, `kshot.ps1`). Stock map extraction: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock` (may vanish).
