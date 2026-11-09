# Harness guardrails (main session and every subagent)

Set by the user on 2026-11-08. The roster and pipeline are in `.claude/HARNESS.md`.

## Builds
- Agents may build and run tests, using only the `Agent` preset: `cmake --preset Agent`, `cmake --build --preset Agent`, `ctest --preset Agent`.
- Never build the `Release` preset locally. It has `HIVEWE_AUTO_RUN_AFTER_BUILD=ON`, which force-kills any running `HiveWE.exe` (losing unsaved map work) and launches a new one.
- One build at a time: agents share `build/Agent`. Do not use parallel worktrees for builds.
- Never kill `HiveWE`, `code`, or any process you did not start.

## Git and releases
- The orchestrator commits and pushes after each finished task, without asking. Subagents do not commit or push.
- Stage named paths only, never `git add -A`.
- Push only to `origin` (`Lufion1420/HiveWE`). Never push to `upstream`, and never open PRs against `stijnherfst/HiveWE`: this fork is not merged back.
- `gh`'s active account may be the work account `Maurice-TPG`. Never use it and never run `gh auth switch`. Scope the identity per command:
  - Bash: `GH_TOKEN=$(gh auth token -u Lufion1420) git push origin main`
  - The same prefix applies to `gh` commands.
- `git push origin main` prints "Bypassed rule violations ... Changes must be made through a pull request". The push still succeeds (owner bypass); verify with `git status -sb`.
- No force-push. Publishing a release (version tag) happens only when the user asks for it; `release-manager` prepares it.

## Designs
- All `.pen` files live in `designs/`. Never Read or Grep them; use `mcp__pencil__*`.
- The file must be open and active in VS Code. Agents open and focus it themselves with `code --reuse-window`, without asking the user. Run one designer at a time.
- New windows and changed layouts need user-approved frames before `cpp-dev` starts.

## Map data
- Never modify `data/test map/` or a user's map in place. Use a copy.

## Verification without the user (decided 2026-11-09)
- SendInput and SendKeys stay banned. Checks QA cannot do without them (brush strokes, copy/paste/undo, in-session edits such as a placed unit with a 5-byte id) are not handed to the user. `cpp-dev` adds in-process automated tests or hooks that drive those code paths directly.
- QA first tries UIA (for example ValuePattern SetValue on Qt line edits, reading IsEnabled) before declaring that a check needs a hook.
- Agents may launch Warcraft III 3.0.1 and its World Editor on sandbox COPIES of maps to check that HiveWE output loads: launch, load, capture the screen, read errors. Never save in the World Editor (Test Map saves, so never use it), never touch the user's real maps. The orchestrator tells the user in chat before each run, because they may have the World Editor open with unsaved work. This replaces the old "no World Editor without new permission" rule for this purpose only.
- Before listing any check for the user, the orchestrator sorts the checks into automatable, blocked by a rule, and truly user-only. Only the last group goes to the user.

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
- It suggests a new session when context looks high and at natural points such as a finished ticket. It does not stop mid-task to do so.
- Before suggesting: commit and push finished work so the tree is clean, update the Asana board and guidance docs, and write `.claude/HANDOFF.md` (overwritten each time, committed). It covers current state, what is running or unfinished, next step, decisions the user made, and open questions.
- Every suggestion includes a paste-ready prompt in the message.
- The user does not read the MD files, including `HANDOFF.md`. Every paste-ready prompt tells the next session to start by listing in chat every check or decision waiting on the user (from `HANDOFF.md` "Open questions"), with exact steps.
- Anything the user must do is stated in chat, not only in a file.

## Self-updating guidance
- When work reveals something that helps later work (a bug pattern, a tool quirk, a better method, a wrong assumption), update the owning rule, skill or agent file without asking the user.
- Subagents do not edit guidance outside their role. They end reports with a `LEARNED:` line when they have something durable, and the orchestrator runs `docs-keeper`.
- Record verified facts only; label guesses `Needs verification`. Remove what turns out wrong. Do not record one-off details.
- Never weakens safety: guardrails, build policy, the map data rules and `.claude/settings.json` permissions change only when the user asks.

## Orchestrator habits
- Do not hold work for a check the next step does not depend on (for example CI on an unrelated commit). Start it, watch in the background, report the result.
- Spot-check subagent claims that drive decisions against the code before filing them. Designer runs have skipped parts of a brief: check that they launched the app.
- Check paths before putting them in a brief.
- `compile.yml` sets `cancel-in-progress: true`, so a second push to `main` cancels the CI run of the previous commit (run 42658845053 on a14cdea was cancelled by the handoff push). Take the code commit's CI result from the newer run and name that run when reporting.
