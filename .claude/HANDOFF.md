# Session handoff

Written 2026-11-28 (session 40 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1 (Opus), cpp-dev x1, reviewer x1 (Opus), qa-verifier x1, release-manager x1, docs-keeper x1, Explore x1 (no-op, started by mistake), designer 0.
- Spawn count this session: 7 (resumes not counted).

## Checks waiting on the user
1. VFX Sandbox, needed before stage 4, no deadline. What does the "Composition" drop-down list when setups have no fixed folder: recent files (default), or remove the drop-down?
2. VFX Sandbox, same. Dragging an effect's bar on the timeline: does its "Remove at" time move with it? Default: yes.
3. VFX Sandbox, needed in stage 2. Starting positions: default is the target in the middle and the caster 500 map units west of it.
4. Optional: in HiveWE, Model Editor > Open > `C:\Users\mauri\Desktop\Model -Conversions WIP\Storm_VFX\fire\Shisui_BurningKunai_Glow\Shisui_BurningKunai_Glow.mdx`. Blade, orange flames and glow should be textured. Known until R1/R2: flames do not scroll, glow sits beside the blade. Also: does `designs/references/kunai/kunai_without_vs_with_glow.gif` (right side) match what WarMash shows?
5. Optional, explains Magos only: open the kunai in Magos and play Stand. Blade visible with an orange tint = Magos ignored the layer fade; still solid orange = probably draws the inner shell over the blade.

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15, every Warcraft III load check until WarMash testing is done, and (manual testing deferred) the World Editor save that shows which type byte it writes for item Equipment/Tag and unit Occlusion.

## State
- Shipped: 955b5ea (1220963622495886, Storm VFX R0: a locally opened model takes textures from its own folder; damaged textures show a placeholder). Review APPROVE, QA PASS on round 2, ctest 612/612. Deployed to the Tools folder 2026-11-28 (HiveWE was not running).
- CI: 955b5ea is pushed together with the commit holding this file: check `gh run list` and report the newer run before the next push.
- Storm VFX plan (parent 1220963798672494, read its notes): R0 done; R1 1220963921479981, R2 1220963921514005, R3 1220963821078869, R4 1220963798776370; then VFX Sandbox stages. VFX tickets were updated with this order.
- Effects session (Claude Code, folder "Model -Conversions WIP"; seen as `model-conversions-wip-c4` and `-4c`): sends facts about the effects; questions about effect files go straight to it. A new HiveWE session has a new name: if the user wants more messages, give them this session's name.
- VFX Sandbox 1220917410779541: both design gates passed. Prototype source is ONLY in the local git stash "vfx-sandbox gate-2 prototype" (24 files; do not drop it). `src/main.cpp` and `hivewe.cpp` changed again in 955b5ea, so restoring it may need a merge. Its `--qa-open-dialog vfx-sandbox` name still has to go into the ui-capture README when it is committed.
- Not verified: any game, WarMash or World Editor load; local `.mdl`, HD materials, particle textures; File > Open dialog and sidebar previews (code read only).
- NCOW fix from session 35 still uncommitted in the user's own repos on purpose (`SasukeHebi.lua` sound path).
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/`, `designs/references/` (kunai reference images, includes game art) and stray `512684032` stay uncommitted on purpose.

## Next step
1220963921479981 "Storm VFX R1: WarMash draw order, scrolling and flipbook textures". Ticket notes and comments are the cpp-dev brief. Standard track, cpp-dev on Opus (shared render path). Visual checks through the model-editor QA hook against the acceptance set in `C:\Users\mauri\Desktop\Model -Conversions WIP\scripts\archive\s60_hivewe_brief\hivewe_viewer_brief.txt`. Use fresh subagents. Then R2, R3, R4, then VFX Sandbox stage 1 (1220934951315968).

## Backlog only, not next steps
1220963821845299, 1220963799571115, 1220936645897527, 1220933866874909, 1220949503069153, 1220930104098194, 1220931674071675, 1220927374069594, 1220928174960676, 1220329469211176, 1220329518098244, 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582, 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

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
User (2026-11-27): Storm VFX comes first: fix the Model Editor renderer, then `.wmpack`, then the VFX Sandbox. First acceptance case: Shisui_BurningKunai_Glow.
User (2026-11-27, "go"): Model Editor renderer first, map instances later; WarMash rules define "correct", checked against the preview GIFs, user's eye final; WarMash code for facts only; a texture next to the model beats same-named game or map files; particle bugs filed, not fixed.
Orchestrator (2026-11-28): an undecodable map or game texture also shows the placeholder and a log line.
Orchestrator (2026-11-26): ribbons: no drawing in version 1 (0 of 124 Storm effects use them).
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
Session 40: 7 spawns, 1 ticket shipped (R0) plus the viewer audit and plan. R0 about 1 h 35 min over 2 rounds (estimate given was 1 h). Subagent time: specialist 15 min (peak 251k), cpp-dev 28 + 14 min (peak 373k), QA 20 + 24 min (peak 210k), reviewer 2.5 min (peak 98k), release and docs about 8 min. No build-lock waits; resave timeouts under outside CPU load. Causes: first brief did not ask for damaged-input handling (one QA FAIL round, about 40 min; rule added to HARNESS); one no-op spawn by the orchestrator (do not use an agent to wait). Quality: no review, test or QA step removed. Kept: effects session's measured brief and acceptance set before implementation, offscreen QA hook, first-round review plus QA in parallel.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\00850cbd-7788-4d95-be52-86fb9a5cdba0\scratchpad` (may vanish; `r0` cpp-dev hook scripts, `qa1` QA fixtures and captures, `scripts` audit MDX scans). Earlier sessions: `...\966b9364-df44-43c1-98d5-494fca272ad5\scratchpad`; NCOW map copy `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\repro\NCOW_Spell_Dev.w3x`; stock map extraction `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock`. All under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a`.
