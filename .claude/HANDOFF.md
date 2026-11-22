# Session handoff

Written 2026-11-22. Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Opus 5.5. Subagents: cpp-dev (Sonnet for weather, Opus for doodad), reviewer (Opus), qa-verifier, release-manager, docs-keeper, Plan (Sonnet), designer. 8 spawns; resumes not counted.

## Checks waiting on the user
1. Pencil MCP is disconnected. After the user ran Developer: Reload Window, `get_app_state` still reports "A document needs to be open in the editor", from both the main session and the designer. Likely cause: the reload restarted the Pencil extension and the Claude session keeps a stale MCP connection; a new Claude Code session should reconnect it.
   - Steps: open `designs/current/main-window.pen` in VS Code so the canvas shows, start the new session, and do not click in VS Code while the designer runs.
   - If it still fails: run `/mcp` and reconnect `pencil`. Otherwise the designer can launch its own non-elevated VS Code with a scratchpad `--user-data-dir` (the user must approve the extension install).
2. Optional: the real-game 3.0.1 load check of the water colour and start-location output (d86f067, 61ce7a8). The user must first confirm the Battle.net login works.

## State
- `main` is clean and pushed at the handoff commit. This session's commits:
  - 085e648: Map Info weather fix.
  - d2de037: SLK and UIA notes.
  - ba3a385: Choosing-work anti-drift rule.
  - f1d423f: doodad skin fix.
  - 3fc5fff: sixth sweep report.
- CI: b25f32d passed (run 44778547537). Later pushes cancel earlier runs, so check the newest run for f1d423f/3fc5fff.
- The Tools-folder HiveWE (`D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`) runs the f1d423f build, deployed 2026-11-22 08:48.
- Asana: closed 1220749867095780 (weather) and 1220369827454184 (doodad skin). New 1220767565409503 (Next up): custom sound combo, same SLK first-column bug. New 1220767516485546 (follow-ups): dangling skin_id after a custom type is deleted, plus a tighter resave_smoke regex.
- UI modernization 1220328401966062 is in progress:
  - The Plan evaluation is in the ticket comment: stay on Qt Widgets with a design-token theme; QML only later for an isolated panel; WebEngine is not viable.
  - No frames exist yet; the empty proposal file was removed.
  - The designer had read `main_ribbon.cpp` and mapped the ribbon commands for the command-map frame. A fresh designer must redo this.

## Next step
1. UI modernization 1220328401966062: designer frames in a new file `designs/proposals/ui-modernization.pen`: `Main Window / Flat Dark`, `Main Window / Warcraft`, `Tokens / Both directions`, `Toolbar / Command map`. Then stop for user approval. Next: the Object Editor in the chosen direction, then the first code slice (token file plus a QSS template that also builds the QPalette, then the ribbon and ADS chrome).
2. "[Parity 2a] New trigger variables get no type and break the script" 1220328820523202.
3. "Map save can report success after a failed or truncated file write" 1220330960297388.
4. At most one parity item from "map output on save".

## Backlog only, not next steps
1220767565409503 custom sound, 1220767516485546 dangling skin, the four earlier follow-ups (1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131), the weather name showing with literal SLK quotes, and `ColorButton` dropping alpha.

## Decisions
- User, 2026-11-22: work order is (1) finish doodad skin 1220369827454184; (2) "Decide UI modernization approach" 1220328401966062 (`Plan` agent compares Qt Widgets+QSS / QML / web, then `designer` mocks the main window and one editor in `designs/proposals/`, then stop for approval); (3) "[Parity 2a] New trigger variables get no type and break the script" 1220328820523202; (4) "Map save can report success after a failed or truncated file write" 1220330960297388.
- User, 2026-11-22, UI modernization answers: mock two directions side by side, Flat dark IDE and Warcraft gold and stone; Dark and Light themes from one token file; approved qtsvg SVG icons, a bundled UI font, and a compact toolbar replacing the ribbon.
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
- Orchestrator call the user may overrule (2026-11-22): the doodad script passes the .doo skin id. Whether the World Editor does is unverified (open question in `.claude/tools/map-diff/sweep/README.md`).
- Orchestrator calls the user may overrule: the specialist stays on Opus; worktree builds stay banned; commit only after QA PASS; the orchestrator picks the next ticket itself. Labelled sounds write only duration and volume until the UI/SoundInfo SLKs load (ticketed). An unknown sound duration leaves `SetSoundDuration` out. No Blizzard files in the repo; stock tests read external files via env vars. `--resave` exits 3 on script compile error. Unit x/y/z and record order are not byte-exact on save. Camera 3.0 script calls are deferred. JASS output keeps MapScriptWriter's spacing. Map 0070's 16 `SetUnitColor` lines are accepted as a stale WE script.

## Scratch (may vanish)
- Sweep working dir: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\w4\` (rebuild with `.claude/tools/map-diff/sweep/README.md`).
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\1cbd544a-d0b6-434d-9374-80cfb8538dc2\scratchpad\sixth-sweep.md`: already copied into the repo.
- NCOW originals (`D:\Stuff\Warcraft_3_All_Assets\Projects\Naruto Clash of Worlds Content\Map\NCOW-Arena\`) stay read-only; work on a scratchpad copy.
