# Session handoff

Written 2026-11-26 (session 38 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1 (Opus), cpp-dev x3 (two Opus), reviewer x1 (Opus), qa-verifier x1, release-manager x1, docs-keeper x1, designer 0.
- Spawn count this session: 8 (resumes not counted).

## Checks waiting on the user
1. VFX Sandbox scope: of 454 effect models in the NCOW copy (dated 2026-11-20), 17 (3.7%) use ribbons that HiveWE cannot draw; none use model emitters or popcorn. Question: build the sandbox without ribbon drawing, mark those effects "ribbons not shown", and file ribbon drawing as a later ticket? Default: yes.
2. Optional, no deadline: when some converted Naruto Storm effects exist, send their folder so they can be checked for ribbons and other unsupported parts.

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15, every Warcraft III load check until WarMash testing is done, and (manual testing deferred) the World Editor save that shows which type byte it writes for item Equipment/Tag and unit Occlusion.

## State
- Shipped 2026-11-26, all review APPROVE and QA PASS, all deployed to the Tools folder (last deploy 05a15da, HiveWE was not running):
  - e4936b8 unknown object-data fields are kept on save; 20 3.0 fields added (1220328770305449). ctest 569/569; sweep 244 maps, 5 differ only by keeping dropped records.
  - e84afdc three 3.0 map files off the imports list (1220328770232513). ctest 574/574; 2 maps differ in war3map.imp only.
  - 05a15da string escaping in generated scripts (1220368404765486). ctest 583/583; generated script differs on 9 of 245 maps, matching the World Editor.
- CI: e4936b8 passed (run 45829578942). ffaca38 (holds e84afdc and 05a15da) was still running when this was written. The commit after it (approved VFX mockups, guidance, this file) is pushed only after that run; check `git status -sb` and `gh run list`.
- VFX Sandbox 1220917410779541: gate 1 passed (layout A, mockups committed in `designs/proposals/vfx-sandbox/`). All design answers are comments on the ticket. No code yet.
- The `.codegraph` index is removed; guidance and `CLAUDE.md` no longer name it.
- Closed as already shipped: 1220328770305385 (237b507), 1220328579880627 (9a38559).
- Not verified: any game or World Editor load, Map Protector export end to end for the imports change.
- Filed this session: 1220930104098194 (unknown-field leftovers), 1220931674071675 (optimizer skin bone indices); comments on 1220367279958021, 1220845243296342, 1220369437207221.
- NCOW fix from session 35 still uncommitted in the user's own repos on purpose (`SasukeHebi.lua` sound path).
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
1220917410779541 "VFX Sandbox": the Qt prototype of layout A (gate 2: real window with the theme applied, screenshotted, user approves) with the scope from check 1 (take the default if unanswered). The user chooses the caster and target MDX; setups save through a folder picker; each effect ends "After animation" or at a set time. After the prototype is shown, while waiting for approval: 1220364334409503 "[3.0] Stock 3.0 map logs model and texture load errors" (check `git log` first), then at most one parity subtask (for example 1220371123798040, check it is open). Use fresh subagents.

## Backlog only, not next steps
1220930104098194, 1220931674071675, 1220927374069594, 1220928174960676, 1220329469211176, 1220329518098244, 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582, 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE runs and say so.
User (2026-11-24): move done tickets to "Done — shipped"; focus on Asana board tickets over small edge cases.
User (2026-11-24): hand off when context is high; offer it after a large ticket ships, before starting the next.
User (2026-11-25, twice): after a denied combined build command, chose "run steps separately". Not a standing approval: ask again if it happens.
User (2026-11-25): the WarMash script is the allowed standing method for game load checks; no retry chains. It may take window focus on announced runs.
User (2026-11-25): manual testing is deferred; map defects the agent can fix at the source are fixed, not handed back.
User (2026-11-25): keep `security-guidance` and `i-have-adhd` on; unused plugins and skills off per project.
User (2026-11-25): the full map sweep may be skipped under the three conditions in HARNESS.md.
User (2026-11-25): evaluate context size in main session and subagents in the retrospective.
User (2026-11-26): automated checks must make no sound.
User (2026-11-26): hidden or offscreen test windows never get simulated input (guardrail changed).
User (2026-11-26): remove the `.codegraph` index; stopping its HiveWE helper process was approved for that one time.
User (2026-11-26): Export MPQ after a failed script generation still exports, with a warning.
User (2026-11-26): VFX Sandbox: one merged ticket, timeline playback, all per-effect settings, custom and stock sources with an easy filter, save setups then Lua copy; layout A; Save and Open with a folder picker; caster and target models chosen by the user; per-effect end "After animation" or "Remove at".
Orchestrator (2026-11-26): unknown object fields are kept as raw records; known 3.0 skin fields move to the main object file like other skin fields; text (.ini) export still drops unknown fields (filed).
Orchestrator (2026-11-26): `war3mapPostProcessing.txt` counts as a map file; `war3map.soundassets`, `war3mapPreview.dds`, `war3mapmap.dds` were left as they were.
Orchestrator (2026-11-26): backslashes in string variable defaults are written literally, as the World Editor does.
Orchestrator (2026-11-26): a Lua syntax error keeps the previous `war3map.lua`, like a JASS compile failure.
Orchestrator (2026-11-26): Test Map does not launch when the script could not be updated.
Orchestrator (2026-11-26): Trigger Explorer drag and drop stays off (filed).
Orchestrator (2026-11-26): Export MPQ shows a success message; a read-only target is refused.
Orchestrator (2026-11-25): backups cover the files Save can change, keep newest 10 plus first per session and per day for 14 days, no restore window; a failed backup never blocks a save.
Orchestrator (2026-11-25): Save As refuses a folder that holds files; no overwrite option.
Orchestrator (2026-11-25): a script compile failure still counts as saved (filed); the script is kept whenever wtg or wct is missing.
Orchestrator (2026-11-25): in the Asset Manager an uncertain reference counts as used.
Orchestrator (2026-11-24): an unreadable trigger or script file refuses the map; wtg deleted-item lists are written back as read; new object ids are exactly 4 ASCII letters or digits; Open Map (MPQ) and Map Protector refuse unsafe entry names; a `Map::load` failure after the body check opens a blank template map; `--resave` exit 5 means objects were dropped (exit 3 wins).

## Retro
Session 38: 8 spawns, 3 tickets shipped plus 2 closed as already done. Wall clock: ticket 1 about 1 h 45 min, ticket 2 about 32 min, ticket 3 about 40 min. Subagent time: QA 101 min (4 runs, 40 min longest, peak 400k), cpp-dev 36 + 9 + 13 min (peak 327k), specialist 15 min, reviewer 9 min, others 5 min. No build-lock waits seen. Causes: one fix round on ticket 1 (import collisions and an item UI hook not in the first brief); one invalid sweep run (old sandbox took new data); two tickets open although shipped; one QA run took window focus. Fixes recorded in HARNESS and tool docs (check git log before picking a ticket; name GUI hooks and collision tests in briefs; compare `input.lua`; no input to offscreen windows). Quality: review added to the small ticket 2, a full sweep on all three; no review or test removed. Context: no C++ source file reads in the main session (size not measured). Kept: specialist first (found a second bug), reviewer on import paths, changelog check that surfaced the backslash change.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\f9d7bd99-1a7e-4d53-9b52-040346800282\scratchpad` (may vanish; `qa` holds the sweep results, `casc_out` the stock 3.0.1 metadata, `scripts` the specialist scans including `vfx_scan.py`, `vfx.json` the effect count). NCOW map copy used for the count: `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\repro\NCOW_Spell_Dev.w3x`; stock map extraction `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock`; session 35 `...\fab3f1c0-aa86-4152-a57d-9bca50b6855d\scratchpad` (`qav\code` holds `prep.ps1` and `cmp_sweep.py`). All under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a`.
