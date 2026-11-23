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
- A combo filled from an SLK must be checked for the row-key and first-row pitfalls in `hivewe-file-formats.md`.
- Errors: Qt dialogs where surrounding code already does.
- `ColorButton::changeColor` has no `ShowAlphaChannel`, so a picked colour gets alpha 255 (ticketed as backlog).
- `HiveWE::save_current_map_as` creates the target folder and sets `hierarchy.map_directory` before `Map::save` runs, so a save refusal check must run before the folder picker, as `Map::ensure_unit_ids_fit` does.

- Shortcuts (074f2d1, 2026-11-22): Ctrl+T is Test Map, Ctrl+Shift+W is Wireframe, Ctrl+W is "Show or hide water". `HiveWE.ui` `actionTest_Map` had never been added to a widget, so Ctrl+T used to only toggle Wireframe.
- Layout traps (verified 2026-11-22): a `QStackedWidget` takes the largest `minimumSizeHint` of all pages (wrap tall pages in a `QScrollArea`); a `QComboBox` defaults to its longest item width; `FlowLayout::minimumSize()` is the widest laid-out row unless `set_shrinkable()` (opt-in) is used; `Palette::fit_to_column()` runs for every theme, so a palette layout change needs a check under the old Dark and Light themes too.
- Unit/doodad selection edits (drag, rotate, nudge, undo of `UnitStateAction`) do not emit `Brush::selection_changed`. Read-only views of the selection need their own refresh: the Flat Properties panel polls from the 250 ms status timer, guarded by an address-in-vector check that is safe for read-only displays but not for edits.
- `Unit::health` is a percentage of max (-1 = default). WorldEditStrings `WESTRING_UNITCOLOR_xx` values contain literal quote characters; strip them before display.
- Qt stylesheets cannot target top-level `QTreeWidget` items: build per-item colours from palette tokens in code and rebuild them on `QEvent::PaletteChange`.
- UIA Toggle on a `QPushButton` emits `toggled`, not `clicked`; the sidebar mode buttons use `toggled`.

## Flat themes (074f2d1)

- Data: `data/themes/Flat.tokens` plus `Flat.qss.in`. `src/utilities/theme_tokens.ixx`: an unknown token is an error; `apply_with_fallback` goes Flat -> Dark -> unstyled.
- `src/main_window/theme.cpp`: palette, Inter from `data/fonts`, generated arrow PNGs in the cache dir, and `theme::icon` recolours the SVGs in `data/icons/flat`.
- `src/main_window/flat_chrome.cpp`: menu bar, toolbar forwarding to the ribbon buttons, columns, status bar. The old themes keep the ribbon.
- `save_map_reporting_errors` is the one place that marks the map saved (`WorldUndoManager::mark_saved`).

## Verification

Open affected window/palette; check shortcuts, focus, docking, save/reload if editing map data.
