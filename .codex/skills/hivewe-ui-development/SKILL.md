# hivewe-ui-development

## Purpose
Use this skill when changing HiveWE editor windows, palettes, docking, shortcuts, or Qt model/view behavior.

## When to use this skill
- The task touches the main window, ribbon, palettes, dialogs, docked editors, minimap, object editor, trigger editor, or model editor.
- The task involves Qt signals/slots, `QShortcut`, `.ui` files, or Qt item models.

## Relevant folders/files
- `src/main_window/`
- `src/menus/`
- `src/object_editor/`
- `src/trigger_editor/`
- `src/model_editor/`
- `src/models/`
- `src/custom_widgets/`
- `src/main_window/HiveWE.ui`
- `src/trigger_editor/*.ui`
- `src/object_editor/object_editor.ui`
- `src/menus/*.ui`

## Workflow
1. Find the owning window or palette from the ribbon/menu action in `src/main_window/hivewe.cpp`.
2. Check whether the behavior is driven by:
   - A `.ui` layout file.
   - A Qt model in `src/models/`.
   - A palette/dialog/window class.
3. Reuse existing patterns:
   - `window_handler.create_or_raise<T>()` for reusable windows.
   - `Palette` subclasses for brush/context tools.
   - `QShortcut` setup close to the owning widget.
   - Dock widgets via `ads::CDockManager`.
4. If the UI reflects map/object data, trace the connected `TableModel` or tree/list model before adding local widget state.
5. Keep user-facing errors in Qt dialogs where the surrounding code already does that.

## Coding rules
- Respect existing Qt ownership patterns. If you allocate with `new`, verify parent ownership or explicit cleanup.
- Do not move parsing or resource loading logic into widgets unless the repo already does so in that exact area.
- Keep keyboard shortcuts and signal wiring near the owning UI class.
- Prefer extending current models over building duplicate view-specific state.

## Verification
- Open the affected window/palette/editor.
- Verify shortcuts, focus behavior, docking/tabbing, and open/close behavior.
- If the UI edits map data, verify save and reload still reflect the change.

## Common mistakes to avoid
- Adding hidden widget-local state that diverges from `Map` or Qt models.
- Breaking singleton-like editor behavior by bypassing `window_handler`.
- Forgetting `Qt::WA_DeleteOnClose` or parent ownership when it matters for cleanup.
