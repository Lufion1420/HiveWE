# hivewe-repo-navigation

## Purpose
Use this skill to orient quickly inside HiveWE without guessing where code lives. The repo mixes Qt widget code, C++ modules, file-format parsers, rendering/resources, and Warcraft III map systems.

## When to use this skill
- A task starts with "find where this lives" or "understand this subsystem".
- You need to trace behavior across UI, map state, rendering, and serialization.
- You are unsure whether a change belongs in `src/base/`, `src/file_formats/`, `src/resources/`, or an editor window.

## Relevant folders/files
- `src/main.cpp`
- `src/main_window/`
- `src/base/`
- `src/base/map/`
- `src/base/triggers/`
- `src/file_formats/`
- `src/resources/`
- `src/object_editor/`
- `src/trigger_editor/`
- `src/model_editor/`
- `src/menus/`
- `src/models/`
- `tests/`
- `data/`

## Workflow
1. Start at the entry point:
   - `src/main.cpp` for startup, CASC initialization, theme loading, and first map load.
   - `src/main_window/hivewe.cpp` for top-level user actions and editor window wiring.
2. Identify the owning layer:
   - UI interaction: `main_window`, `menus`, `object_editor`, `trigger_editor`, `model_editor`.
   - Core map state: `src/base/`.
   - Binary/text format parsing: `src/file_formats/` or `src/base/triggers/`.
   - GPU/resource behavior: `src/resources/`, `src/base/render_manager.ixx`, `src/main_window/glwidget.cpp`.
3. If the issue touches map load/save, read `src/base/map/map.ixx` early.
4. If the issue touches Warcraft asset lookup, read `src/base/hierarchy.ixx`.
5. If the issue touches editor tables/tree views, inspect `src/models/` plus the specific editor folder.
6. If the issue touches tests or parser safety, check `tests/` for existing coverage before editing code.

## Coding rules
- Put changes in the subsystem that already owns the behavior.
- Do not introduce new cross-cutting helpers unless the same pattern already exists in multiple places.
- Preserve the split between UI code and file-format/data code.

## Verification
- Confirm the call path from UI action to subsystem before editing.
- Confirm the save/load owner before changing serialized behavior.
- If you moved across more than one subsystem, re-check whether the change belongs closer to the data owner.

## Common mistakes to avoid
- Patching symptoms in `hivewe.cpp` when the real bug is in `Map`, `Hierarchy`, or a file-format parser.
- Treating `.ixx` modules as headers only; many of them are the primary implementation unit.
- Editing editor UI code before checking whether the underlying Qt model already has the needed behavior.
