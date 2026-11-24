# Session handoff

Written 2026-11-24 (session 23 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x3, qa-verifier x2, reviewer x1, release-manager x3, docs-keeper x2.
- Spawn count this session: about 11 (spawn budget reached).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed. Nothing new from this session needs a user check.
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

## State
- 5ae966c: a startup map that fails to load now shows an "Opening map failed" error dialog instead of crashing (try/catch around the startup map load; Asana 1220363908605247, QA PASS: mark done).
- 8f9d064: `Doodads::save` pads ids to 4 bytes via `Doodads::write_id`; ids over 4 bytes throw (Asana 1220331565174902, reviewer APPROVE, QA PASS).
- Tools folder deployed with 8f9d064 (exe sha F357FEAF...).
- CI: run 45421142196 (success) covered 150f018. Run 45425937970 on 8f9d064 was in progress at handoff: check it first. 5ae966c's run 45423645073 was cancelled by the newer push.
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased:
  - No `Doodads::find_id_over_4_bytes` pre-check in `Map::save`: the reviewer knows no way to create such an id. Verify SLK import and copy-paste paths before adding one.
  - The startup error text says only "Bad expected access" (`slk.ixx` `.value()` carries no file name): wrap in `slk::SLK` to name the file. Only a missing Cliffs.slk was tested.
  - Rules file line 43 about `save_modification_table` `write_string` is unverified.
  - Region hook compares the palette to the same in-memory vector (add one value read from the file); several `--qa-*-check` flags can be combined (first exit wins); the Copy dialog (`duplicate_object_entry`) has no hook step for bad ids; `is_valid_object_id` exists 3 times (`object_data_io`, `object_data_text`, `Units::is_valid_new_object_id`): consolidate; `next_sequential_custom_id` prefill for a stock source whose first char is non-alphanumeric; reviewer minors on the refusal hook (no stop if the damaged-map refusal fails; watchdog FAIL is not in `--qa-script-out`); the new-dialog hook has no RED run; the item-4 hook covers `load_map` only; `save_map_reporting_errors` ignores `map->headless`; older filed items (doodad hook minors, `write_active_mdx` `[[nodiscard]]`, DoodadAddAction stale selection, DoodadDeleteAction redo, MDL camera DOF) are in the git history of this file.
- QA focus: log the foreground pid, not the change count (the monitor counts the user's own window switches).
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
Asana 1220329514595316 "Open Map (MPQ) ignores failed extraction" (standard; risky if the Map Protector path changes). `MpqArchive` unpack in `src/file_formats/mpq.ixx` ignores `SFileExtractFile` results; `extract_file` must tell an absent file from a failed one. Then the other subtasks of 1220329527029506: 1220330336348703 (tileset undo/clipboard stale indices), 1220330336212700, 1220330457516219, 1220329514601668, 1220369536243160. 1220362579055972 only if WarMash testing is done. Skip 1220841054745630 (HD only).

## Backlog only, not next steps
1220826761382041, 1220824126875159, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, 1220829561061744, 1220829428496325, 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): standing order, redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE is running and say so in chat (in guardrails).
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs, because `MapScriptWriter::four_cc` writes ids unescaped; `id_fits` stays length-only so existing maps still save.

## Retro
Timings: cpp-dev startup 38 s, QA 194 s, release 8-21 s; doodad cpp-dev 440 s, review 82 s, QA 105 s, test-tighten 131 s. No REJECT; one reviewer minor round (test strictness, changelog) ran in parallel with docs. Kept: parallel reviewer+QA, parallel changelog/docs/test-fix after QA. Proposal: brief cpp-dev to write the changelog line and strict `CHECK_THROWS_AS` from the start (does not lower quality: same checks, earlier).

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\6c2fa147-9e55-4ac9-9ba0-4d3a42d3276d\scratchpad` (may vanish).
