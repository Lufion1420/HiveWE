# Session handoff

Written 2026-11-24 (session 19 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: docs-keeper, wc3-format-specialist, cpp-dev, reviewer (APPROVE), qa-verifier (PASS), release-manager.
- Spawn count this session: 9 (resume not counted): docs-keeper README, wc3-format-specialist, cpp-dev camera, reviewer, qa-verifier, cpp-dev test, release-manager, docs-keeper x2.

## Checks waiting on the user
The user's maps are SD only and Lua only: do not list HD checks. No per-step approvals needed.
1. Optional: the session-8 feedback checks. The deployed Tools build (`D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`) was current with f5aacf5 (deployed 2026-11-24 10:56, resave smoke passed), so they apply directly.
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: Models and VFX/SFX sidebar rows work, file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing). After OK the Model File cell updates at once.
   - Tiles in view play Stand (up to 12). Hovering a tile beyond 12 moves the animation, scrolling freezes it, another window in front pauses it.
   - Watch CPU/GPU cost and whether opening Browse Models many times makes the app sluggish (about 6 MB growth per open).
   - Open a map whose w3i has a literal (non-TRIGSTR) name. Map Description shows it. Edit it and Save: the new text persists after reopening. Automation covered this only through unit tests.
   - Blocked by rule, not for the user: grid-pick Open commit path, on-screen cell repaint.
2. Decision (WarMash direct-exe game launch as the standing method): now moot until WarMash is done, see Decisions. Keep it open; ask again when the user says the WarMash game tests are finished.

## State
- ui-capture README (99f0a98, Asana 1220832862236571) DONE.
- MDX camera tracks (78a35a0, Asana 1220826774009614) DONE: camera variant byte, IDUF/ELAF/PTSF depth-of-field tracks and unknown camera tracks kept on save. Reviewer APPROVE, QA PASS, ctest 405/405.
- CI: the newest runs after 78a35a0 were not checked (`cancel-in-progress`); take the f5b2461 result from the newest run.
- Tools folder lacks the MDX guard and the camera change: no redeploy unless the user asks.
- Asana 1220832845348582 (~6 MB growth per reopen) stays filed.
- Asana 1220329469370593 (listfile) still open pending the real-game check, which waits for WarMash (see Decisions).
- Filed this session, not chased: MDL reader/writer still drops camera DOF/raw tracks; a raw unknown track after KCTR moves before it on resave; a variant-0 camera over 16 MB writes a size the reader refuses; stale comment "three times" above `qa_reopen_step()` in `browse_models_dialog.cpp` (default is 5); 3.0.1 acceptance of a variant 0 camera in v1800 waits for WarMash.
- Earlier filed items: `mdx_reader.cpp:750` `read_vector<float>(reader.read<uint32_t>() * 12)` multiplies in 32 bits and can wrap past the bounds check; test "MapInfo::save keeps force flags 0x3B with shared vision at 0x08" duplicates the first half of the 0x08 round-trip test; scenario JSON import (`scenario_data_io.ixx`) drops `unknown_flags`; checkbox-to-field wiring in `scenario_properties_editor.cpp` is not exercised (needs NEEDS_HOOK); ctest flakes #115 and `resave_smoke` failed once under load, passed on rerun; on pack failure the half-written `output_path` stays and the previous protected file is already deleted (pack to a temp path, then rename); the Map Protector test should also pin the OFF-path header (version 0, sector 0x1000); no test for the two animated-tile failure paths (Renderer has no GL test seam).
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
Pick any eligible "Next up" ticket (Asana project 1220328345553589) and start at once. Skip 1220841054745630 (GroundTexture hd): HD only while maps are SD. 1220832845348582 stays filed.

## Backlog only, not next steps
1220826761382041 (ModelView switch; old ModelView is unused dead code), 1220824126875159, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800), 1220827826702332 (Save MDX hook), 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done. The listfile-less protected map load check and the real-game vision check (3.0.1) wait for that.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; the orchestrator checks `Get-Process HiveWE` and deploys after each shipped fix when it is not running.
User (2026-11-24, earlier): game test yes, using the SendInput-free launch per the WarMash method; "no, open it yourself" = agents do the game testing. The launch itself is now paused by the decision above.
User (2026-11-24): keep working across tickets without waiting for "continue"; see `harness-guardrails.md` "Keep working".

Orchestrator calls the user may overrule (2026-11-24):
- Fixed `Imports::save` in-task because it corrupts saves (reviewer finding).
- Filed the other `.string()` sites instead of chasing them.
- Closed 1220824029601558 with slots 6-19 unverified.
- Picked a non-parity data-loss bug (w3i literals) instead of the HD-only ticket.
- Filed the pack-failure cleanup instead of fixing it in the listfile task.
- Filed the `scenario_data_io.ixx` unknown_flags loss and the redundant test instead of chasing them in the vision task.
- Filed the 32-bit `* 12` wrap at `mdx_reader.cpp:750` instead of fixing it in the TrackHeader task.
- No redeploy to the Tools folder for the MDX guard or the camera change.
- Filed the camera follow-ups listed under State instead of chasing them.

## Retro
2 tickets; wc3-format-specialist note 433 s, cpp-dev camera 355 s; no fix rounds except one test-tightening. Main-session stalls came from status-only turns; fixed by the "Keep working" rule.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\4d79355e-df39-497e-9bd9-6f2d1abd5e8c\scratchpad` (may vanish; holds mdx_dof 46 stock models and camscan scripts).
