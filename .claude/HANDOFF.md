# Session handoff

Written 2026-11-24 (session 14 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev (resumed x1), reviewer Opus (resumed x1), qa-verifier (resumed x1), release-manager, docs-keeper = 5 spawns plus resumes.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional: the session-8 feedback checks. They now apply to a build deployed from 8fc8747. The deployed Tools build (`D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`) predates cfa723a, 78c44b3, adae324 and f5aacf5 and needs a redeploy with HiveWE closed: ask the user to close HiveWE so the orchestrator runs `.claude/tools/deploy-local/`.
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - New (after redeploy): open a map whose w3i has a literal (non-TRIGSTR) name. Map Description shows it. Edit it and Save: the new text persists after reopening. Automation covered this only through unit tests.
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.
2. Decision: confirm the WarMash direct-exe game launch (`-launch -loadfile` with the saved Battle.net login, method in `.claude/tools/ui-capture/README.md`) as the standing method, replacing the guardrail "via Battle.net, never the exe directly". The orchestrator treats it as standing unless the user says no.

## State
- f5aacf5 pushed: literal w3i name/author/description now display and save; changed literals become `TRIGSTR_` keys; `generate_map_configuration` escapes literal name/description; fixed `log10(0)` key padding; New Map stores name and suggested players as TRIGSTR keys and clears description/author. Tests 388/388. Reviewer REJECT round 1 (edited literals reached the Lua script unescaped), APPROVE round 2; QA PASS (rebuild).
- Asana 1220329766735363 closed and moved to Done; follow-up 1220845243296342 filed.
- CI: the main run for ac0f933 (45368324605) was in progress at session start. The run for f5aacf5 must be re-checked next session with `GH_TOKEN=$(gh auth token -u Lufion1420) gh run list -R Lufion1420/HiveWE` and named (a newer push cancels the older run).
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
Asana 1220329469370593 (Map Protector: Remove listfile option probably keeps the listfile; Risky track, `wc3-format-specialist` first; first step is to protect a copy and inspect for `(listfile)`). Alternative: 1220329472016779 (forces shared vision bit, Risky). 1220841054745630 (GroundTexture hd) is HD only: skip while maps are SD.

## Backlog only, not next steps
1220826761382041 (ModelView switch; old ModelView is unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800), 1220827826702332 (Save MDX hook), 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342.

## Decisions
User (2026-11-24): game test yes; allowed the SendInput-free game launch per the WarMash method (pointed to the WarMash repo); "no, open it yourself" = agents do the game testing.

Orchestrator calls the user may overrule (2026-11-24):
- Fixed `Imports::save` in-task because it corrupts saves (reviewer finding).
- Filed the other `.string()` sites instead of chasing them.
- Closed 1220824029601558 with slots 6-19 unverified.
- Picked a non-parity data-loss bug (w3i literals) instead of the HD-only ticket named in the handoff.
- Fixed the escaping problem in-task because it would break Lua scripts; filed the rest.

## Retro
cpp-dev 218 s + 328 s (1 fix round after 1 REJECT), reviewer 66 s + 30 s, QA 72 s (reuse) + 131 s (rebuild), release-manager 9 s. Cause of the extra round: the first brief did not say to grep every writer that emits the field into script text; cpp-dev also wrote the fix before a RED test. Accepted into HARNESS.md: string/field storage briefs say "grep every place the field is emitted into script/save text"; cpp-dev briefs ask for a RED run of the new test on old code when cheap. Quality guard: no review or QA removed. Kept: parallel review+QA, resuming the same agents.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\29db405c-c49f-428d-beb5-7864e1d7c765\scratchpad` (may vanish).
