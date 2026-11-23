# Session handoff

Written 2026-11-23 (session 7 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist (Opus), cpp-dev (Opus, 1 resume), reviewer x2 (Opus), qa-verifier x2, release-manager, docs-keeper x2. 9 spawns (budget reached).

## Checks waiting on the user
The user postponed testing to the end (2026-11-23). The user also asked to be asked for feedback on the dialogs, Model Editor and Browse Models: still unanswered, ask first.
1. Open the Tools-folder HiveWE (deployed 22:25 from 237b507). File > Settings > Theme > Flat Dark, then Flat Light. Review the earlier restyled dialogs and report anything ugly or broken:
   - Settings, Unit Properties (select a unit, Edit), Map Settings, Scenario, Gameplay Constants (names readable in Light, no clipped first row).
   - Object Editor: Flat Light must be light with tree arrows. Click Custom in the Units browser, then Edit from a placed stock unit: it must not crash.
   - New Map, Asset Manager, Map Protector, Item Tables, Config, Change Tileset, Tile Pathing.
   - Selection Properties panel: select a unit and a doodad, move or rotate it; the panel updates and survives a live theme switch.
2. Model Editor: open any model via the Model Browser's "Open in Model Editor", or the Model Editor tab.
   - Check the toolbar, sequence combo, timeline (play/pause/scrub/loop/speed), the Sequences/Geosets/Info docks with Show checkboxes and Wireframe, and the status bar.
   - Drag or undock a dock tab and check the viewport survives.
   - Browse Models (toolbar): categories with counts; search "footman"; double-click opens it as a tab.
   - New: HD models (previously "!" tiles) should now open whole and show thumbnails in Browse Models. Report anything ugly.
3. Model Editor Save MDX now keeps the model's version (HD stays v1800). Save an HD model to a copy and reopen it.
4. Optional real-game 3.0.1 load check of a resaved v1800 (ticket 1220829428496325): only if the user says go after confirming the Battle.net login.

## State
- Pushed to `origin/main`: 237b507 "Load HD v1800 MDX models whole: u16 SKIN in v1400+ geosets". Deployed to the Tools folder at 22:25 (deploy smoke OK). Asana 1220828998277683 done with comment.
- Verified: ctest 335/335; stock sweep 20/20 whole (10 HD skins); offscreen Browse Models HD tiles render; resave smoke.
- Not verified: v1300 boundary, real-game load of resaved v1800, Save MDX dialog wiring (test-covered only).
- CI run 45270385308 on 237b507 was in progress at write time. This handoff commit cancels it (`cancel-in-progress`): take the result from the newest run and name that run.
- New tickets: 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (verify 3.0.1 loads resaved v1800), 1220827826702332 (Save MDX hook).
- `.claude/plans/wc3-3.0-compatibility.md` may be stale (Needs verification).
- Uncommitted: many untracked `designs/current/*.png` QA screenshots (leave).

## Next step
Alternative A: Asana 1220824029601558 Change Tileset preview (designer, gate 1). Alternative B: 1220829561061744 (Risky). At most one parity item from 1220328838360686.

## Backlog only, not next steps
1220826761382041 Object Editor ModelView switch to catalog/cache, 1220824126875159 Model Editor build leftovers, plus 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, `ColorButton` alpha.

## Decisions
- User, 2026-11-23: dialog testing postponed to the end; no per-step approvals. Earlier decisions: see the git history of this file.
- Orchestrator calls the user may overrule (2026-11-23):
  - Accepted QA `NEEDS_HOOK` for Save MDX as test-covered; hook ticket 1220827826702332.
  - HD resave game check deferred (ticket 1220829428496325).
  - The orphan `HiveWE_tests` PID was QA's and was stopped.

## Scratch (may vanish)
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\cea7641d-7ebb-411d-a0f1-9f73d7de3c01\scratchpad`: `scripts\` and `ctest_r2.log`.
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c6dc9975-ff77-4de1-9d04-20cb016dce09\scratchpad\mdx\`: extracted models.
