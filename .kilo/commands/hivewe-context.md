---
description: Load full HiveWE architecture context, conventions, and build policy
---

# HiveWE Full Context

## Architecture boundaries

- File formats: `src/file_formats/` or owning `src/base/` component — not UI classes.
- Qt/editors: `src/main_window/`, `src/menus/`, `src/object_editor/`, `src/trigger_editor/`, `src/model_editor/`.
- Rendering/resources: `src/resources/`, `src/base/render_manager.ixx`, `src/main_window/glwidget.cpp`.
- `Map` (`src/base/map/map.ixx`) orchestrates load/save order, undo, rendering wiring.
- `Hierarchy` owns asset lookup order (overrides → local → map → CASC). Do not bypass.
- `ResourceManager` caches shared resources. Do not duplicate loading ad hoc.
- `GLThreadPool` and post-load `glFinish()` are intentional sync points.

## C++ module conventions

- Primary implementation units are `.ixx` modules; Qt UI stays in `.cpp`/`.h`.
- Tabs, width 4, line limit 140 (`.clang-format`, `tabs.editorconfig`).
- `snake_case` functions/fields/files; `PascalCase` types.
- Imports by logical name: `import Map;`, `import BinaryReader;`, `import Terrain;`. Do not auto-sort includes.
- Intentional globals: `map`, `camera`, `hierarchy`, `resource_manager`, `gl_thread_pool`. Do not add new globals casually.
- Error handling: UI flows → `QMessageBox`; parsing/resources → `std::expected<..., std::string>`; low-level binary → exceptions; diagnostics → `std::println`.

## Build policy (Windows-first)

- **Do not run `cmake --build ...` or any build/test command** unless the user explicitly re-allows for the current task.
- Prefer fixing compile issues from **pasted build errors** first.
- When allowed, use presets: `cmake --preset Release`, `cmake --build --preset Release`, `ctest --preset Release`.
- Presets: `Debug`, `Release`, `ReleaseTracing` (Tracy profiling).
- VS 2022 17.14+, `VCPKG_ROOT`, overlay ports in `overlay-ports/`.
- `LNK1104` on `HiveWE.exe` is often a running editor holding the file lock.

## Scoped rules — read before editing matching files

| When touching | Read | Topic |
|---|---|---|
| `**/*.ixx` | `.cursor/rules/hivewe-cpp-modules.mdc` | Module style, naming, globals, error handling |
| `src/{main_window,menus,object_editor,trigger_editor,model_editor,custom_widgets}/**` | `.cursor/rules/hivewe-qt-ui.mdc` | Qt docking, palettes, models, shortcuts |
| `src/{file_formats,base/map,base/triggers}/**` | `.cursor/rules/hivewe-file-formats.mdc` | WC3 binary format compatibility, round-trips |
| `src/resources/**`, `src/main_window/**`, `data/shaders/**` | `.cursor/rules/hivewe-rendering.mdc` | OpenGL, GPU resources, shaders |

## Subagent routing (Kilo Code)

- **Inline:** single-file fixes, pasted compiler errors, known subsystem owner.
- **`task(subagent_type: "explore")`:** unknown ownership across Map / Hierarchy / hivewe.cpp; launch two in parallel if UI and format layers are both unknown.
- **`task(subagent_type: "general")`:** cross-cutting features spanning 3+ folders.
- **Shell via `bash`:** git, preset listing, file inventory — run directly, no delegation.
- **Do not auto-build** unless user explicitly re-allows.

## Workflows

**Bug fix:** Identify layer (UI / map / render / format) → read owner first → narrowest test → broader verification.

**Feature:** Extend existing subsystem → match undo/save flows → identify WC3 files → add tests for parsers.

## Pitfalls

- Do not break `Map` load/save sequencing or `Hierarchy` lookup order.
- Do not remove `glFinish()` / GL thread-pool sync without proof.
- No broad abstractions for one-off editor behavior.
- Regions/player starts/palettes are recent — check viewport AND save/script paths.
- Watch Qt ownership, `QProcess`, GL resources on map reload.

## Verification

- Parser changes: `ctest --preset Release`; MDL/MDX round-trip tests.
- UI: open affected editor, check shortcuts/docking/persistence.
- Map save: `data/test map/` load/save smoke test.
- State explicitly if build/tests were not run.

## Source of truth

`.cursor/rules/` and `.cursor/skills/` are canonical. Update them when you learn durable repo facts, not this file.
