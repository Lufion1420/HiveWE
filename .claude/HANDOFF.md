# Session handoff

Written 2026-11-26 (session 37 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1 (Opus), cpp-dev x2 (one Opus), reviewer x1 (Opus), qa-verifier x1, release-manager x2, docs-keeper x1, designer 0.
- Spawn count this session: 8 (resumes not counted).

## Checks waiting on the user
1. VFX Sandbox mockups (gate 1). Open `designs/proposals/vfx-sandbox/renders/a-playing-dark.png` (A, recommended: draggable timeline bars) and `b-steps-dark.png` (B: editable table). Reply A or B plus changes.
2. Question: where are compositions stored? Default: one file each, outside the map folder, grouped by map.
3. Question: caster and target models for scale? Default: yes, on by default (Archmage, Footman), changeable and switchable off.
4. Question: may each effect have an end time? Default: yes ("After animation" or "Remove at").
5. Question: HiveWE draws only meshes and ParticleEmitter2 particles, not ribbons, model emitters or popcorn effects. Default: first measure how many of the map's effects use those, then decide whether ribbon support goes into the plan.
6. Question: any system sound around 10:39 on 2026-11-26 (muted QA run)? yes / no.
7. Question: the `.codegraph` index skips all `.ixx` files. Remove it (recommended) or keep it with the warning in the rules?
8. Only if the Map Protector window was opened around 11:45 on 2026-11-26: check its checkboxes before exporting (QA changed the saved settings for a few minutes and restored them).

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15, and every Warcraft III load check until WarMash testing is done.

## State
- 1220926531963465 (Lua syntax check on save) shipped as 9d33c05, CI passed (run 45805999063), ticket in "Done — shipped". Review APPROVE, QA PASS, ctest 551/551, 236 maps byte-identical old vs new.
- Batch 1220884498398806 (Trigger Editor categories) + 1220927297736976 (Test Map after script failure) committed and pushed as 9af0421. Review APPROVE, QA PASS, ctest 557/557, 60 maps byte-identical, CI passed (run 45810093727), both tickets in "Done — shipped".
- Not verified: in-game load, a real Test Map launch, appearance of the new message boxes, a real mouse drag.
- NOT DEPLOYED: HiveWE was running (PID 5240, started 11:43) at both commits. First thing next session: if `Get-Process HiveWE` shows nothing and `build/Agent/Release` matches 9af0421, deploy via `release-manager` / `.claude/tools/deploy-local/deploy.ps1`.
- 1220917410779541 "VFX Sandbox": mockups in `designs/proposals/vfx-sandbox/`, untracked on purpose until approved. No code.
- NCOW fix from session 35 still uncommitted in the user's own repos on purpose (`SasukeHebi.lua` sound path).
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220927374069594 (Protector scanner misreads Lua), 1220927808309840 (Lua check leftovers), 1220928174960676 (Protector keeps old war3map.imp), 1220928841134189 (trigger drag and drop off), 1220928918656637 (Trigger Explorer leftovers).

## Next step
1220917410779541 "VFX Sandbox: preview map and stock effects together on a timeline". After the user's gate 1 reply: measure ribbon/emitter use in copies of the user's imported effects, settle scope, then the Qt prototype (gate 2). Without a reply: the top eligible open "Next up" ticket that needs no user decision and no game launch, non-parity first (open subtasks of 1220328751909169 "Support Warcraft III 3.0"), then at most one parity subtask (for example 1220328838083514 "[Parity 3]", check it is open).

## Backlog only, not next steps
1220927374069594, 1220928174960676, 1220329469211176, 1220329518098244, 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582, 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

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
User (2026-11-26): Export MPQ after a failed script generation still exports, with a warning.
User (2026-11-26): VFX Sandbox: one merged ticket, timeline playback, all per-effect settings, custom and stock sources with an easy filter, save compositions then Lua copy.
Orchestrator (2026-11-26): a Lua syntax error keeps the previous `war3map.lua`, like a JASS compile failure.
Orchestrator (2026-11-26): Test Map does not launch when the script could not be updated.
Orchestrator (2026-11-26): Trigger Explorer drag and drop stays off (filed).
Orchestrator (2026-11-26): Export MPQ shows a success message; a read-only target is refused. VFX Sandbox is the next ticket.
Orchestrator (2026-11-25): backups cover the files Save can change, keep newest 10 plus first per session and per day for 14 days, no restore window; a failed backup never blocks a save.
Orchestrator (2026-11-25): Save As refuses a folder that holds files; no overwrite option.
Orchestrator (2026-11-25): a script compile failure still counts as saved (filed); the script is kept whenever wtg or wct is missing.
Orchestrator (2026-11-25): in the Asset Manager an uncertain reference counts as used.
Orchestrator (2026-11-24): an unreadable trigger or script file refuses the map; wtg deleted-item lists are written back as read; new object ids are exactly 4 ASCII letters or digits; Open Map (MPQ) and Map Protector refuse unsafe entry names; a `Map::load` failure after the body check opens a blank template map; `--resave` exit 5 means objects were dropped (exit 3 wins).

## Retro
Session 37: 8 spawns; risky Lua check about 55 min (specialist 5 min / 98k tokens; cpp-dev 24 + 6 min / 299k; reviewer 3.5 min; QA 19 min), standard batch about 26 min (cpp-dev 16 min / 290k; reviewer 2 min / 155k; QA 9 min / 290k). No build-lock waiting seen. Causes: one extra test-only round (6 min) because no test pinned whole script bytes (fixed: HARNESS brief rule; still saved the 25 min sweep); a fresh release-manager for the second changelog used a spawn that a resume would have saved (habit: resume small roles); the orchestrator asked the user to close HiveWE against the standing rule (corrected). Context: no source file reads in the main session (not measured); subagents peaked at 299k. Kept: specialist first (stopped an unpinned Lua 5.5), QA old-vs-new on real maps, in-process hooks (found a real drop-handler crash). No review or test removed.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\25420082-36d0-44c1-b5d1-6b5b1f104335\scratchpad` (may vanish; `qa` and `qa2` hold the old-vs-new results, `luachk` the Lua parse probes). Older: session 36 `...\08e71f0d-0729-4cee-b84a-c249a848d5ed\scratchpad` (`qa_mpq`); session 35 `...\fab3f1c0-aa86-4152-a57d-9bca50b6855d\scratchpad` (twelfth sweep; `qav\code` holds `prep.ps1` and `cmp_sweep.py`); session 33 `...\c85be5c8-e658-4d4f-963a-a25e71743bd5\scratchpad` (eleventh sweep baseline); stock map extraction `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock`. All under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a`.
