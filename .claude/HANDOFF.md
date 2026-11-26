# Session handoff

Written 2026-11-26 (session 36 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1 (Opus), cpp-dev x1 (Opus), reviewer x1 (Opus), qa-verifier x1, release-manager x1, docs-keeper x1, designer x1.
- Spawn count this session: 7 (resumes not counted).

## Checks waiting on the user
1. VFX Sandbox mockups (gate 1). Open `designs/proposals/vfx-sandbox/renders/a-playing-dark.png` (A, recommended: draggable timeline bars) and `b-steps-dark.png` (B: editable table). Reply A or B plus changes.
2. Question: where are compositions stored? Default: one file each, outside the map folder, grouped by map.
3. Question: caster and target models for scale? Default: yes, on by default (Archmage, Footman), changeable and switchable off.
4. Question: may each effect have an end time? Default: yes ("After animation" or "Remove at").
5. Question: HiveWE draws only meshes and ParticleEmitter2 particles, not ribbons, model emitters or popcorn effects. Default: first measure how many of the map's effects use those, then decide whether ribbon support goes into the plan.
6. Question: any system sound around 10:39 on 2026-11-26 (muted QA run)? yes / no.
7. Question: the `.codegraph` index skips all `.ixx` files. Remove it (recommended) or keep it with the warning in the rules?

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15, and every Warcraft III load check until WarMash testing is done.

## State
- 1220329279925658 (safe Export MPQ with result dialogs) shipped as 1476eba, notes ed5ab1a; CI passed (run 45794260491); deployed to the Tools folder; ticket in "Done — shipped". Review APPROVE twice, QA PASS, ctest 541/541, export hook 30/30, NCOW-Main (1126 files) and NCOW-Arena (4080 files) archives byte-identical (before the final small fix round).
- Not verified: loading an export in the game, dialog appearance, failure of the final archive check or folder listing, silence of the muted QA runs by ear.
- Offscreen QA runs are muted from 1476eba on (user complaint).
- 1220917410779541 "VFX Sandbox" (user's request, merged with the old Effect Placer idea): mockups in `designs/proposals/vfx-sandbox/`, untracked on purpose until approved. No code.
- NCOW fix from session 35 still uncommitted in the user's own repos on purpose (`SasukeHebi.lua` sound path).
- `.claude/settings.local.json` (local, ignored) turns off 4 plugins and 14 skills; removal check passed this session.
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220925910268643 (Map Protector pack delete-first), 1220925924167629 (MPQ hash table and 2 GB), 1220926531963465 (Lua syntax check, "Next up").

## Next step
1220917410779541 "VFX Sandbox: preview map and stock effects together on a timeline". After the user's gate 1 reply: measure ribbon/emitter use in copies of the user's imported effects, settle scope, then the Qt prototype (gate 2). If the user has not replied, take 1220926531963465 "Lua maps: saving does not check the script for syntax errors" (risky track).

## Backlog only, not next steps
1220329469211176, 1220329518098244, 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582, 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

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
Orchestrator (2026-11-26): Export MPQ shows a success message; a read-only target is refused. VFX Sandbox is the next ticket.
Orchestrator (2026-11-25): backups cover the files Save can change, keep newest 10 plus first per session and per day for 14 days, no restore window; a failed backup never blocks a save.
Orchestrator (2026-11-25): Save As refuses a folder that holds files; no overwrite option.
Orchestrator (2026-11-25): a script compile failure still counts as saved (filed); the script is kept whenever wtg or wct is missing.
Orchestrator (2026-11-25): in the Asset Manager an uncertain reference counts as used.
Orchestrator (2026-11-24): an unreadable trigger or script file refuses the map; wtg deleted-item lists are written back as read; new object ids are exactly 4 ASCII letters or digits; Open Map (MPQ) and Map Protector refuse unsafe entry names; a `Map::load` failure after the body check opens a blank template map; `--resave` exit 5 means objects were dropped (exit 3 wins).

## Retro
Session 36: 7 spawns, 1 risky ticket in about 50 minutes (2 rounds) plus mockups. Specialist 4 min / 144k tokens; cpp-dev 19 + 6 min / 209k; reviewer 2 + 0.5 min / 89k; QA 12 + 1 min / 165k; release-manager 1 min / 78k; docs-keeper 1 min / 99k; designer 12 min / 260k. No build-lock waiting seen; no full sweep needed (about 25 min saved against session 35). Causes: one fix round from audible QA dialogs (fixed for good) and a read-only target left out of the first brief (put every specialist "trap" into the cpp-dev brief as require or exclude); the main session read two source files, which injected four rule files (use Grep with context, habit already recorded); the first CI watch used a short SHA and failed (pending docs-keeper note: list runs, then watch by run id). Kept: specialist first (found the silent missing-listfile failure), QA on real-map copies (found the missing Lua syntax check). No review or test removed.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\08e71f0d-0729-4cee-b84a-c249a848d5ed\scratchpad` (may vanish; `qa_mpq` holds the real-map archive checks). Older: session 35 `...\fab3f1c0-aa86-4152-a57d-9bca50b6855d\scratchpad` (`qav\sweep\w4\work`, twelfth sweep; `qav\code` holds `prep.ps1` and `cmp_sweep.py`); session 33 `...\c85be5c8-e658-4d4f-963a-a25e71743bd5\scratchpad` (eleventh sweep baseline); stock map extraction `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock`. All under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a`.
