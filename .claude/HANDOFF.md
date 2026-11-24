# Session handoff

Written 2026-11-24 (session 24 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x2 (resumed several times), qa-verifier x7, reviewer x6, release-manager x2.
- Spawn count this session: about 17 (spawn budget reached).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional: the session-8 feedback checks.
   - Select a unit, left-drag its Selection Properties preview: starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file": Browse Models opens with a Path line; OK updates the cell at once.
   - Tiles in view play Stand (up to 12).
   - Map with a literal (non-TRIGSTR) w3i name: Map Description shows it; edit and Save persists.
2. Decision (WarMash direct-exe game launch as the standing method): ask again when the user says the WarMash game tests are finished.
3. Optional sanity look: in HiveWE select 2 doodads, press Delete, then Ctrl+Z: they come back (automation covered this via `--qa-doodad-undo-check`).
4. Optional: in Object Editor > Add Units, right-click a unit, open and cancel the Add dialog once (the dialog was refactored into `ObjectEditor::build_new_object_dialog`).
5. Optional: in the Object Editor New dialog type `hfö`, `ab'c` and `h000` as the new id: OK stays disabled for the first two and is enabled for `h000`.
6. Optional: open one normal `.w3x` via File > Open Map (MPQ), and one via Map Protector (sandbox copies), to confirm the success path still works after the MPQ failure and path-safety changes.
7. Optional, low priority: in HiveWE copy a terrain selection, run Change Tileset removing a tile: Ctrl+V pastes nothing and Ctrl+Z does nothing; a texture-only change keeps undo and closing prompts to save (automation covers this via `--qa-tileset-clipboard-check`).

## State
- 7d76ecb: MPQ unpack and `extract_file` report failures; Open Map and Map Protector stop with the failed file names (Asana 1220329514595316, done). CI green.
- 6fcee87: Change Tileset drops the terrain clipboard and undo when the tile list changes; a texture-only change calls `WorldUndoManager::mark_changed`; hook `--qa-tileset-clipboard-check` (Asana 1220330336348703, done). CI green (run 45433718693).
- e85c6c2: MPQ path safety (`mpq::is_safe_archive_name`; unpack and `extract_file` refuse unsafe entry names) from a security review, no Asana ticket. QA PASS, ctest 424/424 on all three commits. CI pending at handoff: check it first.
- Tools folder deployed with e85c6c2 (exe sha FF292C170BEE, smoke tests passed, HiveWE was not running).
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased (details in `hivewe-qt-ui.md` and `hivewe-file-formats.md`): Ctrl+V twice leaves `TerrainBrush` pasting after `clear_clipboard`; QA hook does not restore `hierarchy.map_directory`; OK with no change in Change Tileset marks the map unsaved; narrow-string `.string()` paths in MPQ extract on CJK code pages; `war3map.imp` parser edge cases; no test for the new-map cliff path. Open Map and Map Protector now deliberately refuse maps whose archive holds unsafe entry names.
- Reviewer notes from the session-23 handoff stay filed: no `Doodads::find_id_over_4_bytes` pre-check in `Map::save`; startup error text says only "Bad expected access" (`slk.ixx` `.value()`); `is_valid_object_id` exists 3 times (`object_data_io`, `object_data_text`, `Units::is_valid_new_object_id`), consolidate; region hook compares the palette to the same in-memory vector; several `--qa-*-check` flags can be combined (first exit wins); Copy dialog (`duplicate_object_entry`) has no hook step for bad ids; `next_sequential_custom_id` prefill for a stock source whose first char is non-alphanumeric; refusal-hook minors; new-dialog hook has no RED run; item-4 hook covers `load_map` only; `save_map_reporting_errors` ignores `map->headless`; older items in git history of this file.
- QA focus: log the foreground pid, not the change count.
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
Other open subtasks of Asana 1220329527029506: 1220330336212700, 1220330457516219, 1220329514601668, 1220369536243160 (read each ticket first). 1220362579055972 only if the user says WarMash testing is done. Skip 1220841054745630 (HD only); 1220832845348582 stays filed; 1220329469370593 waits for WarMash.

## Backlog only, not next steps
1220826761382041, 1220824126875159, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, 1220829561061744, 1220829428496325, 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): standing order, redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE is running and say so in chat (in guardrails).
User (2026-11-24): always move done tickets to the Done column ("Done — shipped"); done for all finished ones (in `HARNESS.md` "Choosing work").
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs, because `MapScriptWriter::four_cc` writes ids unescaped; `id_fits` stays length-only so existing maps still save.
Orchestrator (2026-11-24): Open Map (MPQ) and Map Protector refuse archives with unsafe entry names (in CHANGELOG).

## Retro
Tasks: MPQ cpp-dev 385 s + follow-up 215 s, QA 130 + 82 s, review 88 s; tileset 3 cpp-dev rounds (286 + 306 + 165 s), 3 review rounds (2 REJECT with real defects: lost unsaved marker, hook writing into `data/test map`), QA x3; path safety 3 rounds with about 740 s lost on a stale `build.lock`; about 17 subagent runs. Proposals: cpp-dev runs the mandatory review checks itself first (hook writes only to a temp folder; mark-changed side effects); brief QA to flag stale locks; keep parallel reviewer+QA. Quality guard: nothing weakened, the same reviews and QA still run, only earlier.

## Scratch
Orchestrator scratchpad for this session: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\1df04c30-8eb6-4105-b317-88b90f2f2d0a\scratchpad` (may vanish; nothing in it is required).
