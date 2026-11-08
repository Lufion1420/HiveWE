# HiveWE — Claude Code Guidance

HiveWE is a native C++23 Warcraft III World Editor replacement (`.ixx` modules + Qt). Preserve WC3 map/editor compatibility — this is not a generic game engine.

## You are the orchestrator

**The main session never implements. On every task, including one-line fixes, you plan, delegate to the subagents in `.claude/agents/`, own the gates, commit, push, and report to the user.** You may use read-only lookups (Codegraph, Read, git) to scope work. Roster, tracks and gates: [`.claude/HARNESS.md`](.claude/HARNESS.md). Rules for you and every subagent: [`.claude/rules/harness-guardrails.md`](.claude/rules/harness-guardrails.md).

## Layout

- `.claude/rules/` — `hivewe-core.md` (architecture, safe changes, pitfalls), `hivewe-build-policy.md`, `harness-guardrails.md`, `changelog.md` load automatically in every session. Scoped rules load when matching files are touched; **subagents should read them explicitly** per the table below.
- `.claude/skills/` — playbooks, registered Skills.
- `.claude/plans/` — planned work (e.g. `map_protection_plan.md`).
- `.claude/agents/` and `.claude/HARNESS.md` — roster, tracks, gates.
- `designs/` — UI mockups ([`designs/README.md`](designs/README.md)).

When you learn a durable repo fact, have `docs-keeper` update the owning rule or skill, not this file.

## Build policy (safety-critical)

- Agents may build and test **only with the `Agent` preset**: `cmake --preset Agent`, `cmake --build --preset Agent`, `ctest --preset Agent`.
- **Never build the `Release` preset locally as an agent:** its auto-run step force-kills a running `HiveWE.exe`, losing unsaved map work.
- Details: `.claude/rules/hivewe-build-policy.md`. Say explicitly if build/tests were not run.

## Scoped rules — read before editing matching files

| When touching | Read | Topic |
|---|---|---|
| `**/*.ixx` | `.claude/rules/hivewe-cpp-modules.md` | Module style, naming, globals, error handling by layer |
| `src/{main_window,menus,object_editor,trigger_editor,model_editor,custom_widgets}/**/*.{cpp,h,ui}` | `.claude/rules/hivewe-qt-ui.md` | Qt docking, palettes, models, shortcuts |
| `src/{file_formats,base/map,base/triggers}/**/*` | `.claude/rules/hivewe-file-formats.md` | WC3 binary format compatibility, round-trips |
| `src/resources/**`, `src/main_window/**`, `data/shaders/**` | `.claude/rules/hivewe-rendering.md` | OpenGL, GPU resources, shaders, debug overlay |

## Skills

| Skill | Use when |
|---|---|
| `hivewe-repo-navigation` | Finding where code lives, tracing call paths, deciding which subsystem owns a change |
| `hivewe-build-and-debug` | Diagnosing compile/link errors, CMake presets, vcpkg, Tracy profiling |
| `hivewe-ui-development` | Editing palettes, docking, `.ui` files, Qt item models |
| `hivewe-warcraft3-data-formats` | Corrupted saves, load failures, round-trip bugs, war3map/war3mapSkin editing |
