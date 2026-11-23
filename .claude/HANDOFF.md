# Session handoff

Written 2026-11-23 (session 5 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist (Opus), cpp-dev x2, reviewer, qa-verifier, release-manager x2, docs-keeper x2. About 11 spawns, many resumes (budget reached).

## Checks waiting on the user
The user postponed testing to the end (2026-11-23).
1. Open the Tools-folder HiveWE (deployed 21:14 from 7d0cc93). File > Settings > Theme > Flat Dark, then Flat Light. Review the earlier restyled dialogs and report anything ugly or broken:
   - Settings, Unit Properties (select a unit, Edit), Map Settings, Scenario, Gameplay Constants (names readable in Light, no clipped first row).
   - Object Editor: Flat Light must be light with tree arrows. Click Custom in the Units browser, then Edit from a placed stock unit: it must not crash.
   - New Map, Asset Manager, Map Protector, Item Tables, Config, Change Tileset, Tile Pathing.
   - Selection Properties panel: select a unit and a doodad, move or rotate it; the panel updates and survives a live theme switch.
2. New, Model Editor: open any model via the Model Browser's "Open in Model Editor", or the Model Editor tab.
   - Check the toolbar, sequence combo, timeline (play/pause/scrub/loop/speed), the Sequences/Geosets/Info docks with Show checkboxes and Wireframe, and the status bar.
   - Drag or undock a dock tab and check the viewport survives.
   - Browse Models (toolbar): categories with counts; search "footman"; double-click opens it as a tab.
   - Expect "!" tiles for stock models with a CAMS chunk (known bug, ticket 1220828986142563). Report anything ugly.
3. Optional real-game 3.0.1 load check: only if the user says go after confirming the Battle.net login (nothing in this session changed save output).

## State
- Pushed to `main`: a96329e (Model Editor part A) and 7d0cc93 (Browse Models + thumbnail cache).
- CI: a96329e green (run 45252917329). 7d0cc93 result unknown; `compile.yml` cancels older runs, so take it from the newest run.
- Asana 1220826718190444 done. Deployed to the Tools folder at 21:14 from 7d0cc93.
- Uncommitted: many untracked `designs/current/*.png` QA screenshots (leave).

## Next step
Asana 1220828986142563 "MDX loader: models with a CAMS chunk fail to parse": `wc3-format-specialist` first, Risky track. QA analysis: size dword top-byte flag in `read_CAMS` in `src/file_formats/mdx/mdx_reader.cpp`. Then 1220824029601558 Change Tileset preview + custom tilesets (designer mockup, gate 1, then the format specialist). At most one parity item from 1220328838360686.

## Backlog only, not next steps
1220826761382041 Object Editor ModelView switch to catalog/cache, 1220824126875159 Model Editor build leftovers, plus 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, `ColorButton` alpha.

## Decisions
- User, 2026-11-23: dialog testing postponed to the end; no per-step approvals. Earlier decisions: see the git history of this file.
- Orchestrator calls the user may overrule (2026-11-23):
  - Model Editor stays view-only (no new save path).
  - Thumbnails for SD folders only.
  - Object Editor ModelView not switched.
  - CAMS parser bug ticketed, not fixed in the Model Editor task.
  - Speed combo sits in the timeline bar.
  - Lighting and other viewport toggles from the mockup not built.

## Scratch (may vanish)
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c6dc9975-ff77-4de1-9d04-20cb016dce09\scratchpad`: `hookshot.ps1`, `hookfgshot.ps1`, `mshot.ps1`, `qa-run.ps1`, `mdx\` extraction scripts.
