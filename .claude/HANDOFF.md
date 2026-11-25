# Session handoff

Written 2026-11-25 (session 35 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist x1 (Opus), cpp-dev x1 (Opus), reviewer x1 (Opus), qa-verifier x1, release-manager x1, docs-keeper x1.
- Spawn count this session: 6 (resumes not counted).

## Checks waiting on the user
The user deferred all manual testing on 2026-11-25. Do not list the deferred checks unless the user asks. Only these are open:
1. First thing in a new session (the orchestrator checks this itself): `product-management:*` skills, `plugin:product-management:*` sign-in entries and the `hyperframes` skills are gone. If product-management is still listed, fix its name in `.claude/settings.local.json`.
2. Question: the new `.codegraph` index skips all `.ixx` files (most of the code), so its "who calls this" answers are incomplete. Remove it (recommended) or keep it with the warning now in the rules? Checked: both installed versions (0.9.9 and 1.0.1) lack `.ixx`; releases newer than 1.0.1 are unchecked.

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15, and every Warcraft III load check until WarMash testing is done.

## State
- 1220329518098116 shipped as 2b583f1 and deployed to the Tools folder: GUI saves back up the map files first (`%LOCALAPPDATA%\HiveWE\HiveWE\backups`), Save As refuses a non-empty folder or one inside the map, a failed Save As no longer redirects the next Save, `war3mapMisc.txt` and the script are replaced through a temp file. Review APPROVE twice, QA PASS (targeted, then smoke and full sweep with 0 differences), ctest 525/525.
- Not verified: the real AppData backup root in a normal (non-offscreen) run, native folder picker, backups through Test Map and Export MPQ, locked file or full disk during a backup.
- NCOW fix, uncommitted in the user's own repos on purpose: `NCOW-Arena` and `NCOW-Main` `src/Characters/Sasuke/SasukeHebi.lua` now name `Sound/Spells/Sasuke/NCOW_Sound_Seinsashu.mp3`. Takes effect after their next build. NCOW-Main's map folder has no `Sound` folder: whether Main loads the file is unverified.
- `.claude/settings.local.json` (local, ignored) turns off 4 plugins and 14 skills for this repo; the user also turned off unused MCP servers.
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/` and stray `512684032` stay uncommitted on purpose.
- Filed this session: 1220898431103598 (QA hooks write the user's recent maps), 1220899403091128 (backup leftovers), and a ticket to move the QA hooks out of `hivewe.cpp`.

## Next step
1220329279925658 "[Wish] Report Export MPQ results and keep the old file until the new one succeeds". Verified in code today: `write_map_archive` (`hivewe.cpp`) deletes the old archive first and ignores add, compact and close failures. Overlaps one item of 1220767791486098. Game checks wait.

## Backlog only, not next steps
1220329469211176 (non-map files in imports), 1220329518098244 (Map saved after script error), 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944 (unpack-folder item), 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (UI modernization, needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582 (memory growth), 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only), "Harness and quality follow-ups" section.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE runs and say so.
User (2026-11-24): move done tickets to "Done — shipped"; focus on Asana board tickets over small edge cases.
User (2026-11-24): hand off when context is high; offer it after a large ticket ships, before starting the next.
User (2026-11-25, twice): after a denied combined build command, chose "run steps separately" (plain Agent-preset commands). Not a standing approval: ask again if it happens.
User (2026-11-25): the WarMash script is the allowed standing method for game load checks; it may fail on Blizzard login, so no retry chains.
User (2026-11-25): manual testing is deferred; map defects the agent can fix at the source are fixed, not handed back.
User (2026-11-25): keep `security-guidance` and `i-have-adhd` on; unused plugins and skills off per project.
User (2026-11-25): the WarMash script may take window focus (`-Foreground`) on announced game runs. General permission only.
User (2026-11-25): the full map sweep may be skipped when a writer change keeps bytes identical, tests pin that, and an old-versus-new `--resave` comparison on 30 or more maps shows 0 differences.
User (2026-11-25): run `codegraph init` (done; see question 2).
User (2026-11-25): evaluate context size in main session and subagents in the retrospective.
Orchestrator (2026-11-25): backups cover the files Save can change (not imports), keep newest 10 plus first per session and per day for 14 days, no restore window; a failed backup never blocks a save.
Orchestrator (2026-11-25): Save As refuses a folder that holds files; no overwrite option.
Orchestrator (2026-11-24): a trigger or script file HiveWE cannot read fully (bad header included) refuses the map.
Orchestrator (2026-11-24): wtg deleted-item lists are written back as read; new ids skip deleted ids.
Orchestrator (2026-11-24): new object ids are exactly 4 ASCII letters or digits in the dialogs.
Orchestrator (2026-11-24): Open Map (MPQ) and Map Protector refuse archives with unsafe entry names.
Orchestrator (2026-11-24): a `Map::load` failure after the body check opens a blank template map.
Orchestrator (2026-11-24): `--resave` exit 5 means objects were skipped and dropped; exit 3 wins when both apply.
Orchestrator (2026-11-25): a script compile failure still counts as saved (filed, not changed); the script is kept whenever wtg or wct is missing.
Orchestrator (2026-11-25): in the Asset Manager an uncertain reference counts as used.

## Retro
Session 35: 6 spawns, 1 risky ticket, 2 rounds, about 1.5 hours. Specialist 4 min / 103k tokens; cpp-dev 27 + 5 min / 266k; reviewer 5 + 0.3 min / 122k; QA 11 + 28 min (25 min of it the full sweep) / 235k; release-manager 1 min / 76k; docs-keeper 2 min / 135k; main session 248k already during the first ticket. Causes: unused MCP servers, plugins and skills in every context (turned off, effect not yet measured); broad Asana tool load and source-file reads in the main session (now recorded as habits); `hivewe.cpp` 220 KB with half QA hooks (ticket filed); codegraph was not initialized (now run, but it skips `.ixx`: question 2). Kept: specialist first (found 2 data-loss bugs), QA on real-map copies, QA code reading (found the clock-change bug). No review or test removed.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\fab3f1c0-aa86-4152-a57d-9bca50b6855d\scratchpad` (may vanish; `qav\sweep\w4\work` is the twelfth sweep, 0 differences to the eleventh; `qav\code` holds `prep.ps1` and `cmp_sweep.py`). Session 33 scratch with the eleventh sweep baseline: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c85be5c8-e658-4d4f-963a-a25e71743bd5\scratchpad`. Stock map extraction: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock` (may vanish).
