# Session handoff

Written 2026-11-24 (session 9 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x2 (Sonnet), reviewer x1 (Opus), qa-verifier x1, release-manager x1 and the docs-keeper that wrote this file. 6 spawns.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. GATE 2 (Change Tileset Variant B prototype): look at `designs/current/change-tileset-proto-dark.png` and `designs/current/change-tileset-proto-light.png` (Flat Dark and Light, 1046x880) and approve or comment. Known deviations from the mockup: Import Custom Tile and the pathing checkboxes are disabled (a later step), no CUSTOM/NEW pills, no second swatch, the tileset combo starts on the map's tileset, OK does nothing on an empty list, Remove is blocked at one tile.
2. Open feedback from session 8 (HiveWE in Tools was deployed from 98372d6; HiveWE must be closed to redeploy):
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.

## State
- Pushed: 487f502 (HD double-`_diffuse` fix in `Terrain::change_tileset`; reviewer APPROVE, QA PASS, ctest 347/347, changelog added). CI on 580092a was green (run 45291470723). CI for 487f502 is not checked yet: take the newest run on main with `GH_TOKEN=$(gh auth token -u Lufion1420) gh run list -R Lufion1420/HiveWE` and name that run.
- UNCOMMITTED, unreviewed, not QA'd: the gate-2 prototype. `src/menus/tile_setter.ui/.h/.cpp` rewritten as the two-list dialog; `src/main.cpp` and `src/main_window/hivewe.cpp` add the hook `--qa-open-dialog tile-setter-changes`. Built, ctest 347/347 by cpp-dev. The old per-removed-tile TilePicker is no longer used here. The Apply path (OK) is untested. Untracked PNGs in `designs/current/` are captures, stage only the two proto PNGs deliberately.
- No test covers `Terrain::change_tileset` (ticket below).
- Asana: 1220824029601558 (Change Tileset) stays open. New tickets 1220840943600840 (blight uses the never-updated member tileset), 1220841054745630 (GroundTexture cache key ignores HD), 1220841004647015 (change_tileset test/hook). Their permalinks showed project 1213554307368042, not HiveWE 1220328345553589: verify they are on the HiveWE board and move them if not.

## Next step
After gate-2 approval, Asana 1220824029601558: cpp-dev (Standard) builds the full dialog: preview + mapping, and single-file import as a map texture override (by path in the map folder; SD `<dir>/<file>.<ext>`, HD `_hd.w3mod:<dir>/<file>_diffuse.<ext>`). `ResourceManager` cannot evict one key; reload via `custom_identifier`. Add the in-process test/hook for `change_tileset` (1220841004647015). Then reviewer and QA in parallel, then commit prototype and dialog together. NEEDS_GAME_TEST: whether 3.0.1 honours the override and a tile count above 16 (sandbox copies, user warned in chat first, Battle.net login confirmed). If the user rejects the prototype, iterate with cpp-dev.

## Backlog only, not next steps
1220826761382041 (ModelView switch; the old ModelView is now unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, plus 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800, only if the user okays and the Battle.net login is confirmed), 1220827826702332 (Save MDX hook).

## Decisions
User:
- 2026-11-24: prefers Change Tileset Variant B.
- 2026-11-24: no per-step approvals, only continue or hand off; offscreen only; warn before any game or window; Lua-only and SD-only maps; workspace-relative image links; retrospective before each handoff.
- 2026-11-24: maps are SD only.

Orchestrator calls the user may overrule (2026-11-24):
- Stopped at gate 2 because user approval is required there.
- Filed the reviewer's two pre-existing notes as tickets instead of chasing them.
- Defaults kept from session 8: OK enabled with the default replacement; single-file import only for v1; custom tiles saved as an imported texture override.

## Retro
Timings (duration_ms): cpp-dev HD fix 183 s, reviewer 28 s, QA 71 s (reuse mode, ctest only, no rebuild), release-manager 8 s, cpp-dev prototype 439 s (46 tool uses). 0 fix rounds.
Why slow: nothing notable. The one cost was waiting on QA before the prototype could start (no source edits while QA holds the build).
Proposals: none needing a rule change. Quality guard: QA reuse mode was used only with the exact build state verified.
Kept: a tiny fix goes through reviewer and QA in parallel; the offscreen hook for dialog capture.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\6cd4c6ea-90f2-4c25-bb99-a2c965288421\scratchpad` (may vanish).
