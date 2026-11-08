# HiveWE agent harness

Created 2026-11-08, adapted from the Lendrill harness. The main Claude Code session is the orchestrator: it plans, delegates, owns the gates and git, and talks to the user. Subagents do the work. Subagents cannot spawn subagents, so the orchestrator runs each stage. Rules for everyone: [`rules/harness-guardrails.md`](rules/harness-guardrails.md).

**Handoff:** `.claude/HANDOFF.md` holds the state for the next session; rules in the guardrails ("Session handoff"). **Tools:** `.claude/tools/ui-capture/` has the UIA and capture scripts the designer uses (see its README).

**Tickets:** [Asana project "HiveWE"](https://app.asana.com/1/1213523605265041/project/1220328345553589) (GID 1220328345553589). The orchestrator owns the board. Asana MCP: `search_tasks` needs Premium, so use `get_tasks`. Tickets become subtasks with `update_tasks` (set parent, plus `remove_projects`). Subtasks come back in shuffled order, so names must not rely on list position. When a ticket is finished, add a comment (what shipped, how it was verified, what was not), mark it complete, and move it to "Done — shipped".

## Roster

| Agent | Job | Edits | Model |
|---|---|---|---|
| `cpp-dev` | Implements the change and tests, compiles before returning | Source, tests | Sonnet; Opus on the risky track |
| `reviewer` | Adversarial diff review, APPROVE or REJECT | Nothing | Opus |
| `qa-verifier` | Rebuilds, runs tests, writes manual UI checks; PASS, FAIL or COULD-NOT-RUN | Nothing | Sonnet |
| `release-manager` | Changelog, version bumps, `release.yml`, publishing releases | Changelog, version strings, release workflow | Sonnet |
| `designer` | `.pen` mockups for approval, and the as-built baseline | `designs/` only | Sonnet |
| `wc3-format-specialist` | Sourced note on formats and compatibility before implementation | Nothing | Opus |
| `build-engineer` | CMake, vcpkg, overlay ports, CI, ICE and linker failures | Build files | Sonnet |
| `docs-keeper` | `.claude/rules/`, `.claude/skills/`, `.claude/plans/`, `.claude/agents/`, `.claude/HARNESS.md`, `CLAUDE.md` | Docs only | Sonnet |

Built-in `Explore` and `Plan` handle scoping when ownership is unknown; launch two `Explore` agents in parallel when both the UI and format layers are unknown. Use Codegraph (`codegraph_explore`) before wide searches; it needs `projectPath` set to the repo path, and has failed project detection for agents (one saw "not initialized"), so fall back to Grep. `hivewe.cpp` line numbers drift quickly: cite symbol names in tickets and briefs. Run the `security-review` skill only when the user asks (MPQ export, external `QProcess`, file I/O on user paths). Add `security-auditor` (untrusted map parsing, `QProcess`, Map Protector) and `perf-auditor` (Tracy, render loop, load times) when first needed.

## Pipeline

| Track | Use for | Stages |
|---|---|---|
| Small | Tooltip, label, one-line fix | `cpp-dev`, `qa-verifier`, `release-manager` |
| Standard | Most features and bugs | `cpp-dev`, `reviewer`, `qa-verifier`, `release-manager` |
| Risky | Save or format code, threading, map reload, Map Protector | `wc3-format-specialist`, then Standard with `cpp-dev` on Opus |

Add-ons on any track:
- **New window or changed layout:** run `designer` first. Stop at `AWAITING_APPROVAL` until the user approves the frames. After shipping, run `designer` again to update `designs/current/`.
- **Durable fact or convention learned:** run `docs-keeper`.
- **Toolchain failure:** run `build-engineer`.

Then the orchestrator commits and pushes. Internal-only changes get no changelog entry, so `release-manager` is skipped for them.

## Gates
- `reviewer` REJECT goes back to `cpp-dev`, then re-review. After 3 rejections, stop and ask the user.
- `qa-verifier` FAIL goes back to `cpp-dev`, then `reviewer`, then QA again. COULD-NOT-RUN is not a pass.
- Manual checks from QA go to the user as a short list. The change is described as unverified until they confirm.
- Stop and ask on `NEEDS_DECISION` or `NEEDS_GAME_TEST`.
- Keep the orchestrator's context small: subagents report under 25 lines and the orchestrator does not read build logs.

## Limits
- The orchestrator-only rule lives in `CLAUDE.md` as an instruction; nothing mechanically enforces it. The orchestrator still uses read-only lookups, codegraph and git.
- `.claude/settings.json` mechanically denies Read on `.pen`, building the `Release` preset, and pushing to `upstream`.
- Lendrill's push-guard hook was not ported: it was disabled there for causing too many prompts.
- Only one build and one designer can run at a time, so development runs sequentially.
- No automated UI testing exists. A headless `data/test map` load/save round-trip test would be the biggest verification upgrade.

## Sources

What each agent's process drew on. `wc3-format-specialist` and `designer` are essentially ours: nothing suitable exists for WC3 formats or Qt-faithful Pencil mockups. Collections such as wshobson/agents and VoltAgent/awesome-claude-code-subagents were rejected as generic personas for the wrong toolchain.

- `reviewer`: Qt's [qt-cpp-review rules](https://github.com/TheQtCompanyRnD/agent-skills) (vendored, BSD-3, in `references/qt-review-rules.md`), [chrxh/alien reviewer](https://github.com/chrxh/alien/blob/HEAD/.claude/agents/reviewer.md).
- `cpp-dev`: obra/superpowers [implementer prompt](https://github.com/obra/superpowers/blob/HEAD/skills/subagent-driven-development/implementer-prompt.md) and test-driven-development.
- `qa-verifier`: superpowers [verification-before-completion](https://github.com/obra/superpowers/blob/HEAD/skills/verification-before-completion/SKILL.md), [Jinja2Cpp verifier](https://github.com/jinja2cpp/Jinja2Cpp/blob/HEAD/.claude/agents/verifier.md) (ideas only, MPL).
- `release-manager`: public release-manager agents (Donchitos Claude-Code-Game-Studios, mampfes, thedevelobear) for the failure path.
- `docs-keeper`: Anthropic [claude-md-improver update guidelines](https://github.com/anthropics/claude-plugins-official/blob/HEAD/plugins/claude-md-management/skills/claude-md-improver/references/update-guidelines.md), superpowers writing-skills.
- `build-engineer`: [Yuzu build-ci](https://github.com/DevNullLtd/Yuzu) and Jinja2Cpp ci-triage failure classes (ideas only; AGPL and MPL).
- `wc3-format-specialist`: reference list in the agent file (WC3MapSpecification, War3Net, mdx-m3-viewer, StormLib, WC3MapTranslator). wenbinio/wc3 has no license and was not used.
- `designer`: Pencil's own skill docs plus our first baseline run; pen.dev style advice deliberately ignored.
