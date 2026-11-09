# Session handoff

Written 2026-11-09 (night). Overwritten at each handoff.

## State
- `main` holds all work; tree clean after the handoff commit. This session's commits:
  - 729703e, 8615210, 0b59035: guidance (format rules for SetTeams/placement/race selectable, unit colour/gold, map-diff README notes on folder maps, warcraft-vscode bundles, drive.py quirks).
  - 5d36284 report 2 (`.claude/tools/map-diff/reports/2026-11-09-second-sweep.md`): kinds 1 and 2 gone; new kind 78 (waygates).
  - abad589 SetTeams = player count, SetGamePlacement = TEAMS_TOGETHER when >1 human, SetPlayerRaceSelectable = !(w3i 0x20) || dword bit 0x2 || race selectable, ForcePlayerStartLocation only on dword bit 0x1. Reviewed, QA PASS, report 3 (`2026-11-09-third-sweep.md`): kinds 3-5 at 0, nothing else changed. CI run 42712576202 passed.
  - efc379b SetUnitColor (teamcolor, or placed colour when customteamcolor = 1) + SetResourceAmount for Agld/ugol types. Reviewed APPROVE, ctest 253/253. **Fourth sweep NOT run** (stopped at session end; its leftover processes are gone). CI for efc379b not checked yet.
- Asana: placement ticket closed (still needs moving to "Done — shipped" section). Colour/gold ticket open until sweep 4. New tickets: scenario dword 0/1 bug and Map Info water red/blue swap (under "map settings windows"); waygates kind 78 and CreateBlightedGoldmine (under "map output on save"). Format notes for the next batch are pasted as comments on the water/ally ticket and the doodad skin ticket (scratchpad copies may vanish).
- NCOW (user's map, Lua, warcraft-vscode project): `D:\Stuff\Warcraft_3_All_Assets\Projects\Naruto Clash of Worlds Content\Map\NCOW-Arena\` — folder map `NCOW_Spell_Dev.w3x`, packed `.build\release.w3x` + `.build\war3map.lua` (minified bundle; editor part is module `origwar3map.lua`). READ-ONLY: always work on a scratchpad copy; verify size+mtime of all files unchanged afterwards (4083 files, newest 2026-08-26 14:17:40). Round trip before abad589 was a fixed point (only war3map.imp gains `Images\Spells\Pein\2amfbody.blp`). After abad589 two config() lines change: `SetTeams(4)` → `SetTeams(24)`, placement → TEAMS_TOGETHER (= WE behaviour). User code wraps InitGlobals, InitCustomTriggers, RunInitializationTriggers; keep them global.
- Stock root: `C:/Users/mauri/AppData/Local/Temp/claude/11cf287c6d13d22a/624d2e1b-c296-4804-9b85-76d3a36906f9/scratchpad/stock` (+ `stock_scripts`). The sweep kind classifier is a throwaway script in an earlier run's scratchpad (2f985a30-... session); if lost, rebuild from report 2/3 and say so.
- Nothing running.

## Next step
1. Check CI for efc379b (or the handoff commit after it, which cancels it).
2. qa-verifier: fourth sweep (same 237 maps, method as report 3) → kinds 6/7 at 0 in Lua (JASS: only map 0070's 16 stale colour lines expected), no new kinds, no new simdiff errors; NCOW copy line diff; write `2026-11-09-fourth-sweep.md` (or dated the day it runs). Then close the colour/gold ticket.
3. Next batch (cpp-dev on Opus, reviewer, QA+sweep): water colour + ally priorities + InitAllyPriorities omission, then doodad skin ids (load bug in `Doodad::init`). Exact rules are in the Asana comments on those tickets.
4. Then the known items: day/night models, HD water, race skin, item colours, skin fields. JASS-only kinds stay low priority.

## Decisions the user made
- 2026-11-09: user's maps are Lua-only; Lua first, JASS only must not break.
- 2026-11-09: no more open-ended manual game testing; the differential sweep is the default bug finder. The user gets only short, specific in-game checklists for what the tools can't see.
- 2026-11-09: run the sweep on sandbox copies of NCOW (folder map and `.build\release.w3x`); originals never written.
- Units: random type 2 count and empty ids are fixed for all versions.
- Units: a sub 9 v12/v13 file saves as sub 11 only when a unit holds a non-default item table or str, agi or int. w3i flag 0x4000 is left alone.
- Units: an id over 4 bytes is checked before saving, with a dialog. The `write_id` throw stays.
- w3i keeps the source version on save: v39 stays v39, older maps save as v33.
- 3.0 parsers: our own against verified layouts; upstream is reference only.
- MPQ refusal checks before unpacking; preflight covers w3e, w3s, wpm; a map whose wtg or wct fails to parse is refused.
- Playwright allowed for hiveworkshop.com (plain browser, no login, robots.txt).
- NCOW uses no tech tree, upgrades, random groups, custom colours or waygates.
- Related tickets grouped under parent tickets.
- Checks blocked by no-SendInput get in-process hooks; agents may launch the World Editor / game (via Battle.net) on sandbox copies after telling the user; never launch `Warcraft III.exe` directly.
- Script fixes target general save correctness; Forgotten Hollow is a reference, not the goal.

## Orchestrator calls the user can overrule
- Labelled sounds write only duration and volume until the UI/SoundInfo SLKs are loaded (ticketed).
- Unknown sound duration (mp3, missing file) leaves SetSoundDuration out instead of writing 0.
- No Blizzard files in the repo; stock tests read external files via env vars.
- `--resave` exits 3 on script compile error.
- Unit x, y, z and record order are not kept byte-exact on save.
- Camera 3.0 script calls deferred to "[3.0] Emit 3.0 calls in the script generator".
- efc379b committed to main before its sweep (tests + review passed) so the tree was clean at handoff.
- JASS output keeps MapScriptWriter's spacing (`call X(a, b)`), not the WE's `call X( a, b )`.
- Map 0070's 16 unexplained SetUnitColor lines are accepted as a stale WE script.

## Open questions (waiting on the user)
- Optional, 30 s: in the World Editor, Scenario → Map Options on NCOW — is "Fixed Player Settings" ticked, and does your lobby rely on placement "use map settings"? If not, the new `SetTeams(24)` / TEAMS_TOGETHER lines can stay (they match what the WE writes).
