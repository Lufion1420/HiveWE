# Session handoff

Written 2026-11-24 (session 11 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: general-purpose (deploy), cpp-dev, reviewer (Opus), qa-verifier, release-manager, docs-keeper = 6 spawns.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional, postponed by the user: the session-8 feedback checks. They now apply to the build deployed to `D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE` (from 8fc8747, exe 5,367,808 bytes, 09:33:55; it predates cfa723a, the sound-environment fix). Redeploying needs HiveWE closed.
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.
2. Decision (unanswered, treated as NO): may the orchestrator run the Warcraft III 3.0.1 game test of Change Tileset (Asana 1220824029601558, stays OPEN)? Conditions: Battle.net login works, you do not click or type in the window, sandbox copies only. It checks whether 3.0.1 honours the SD map-folder texture override and loads a tile count above 16.

## State
- Pushed: cfa723a (Map Options OK cleared the custom sound environment; Asana 1220767565409503 completed and moved to Done). ctest 374/374, reviewer APPROVE, QA PASS smoke (dialog wiring read in code, not driven offscreen), changelog added. Minors filed as 1220842919702486 (non-UTF-8 stored id rewritten, 2 untested helper branches, case-sensitive findData, duplicate SLK keys) under "Harness and quality follow-ups".
- CI: run 45357831309 (on 0a863ce) was in progress and 45359318702 (on cfa723a) pending when last checked. Take the newest main run with `GH_TOKEN=$(gh auth token -u Lufion1420) gh run list -R Lufion1420/HiveWE` and name it. Last completed green: 45352893298 on 9533591; 8fc8747's own run was cancelled.
- Deployed Tools build is from 8fc8747 (`--resave` smoke passed).
- Asana 1220824029601558 (Change Tileset) stays OPEN until the game test. 1220842369265849 is in "Next up".
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
A non-parity "Next up" ticket. Small candidates: 1220840943600840 (change_tileset blight load) or 1220841054745630. 1220824083919839 is backlog. If the user answers yes to check 2, the Change Tileset game test (1220824029601558) instead. Read "Next up" and confirm the ticket is still open first.

## Backlog only, not next steps
1220826761382041 (ModelView switch; old ModelView is unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800, only if the user okays and Battle.net login is confirmed), 1220827826702332 (Save MDX hook), 1220842369265849, 1220842919702486.

## Decisions
User:
- 2026-11-24: prefers Change Tileset Variant B; approved gate 2 (Change Tileset prototype).
- 2026-11-24: no per-step approvals, only continue or hand off; offscreen only; warn before any game or window; Lua-only and SD-only maps; workspace-relative image links; retrospective before each handoff.
- 2026-11-24: asked for the redeploy of the Agent build to the Tools folder (done).

Orchestrator calls the user may overrule (2026-11-24):
- Treated the unanswered game-test question as NO; it stays a waiting decision.
- Filed the reviewer minors of cfa723a instead of chasing them.
- Change Tileset: no Apply button (OK/Cancel only); SD import accepts only BLP and TGA; single-file import only; custom tiles saved as an imported texture override.

## Retro
cpp-dev 411 s, reviewer 42 s, QA 15 s (reuse mode), release-manager 8 s, deploy 14 s; 0 reject rounds; nothing slow. Proposals: none. Kept: reuse-mode QA, filing reviewer minors instead of chasing.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\4630404b-c13d-459c-962c-1891ffe44e60\scratchpad` (may vanish).
