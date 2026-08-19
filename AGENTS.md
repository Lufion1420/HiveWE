# HiveWE AI Guidance

HiveWE is a native C++23 Warcraft III World Editor replacement (`.ixx` modules + Qt). Detailed guidance lives in `.cursor/` — this file is the index.

## Context layers

| Layer | Path | When it loads |
|-------|------|---------------|
| Always-on rules | [`.cursor/rules/hivewe-core.mdc`](.cursor/rules/hivewe-core.mdc), [`hivewe-build-policy.mdc`](.cursor/rules/hivewe-build-policy.mdc) | Every chat (Cursor) |
| Scoped rules | [`hivewe-cpp-modules.mdc`](.cursor/rules/hivewe-cpp-modules.mdc), [`hivewe-qt-ui.mdc`](.cursor/rules/hivewe-qt-ui.mdc), [`hivewe-file-formats.mdc`](.cursor/rules/hivewe-file-formats.mdc), [`hivewe-rendering.mdc`](.cursor/rules/hivewe-rendering.mdc) | When matching files are in context |
| On-demand skills | [`.cursor/skills/`](.cursor/skills/) | Invoke explicitly or when task matches skill description |
| Feature plans | [`.cursor/plans/`](.cursor/plans/) | Reference for planned work |
| Kilo Code skills | [`.kilo/skills/`](.kilo/skills/) | Auto-loaded by Kilo Code when task matches |
| Kilo Code commands | [`.kilo/commands/`](.kilo/commands/) | Invoked via `/command-name` in Kilo Code |

## Skills (`.cursor/skills/` — canonical; `.kilo/skills/` — Kilo Code copies)

- `hivewe-repo-navigation` — find code, trace call paths
- `hivewe-build-and-debug` — CMake, vcpkg, tests (manual builds by default)
- `hivewe-ui-development` — Qt palettes, docking, models
- `hivewe-warcraft3-data-formats` — map serialization, MDL/MDX, triggers
- `hivewe-delegate-work` — when to use subagents vs inline work

## Subagent routing (summary)

- **Inline:** single-file fixes, pasted compiler errors, known subsystem owner
- **Cursor `codegraph_explore`:** symbol/call-path questions (prefer before wide search)
- **Cursor/Kilo `explore` subagent:** unknown ownership across Map / Hierarchy / hivewe.cpp
- **Cursor/Kilo `generalPurpose`/`general` subagent:** cross-cutting features spanning 3+ folders
- **Do not auto-build** unless the user re-allows — see build-policy rule

Full routing: read [`hivewe-delegate-work`](.cursor/skills/hivewe-delegate-work/SKILL.md).

## Claude Code

See [`CLAUDE.md`](CLAUDE.md) — always-on rules are inlined there; read `.cursor/skills/*/SKILL.md` directly with the Read tool.

## Kilo Code

Skills in `.kilo/skills/` are auto-discovered by Kilo Code. Use `/hivewe-context` slash command to load the full architecture summary. `.cursor/` remains the single source of truth — update rules/skills there, not in `.kilo/`.

## Keeping guidance current

Update `.cursor/rules/` or `.cursor/skills/` when you learn durable repo facts. Prefer concrete paths and verification steps. Label uncertain conventions `Needs verification`.
