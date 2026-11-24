# Session handoff

Written 2026-11-24 (session 21 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x2 (+3 resumes), reviewer x3 (REJECT then APPROVE; APPROVE), qa-verifier x4 (PASS x4), release-manager x2 (deploys), docs-keeper x1.
- Spawn count this session: 12 (spawn budget reached).

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

## State
- f7d5142: `--qa-refusal-checks` hook (5-byte id save refusals, undo after a refused map load; Asana 1220362272870417 items 1 and 4). 47f1c0f: `--qa-object-new-check` hook (item 2; Add dialog extracted to `ObjectEditor::build_new_object_dialog`). Both pushed, QA PASS.
- Tools folder redeployed with 47f1c0f (HiveWE.exe sha 9314C881...).
- Asana 1220362272870417 stays open: only the Regions palette listing on a loaded 3.0 map remains (needs a hook).
- CI: take the result of the newest run covering 47f1c0f (cancel-in-progress cancels older ones). Not checked at handoff.
- Asana 1220863401550342 filed: "Object ids: 4-byte non-ASCII id" (ids like `hfö`). Run 45407665825 (f7d5142) succeeded; run 45410604588 (47f1c0f) was in progress at handoff.
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased: reviewer minors on the refusal hook (no stop if the damaged-map refusal fails; watchdog FAIL is not in `--qa-script-out`); the new-dialog hook has no RED run (permission denied twice); the item-4 hook covers `load_map` only (other open paths call the same `check_map_for_opening`); `save_map_reporting_errors` ignores `map->headless`; earlier filed items (doodad hook minors, `write_active_mdx` `[[nodiscard]]`, DoodadAddAction stale selection, DoodadDeleteAction redo, MDL camera DOF) remain in the git history of this file.
- QA focus finding: the earlier "355 foreground changes" was the monitor counting the user's own window switches; HiveWE never took focus. Log the foreground pid, not the change count.
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
Asana 1220362272870417 Regions palette hook, or another eligible "Next up" ticket (Asana 1220328345553589). Skip 1220841054745630 (HD only); 1220832845348582 stays filed.

## Backlog only, not next steps
1220826761382041, 1220824126875159, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, 1220829561061744, 1220829428496325, 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): standing order, redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE is running and say so in chat (now in guardrails; replaces "no Tools redeploy unless asked").

## Retro
Item 1+4 hook 709 s dev + reject fix round 189 s; item 2 hook 382 s + 195 s minors; QA 127 s, 116 s, 361 s, 122 s; 12 spawns. One reject round: the hook could launch WC3 if a refusal regressed. Proposal: brief cpp-dev that QA hooks calling entry points that launch processes or open pickers must gate each call on the previous refusal. RED runs get denied by permissions when rebuild and launch are chained: orchestrator runs RED steps as separate small commands, or accepts the reviewer's logical RED. Kept: reviewer+QA in parallel, deploy via release-manager.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\f39a05c8-ec54-4e41-be6c-f228aec3e381\scratchpad` (runhook.ps1, fgrun.ps1; may vanish).
