# Session handoff

Written 2026-11-23 (session 4 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: Explore, cpp-dev (Sonnet), reviewer (Opus), qa-verifier (Sonnet), release-manager, docs-keeper, designer. 8 spawns, many resumes (budget reached).

## Checks waiting on the user
1. Open the Tools-folder HiveWE (deployed 19:32 from the ae098a2 build). File > Settings > Theme > Flat Dark, then Flat Light. Open each of these and report anything ugly or broken:
   - Settings, Unit Properties (select a unit, Edit), Map Settings, Scenario Properties, Gameplay Constants (names readable in Light, no clipped first row).
   - Object Editor: Flat Light must be light with tree arrows. Click Custom in the Units browser, then Edit in the Object Editor from a placed stock unit: it must not crash.
   - New Map, Asset Manager, Map Protector, Item Tables, Config, Change Tileset, Tile Pathing.
2. Selection Properties panel: select a unit and a doodad, move or rotate it; the panel should update. Switch theme live with a unit selected.
3. Optional real-game 3.0.1 load check (water colour, start locations): only if the user says go after confirming the Battle.net login.

## State
- Pushed to `main`:
  - a08421d + d67c71b: read-only Selection Properties panel in the Flat Properties dock (Asana 1220768539674005, done).
  - eca506d: Flat restyle of Settings, Unit Properties (incl. Edit Drop Item), Map Settings, Scenario, Gameplay Constants; shared Flat dialog QSS; hooks `--qa-open-dialog <name>` and `--qa-dialog-tab <n>` (need `--qa-offscreen`).
  - ae098a2: Flat restyle of Object Editor, New Map and File menu, Asset Manager, Map Protector, Import Object Data, Item Tables, Config, Change Tileset, Tile Pathing. Fixes an Object Editor `select_id` crash when the Custom browser filter hides the object. Hooks skip QSettings writes offscreen.
- Mockups for all windows are in `designs/proposals/ui-modernization/` (`overview.png`). Trigger Editor is intentionally skipped (GUI rework ticket).
- CI: eca506d green (run 45235175066). ae098a2 was just pushed and its result is unknown; `compile.yml` cancels older runs, so take it from the newest run.

## Next step
Asana 1220826718190444 "Flat UI: Model Editor build" (mockup approved, `renders/model-editor-flat-*.png`). Start with `wc3-format-specialist`, given the references WhiteoutLib and WhiteoutTex (github.com/FernandoS27/...) and the local `D:\Stuff\Warcraft_3_All_Assets\Projects\WarMash`. Then a FRESH `cpp-dev`; review and QA in parallel; no gate 2 (the user reviews at the end). After that: 1220824029601558 Change Tileset preview + custom tilesets (needs a designer mockup, gate 1, then the format specialist). At most one parity item from 1220328838360686.

## Backlog only, not next steps
1220828873067911 Properties dock leftovers, 1220818407294566 Flat dialogs leftovers, 1220824083919839 Flat leftovers batch 2, 1220768408128534 Flat UI leftovers, plus 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, `ColorButton` alpha.

## Decisions
- User, 2026-11-23:
  - All mockups approved; Trigger Editor skipped; Model Editor redone, then approved.
  - The Preferences tab in Map Settings stays as is.
  - Testing happens at the end; the orchestrator does not stop for per-step approvals.
  - The Tileset dialog needs a preview and custom tilesets via import (ticketed).
- Earlier user decisions (2026-11-22 and before): see the git history of this file (bcf5558 and 74233f7).
- Orchestrator calls the user may overrule (2026-11-23):
  - The Regions tab stays a placeholder.
  - Owner colour names come from `WESTRING_UNITCOLOR`.
  - Health is shown as a percent.
  - `.claude/settings.local.json` got Agent-preset allow rules (the user asked); it stays out of git.

## Scratch (may vanish)
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\835c0c73-5f69-4e19-a659-f54a7b73b8b3\scratchpad`: `shot.ps1`, `dshot.ps1`, `sandbox\`, `reg-before.reg`.
