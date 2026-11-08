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
- No force-push. Publishing a release (version tag) happens only when the user asks for it; `release-manager` prepares it.

## Designs
- All `.pen` files live in `designs/`. Never Read or Grep them; use `mcp__pencil__*`.
- The file must be open and active in VS Code. Agents open and focus it themselves with `code --reuse-window`, without asking the user. Run one designer at a time.
- New windows and changed layouts need user-approved frames before `cpp-dev` starts.

## Map data
- Never modify `data/test map/` or a user's map in place. Use a copy.

## Reporting
- Report faithfully: failing tests, skipped checks and unknowns are stated plainly.
- Subagents cannot ask the user. They return `NEEDS_DECISION:` with a recommended default, and the orchestrator asks.
- Hitting a permission rule means stop and report. Never rephrase a command to get around it.
