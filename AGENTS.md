# HiveWE AI Guidance

## Project overview
- HiveWE is a native C++ Warcraft III World Editor replacement focused on faster map loading, faster rendering, and better editing UX than the default editor.
- The codebase is primarily C++23 with C++ modules (`.ixx`) plus Qt widget code in `.cpp` / `.h` files.
- Main app architecture is split between map/data systems, file-format parsing, rendering/resources, and editor/UI windows.

## Build, run, test
- Required toolchain: Visual Studio 2022 17.14+ on Windows. The README explicitly calls out C++20 modules support; the current CMake files request `cxx_std_23`.
- Dependency management: `vcpkg` via `VCPKG_ROOT`, with custom overlay ports in `overlay-ports/`.
- User preference: do not run `cmake --build ...` from Codex unless the user explicitly asks to re-enable that. Leave build/debug execution manual.
- User preference: prefer fixing compile issues from pasted build errors first, and only run build commands when the user explicitly re-allows it for that case.
- Configure/build:
```powershell
cmake --preset Release
cmake --build --preset Release
```
- Tests:
```powershell
ctest --preset Release
```
- Useful presets: `Debug`, `Release`, `ReleaseTracing`. `ReleaseTracing` enables Tracy.
- Visual Studio open-folder builds may need Administrator because the build creates a symlink from the output directory to `data/`.
- CI reference: `.github/workflows/compile.yml`.
- Needs verification: local debugger/run steps beyond standard CMake/Visual Studio workflow were not separately documented.

## Important folders
- `src/main.cpp`: application startup, Qt/OpenGL setup, theme loading, CASC startup, initial map load.
- `src/main_window/`: main window, GL widget, ribbon, app-level commands such as open/save/export/test.
- `src/base/`: core map/editor data systems such as `Map`, terrain, units, doodads, triggers, gameplay constants, undo, rendering, hierarchy, physics.
- `src/base/map/`: map resize and unused-file analysis helpers.
- `src/base/triggers/`: trigger data loading/saving and map script generation.
- `src/file_formats/`: Warcraft III and engine-adjacent file formats (`casc`, `mpq`, `slk`, `ini`, `json`, `blp`, `mdx`).
- `src/resources/`: GPU/resource layer for textures, meshes, shaders, particle systems, skinned meshes.
- `src/object_editor/`: object editor UI and SLK/object-editing helpers.
- `src/trigger_editor/`: trigger editor, JASS editor/tokenizer, trigger tree/model UI.
- `src/model_editor/`: standalone model editor views and camera/widgets.
- `src/menus/`: terrain, pathing, unit, doodad, minimap, settings, map info, gameplay constants dialogs/palettes.
- `src/models/`: Qt item/list/tree/table models used by editors.
- `src/brush/`: terrain/pathing/unit/doodad brush behavior.
- `src/custom_widgets/`: reusable Qt widgets such as `QRibbon`, selectors, labels, color button.
- `src/utilities/`: timers, GL thread pool, helpers, modification-table helpers, math/allocators.
- `tests/`: doctest-based unit tests, especially binary writer and MDL/MDX round-trip coverage.
- `data/`: runtime assets, shaders, themes, icons, Warcraft overrides, bundled tools, and test map fixtures.
- `overlay-ports/`: patched/custom vcpkg ports for dependencies like StormLib, CascLib, SOIL2, Bullet3.

## Conventions observed in the repo
- Formatting is repo-defined in `.clang-format` and `tabs.editorconfig`.
- Indentation uses tabs, width 4. Line limit is 140. Includes are intentionally not auto-sorted.
- Naming is mostly `snake_case` for functions, fields, files, and modules. Types are usually `PascalCase`.
- Modules are grouped by subsystem and imported by logical names such as `import Map;`, `import BinaryReader;`, `import Terrain;`.
- Qt UI classes use `*.ui` plus `*.cpp` / `*.h`. Non-Qt engine/data code often lives in `.ixx`.
- Globals are used intentionally in places such as `map`, `camera`, `hierarchy`, `resource_manager`, and `gl_thread_pool`. Do not introduce new globals casually.
- Error handling is mixed by layer:
  - UI-facing flows often use `QMessageBox`.
  - Parsing/resource code often uses `std::expected<..., std::string>`.
  - Low-level binary/file helpers may throw exceptions on invalid bounds or I/O failure.
  - Diagnostics often use `std::println`.
- Assertions are used as invariants inside parsers and data transforms. Do not replace them with silent fallbacks unless behavior is clearly user-facing.

## Architecture rules
- Keep file-format logic in `src/file_formats/` or the specific `src/base/` map component that owns the format. Do not spread binary parsing logic into UI classes.
- Keep Qt/editor concerns in `src/main_window/`, `src/menus/`, `src/object_editor/`, `src/trigger_editor/`, and `src/model_editor/`.
- Keep rendering/resource lifetime logic in `src/resources/`, `src/base/render_manager.ixx`, and `src/main_window/glwidget.cpp`.
- `Map` in `src/base/map/map.ixx` is a central orchestrator. Changes there can affect load order, save order, editor model wiring, rendering, and undo behavior.
- `Hierarchy` is the boundary for file lookup priority across overrides, local files, map files, and CASC. Preserve that lookup order unless intentionally fixing it.
- `ResourceManager` caches shared resources and is used across map/editor systems. Avoid bypassing it with ad hoc duplicate loading.
- `GLThreadPool` and post-load `glFinish()` calls are intentional synchronization points. Do not remove them casually.

## Safe change rules
- Prefer small, local edits in the owning subsystem.
- Preserve current load/save order for map data unless you have verified binary compatibility and editor behavior.
- Reuse existing Qt models, dock patterns, and palette patterns instead of adding parallel UI state systems.
- Before changing ownership/lifetime, verify whether Qt parent ownership already handles cleanup.
- When changing async or threaded loading, verify main-thread GL assumptions and shared-context visibility.
- Remove dead code only when it is clearly unused and not part of format compatibility or editor workflows.

## Bug-fix workflow
1. Reproduce the issue and identify whether it lives in UI, map state, rendering, or file-format parsing.
2. Read the owning subsystem first instead of patching at the call site.
3. For file-format bugs, use the smallest possible fixture or map artifact and preserve round-trip behavior.
4. For UI bugs, check signal/slot wiring, model updates, and window lifetime before adding state.
5. For rendering bugs, check resource loading, GL context/thread assumptions, and render toggles before changing shaders.
6. After the fix, run the narrowest relevant test and then broader build/test verification if feasible.

## Feature-addition workflow
1. Find the existing subsystem that already owns the behavior.
2. Extend current models, palettes, or map components instead of creating parallel systems.
3. Keep editor actions consistent with existing undo/save flows.
4. For new serialized data, identify the exact Warcraft III file(s) involved and preserve compatibility with existing maps.
5. Add or extend tests when changing parser/serializer behavior or deterministic utilities.

## Warcraft III data and file-format guidance
- Relevant code lives in `src/file_formats/`, `src/base/`, `src/base/triggers/`, and object-editor conversion helpers.
- Treat `BinaryReader` / `BinaryWriter` invariants as format boundaries. Avoid partial writes or silent truncation.
- Preserve round-trip behavior for MDL/MDX, SLK merges, W3* map files, and generated trigger/map script outputs.
- `Map::load()` and `Map::save()` encode important sequencing between SLK/meta loading, map overrides, object tables, terrain/pathing, doodads, units, triggers, gameplay constants, and imports.
- Watch for version-sensitive behavior in model code. The MDL/MDX layer explicitly handles version downgrade/upgrade behavior.
- When uncertain about a file-format rule, mark it as needing verification and test against a fixture or real map asset.

## UI and editor guidance
- Qt Widgets + docking is the established UI model. Reuse existing dock, palette, and model/view patterns.
- Common patterns:
  - `window_handler.create_or_raise<T>()` for singleton-like tool windows.
  - `Palette` subclasses for context-specific editing panels.
  - The main editing palettes now live in a right-hand sidebar managed from `src/main_window/hivewe.cpp`; visibility and active brush context are related but separate concerns.
  - Qt model/view classes in `src/models/` and editor-specific tree/list models.
  - `QShortcut` wiring close to the owning window/dialog.
- Avoid pushing heavy data parsing or resource loading logic down into widgets when a base/resource/file-format subsystem already owns it.
- Respect `.ui` files where they already define layout structure.
- When adjusting editor UX, prefer clear active-state feedback, predictable toggle behavior, and low-noise transient feedback over adding more controls.

## Debug/view guidance
- The debug overlay in `src/main_window/glwidget.cpp` now distinguishes HiveWE-internal terrain-space coordinates from Warcraft III world-space coordinates; user-facing coordinate readouts should prefer Warcraft coordinates.
- Treat debug overlays as user-facing tools in this fork, not just developer diagnostics. Trim low-signal entries rather than expanding them casually.

## Performance-sensitive areas
- Map loading in `src/base/map/map.ixx` uses `std::async` and staged timing output.
- GL resource upload uses `GLThreadPool` shared contexts and explicit synchronization.
- Rendering and animation paths in `Map::update()` / `Map::render()` are hot paths.
- Resource caching in `ResourceManager` is intentional and thread-safe. Avoid repeated uncached resource construction.
- Large SLK/meta merges and object-editor tables can affect startup/editor responsiveness.
- Tracy integration exists; prefer `ReleaseTracing` for profiling changes that affect startup, rendering, or loading.

## Verification checklist
- Build with the relevant preset.
- Run `ctest --preset Release` when parser/serializer or other tested code changes.
- For MDL/MDX work, confirm round-trip tests still pass.
- For UI changes, manually open the affected editor/palette/window and verify shortcuts, docking, and persistence behavior.
- For this fork specifically, many changes are workflow/UI heavy. Expect user-driven manual verification for palettes, object editor behavior, regions, and debug overlays when builds are not being run by Codex.
- For map load/save changes, open the bundled `data/test map/`, save it, and verify no obvious regressions in terrain, objects, triggers, or generated outputs.
- For rendering/resource changes, verify map open, camera movement, minimap, and the affected render toggles.
- For Warcraft III launch/test-map changes, verify paths and external tool assumptions locally if possible.
- If you could not run a build or tests, say so explicitly.

## Common pitfalls for AI agents
- Do not treat this as a generic game-engine repo. A lot of code exists to preserve Warcraft III map/editor compatibility.
- Do not move file-format code into UI classes for convenience.
- Do not break load/save sequencing in `Map`.
- Do not remove `glFinish()` or GL thread-pool synchronization without proving correctness.
- Do not bypass `Hierarchy` path resolution rules when loading assets.
- Do not add broad abstractions for one-off editor behavior. This repo tends to prefer direct subsystem-local code.
- Be careful with raw `new` in Qt code: some objects intentionally rely on Qt parent ownership, but some long-lived objects are manually deleted or recreated.
- Watch for memory/resource leaks around `QProcess`, image buffers, GL resources, and map reload paths. Verify ownership and cleanup instead of assuming.
- Region support, player starts, and several palette flows in this fork are recent additions. When modifying them, check both viewport behavior and save/script generation paths instead of assuming they are mature subsystems.

## Keeping this file current
- Update `AGENTS.md` when you learn a repo-specific rule that is consistently true and useful for future work.
- Prefer adding concrete facts, file paths, and verification steps over generic advice.
- If a convention is still uncertain, label it `Needs verification` and mention how to confirm it.
