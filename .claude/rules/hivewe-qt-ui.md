---
description: Qt editor UI patterns for palettes, docking, models, and shortcuts
paths:
  - "src/main_window/**/*.cpp"
  - "src/main_window/**/*.h"
  - "src/main_window/**/*.ui"
  - "src/menus/**/*.cpp"
  - "src/menus/**/*.h"
  - "src/menus/**/*.ui"
  - "src/object_editor/**/*.cpp"
  - "src/object_editor/**/*.h"
  - "src/object_editor/**/*.ui"
  - "src/trigger_editor/**/*.cpp"
  - "src/trigger_editor/**/*.h"
  - "src/trigger_editor/**/*.ui"
  - "src/model_editor/**/*.cpp"
  - "src/model_editor/**/*.h"
  - "src/model_editor/**/*.ui"
  - "src/custom_widgets/**/*.cpp"
  - "src/custom_widgets/**/*.h"
  - "src/custom_widgets/**/*.ui"
---

# HiveWE Qt UI

Qt Widgets + docking (`ads::CDockManager`). Respect `.ui` layout files.

## Patterns

- `window_handler.create_or_raise<T>()` for singleton-like tool windows.
- `Palette` subclasses for brush/context tools.
- Sidebar palettes orchestrated from `src/main_window/hivewe.cpp` — visibility vs active brush are separate.
- Models in `src/models/` — extend models, not duplicate widget state.
- `QShortcut` wiring close to the owning widget.

## Rules

- Verify Qt parent ownership or explicit cleanup for `new` allocations.
- Do not move parsing/resource loading into widgets unless that area already does.
- UX: active-state clarity, predictable toggles, low-noise feedback.
- User-facing values: prefer Warcraft coordinates/IDs over internal engine values.
- Errors: Qt dialogs where surrounding code already does.
- `HiveWE::save_current_map_as` creates the target folder and sets `hierarchy.map_directory` before `Map::save` runs, so a save refusal check must run before the folder picker, as `Map::ensure_unit_ids_fit` does.

## Verification

Open affected window/palette; check shortcuts, focus, docking, save/reload if editing map data.
