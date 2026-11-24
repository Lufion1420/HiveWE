# Session handoff

Written 2026-11-24 (session 29 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x4, reviewer x2, qa-verifier x2, release-manager x2, docs-keeper x1.
- Spawn count this session: 11. Stopped because the spawn budget was reached.

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
9. Optional (new): in Map Protector, protect a copy of humanx03 with Asset Path Obfuscation enabled and confirm it completes. There is no headless path for this.

## State
- a5ec551 (pushed, CI green, run 45463308985): Map Protector keeps custom objects with an unknown base id as raw bytes (`RawCustomObject`, `build_modification_file_buffer_with_raw`) instead of failing asset obfuscation. Asana 1220877868370898 done.
- 9efd3e8 (pushed, CI not yet checked: check the newest run first, newer pushes cancel older runs): Map Protector rewrite of an existing `war3mapSkin.*` now writes skin=false so its fields survive. Asana 1220878440303422 done.
- Both tickets are in "Done — shipped". Tests 433/433. Deployed to the Tools folder (sha256 D884C9C6...7697); HiveWE was not running.
- Earlier shipped (details in `git log` and CHANGELOG): 3841d46 `--resave` exit 5; bf69845, 462d751, cb173dc (unknown base id skip).
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased (rule details in `hivewe-file-formats.md`, `hivewe-qt-ui.md`):
  - Map Protector: raw-kept unknown-base objects are not path-rewritten, and the verify-nothing-missed scan may flag their old path; `set_count>1` in skin files is unparsed (unverified); float fields round-trip via `std::to_string` (6 decimals) in the shadow-map rewrite.
  - Skipped-object follow-ups: placed items of a skipped .w3t kept; w3u-skip + Skin-valid-base corner case; JASS `gg_unit` global dropped for a skipped placed unit; no raw-record preservation on save (only Map Protector carries raw bytes); humanx03 resave logs script compile errors (`ReturnAction`, `gg_rct_Lieutenant_Pass_0`), not checked if new.
  - `create_new_map` calls `map->load(template)` without try; blank fallback map keeps `hierarchy.map_directory` on `data/test map` until Save As; Map Protector asset-obfuscation/TRIGSTR steps change global `map_directory` from the export QThread; error text uses `fromLocal8Bit` on raw map bytes; no unit test for `load_modification_file`'s file name; `inline_map_info_trigger_strings` has no direct test (not exported); Loading Map box stays behind the error dialog; Ctrl+V twice leaves `TerrainBrush` pasting after `clear_clipboard`; QA hook does not restore `hierarchy.map_directory`; OK with no change in Change Tileset marks the map unsaved; narrow-string `.string()` paths in MPQ extract on CJK code pages; `war3map.imp` parser edge cases; no test for the new-map cliff path.
- Reviewer notes still filed: no `Doodads::find_id_over_4_bytes` pre-check in `Map::save`; startup error text says only "Bad expected access" (`slk.ixx` `.value()`); `is_valid_object_id` exists 3 times (`object_data_io`, `object_data_text`, `Units::is_valid_new_object_id`), consolidate; region hook compares the palette to the same in-memory vector; several `--qa-*-check` flags can be combined (first exit wins); Copy dialog has no hook step for bad ids; `next_sequential_custom_id` prefill for a stock source whose first char is non-alphanumeric; refusal-hook minors; new-dialog hook has no RED run; item-4 hook covers `load_map` only; `save_map_reporting_errors` ignores `map->headless`.
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
No Asana ticket is eligible. 1220330457516219 needs doo/units/w3r/w3c parsers first (unreachable). 1220362579055972 only if the user says WarMash testing is done. Skip 1220841054745630 (HD only); 1220832845348582 stays filed; 1220329469370593 waits for WarMash.

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
Orchestrator (2026-11-24): a `Map::load` failure after the body check opens a blank template map instead of leaving a half-torn-down map (except unknown base ids, which are skipped with a warning).
Orchestrator (2026-11-24): `--resave` exit 5 means objects were skipped and dropped; exit 3 wins when both apply.

## Retro
Session 29: 11 spawns. cpp-dev + reviewer + QA cycle about 10 min per ticket. Two test-only reviewer-minor rounds; proposal: name test layout and guard requirements in the first cpp-dev brief. A bash heredoc ate a backslash in test source (C4129): rule recorded in `hivewe-file-formats.md`. No quality reduction: review and QA stay.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\298ff39a-a78c-4569-8272-383965393cd2\scratchpad` (may vanish; nothing in it is required).
