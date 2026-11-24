# Session handoff

Written 2026-11-24 (session 22 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x2 (+1 resume), reviewer x3 (APPROVE; REJECT; APPROVE), qa-verifier x3 (PASS x3), release-manager x3 (deploys), docs-keeper x1.
- Spawn count this session: about 13 (spawn budget reached).

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

## State
- e4f512a: `--qa-region-palette-check` hook (Regions palette lists the loaded map's regions; Asana 1220362272870417 COMPLETE). 150f018: new object ids must be exactly 4 letters or digits in the New/Add/Copy dialogs; `id_fits` stays length-only (Asana 1220863401550342 COMPLETE). Both pushed, QA PASS, reviewer APPROVE.
- Tools folder redeployed with e4f512a (HiveWE.exe sha F013585C...). Deploy of 150f018 requested from release-manager at handoff: confirm the sha with `Get-FileHash` on the Tools exe.
- CI: newest run covering 47f1c0f was 45411186716 (success). Runs for e4f512a and 150f018 not checked at handoff: take the newest run (cancel-in-progress cancels older ones).
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased: region hook compares the palette to the same in-memory vector (add one value read from the file); several `--qa-*-check` flags can be combined (first exit wins); the Copy dialog (`duplicate_object_entry`) has no hook step for bad ids; `is_valid_object_id` exists 3 times (`object_data_io`, `object_data_text`, `Units::is_valid_new_object_id`): consolidate; `next_sequential_custom_id` prefill for a stock source whose first char is non-alphanumeric (sweep row_headers); reviewer minors on the refusal hook (no stop if the damaged-map refusal fails; watchdog FAIL is not in `--qa-script-out`); the new-dialog hook has no RED run; the item-4 hook covers `load_map` only; `save_map_reporting_errors` ignores `map->headless`; older filed items (doodad hook minors, `write_active_mdx` `[[nodiscard]]`, DoodadAddAction stale selection, DoodadDeleteAction redo, MDL camera DOF) are in the git history of this file.
- QA focus: log the foreground pid, not the change count (the monitor counts the user's own window switches).
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
Pick an eligible Asana "Next up" ticket (project 1220328345553589): 1220362579055972 only if WarMash testing is done, else a non-game ticket such as 1220329527029506 (map load robustness). Skip 1220841054745630 (HD only); 1220832845348582 stays filed.

## Backlog only, not next steps
1220826761382041, 1220824126875159, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, 1220829561061744, 1220829428496325, 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): standing order, redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE is running and say so in chat (in guardrails).
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs, because `MapScriptWriter::four_cc` writes ids unescaped; `id_fits` stays length-only so existing maps still save.

## Retro
cpp-dev region hook 259 s; QA 444 s; id guard dev 317 s + reject fix 347 s; QA 537 s + 262 s; about 13 spawns. One REJECT round: the alphanumeric rule (FourCC is unescaped in the generated script; tightening `id_fits` would have locked saving of existing maps). Proposal (accepted): brief cpp-dev that any id or string validator must be checked against how the value is written into generated scripts and against maps already on disk. Kept: reviewer+QA in parallel; QA waits hold edits.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\90faec8a-05fd-4896-a2cf-6f1a4581d708\scratchpad` (rh.ps1, sbx; may vanish).
