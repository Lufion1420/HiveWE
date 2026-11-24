# Session handoff

Written 2026-11-24 (session 13 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: qa-verifier (game run, resumed x3 incl. re-QA), cpp-dev (resumed x2), Explore (WarMash lookup), reviewer Opus (resumed x2), release-manager, docs-keeper = 6 spawns plus resumes.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional: the session-8 feedback checks. They now apply to a build deployed from 8fc8747. The deployed Tools build (`D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`) predates cfa723a, 78c44b3 and adae324 and needs a redeploy with HiveWE closed: ask the user to close HiveWE so the orchestrator runs `.claude/tools/deploy-local/`.
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.
2. Decision: confirm the WarMash direct-exe game launch (`-launch -loadfile` with the saved Battle.net login, method in `.claude/tools/ui-capture/README.md`) as the standing method, replacing the guardrail "via Battle.net, never the exe directly".

## State
- adae324 pushed: offscreen `--qa-change-tileset` hook, tests 381/381, non-ANSI path crash fixes across Open/Save/Save As/export/recent maps/Test Map/`Imports::save`, upper-case texture extension fix, changelog. QA PASS (reuse build); reviewer REJECT round 1, then APPROVE after 2 fix rounds.
- Game results: real Warcraft III 3.0.1.24371 loaded HiveWE's own Change Tileset output (20 tiles, 2 custom TGA tiles) and drew the custom texture; the control run showed stock dirt. World Editor 3.0.1 also showed hand-made maps correctly. NOT verified: slots 6-19 rendered, BLP override, HD.
- Asana 1220841004647015 and 1220824029601558 closed and moved to Done. Follow-ups filed: 1220844868518223 (remaining non-ANSI `.string()` sites), 1220844868394880 (hook for Open Map / `update_map_context` non-ANSI), 1220844756126723 (tile slots above 16 rendered in game).
- CI: the newest main run 45366950127 on adae324 was in progress at handoff time (previous bf484fc run 45362706666 success). Next session re-checks with `GH_TOKEN=$(gh auth token -u Lufion1420) gh run list -R Lufion1420/HiveWE` and names the run.
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
Asana 1220841054745630 (GroundTexture cache key ignores `hierarchy.hd`; HD only, low priority), or check "Next up" for a non-parity feature ticket and confirm it is still open.

## Backlog only, not next steps
1220826761382041 (ModelView switch; old ModelView is unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800), 1220827826702332 (Save MDX hook), 1220842919702486, plus the 3 new tickets above.

## Decisions
User (2026-11-24): game test yes; allowed the SendInput-free game launch per the WarMash method (pointed to the WarMash repo); "no, open it yourself" = agents do the game testing.

Orchestrator calls the user may overrule (2026-11-24):
- Fixed `Imports::save` in-task because it corrupts saves (reviewer finding).
- Filed the other `.string()` sites instead of chasing them.
- Closed 1220824029601558 with slots 6-19 unverified.

## Retro
First game attempt 206 s COULD-NOT-RUN because the brief did not name the ui-capture README (it had the documented route), then a 100 s retry. cpp-dev 422 s + 373 s + 227 s (2 fix rounds after 1 REJECT: hook wrote into `data/test map`; non-ANSI chain incomplete; `Imports::save`), reviewer 92/46/30 s, QA 72/30/27 s on reuse builds, release-manager 11 s, game run 428 s with 0 stalls. Accepted into HARNESS.md: game-load briefs name the README and WarMash method; path/encoding fix briefs say "grep the whole chain". Quality guard: no review or QA removed. Kept: reuse-build QA, parallel review+QA, Save-As-first hook design.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\37332c8e-2382-43ff-99b2-3c40a855a256\scratchpad` (qa\hivewe_changetileset_out, qa\cts.w3x, wc3ref\run_r1, run_r2, cmp_custom_vs_control.png; may vanish).
