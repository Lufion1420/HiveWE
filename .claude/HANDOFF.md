# Session handoff

Written 2026-11-22 (third session of the day). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Opus 5.5. Subagents: cpp-dev (Opus for the save fix, Sonnet for the UI), reviewer (Opus), qa-verifier (Sonnet), designer (Sonnet), docs-keeper (Sonnet), release-manager (Sonnet). 8 spawns (budget reached); resumes not counted.

## Checks waiting on the user
1. Look at the new Flat UI in the Tools-folder HiveWE (deployed 12:09, commit 074f2d1): File > Settings > Theme > "Flat Dark" or "Flat Light" > OK. Check the toolbar, the left sidebar (mode buttons, Minimap, palette), the right Properties/Preview, the status bar, the Terrain tabs, Ctrl+T = Test Map and Ctrl+Shift+W = Wireframe. Say what still differs from the mockup (`designs/proposals/ui-modernization/renders/main-window-flat-dark.png`). The UI ticket stays open until then.
2. Optional: real-game 3.0.1 load check of the water colour and start-location output (d86f067, 61ce7a8). Agents run it; the user only confirms that the Battle.net login works and says go.

## State
- `main` is clean and pushed.
- Shipped this session:
  - fb3f315: Pencil replaced by HTML mockups plus the Qt prototype gate; `.pen` files archived in `designs/archive/pencil`.
  - 5ed01f8: save-failure fix (Asana 1220330960297388, closed). The full sweep was identical twice (reports 7 and 8).
  - a2bc029: mockups.
  - 45027c0: docs.
  - 074f2d1: Flat themes (Asana 1220328401966062, open pending the user's look).
- The Tools-folder HiveWE runs 074f2d1 (deployed 12:09; `Qt6Svg.dll` and `imageformats/qsvg.dll` present).
- CI: 9db2609 passed (run 45003572795). Check the newest run for 074f2d1 (a watch was started at handoff).

## Next step
"Flat UI: Selection properties panel" (Asana 1220768539674005, Next up), after the user's look at the Flat UI; fold in the user's feedback first. At most one parity item from 1220328838360686 may follow.

## Backlog only, not next steps
1220768408128534 Flat UI leftovers, 1220767914607357 save-failure leftovers, 1220768028401968 Protector (listfile) check, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, `ColorButton` alpha.

## Decisions
- User, 2026-11-22:
  - Drop Pencil; use HTML/CSS mockups plus the Qt prototype gate.
  - Direction A "Flat" (light plus a real dark grey mode, no stone or brown).
  - Mode buttons sidebar-only; the toolbar has only show/hide toggles.
  - Open MPQ and recent maps only in the File menu and the Open dropdown.
  - Brush, Click Helpers and Debug are menu-only.
  - Terrain palette tabs.
  - Ctrl+T = Test Map.
  - Align the window as close as possible to the mockup.
  - QA and test windows must never appear on or take focus from the user's screen.
  - Images in chat are always clickable workspace-relative links.
  - Work order: doodad skin (done), UI modernization, trigger variables (done), save-reports-success (done).
  - UI modernization: Dark and Light from one token file; qtsvg SVG icons, a bundled UI font and a compact toolbar replacing the ribbon (approved).
- User, 2026-11-20: WarMash's harness is the base. Keep the Tools-folder HiveWE current.
- User, 2026-11-09:
  - Maps are Lua-only: Lua first, and JASS must not break.
  - The differential sweep is the default bug finder.
  - The user gets only short, specific checklists.
  - Sweeps run on sandbox copies; originals are never written.
- User, 2026-11-09 and earlier (full list in git history of this file, 74233f7): units and w3i versions, our own 3.0 parsers, MPQ refusal checks, Playwright for hiveworkshop, NCOW feature use, in-process hooks instead of SendInput, WE/game launches only via Battle.net on sandbox copies.
- Orchestrator calls the user may overrule:
  - 2026-11-22: Wireframe on Ctrl+Shift+W (Ctrl+W is water).
  - 2026-11-22: Properties/Regions stays a placeholder until its own ticket.
  - 2026-11-22: Terrain tabs keep a small enable checkbox.
  - 2026-11-22: F5 (Protector ofstreams) split out.
  - 2026-11-22: TriggerVariable unknown=1 default (unverified against a WE wtg; ticketed).
  - Earlier: the doodad script passes the .doo skin id; the specialist stays on Opus; worktree builds stay banned; commit only after QA PASS; the orchestrator picks the next ticket itself.

## Scratch (may vanish)
- Sweep setup (`stock`, `stock_scripts`, `w4`) from the earlier session: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\w4\`. Copy it rather than rebuild.
- Gate 2 extras: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\eee3cef5-9d19-4890-b92d-7a83ec5d894c\scratchpad\gate2-extra\`.
- NCOW originals (`D:\Stuff\Warcraft_3_All_Assets\Projects\Naruto Clash of Worlds Content\Map\NCOW-Arena\`) stay read-only; work on a scratchpad copy.
