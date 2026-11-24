# Session handoff

Written 2026-11-24 (session 10 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x1 (3 runs via resume, Sonnet), reviewer x1 (3 runs, Opus), qa-verifier x1 (3 runs), release-manager x1, docs-keeper x2 plus the one that wrote this file. 5 spawns plus resumes.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional, postponed by the user ("will wait"): feedback from session 8 (HiveWE in Tools was deployed from 98372d6; the user must close HiveWE before a redeploy):
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.
2. Decision: may the orchestrator redeploy the Agent build (8fc8747) to `D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`? It needs HiveWE closed.
3. Decision: may the orchestrator run the Warcraft III 3.0.1 game test of Change Tileset? Battle.net login must work, you must not click or type in the window, sandbox copies only. It checks whether 3.0.1 honours the SD map-folder texture override and loads a tile count above 16.

## State
- Pushed: 8fc8747 (Change Tileset full dialog + prototype; reviewer APPROVE after 2 reject rounds, QA PASS, ctest 372/372, changelog added, docs updated). CI: run 45352893298 on 9533591 was green (covers 487f502, whose own run was cancelled). CI for 8fc8747 is not checked yet: take the newest run on main with `GH_TOKEN=$(gh auth token -u Lufion1420) gh run list -R Lufion1420/HiveWE` and name it.
- Asana 1220824029601558 (Change Tileset) stays OPEN until the game test.
- Tickets 1220840943600840, 1220841054745630, 1220841004647015 had empty memberships (not on the HiveWE board); added to project 1220328345553589 without a section. 1220841004647015 (change_tileset test) is partly done: logic tests exist in `tests/tileset_change_test.cpp`, but the GL tail of `Terrain::change_tileset` is still untested. Comment or decide whether to close.
- New ticket 1220842369265849: `generic_string()` can throw on non-code-page folder names in `write_tile_overrides`; `has_pending_changes` is now unused.
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
Asana 1220824029601558 game test (NEEDS_GAME_TEST), per the user's answer to check 3. If the user declines, pick the next non-parity "Next up" ticket from the Asana board (the orchestrator reads "Next up" at session start).

## Backlog only, not next steps
1220826761382041 (ModelView switch; the old ModelView is now unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, plus 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800, only if the user okays and the Battle.net login is confirmed), 1220827826702332 (Save MDX hook), 1220842369265849.

## Decisions
User:
- 2026-11-24: prefers Change Tileset Variant B.
- 2026-11-24: no per-step approvals, only continue or hand off; offscreen only; warn before any game or window; Lua-only and SD-only maps; workspace-relative image links; retrospective before each handoff.
- 2026-11-24: maps are SD only.
- 2026-11-24: approved gate 2 (Change Tileset prototype).

Orchestrator calls the user may overrule (2026-11-24):
- Filed the CJK/`generic_string` reviewer finding instead of chasing it (not reachable in the user's maps).
- Did not redeploy to the Tools folder because the user may have HiveWE open.
- No Apply button in Change Tileset (OK/Cancel only); SD import accepts only BLP and TGA.
- Defaults kept from session 8: OK enabled with the default replacement; single-file import only for v1; custom tiles saved as an imported texture override.

## Retro
Timings (duration_ms): cpp-dev 459 s, fix round 1 393 s, fix round 2 261 s; reviewer 125 s, 86 s, 47 s; QA 123 s, 173 s, 131 s (reuse mode all three, no rebuild); release-manager 16 s; docs-keeper 27 s. 2 reviewer REJECT rounds (round 1: undo not cleared, DDS no effect in SD, path traversal, missing OK-path tests; round 2: clearing undo hid unsaved work from the unsaved-changes prompt, a bug introduced by the first fix).
Why slow: the reviewer found the undo bug only after the fix was added; QA of the rejected round 1 (~123 s) was wasted because the review landed first.
Proposals: no rule change. Consider telling cpp-dev in briefs to check every consumer of any state it clears (the round-2 blocker class). Quality guard: all QA runs were reuse mode with staleness verified; no review skipped.
Kept: parallel review+QA with reuse mode; hook-based QA of dialog guards.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\ea154186-d152-4a41-9584-543418558cc2\scratchpad` (may vanish).
