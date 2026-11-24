# Harness guardrails (main session and every subagent)

Set by the user on 2026-11-08. The roster and pipeline are in `.claude/HARNESS.md`.

## Builds
- Agents may build and run tests, using only the `Agent` preset: `cmake --preset Agent`, `cmake --build --preset Agent`, `ctest --preset Agent`.
- Never build the `Release` preset locally. It has `HIVEWE_AUTO_RUN_AFTER_BUILD=ON`, which force-kills any running `HiveWE.exe` (losing unsaved map work) and launches a new one.
- One build at a time: agents share `build/Agent`. Worktree builds stay banned.
- Lock file: create `<scratchpad>\build.lock` before any build, `ctest` or app launch and delete it after; poll every 4 seconds, not 30. A `HOLD` file next to it means stop starting builds or app windows (the user has a window open or is playing). Every brief explains both. A source edit while QA builds gets compiled into QA's build: hold edits until QA returns.
- `qa-verifier` cannot write under `.claude/` (the writes are denied). It returns the sweep report text and `docs-keeper` writes it to `.claude/tools/map-diff/reports/`.
- Never kill `HiveWE`, `code`, or any process you did not start.

## Git and releases
- The orchestrator commits and pushes after each finished task, without asking, but only after QA PASS (and reviewer APPROVE on Standard and Risky). There is no commit-before-sweep exception. Subagents do not commit or push.
- Stage named paths only, never `git add -A`.
- A second Claude window on the same repo can push docs commits mid-task. Before committing, run `git fetch` and `git log HEAD..origin/main`.
- Push only to `origin` (`Lufion1420/HiveWE`). Never push to `upstream`, and never open PRs against `stijnherfst/HiveWE`: this fork is not merged back.
- `gh`'s active account may be the work account `Maurice-TPG`. Never use it and never run `gh auth switch`. Scope the identity per command:
  - Bash: `GH_TOKEN=$(gh auth token -u Lufion1420) git push origin main`
  - The same prefix applies to `gh` commands.
- `git push origin main` prints "Bypassed rule violations ... Changes must be made through a pull request". The push still succeeds (owner bypass); verify with `git status -sb`.
- No force-push and no history rewrite unless the user okays that specific rewrite in chat. Then only `git push --force-with-lease=<branch>:<sha> origin`, after a backup bundle. `settings.json` denies plain `--force`/`-f`, `--mirror`, `+refspec` and `:`-delete pushes and puts `--force-with-lease` on "ask".
- Never change repo visibility. Never delete `main` on origin (`git push --delete`/`-d`, `gh api` on `refs/heads/main`); other branches stay deletable.
- Commit and push as separate commands: deny patterns match the whole command line, so a commit message quoting a blocked push is itself blocked.
- Deploy (user's standing order, 2026-11-24): after every QA-PASS commit, redeploy `build/Agent/Release` to `D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE` with `.claude/tools/deploy-local/deploy.ps1` (via `release-manager`), without asking. Skip only when `Get-Process HiveWE` shows it running, and say so in chat. Never ask the user to close HiveWE and never kill it.
- Publishing a release (version tag) happens only when the user asks for it; `release-manager` prepares it.

## Designs
Decided 2026-11-22: Pencil (`.pen`) was dropped for HTML/CSS mockups plus a Qt prototype gate. Pencil's VS Code extension exposes one fixed named pipe per machine (`\.\pipe\pencil-visual_studio_code`); with two VS Code windows open the first window owns it, so the MCP server talks to the wrong window and `get_app_state` keeps saying "A document needs to be open". Each Claude session also starts its own pencil MCP server. Too fragile.
- Mockups are plain text in `designs/proposals/<name>/` (`index.html` or several pages, one shared `tokens.css` for Dark and Light, SVG icons, PNGs in `renders/`). Process, CSS limits and rendering command: `designs/README.md`.
- Only CSS that QSS can express: no box-shadow, blur/filter, backdrop, transforms, transitions or animation, grid tricks Qt layouts cannot do; border-radius and gradients only where Qt stylesheets support them. Fonts: the bundled UI font plus system fallback. Icons: the SVG files the app will ship.
- Render headless (no window, no VS Code, no MCP). Do not require the Python or npm playwright packages; they are not installed.
- Gate 1: the user approves the rendered PNGs (direction pick). Gate 2: the first `cpp-dev` step of an approved UI change is a real Qt prototype (the actual window with the theme QSS applied), screenshotted with `.claude/tools/ui-capture/`; the user approves that screenshot before the rest of the UI work proceeds. QA window announcements (see Chat rules) still apply.
- Run one designer at a time.
- Archived `.pen` files in `designs/archive/pencil/` are encrypted: never Read or Grep them (`settings.json` denies it).
- New windows and changed layouts need both approvals before the rest of the implementation.

## Map data
- Never modify `data/test map/` or a user's map in place. Use a copy.

## Verification without the user (decided 2026-11-09)
- SendInput and SendKeys stay banned. Checks QA cannot do without them (brush strokes, copy/paste/undo, in-session edits such as a placed unit with a 5-byte id) are not handed to the user. `cpp-dev` adds in-process automated tests or hooks that drive those code paths directly.
- QA first tries UIA (for example ValuePattern SetValue on Qt line edits, reading IsEnabled) before declaring that a check needs a hook.
- Agents may launch Warcraft III 3.0.1 and its World Editor on sandbox COPIES of maps to check that HiveWE output loads: launch, load, capture the screen, read errors. Never save in the World Editor (Test Map saves, so never use it), never touch the user's real maps. The orchestrator tells the user in chat before each run, because they may have the World Editor open with unsaved work. This replaces the old "no World Editor without new permission" rule for this purpose only.
- Automated checks must never show UI on the user's screen or take their focus (user, 2026-11-22). QA GUI runs use the offscreen flags in `.claude/tools/ui-capture/README.md`; tests never construct a `QApplication` or `QWidget`.
- Before listing any check for the user, the orchestrator sorts the checks into automatable, blocked by a rule, and truly user-only. Only the last group goes to the user.

## Real-game launches (3.0.1 load checks)
- The user confirms the Battle.net login works before a launch; launch through Battle.net, never the exe directly. If it fails, Warcraft III can exit by itself within seconds with no error text: stop after repeated silent exits instead of retrying in a chain.
- The orchestrator's message announcing a run says "do not click or type in the window" (a user click once spoiled a run announced only in general terms).
- `Start-Process -Wait` also waits for `BlizzardError.exe` after a crash and stalls the chain. Use a watcher that stops only a `BlizzardError.exe` whose parent is a game process this run started.
- Screenshots can show the user's Battle.net e-mail: keep them local, never commit or paste them.
- A Warcraft III process that exists before the launch is the user's: leave it alone and return `NEEDS_DECISION:`.

## Resource safety (incident 2026-11-09)
- An agent's StormLib extraction with `SFileFindFirstFile` hung and grew to 20 GB RAM on the user's PC. Never use `SFileFindFirstFile` on these maps; extract through `(listfile)` plus known names.
- Every long Python job gets a wall-clock timeout and handles one map per process. Stop your own process above about 2 GB and kill your own leftovers before returning.
- Load StormLib/CascLib from a scratchpad copy of the DLLs, not `build/Agent` (it locks the DLLs during builds).

## Reporting
- Report faithfully: failing tests, skipped checks and unknowns are stated plainly.
- Subagents cannot ask the user. They return `NEEDS_DECISION:` with a recommended default, and the orchestrator asks.
- Hitting a permission rule means stop and report. Never rephrase a command to get around it.

## Session handoff
- The orchestrator cannot measure context exactly, so it estimates: subagent reports so far, long tool results, whether the session was already summarized.
- Spawn budget: after 8 to 10 subagent runs (resumes do not count) it finishes the running task, writes the handoff and offers a new session without being asked; it starts no new task past that point unless the user says so.
- It suggests a new session when context looks high and at natural points such as a finished ticket. It does not stop mid-task to do so.
- Before suggesting: commit and push finished work so the tree is clean, update the Asana board and guidance docs, and write `.claude/HANDOFF.md` (overwritten each time, committed). Format, in this order:
  1. Header: date, models used, spawn count.
  2. `Checks waiting on the user`: numbered list with exact steps (drop checks the user already passed unless a later commit changed them in a way automation cannot see).
  3. `State`: what is committed, running or unfinished.
  4. `Next step`: names an Asana ticket from "Next up".
  5. `Backlog only, not next steps`: one line of leftovers.
  6. `Decisions`: the user's, then orchestrator calls the user may overrule, each dated.
  7. `Retro`: one line with the numbers and accepted proposals from the session retrospective (below).
  8. `Scratch`: absolute paths of scratchpad files the next session needs (they may vanish; say so).
- Every suggestion includes a paste-ready prompt in the message.
- Decided 2026-11-24: the orchestrator ends the last message of a real stop (see "Keep working": no eligible ticket, spawn budget reached, or the user said stop), not only when it suggests a new session, with a fixed block titled "Handoff prompt": a short paste-ready prompt naming the next-step Asana ticket, telling the next session to start by listing in chat every check or decision waiting on the user per `HANDOFF.md`, then start working immediately, pick the next ticket itself and not wait for "continue", and giving scratch paths. Not after one small task mid-session, on mid-task status messages, short replies or answers to questions. Reason: the user lost the prompt cache to idle twice on 2026-11-24 because the rule only fired on a "suggestion" and was forgotten.
- After each committed task the orchestrator refreshes `.claude/HANDOFF.md` (a one-line update of `State` and `Next step` is enough), so the Handoff prompt matches it.
- The user does not read the MD files, including `HANDOFF.md`. Every paste-ready prompt tells the next session to start by listing in chat every check or decision waiting on the user (from `HANDOFF.md` "Open questions"), with exact steps.
- Anything the user must do is stated in chat, not only in a file.

## Keep working (decided 2026-11-24)
- The orchestrator never ends a turn with text alone while work remains: a status message without a tool call stops the session, and the user had to type "continue" repeatedly.
- After a subagent notification or a pushed commit, the same turn makes the next tool call: next agent, commit, or the next eligible Asana "Next up" ticket.
- A turn may end only when (a) a subagent is still running and nothing else can be done (one brief line), or (b) a stop condition holds: no eligible ticket, spawn budget (8 to 10 runs) reached, or the user said stop.
- At a stop: retrospective, `HANDOFF.md` refresh, commit, Handoff prompt block. Never give a Handoff prompt after one small task mid-session.

## Session retrospective (decided 2026-11-24)
Before every handoff the orchestrator runs a retrospective, reports it in chat (short, with numbers) and records the `Retro` line in `HANDOFF.md`. It covers:
1. Timing: wall-clock per task and per subagent run (task-notification `duration_ms`), rounds per task, longest runs, time spent waiting on the build lock or the one-build-at-a-time rule.
2. Why slow: bug hunts, edge-case chasing, repeated fix rounds, oversized briefs, redundant QA or rebuilds, premature opus use.
3. For each cause: can it be optimized, and should it be (cost against benefit)? Propose a rule or brief change.
4. Quality guard: every speed-up must keep code quality. No removal of review on non-trivial source changes, no weaker tests, no skipping QA on behaviour changes. State why each proposal does not lower quality.
5. What went well and should be kept.
6. Bug-hunt guard: an investigation past the time cap, or a finding outside HARNESS.md 'Choosing work' criteria, is filed, not chased.
- `docs-keeper` applies proposals that change harness rules without asking only if they weaken no safety rule and no quality gate. Anything else goes to the user as a one-line question.

## Chat rules
- The numbered test list for the user goes in the LAST message of the turn (a list posted before the final tool call was missed).
- After opening a window for the user, end the turn at once and do not poll it.
- Every question to the user is written out in full in chat. Never re-list a check the user already passed.
- Announce QA GUI windows to the user before they open, in chat (one opened unannounced on 2026-11-20 and the user asked about it; they may also mistake a stock-map run for a defect).

## Self-updating guidance
- When work reveals something that helps later work (a bug pattern, a tool quirk, a better method, a wrong assumption), update the owning rule, skill or agent file without asking the user.
- Subagents do not edit guidance outside their role. They end reports with a `LEARNED:` line when they have something durable, and the orchestrator runs `docs-keeper`.
- Record verified facts only; label guesses `Needs verification`. Remove what turns out wrong. Do not record one-off details.
- Never weakens safety: guardrails, build policy, the map data rules and `.claude/settings.json` permissions change only when the user asks.

## Orchestrator habits
- Do not hold work for a check the next step does not depend on (for example CI on an unrelated commit). Start it, watch in the background, report the result.
- Spot-check subagent claims that drive decisions against the code before filing them. Designer runs have skipped parts of a brief: check that they launched the app.
- Check paths before putting them in a brief.
- Do not chase edge cases: file them per HARNESS.md 'Choosing work'.
- Explain a subsystem briefly the first time a task touches it (the user is building a mental map of the repo).
- Name in every brief the scoped rules (`.claude/rules/`) and skills the agent must read.
- If the work turns into a cosmetic World Editor byte-parity fight (differences that do not change what loads or runs), stop and ask before grinding further.
- `compile.yml` sets `cancel-in-progress: true`, so a second push to `main` cancels the CI run of the previous commit (run 42658845053 on a14cdea was cancelled by the handoff push). Take the code commit's CI result from the newer run and name that run when reporting.
