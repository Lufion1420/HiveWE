# Session handoff

Written 2026-11-24 (session 17 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev (399/399), reviewer (APPROVE), qa-verifier (PASS), release-manager, docs-keeper.
- Spawn count this session: 5 (resume not counted).

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
- MDX TrackHeader `read_count` guard (Asana 1220826773700213) DONE: cpp-dev, tests 399/399, reviewer APPROVE, QA PASS. Being committed by the orchestrator. Facts are in `hivewe-file-formats.md` "MDX reader".
- Tools-folder deploy of 7b833b5 done 2026-11-24, before the MDX guard task: the Tools copy lacks the guard. Low value, no redeploy unless the user asks.
- CI of 7b833b5 was cancelled by the 94c9b9d push (`cancel-in-progress`): take its result from run 45392155542.
- Forces Share Vision bit fix (Asana 1220329472016779) closed, commit 7b833b5. Keep 0x04 as a permanent legacy alias for vision (orchestrator call).
- Asana 1220329469370593 (listfile) still open pending the real-game check, which waits for WarMash (see Decisions).
- Stock-model sweep of the new MDX guard NOT run: no extracted models and `HIVEWE_MDX_DIR` unset.
- Filed, not chased: `mdx_reader.cpp:750` `read_vector<float>(reader.read<uint32_t>() * 12)` multiplies in 32 bits and can wrap past the bounds check. Test "MapInfo::save keeps force flags 0x3B with shared vision at 0x08" duplicates the first half of the 0x08 round-trip test; scenario JSON import (`scenario_data_io.ixx`) drops `unknown_flags`; checkbox-to-field wiring in `scenario_properties_editor.cpp` is not exercised (needs NEEDS_HOOK); ctest flakes #115 and `resave_smoke` failed once under load, passed on rerun.
- Earlier filed items: on pack failure the half-written `output_path` stays and the previous protected file is already deleted (pack to a temp path, then rename); the Map Protector test should also pin the OFF-path header (version 0, sector 0x1000).
- Untracked `qa-*.png` captures in `designs/current/` stay uncommitted on purpose.

## Next step
Pick another "Next up" ticket. Skip 1220841054745630 (GroundTexture hd): HD only while maps are SD.

## Backlog only, not next steps
1220826761382041 (ModelView switch; old ModelView is unused dead code), 1220824126875159, 1220826774009614, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800), 1220827826702332 (Save MDX hook), 1220842919702486, 1220844868518223, 1220844868394880, 1220844756126723, 1220845243296342, plus the filed items above.

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done. The listfile-less protected map load check and the real-game vision check (3.0.1) wait for that.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; the orchestrator checks `Get-Process HiveWE` and deploys after each shipped fix when it is not running.
User (2026-11-24, earlier): game test yes, using the SendInput-free launch per the WarMash method; "no, open it yourself" = agents do the game testing. The launch itself is now paused by the decision above.

Orchestrator calls the user may overrule (2026-11-24):
- Fixed `Imports::save` in-task because it corrupts saves (reviewer finding).
- Filed the other `.string()` sites instead of chasing them.
- Closed 1220824029601558 with slots 6-19 unverified.
- Picked a non-parity data-loss bug (w3i literals) instead of the HD-only ticket.
- Filed the pack-failure cleanup instead of fixing it in the listfile task.
- Filed the `scenario_data_io.ixx` unknown_flags loss and the redundant test instead of chasing them in the vision task.
- Filed the 32-bit `* 12` wrap at `mdx_reader.cpp:750` instead of fixing it in the TrackHeader task.
- No redeploy to the Tools folder for the MDX guard.

## Retro
Not measured this session beyond the subagent runs (no durations recorded); no fix rounds reported.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\b6bf64ac-5e7e-42f5-bade-9bf3fc7457e2\scratchpad` (may vanish).
