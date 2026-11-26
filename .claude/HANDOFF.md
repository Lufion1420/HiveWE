# Session handoff

Written 2026-11-26 (session 39 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x2 (one Opus), wc3-format-specialist x1 (Opus), Plan x1, reviewer x1 (Opus), qa-verifier x2, release-manager x1, docs-keeper x1, designer 0.
- Spawn count this session: 9 (resumes not counted).

## Checks waiting on the user
1. VFX Sandbox, needed before stage 4, no deadline. What does the "Composition" drop-down list when setups have no fixed folder: recent files (default), or remove the drop-down?
2. VFX Sandbox, same. Dragging an effect's bar on the timeline: does its "Remove at" time move with it? Default: yes.
3. VFX Sandbox, needed in stage 2. Starting positions: default is the target in the middle and the caster 500 map units west of it.
4. Optional: when some converted Naruto Storm effects exist, send their folder so they can be checked for ribbons and other unsupported parts.

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15, every Warcraft III load check until WarMash testing is done, and (manual testing deferred) the World Editor save that shows which type byte it writes for item Equipment/Tag and unit Occlusion.

## State
- Shipped: e026f60 (1220364334409503: aliased `.dds` textures load, pathing texture fallback, doodad skin case merge, placeholder for unreadable images). Review APPROVE, QA PASS on round 3, ctest 597/597. Deployed to the Tools folder 2026-11-26 (HiveWE was not running).
- CI: ffaca38 passed (run 45842002599), 8ca467a passed (run 45844092873). e026f60 is pushed together with the commit holding this file: check `gh run list` before the next push.
- VFX Sandbox 1220917410779541: both design gates passed (Qt prototype approved 2026-11-26; screenshots in `designs/proposals/vfx-sandbox/prototype/`). Prototype source is ONLY in the local git stash "vfx-sandbox gate-2 prototype" (24 files; do not drop it). `src/main.cpp` and `hivewe.cpp` changed since, so restoring it in stage 2 may need a small merge. Its `--qa-open-dialog vfx-sandbox` name still has to go into the ui-capture README when it is committed.
- Build plan: 6 stage subtasks on the ticket, menu entry visible only in stage 6. No stage has started.
- Filed: 1220933866874909 (DE graphics mode), 1220949503069153 (ribbon drawing), 1220936645897527 (image loading leftovers).
- Not verified: any game or World Editor load; texture paths that need clicks (tile picker, trigger editor icons); HD app view.
- NCOW fix from session 35 still uncommitted in the user's own repos on purpose (`SasukeHebi.lua` sound path).
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.

## Next step
1220934951315968 "VFX Sandbox stage 1: effect engine core (no UI)". The ticket notes are the cpp-dev brief (read them in full). It edits the shared render path, so run Standard track with cpp-dev on Opus and a before/after Model Editor capture in QA. The prototype stays stashed during stage 1. Then stage 2 (1220934915988908: restore the stash, live 3D view). At most one parity ticket may follow (for example 1220371123798040, check it is open). Use fresh subagents.

## Backlog only, not next steps
1220936645897527, 1220933866874909, 1220949503069153, 1220930104098194, 1220931674071675, 1220927374069594, 1220928174960676, 1220329469211176, 1220329518098244, 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582, 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

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
User (2026-11-26): VFX Sandbox Qt prototype approved ("looks really good and thought through").
Orchestrator (2026-11-26): ribbons question unanswered, default applied: no ribbon drawing in version 1, affected effects marked "Ribbons not shown".
Orchestrator (2026-11-26): an unreadable image gets a placeholder and a log line instead of an error; Definitive-only content is a separate ticket.
Orchestrator (2026-11-26): VFX Sandbox is built in 6 stages; the menu entry appears only in the last.
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
Session 39: 9 spawns, 1 ticket shipped plus the VFX prototype (approved) and build plan. Wall clock about 2.5 h; the shipped ticket about 2 h 10 min with 3 rounds. Subagent time: QA 86 min (31 + 18 + 36, peak 417k), cpp-dev 15 + 18 + 14 + 11 min (peak 251k), specialist 13 min, Plan 4 min (peak 251k), reviewer about 4 min over 3 rounds (peak 111k), others 3 min. No build-lock waits. Causes: the alias fix changed which decoder got the bytes (found by cpp-dev); the orchestrator forwarded "throw on failed decode" without asking for a caller check (one QA FAIL round, about 40 min); round 3 QA repeated a 35-map comparison. Fixes recorded in HARNESS: caller check on stricter functions, Smoke tier plus failed cases on fix rounds. Quality: no review, test or QA step removed. Context: no C++ source reads in the main session, 3 screenshots read. Kept: specialist first, read-only planning during builds, stash to keep an unapproved prototype out of other commits.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\966b9364-df44-43c1-98d5-494fca272ad5\scratchpad` (may vanish; `qa`, `qa2`, `qa3` hold QA results and sweep scripts, `spec` the triage note and CASC extraction). Earlier: `...\f9d7bd99-1a7e-4d53-9b52-040346800282\scratchpad` (`scripts\vfx_scan.py`, `vfx.json` effect count); NCOW map copy `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\repro\NCOW_Spell_Dev.w3x`; stock map extraction `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock`. All under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a`.
