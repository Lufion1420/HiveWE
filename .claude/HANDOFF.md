# Session handoff

Written 2026-11-24 (session 20 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x3, reviewer x3 (all APPROVE), qa-verifier x3 (PASS x2, NEEDS_HOOK x1 then PASS), release-manager.
- Spawn count this session: 10 (spawn budget reached).

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional: the session-8 feedback checks (deployed Tools build current with f5aacf5).
   - Select a unit, left-drag its Selection Properties preview: starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file": Browse Models opens with a Path line; OK updates the cell at once.
   - Tiles in view play Stand (up to 12).
   - Map with a literal (non-TRIGSTR) w3i name: Map Description shows it; edit and Save persists.
2. Decision (WarMash direct-exe game launch as the standing method): ask again when the user says the WarMash game tests are finished.
3. Optional new fix check: in HiveWE select 2 doodads, press Delete, then Ctrl+Z: they come back (automation covered this via `--qa-doodad-undo-check`, so this is only a sanity look). Tools folder is NOT redeployed with it.

## State
- 8fb9ed7: Model Editor `save-mdx:<name>` QA hook + stale reopen-cycle comment fixed. Asana 1220827826702332 done.
- 6c84f94: DoodadDeleteAction fill loop restored (Ctrl+Z after delete/cut restored nothing), `--qa-doodad-undo-check` hook, 3 doodad data tests, CHANGELOG entry. Pushed. Asana 1220362272870417 stays open (items 1, 2, 4 need hooks; comment on the ticket has the designs).
- CI: newest run covering these: take the result of the run for 6c84f94 (cancel-in-progress cancels older ones). Not checked at handoff.
- Stop hook (stop-guard.ps1) is registered in settings.json; it had not fired by the time of handoff.
- Asana 1220832845356081 closed (fixed by f5b2461).
- Tools folder lacks the MDX guard, camera change, doodad undo fix: no redeploy unless the user asks.
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed. 1220329469370593 (listfile) waits for WarMash.
- Filed, not chased: reviewer minors on the doodad hook (hand-built WorldEditContext instead of ribbon undo slot; `--help` spacing; `--palette` value unchecked; `--qa-script-out` write failure unreported); `write_active_mdx` lacks `[[nodiscard]]`; DoodadAddAction::undo / place_clipboard may leave stale selection pointers (unverified); redo of DoodadDeleteAction removes the last N doodads (wrong after interleaved edits); MDL reader/writer drops camera DOF; earlier filed items remain in git history of this file (mdx_reader.cpp:750 32-bit wrap, scenario JSON unknown_flags, pack-failure cleanup, etc.).
- Untracked `qa-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
Pick any eligible "Next up" ticket (Asana 1220328345553589) and start at once. Good candidates: finish 1220362272870417 items 1, 2, 4 (hook designs in the ticket comment); skip 1220841054745630 (HD only); 1220832845348582 stays filed.

## Backlog only, not next steps
1220826761382041, 1220824126875159, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, 1220829561061744, 1220829428496325, 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE` and deploy when not running.
User (2026-11-24): keep working across tickets without waiting for "continue".

Orchestrator calls the user may overrule (2026-11-24): no Tools redeploy for MDX/doodad fixes; filed reviewer minors instead of a fix round; picked 1220362272870417 over larger tickets because it was concrete and found a real data bug.

## Retro
3 tasks (comment+save-mdx hook 209 s dev, doodad undo tests 273 s dev, hook 374 s dev); 10 spawns; QA run 295 s, 111 s, 145 s. Slow spot: QA needed a second dev round (NEEDS_HOOK) because the first cpp-dev brief did not require a hook for an untestable fix; proposal: when a bug fix cannot be unit-tested, brief cpp-dev to add the hook in the same round. Kept: reviewer+QA in parallel, changelog by release-manager in parallel with QA.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\3c085125-e01f-4ebc-9cae-78f1c793455c\scratchpad` (may vanish).
