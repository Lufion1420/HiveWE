# Session handoff

Written 2026-11-24 (session 26 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: qa-verifier x1.
- Spawn count this session: 1. Stopped because no eligible ticket is left.

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
8. Optional: File > Open Map on a normal `.w3x` still works after the `Map::load` failure handling change (462d751).

## State
- bf69845: Map Protector restores `hierarchy.map_directory` on every exit (RAII guard) when a w3e is shorter than its magic; regression test (Asana 1220329514601668, done). QA PASS 425/425, reviewer APPROVE. Deployed (sha 69745C05...).
- 462d751: `Map::load` failure after the body check shows a dialog naming the cause and opens a blank template map; `gl_thread_pool` `wait_for_all` used in Doodads/Units `create()`; unknown base id and texture load errors named; `--qa-refusal-checks` item 5 (Asana 1220330336212700, done). QA PASS 428/428 + hook exit 0, reviewer APPROVE. Deploy by release-manager was running at handoff: check `Get-Process HiveWE` and the deploy result in chat first.
- CI: run 45439779012 covers 009b46b; newer pushes cancel older runs, so check the newest run for 462d751 first.
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased (rule details in `hivewe-file-formats.md`, `hivewe-qt-ui.md`): `create_new_map` calls `map->load(template)` without try; blank fallback map keeps `hierarchy.map_directory` on `data/test map` until Save As; Map Protector asset-obfuscation/TRIGSTR steps change global `map_directory` from the export QThread; error text uses `fromLocal8Bit` on raw map bytes; no unit test for `load_modification_file`'s file name; `inline_map_info_trigger_strings` has no direct test (not exported); Loading Map box stays behind the error dialog; Ctrl+V twice leaves `TerrainBrush` pasting after `clear_clipboard`; QA hook does not restore `hierarchy.map_directory`; OK with no change in Change Tileset marks the map unsaved; narrow-string `.string()` paths in MPQ extract on CJK code pages; `war3map.imp` parser edge cases; no test for the new-map cliff path.
- Reviewer notes still filed: no `Doodads::find_id_over_4_bytes` pre-check in `Map::save`; startup error text says only "Bad expected access" (`slk.ixx` `.value()`); `is_valid_object_id` exists 3 times (`object_data_io`, `object_data_text`, `Units::is_valid_new_object_id`), consolidate; region hook compares the palette to the same in-memory vector; several `--qa-*-check` flags can be combined (first exit wins); Copy dialog has no hook step for bad ids; `next_sequential_custom_id` prefill for a stock source whose first char is non-alphanumeric; refusal-hook minors; new-dialog hook has no RED run; item-4 hook covers `load_map` only; `save_map_reporting_errors` ignores `map->headless`.
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Update (session 27)
- Asana 1220369536243160 done: `nbsh` is absent from 3.0.1 CASC (legacy leftover in humanx03, unused n003). Custom objects with an unknown base id are now skipped with one warning dialog (`Map::skipped_objects`), placed units of them dropped (`Units::load` ignored_ids); dropped on save. This replaces the 462d751 blank-map fallback for that case. QA PASS 429/429 + hook exit 0, reviewer APPROVE (2nd round). CI for d4c67cb green (45449044376).
- Filed, not chased: Map Protector `rewrite_object_data_file` still refuses unknown parents; `headless_resave` exits 0 when objects were skipped; placed items of skipped .w3t items kept; w3u-skip + Skin-valid-base corner case; JASS `gg_unit` global dropped for skipped placed unit; no raw-record preservation on save; humanx03 resave logs script compile errors (`ReturnAction`, `gg_rct_Lieutenant_Pass_0`), not checked if new.

## Next step
No eligible ticket (see Update above; 1220369536243160 is resolved). 1220330457516219 needs doo/units/w3r/w3c parsers first (unreachable). 1220362579055972 only if the user says WarMash testing is done. Skip 1220841054745630 (HD only); 1220832845348582 stays filed; 1220329469370593 waits for WarMash.

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
Orchestrator (2026-11-24): a `Map::load` failure after the body check opens a blank template map instead of leaving a half-torn-down map.

## Retro
One qa-verifier run, 274 s: both stock maps load to the w3u read and fail with the named nbsh dialog; no builds, no rework. Proposals: none.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\966e4294-ef6d-4700-b247-874c09307116\scratchpad` (may vanish; nothing in it is required).
