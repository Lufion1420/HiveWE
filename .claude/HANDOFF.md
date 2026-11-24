# Session handoff

Written 2026-11-24 (session 31 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1, cpp-dev x2 (one opus, one stopped for this handoff), reviewer x1, qa-verifier x1, release-manager x1, docs-keeper x1.
- Spawn count this session: 7 (resumes not counted).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Recommended (new): open any script-heavy map saved with an older HiveWE and spot-check that each custom script trigger holds its own code. A map saved that way twice may still be mixed up: compare with a backup. QA found three local copies the old build had mispaired and the new build reads correctly: `Documents\Warcraft III\Maps\HiveWE test\2 hivewe\mapA_dest_fixed_build.w3x`, `...\3 rings fix\mapA_rings_fixed.w3x`, `...\4 sound and rings fix\mapA_sound_rings_fixed.w3x`.
2. Decision (WarMash direct-exe game launch as the standing method): ask again when the user says the WarMash game tests are finished.
3. Optional: select a unit, left-drag its Selection Properties preview: starts from the front and orbits.
4. Optional: select 2 doodads, press Delete, then Ctrl+Z: they come back.
5. Optional, low priority: copy a terrain selection, run Change Tileset removing a tile: Ctrl+V pastes nothing and Ctrl+Z does nothing; a texture-only change keeps undo and closing prompts to save.
6. Optional: File > Open Map on a normal `.w3x`, and a sandbox copy through Map Protector, both succeed.
7. Optional: Map Protector, protect a copy of humanx03 with Asset Path Obfuscation enabled: it completes.
8. Optional: Browse Models: Models and VFX/SFX sidebar rows work, sizes in MB, tiles in view play Stand (up to 12).
9. Optional: Object Editor, double-click a unit's "model file": Browse Models opens with a Path line; OK updates the cell.
10. Optional: Object Editor > Add Units, right-click a unit, open and cancel the Add dialog.
11. Optional: New object dialog, type `hfö`, `ab'c`, `h000`: OK disabled for the first two, enabled for `h000`.
12. Optional: a map with a literal (non-TRIGSTR) w3i name: Map Description shows it; edit and Save persists.

## State
- f176390 (pushed, CI green, deployed to the Tools folder, exe sha256 4EF00BC1A28407C0): wtg reads all eight deleted-item lists (human05, undead02 open); unreadable wtg/wct refuses the map; wct written in trigger order (old writer moved custom text between triggers on resave); older-HiveWE files keep their pairing; Trigger Editor category/variable fixes. ctest 467/467, 466-map sweep 0 text differences, review APPROVE, QA PASS.
- 0453f69 (pushed): format notes and ninth sweep report. CI was running at handoff.
- Asana 1220330457516219, 1220882188008627 and parent 1220329527029506 done and in "Done — shipped".
- UNCOMMITTED, UNFINISHED: partial work for 1220329518098180 in `src/main.cpp`, `src/main_window/hivewe.cpp`, `src/main_window/hivewe.h` (+462 lines). The cpp-dev was stopped for this handoff while adding the RED-step offscreen hook against the current prompt logic (extracted into `has_unsaved_work()`). Not built, not tested, may not compile. Give the diff to a fresh cpp-dev to finish, or discard it if it is worse than starting over.
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220882874361280 (map without wtg loses its script on save, Next up), 1220884498398806 (category dragged into own subcategory), 1220884688687045 (trigger save leftovers).

## Next step
Finish 1220329518098180: quit, New Map and Open Map must ask to save after Object Editor, trigger, Map Info/Scenario and other map-data edits, with no false prompt after open or save. Standard track (cpp-dev, then reviewer and QA). Acceptance: (1) every edit Save would write marks the map unsaved; (2) no prompt after open or save; (3) text still only in an open editor widget counts; (4) Cancel changes nothing, a failed save does not quit; (5) offscreen/`--resave` runs gain no dialogs. No layout changes. Then 1220882874361280 (risky).

## Backlog only, not next steps
1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (UI modernization, needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents; open wishlist items are real candidates, e.g. 1220329518098116 backups, 1220329469211176 non-map files in imports, 1220329290981221 used files flagged unused), 1220832845348582 (memory growth), 1220329469370593 (listfile, waits for WarMash), 1220362579055972 (waits for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE runs and say so.
User (2026-11-24): move done tickets to "Done — shipped"; focus on Asana board tickets over small edge cases.
User (2026-11-24): hand off when context is high (they raised it at ~250k tokens).
Orchestrator (2026-11-24): a trigger or script file HiveWE cannot read fully (bad header included) refuses the map.
Orchestrator (2026-11-24): wtg deleted-item lists are written back as read (counts may lag after edits; World Editor behaviour unverified); new ids skip deleted ids.
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs.
Orchestrator (2026-11-24): Open Map (MPQ) and Map Protector refuse archives with unsafe entry names.
Orchestrator (2026-11-24): a `Map::load` failure after the body check opens a blank template map.
Orchestrator (2026-11-24): `--resave` exit 5 means objects were skipped and dropped; exit 3 wins when both apply.

## Retro
Session 31: 7 spawns, 1 ticket plus 2 related shipped in about 100 minutes; longest runs QA full sweep 32 min and cpp-dev round 1 24 min; 3 fix rounds. Cause: the first brief did not require a per-trigger text comparison, so the wct mispairing surfaced only in review/QA. Proposal (apply): save-format briefs require per-record content comparison against originals on stock maps with non-empty data, plus truncated-file cases. Handoff was offered late (the user had to ask): offer it right after a large ticket ships, before starting the next. No quality reduction; review and QA caught real data-loss bugs, keep them.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\e02cd7c9-cdb6-4d5b-a6b0-34d47178df21\scratchpad` (may vanish; `qa\code`, `qa2\code` and `scripts\wtgw.py` are optional). Stock map extraction: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock` (may vanish).
