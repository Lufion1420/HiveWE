# Session handoff

Written 2026-11-20. Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Opus 5.5; subagents cpp-dev (Opus), a general-purpose comparison agent, docs-keeper, reviewer (Opus), qa-verifier, release-manager. 6 new spawns this session, the rest were resumes.
- The QA and cpp-dev contexts are large (about 300k and 225k tokens): start fresh agents next session.

## Checks waiting on the user
None. (The Fixed Player Settings question was answered from the w3i: 0x20 is set, and the 466-map check showed it makes no difference.)
Not verified in the real game: the water colour and start-location priority output.

## State
- `main` is clean and pushed at 61ce7a8. This session's commits:
  - fde6edf: adopted the WarMash harness.
  - 1cee78f: save crash fix. A silent `exit(0)` is now a refused save with a dialog. The root cause of the user's crash was a stale 2026-06-09 build.
  - ee787ee: `deploy-local` script.
  - f992a97, 2727caa, b88a831: guidance, the 4th and 5th sweep reports, and the sweep scripts now in `.claude/tools/map-diff/sweep/`.
  - d86f067: SetWaterBaseColor and WE start-location priorities. The fifth sweep matches WE by argument in 49 Lua and 185 JASS maps.
  - 61ce7a8: Map Info water/fog colours stored B,G,R,A, plus the `player_mask_bit` guard.
- The user's Tools-folder HiveWE (`D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`) runs the 61ce7a8 build, deployed 17:49. Redeploy after every shipped fix.
- Asana: closed the save crash, colour/gold, placement, water/ally and Map Info water swap tickets. New: 1220749867095780 "Map Info OK clears global weather" (Next up). Under "Harness and quality follow-ups": 1220746915086260 (protector save RAII), 1220746806783937 (protector skin rewrite), 1220749450812492 (region colours B,G,R?), 1220749450805131 (`generate_custom_teams` shift UB).
- NCOW: the user saved it with the new build at 17:11. `SetTeams(24)` / `TEAMS_TOGETHER` are correct WE behaviour (0x20 is irrelevant; the old lines were HiveWE output). The originals stay read-only for agents.
- Nothing running.

## Next step
1. Asana 1220749867095780, weather cleared on Map Info OK: `cpp-dev`, `reviewer` and `qa-verifier` with a GUI check.
2. Asana 1220369827454184, doodad skin id reset to the base id on save. The rules are in the ticket comment; the load bug is in `Doodad::init`. Run a sweep afterwards.
3. Then the remaining "map output on save" items: day/night models, HD water, race skin, item colours, skin fields.

## Backlog only, not next steps
The four follow-up tickets above, and `ColorButton` dropping alpha when a new colour is picked.

## Decisions
- User, 2026-11-22: work order is (1) finish doodad skin 1220369827454184; (2) "Decide UI modernization approach" 1220328401966062 (`Plan` agent compares Qt Widgets+QSS / QML / web, then `designer` mocks the main window and one editor in `designs/proposals/`, then stop for approval); (3) "[Parity 2a] New trigger variables get no type and break the script" 1220328820523202; (4) "Map save can report success after a failed or truncated file write" 1220330960297388.
- User, 2026-11-20: WarMash's harness is the base. Keep the Tools-folder HiveWE current.
- User, 2026-11-09: maps are Lua-only (Lua first, JASS must not break). No open-ended manual game testing; the differential sweep is the default bug finder, and the user gets only short, specific checklists. Sweeps run on sandbox copies of NCOW; originals never written.
- User, 2026-11-09 and earlier:
  - Units: random type 2 count and empty ids fixed for all versions. A sub 9 v12/v13 file saves as sub 11 only when a unit holds a non-default item table or str/agi/int; w3i flag 0x4000 is left alone. An id over 4 bytes is checked before saving, with a dialog; the `write_id` throw stays.
  - w3i keeps the source version on save (v39 stays v39, older save as v33).
  - 3.0 parsers are our own against verified layouts; upstream is reference only.
  - MPQ refusal checks run before unpacking; preflight covers w3e, w3s, wpm; a map whose wtg or wct fails to parse is refused.
  - Playwright is allowed for hiveworkshop.com (plain browser, no login, robots.txt).
  - NCOW uses no tech tree, upgrades, random groups, custom colours or waygates.
  - Related tickets are grouped under parent tickets.
  - Checks blocked by no-SendInput get in-process hooks; agents may launch the World Editor or game (via Battle.net) on sandbox copies after telling the user; never launch `Warcraft III.exe` directly.
  - Script fixes target general save correctness; Forgotten Hollow is a reference, not the goal.
- Orchestrator calls the user may overrule: the specialist stays on Opus; worktree builds stay banned; commit only after QA PASS; the orchestrator picks the next ticket itself. Labelled sounds write only duration and volume until the UI/SoundInfo SLKs load (ticketed). An unknown sound duration leaves `SetSoundDuration` out. No Blizzard files in the repo; stock tests read external files via env vars. `--resave` exits 3 on script compile error. Unit x/y/z and record order are not byte-exact on save. Camera 3.0 script calls are deferred. JASS output keeps MapScriptWriter's spacing. Map 0070's 16 `SetUnitColor` lines are accepted as a stale WE script.

## Scratch
- Stock dump and sweep working dir: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\w4\` (about 99 MB, may vanish; rebuild with `.claude/tools/map-diff/sweep/README.md`).
- NCOW (read-only): `D:\Stuff\Warcraft_3_All_Assets\Projects\Naruto Clash of Worlds Content\Map\NCOW-Arena\`. Work on a scratchpad copy; verify size and mtime afterwards.
