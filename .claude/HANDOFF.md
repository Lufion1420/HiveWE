# Session handoff

Written 2026-11-24 (session 8 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: designer, wc3-format-specialist (Opus), cpp-dev x2 (batch 1 Sonnet, batch 2 Opus; resumes), reviewer x2 (Opus), qa-verifier x2, release-manager x2, docs-keeper (harness) and the docs-keeper that wrote this file. 11 spawns (budget reached).

## Checks waiting on the user
HiveWE in the Tools folder was deployed 2026-11-24 00:35 from commit 98372d6; the user's HiveWE is closed. The user's maps are SD only: do not list HD checks. No per-step approvals needed.
1. Batch 1 (7f1e541):
   - Select a unit, left-drag its Selection Properties preview: it starts from the front and orbits.
   - Browse Models: the Models and VFX/SFX sidebar rows work, and file sizes show in MB.
   - Object Editor: double-click a unit's "model file". Browse Models opens with a Path line (current model preseeded, typed path OK, empty clears, Cancel changes nothing).
   - After OK, confirm the Model File cell updates at once (offscreen it did not repaint; the code path was analysed as unchanged).
2. Batch 2 (98372d6):
   - Browse Models tiles in view play Stand (up to 12, 15 fps). Hovering a tile beyond 12 moves the animation to it. Scrolling freezes the animation, then it resumes. Switching to another window pauses it.
   - Watch CPU/GPU cost on a real screen, and whether opening Browse Models many times makes the app sluggish (reopen growth is about 6 MB per open; trigger to switch to hover-only via `animate_browse_tiles`).
3. Decisions needed from the user: none blocking. The user confirmed Change Tileset Variant B. Orchestrator defaults: OK enabled with the default replacement; single-file import only for v1; custom tiles saved as an imported texture override (unverified in 3.0.1, needs a game test on sandbox copies with the user warned).

## State
- Pushed to origin/main at the end of session 8 (verify with `git status -sb`). Commits: 98372d6 (animated tiles), 163ee32 (harness speed-ups), and this handoff commit. CI: take the result from the newest run on main and name that run (`cancel-in-progress` cancels older ones).
- Verified: ctest 347/347; reviewer APPROVE; QA PASS offscreen (12/12 tiles animate, 30 s soak flat, minimize 0% CPU, picker flow).
- Not verified: on-screen performance, HD, picker Open commit path via grid pick, cell repaint on screen.
- Asana done: 1220830442947390, 1220830558416533. New tickets: 1220832845348582 (reopen growth trace), 1220832845356081 (still_only/drop_tiles_except minors), 1220832862236571 (ui-capture README: HIVEWE_QA_REOPEN_CYCLES, HIVEWE_QA_HOLD_SECONDS).
- Harness changes approved by the user 2026-11-24 (in HARNESS.md, agents/cpp-dev, qa-verifier, reviewer): trivial fix rounds skip new QA; Sonnet re-review of small fix diffs (override model per run); 20-minute / two-hypothesis investigation cap; QA build reuse with an explicit "reuse"/"rebuild" mode in every QA brief (staleness recipe untested, exe path `build/Agent/Release/HiveWE_tests.exe` unverified).

## Next step
Change Tileset Variant B (Asana 1220824029601558):
1. First cpp-dev step: fix the HD double-`_diffuse` bug in `Terrain::change_tileset` (see the wc3-format note).
2. Gate 2: Qt prototype of Variant B, screenshotted offscreen with ui-capture, for the user to approve.
3. Then the full dialog (preview + mapping, single-file import as a map texture override).

Mockups: `designs/proposals/change-tileset/` (B = `renders/b-lists-dark.png` and `renders/b-lists-light.png`; import pages `import-file-*.png`).
Format note: the override goes by path in the map folder (SD path `<dir>/<file>.<ext>`, HD `_hd.w3mod:<dir>/<file>_diffuse.<ext>`). `ResourceManager` cannot evict one key; reload via `custom_identifier`. Unknown: whether 3.0.1 honours the override, and tile count above 16 (NEEDS_GAME_TEST on sandbox copies).
Also consider the three new tickets as at most one leftover batch.

## Backlog only, not next steps
1220826761382041 (ModelView switch; the old ModelView is now unused dead code), 1220824126875159, 1220826774009614, 1220826773700213, 1220828873067911, 1220818407294566, 1220824083919839, 1220768408128534, 1220767914607357, 1220768028401968, 1220767861345597, 1220767791486098, 1220767565409503, 1220767516485546, 1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131, weather literal SLK quotes, ColorButton alpha, plus 1220829561061744 (LITE v1300+/DILG, Risky), 1220829428496325 (3.0.1 load of resaved v1800, only if the user okays and the Battle.net login is confirmed), 1220827826702332 (Save MDX hook).

## Decisions
User:
- 2026-11-24: do not wait for an explicit go, only continue or hand off.
- 2026-11-24: prefers Change Tileset Variant B.
- 2026-11-24: maps are SD only.

Orchestrator calls the user may overrule (2026-11-24):
- Accepted about 6 MB growth per Browse Models reopen and shipped animation on.
- "Objects" (133) and "Shared Models" (20) count as Models, not VFX/SFX.
- Old ModelView left as dead code.

## Retro
Timings (subagent duration_ms):
- designer 295 s; format-specialist 115 s (parallel).
- Batch 1 cpp-dev 245 s + fix rounds 264 s and 107 s; reviewer 109 s then 45 s; QA 267 s then 219 s.
- Batch 2 cpp-dev 966 s + 713 s fix + 1,728 s leak bisect; reviewer 163 s, 57 s; QA 201 s, 331 s.
- 2 review rounds per batch, QA rebuilt each round.

Why slow:
1. About 45 min of cpp-dev (~220-277k tokens) on a memory-growth hunt that ended as an accepted residual of ~6 MB per reopen: open-ended, no cap.
2. Batch 1 needed a second round because the reviewer caught that replacing the old model picker dropped free-text/clear behaviour: preventable by a brief checklist.
3. The orchestrator idle-waited on QA holding build/Agent before starting fix rounds.
4. QA re-ran a full rebuild + ctest for test-only/changelog changes.

Optimizations applied this session (user approved 2026-11-24): trivial-round QA skip, Sonnet re-review, 20 min investigation cap, QA build reuse. New: when a task replaces a UI component, the cpp-dev brief lists the old component's behaviours that must be preserved (rule in `.claude/agents/cpp-dev.md`, check in `reviewer.md`).

Quality guard: none of this removes review on non-trivial source changes, weakens tests or skips QA on behaviour changes; the lost-free-text catch came from the reviewer, which stays.

Kept: parallel reviewer+QA, designer/specialist running in parallel with implementation prep, offscreen QA hooks (found the dialog-constructor crash and the leak), tickets for residuals instead of chasing.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\caf94239-0e7f-4986-b53a-337d75dcd7c8\scratchpad` holds the QA driver scripts drive.ps1, oeset.ps1, oedbl.ps1 and uia.ps1. They may vanish.
