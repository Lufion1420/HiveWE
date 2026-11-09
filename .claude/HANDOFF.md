# Session handoff

Written 2026-11-09 (late evening). Overwritten at each handoff.

## State
- `main` holds all work; tree clean after the handoff commit. This session's commits:
  - c71ea25 gg_item_ globals for trigger-referenced placed items (rings). User-confirmed in game.
  - 2fe340c InitSounds like the World Editor (voice lines, music, combat sounds). User-confirmed in game.
  - map-diff tools (fdiff, sdiff, simdiff), guidance commits, changelog commits.
  - dc9763a `HiveWE.exe --resave <in> <out>` headless mode (exit 0 ok / 1 refused / 2 usage / 3 script compile error / 4 no CASC or GL) + `resave_smoke` ctest + checked map file writes. CI passed on 70288af (run 42698411034), which contains it.
  - 1091957 first sweep report: `.claude/tools/map-diff/reports/2026-11-09-first-sweep.md` (237 maps, 76 kinds).
  - 5eba4ca autocast orders as char lists (kind 1) + Lua converted-trigger init by wtg classifier (kind 2, also fixed the Cursed_Flute extra). QA PASS, 239/239. CI for 5eba4ca/2dc3186 not checked yet.
- Closed in Asana: sound, rings, "3.0 map refused at Create", sweep ticket, headless round-trip test, kinds 1 and 2.
- New tickets from the sweep (under "World Editor parity: map output on save" unless noted): SetGamePlacement/SetPlayerRaceSelectable vs w3i 0x20/0x40; SetUnitColor + gold SetResourceAmount dropped; SetWaterBaseColor + ally priorities; doodad skin id reset; JASS compile errors (ReturnAction() etc.); humanx03 load failure (under map load robustness). Also: skin fields moved into base object files + upor dropped; imp _Locales entries; Misc.txt case/BOM; BlzSetItemColor; labelled-sound SLK setters; .wav→.ogg paths; Map Protector TRIGSTR inlining; full string-literal escaping (from the security review); JASS nested helpers.
- Test maps for the user: `C:\Users\mauri\Documents\Warcraft III\Maps\HiveWE test\` (1 original, 2 hivewe, 3 rings fix, 4 sound and rings fix). All checks there are done; the folder can be deleted when the user wants.
- Stock root: `C:/Users/mauri/AppData/Local/Temp/claude/11cf287c6d13d22a/624d2e1b-c296-4804-9b85-76d3a36906f9/scratchpad/stock` (466 maps by number, 0239 = Forgotten Hollow). WE scripts: `.../scratchpad/stock_scripts/` (subset.tsv = 191 v39 maps; all.json = all, 49 Lua). Scratchpad folders may vanish; `extract_scripts.py` rebuilds stock_scripts from CASC.
- Nothing running.

## Next step
1. Check CI for 2dc3186 (covers 5eba4ca).
2. Rerun the sweep (`.claude/tools/map-diff/sweep/resave.py` → fdiff/sdiff/simdiff, one map at a time, max-diffs 20000) to confirm kinds 1 and 2 are gone; write report 2.
3. Fix the remaining Lua-affecting kinds in report order: SetTeams (222/49), SetGamePlacement + race selectable (48/47), SetUnitColor + SetResourceAmount (44/43), SetWaterBaseColor + ally priorities, doodad skin ids, then the known items (day/night models, HD water, race skin, item colours, skin fields). Rerun the sweep after each batch.
4. JASS-only kinds are low priority (user is Lua-only).

## Decisions the user made
- 2026-11-09: user's maps are Lua-only; Lua first, JASS only must not break.
- 2026-11-09: no more open-ended manual game testing; the differential sweep is the default bug finder. The user gets only short, specific in-game checklists for what the tools can't see.
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
- Labelled sounds write only duration and volume until the UI/SoundInfo SLKs are loaded (ticketed). A user's channel/pitch/distance change on a labelled sound is still lost.
- Unknown sound duration (mp3, missing file) leaves SetSoundDuration out instead of writing 0.
- No Blizzard files in the repo; stock tests read external files via env vars.
- `--resave` exits 3 on script compile error.
- Unit x, y, z and record order are not kept byte-exact on save.
- Camera 3.0 script calls deferred to "[3.0] Emit 3.0 calls in the script generator".

## Open questions (waiting on the user)
- None. Optional: tell the agent whether to run the sweep on a sandbox copy of the user's own Lua map (NCOW); needs the map's path.
