# Session handoff

Written 2026-11-22 (second session of the day). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Opus 5.5. Subagents: cpp-dev (Sonnet), reviewer (Opus), qa-verifier (Sonnet), wc3-format-specialist (Opus), release-manager (Sonnet), docs-keeper (Sonnet). 6 spawns; resumes not counted.
- Ended early: the user restarts VS Code to fix the Pencil connection.

## Checks waiting on the user
1. Pencil: after the user restarted VS Code, call `mcp__pencil__get_app_state`. `/mcp` showed pencil "connected", but `get_app_state` reported "A document needs to be open in the editor" even with `designs/current/main-window.pen` opened via `code --reuse-window`.
   - If it still fails: the user clicks the `.pen` tab in the non-chat group, checks that the canvas renders and not a sign-in screen, then runs `/mcp` and reconnects `pencil`.
2. Optional: the real-game 3.0.1 load check of the water colour and start-location output (d86f067, 61ce7a8). Agents run it; the user only confirms that the Battle.net login works and says go.

## State
- `main` is clean and pushed at the handoff commit.
- afe0003: new trigger variables get type integer and unique names (Asana 1220328820523202, closed). Reviewer APPROVE, QA PASS 279/279 plus a UIA Trigger Editor check.
- The Tools-folder HiveWE runs the afe0003 build, deployed 2026-11-22 09:31.
- CI: 3fc5fff passed (run 45001160127). Check the newest run for afe0003 and the handoff commit.
- Save-failure ticket 1220330960297388: the specialist note is done and posted as a ticket comment with fix plan F1-F5, acceptance criteria and test seams. It is the cpp-dev brief; no need to rerun the specialist. Track: risky (cpp-dev on Opus).
- New follow-ups:
  - 1220767861345597: trigger-variable leftovers (rename duplicates, test gaps, verify unknown=1, the stray-quote ECA bug).
  - 1220767791486098: whole-map atomic save and the export_mpq checks.

## Next step
1. UI modernization 1220328401966062 (after Pencil works). Fresh designer, new file `designs/proposals/ui-modernization.pen` with four frames:
   - `Main Window / Flat Dark`
   - `Main Window / Warcraft`
   - `Tokens / Both directions`
   - `Toolbar / Command map`, from `main_ribbon.cpp`.

   Open the file with `code --reuse-window` only; the chat is a locked editor group. Then stop for user approval.
2. "Map save can report success after a failed or truncated file write" 1220330960297388: cpp-dev (Opus) from the ticket comment, then reviewer and qa-verifier in parallel. Run it while the designer waits for approval.
3. At most one parity item from "map output on save" (1220328838360686); 2a was this session's parity item.

## Backlog only, not next steps
1220767861345597, 1220767791486098, 1220767565409503 custom sound, 1220767516485546 dangling skin, the four earlier follow-ups (1220746915086260, 1220746806783937, 1220749450812492, 1220749450805131), the weather name with literal SLK quotes, `ColorButton` dropping alpha.

## Decisions
- User, 2026-11-22: work order is doodad skin (done), UI modernization, [Parity 2a] trigger variables (done), save-reports-success.
- User, 2026-11-22, UI modernization:
  - Mock two directions side by side: Flat dark IDE, and Warcraft gold and stone.
  - Dark and Light themes come from one token file.
  - Approved: qtsvg SVG icons, a bundled UI font, and a compact toolbar replacing the ribbon.
- User, 2026-11-22: the Claude chat is a locked VS Code editor group. Open `.pen` files with `code --reuse-window` and never close tabs.
- User, 2026-11-20: WarMash's harness is the base. Keep the Tools-folder HiveWE current.
- User, 2026-11-09:
  - Maps are Lua-only: Lua first, and JASS must not break.
  - The differential sweep is the default bug finder.
  - The user gets only short, specific checklists.
  - Sweeps run on sandbox copies; originals are never written.
- User, 2026-11-09 and earlier: unchanged from the previous handoff. See git history of this file (74233f7) for the full list:
  - units and w3i versions
  - our own 3.0 parsers
  - MPQ refusal checks
  - Playwright for hiveworkshop
  - NCOW feature use
  - in-process hooks instead of SendInput
  - WE/game launches only via Battle.net on sandbox copies
- Orchestrator calls the user may overrule:
  - 2026-11-22: TriggerVariable unknown=1 default (unverified against a WE wtg; ticketed).
  - 2026-11-22: the 2a parity item ran ahead of save-reports-success, following the user's work order.
  - Earlier: the doodad script passes the .doo skin id; the specialist stays on Opus; worktree builds stay banned; commit only after QA PASS; the orchestrator picks the next ticket itself.

## Scratch (may vanish)
- Sweep working dir: `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\w4\`. It holds no war3map.wtg. Rebuild it with `.claude/tools/map-diff/sweep/README.md`.
- NCOW originals (`D:\Stuff\Warcraft_3_All_Assets\Projects\Naruto Clash of Worlds Content\Map\NCOW-Arena\`) stay read-only; work on a scratchpad copy.
