# Session handoff

Written 2026-11-24 (session 12 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev (resumed once), reviewer Opus (resumed once on a Sonnet-equivalent re-review), qa-verifier (resumed once), release-manager, docs-keeper x2 = 7 spawns plus 2 resumes.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional, postponed by the user: the session-8 feedback checks. They now apply to the build deployed to `D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE` (from 8fc8747, exe 5,367,808 bytes, 09:33:55; it predates cfa723a, the sound-environment fix). Redeploying needs HiveWE closed.
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.
2. Decision (unanswered: the reply had the placeholder "<yes/no>" unfilled, treated as NO): may the orchestrator run the Warcraft III 3.0.1 game test of Change Tileset (Asana 1220824029601558, stays OPEN)? Conditions: Battle.net login works, you do not click or type in the window, sandbox copies only. It checks whether 3.0.1 honours the SD map-folder texture override and loads a tile count above 16.

## State
- Asana 1220840943600840 (change_tileset blight) closed as NOT A BUG (premise did not reproduce; callers verified; no pinning test feasible because the line is inside GL texture loading) and moved to Done. The fact is recorded in `.claude/rules/hivewe-file-formats.md`.
- Asana 1220842369265849 shipped: `write_tile_overrides` plus `tile_setter.cpp` picker/label/extension handling plus `utilities.ixx` read_file errors (MSVC `.string()` crash on non-ANSI folders). ctest 376/376; reviewer REJECT then APPROVE (1 fix round); QA PASS including `--resave` from a `地图_КАРТА` folder; changelog added. Commit hash: 78c44b3.
- New follow-ups filed under "Harness and quality follow-ups": `decode_tile_image` case-sensitive `.blp` compare; `ec.message()` ANSI text shown as UTF-8; `WorldUndoManager::has_pending_changes` has 0 callers (remove or keep on purpose).
- CI: the newest main run at session start was 45359448667 on 0c6c0d3 (in progress then); earlier cfa723a/0a863ce runs were cancelled; last completed green 45352893298 on 9533591. This session's handoff push starts a newer run that cancels it: the next session must check the newest run with `GH_TOKEN=$(gh auth token -u Lufion1420) gh run list -R Lufion1420/HiveWE` and name it.
- Deployed Tools build is from 8fc8747 (`--resave` smoke passed).
- Asana 1220824029601558 (Change Tileset) stays OPEN until the game test.
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
Asana 1220841054745630 (GroundTexture cache key ignores `hierarchy.hd`; HD only, low priority because the user's maps are SD) is the remaining small "Next up" item. Check "Next up" first for another non-parity ticket (or a feature) and confirm it is still open. 1220824083919839 is backlog. If the user answers yes to check 2, the Change Tileset game test (1220824029601558) instead.

## Backlog only, not next steps
1220826761382041 (ModelView switch; old ModelView is unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800, only if the user okays and Battle.net login is confirmed), 1220827826702332 (Save MDX hook), 1220842919702486.

## Decisions
User:
- 2026-11-24: prefers Change Tileset Variant B; approved gate 2 (Change Tileset prototype).
- 2026-11-24: no per-step approvals, only continue or hand off; offscreen only; warn before any game or window; Lua-only and SD-only maps; workspace-relative image links; retrospective before each handoff.
- 2026-11-24: asked for the redeploy of the Agent build to the Tools folder (done).

Orchestrator calls the user may overrule (2026-11-24):
- Treated the unanswered game-test question as NO; it stays a waiting decision.
- Filed the reviewer minors of cfa723a instead of chasing them.
- Change Tileset: no Apply button (OK/Cancel only); SD import accepts only BLP and TGA; single-file import only; custom tiles saved as an imported texture override.
- Accepted "premise did not reproduce" for 1220840943600840 without a test, because the blight line is inside GL texture loading (HARNESS says tests are normally required).

## Retro
cpp-dev 434 s + fix round 212 s, reviewer 83 s then 14 s, QA 73 s + resume, 1 reject round (reviewer found the same MSVC `.string()` crash in 3 callers outside the named function; lesson: grep the whole call chain in the brief). Proposal: cpp-dev briefs for exception-class fixes must say "grep all callers/the dialog chain". Quality guard: no review or QA removed. Kept: tracing the caller list before editing.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\3e68fc89-e1ca-4ecf-8641-70a4d12a4fce\scratchpad` (may vanish).
