# Session handoff

Written 2026-11-23 (session 6 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: wc3-format-specialist (Opus), cpp-dev (Opus, 1 resume x2), reviewer x2, qa-verifier x2, release-manager + resume, docs-keeper x2. 9 spawns (budget reached).

## Checks waiting on the user
The user postponed testing to the end (2026-11-23). The user also asked to be asked for feedback on the dialogs, Model Editor and Browse Models: still unanswered, ask first.
1. Open the Tools-folder HiveWE (deployed 21:47 from 3267526). File > Settings > Theme > Flat Dark, then Flat Light. Review the earlier restyled dialogs and report anything ugly or broken:
   - Settings, Unit Properties (select a unit, Edit), Map Settings, Scenario, Gameplay Constants (names readable in Light, no clipped first row).
   - Object Editor: Flat Light must be light with tree arrows. Click Custom in the Units browser, then Edit from a placed stock unit: it must not crash.
   - New Map, Asset Manager, Map Protector, Item Tables, Config, Change Tileset, Tile Pathing.
   - Selection Properties panel: select a unit and a doodad, move or rotate it; the panel updates and survives a live theme switch.
2. Model Editor: open any model via the Model Browser's "Open in Model Editor", or the Model Editor tab.
   - Check the toolbar, sequence combo, timeline (play/pause/scrub/loop/speed), the Sequences/Geosets/Info docks with Show checkboxes and Wireframe, and the status bar.
   - Drag or undock a dock tab and check the viewport survives.
   - Browse Models (toolbar): categories with counts; search "footman"; double-click opens it as a tab.
   - New: SD stock models with a CAMS chunk (gnoll, gnollarcher, centaurkhan, harpywitch, goblinlandmine, bristleback) should now open and show thumbnails. HD models still show "!" (ticket 1220828998277683). Report anything ugly.
3. Optional real-game 3.0.1 load check: only if the user says go after confirming the Battle.net login.

## State
- Pushed to `origin/main`: 3267526 (MDX CAMS fix). Deployed to the Tools folder at 21:47 (deploy.ps1 smoke OK). Asana 1220828986142563 done with comment.
- Verified: ctest 328/328; env-gated stock sweep (`HIVEWE_MDX_DIR`) over 20 real SD+HD models: cameras parse and round-trip; SD whole models load. Not verified: GUI view of Browse Models / Model Editor, real game.
- CI for 3267526 not yet checked: `compile.yml` cancels older runs, so take the result from the newest run.
- New tickets: 1220828998277683 HD v1800 whole-model refuse (suspected SKIN count in `read_GEOS`), 1220826774009614 preserve unknown camera tracks on save, 1220826773700213 TrackHeader `reserve()` hardening.
- `.claude/plans/wc3-3.0-compatibility.md` may be stale (Needs verification).
- Uncommitted: many untracked `designs/current/*.png` QA screenshots (leave).

## Next step
Asana 1220828998277683 (HD v1800 GEOS, Risky, `wc3-format-specialist` first): it affects the visible Browse Models tiles. Alternative: 1220824029601558 Change Tileset preview (designer, gate 1). At most one parity item from 1220328838360686.

## Backlog only, not next steps
1220826761382041 Object Editor ModelView switch to catalog/cache, 1220824126875159 Model Editor build leftovers, plus 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, `ColorButton` alpha.

## Decisions
- User, 2026-11-23: dialog testing postponed to the end; no per-step approvals. Earlier decisions: see the git history of this file.
- Orchestrator calls the user may overrule (2026-11-23):
  - HD whole-model load ticketed, not fixed in the CAMS task; HD variants 1/2 refused.
  - Camera-track preservation on save deferred.
  - `cpp-dev` hit a permission denial mid-build once; QA did the build instead.

## Scratch (may vanish)
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c6dc9975-ff77-4de1-9d04-20cb016dce09\scratchpad\mdx\`: 20 extracted models.
- `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\10373ca8-7927-404b-8901-dbe6b2f7a0e9\scratchpad`: this session, `scripts\walk.py`, `flags.py`.
