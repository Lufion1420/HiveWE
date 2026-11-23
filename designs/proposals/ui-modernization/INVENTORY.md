# HiveWE window and dialog inventory (source read 2026-11-23)

Drawn from `.ui` files and C++ code, not from a running app (no screenshots exist for these windows in `designs/current/`).
`Mockup page` is the file `<page>-flat.html` (open with `#light` for Light); PNGs in `renders/<page>-flat-dark.png` and `-light.png`.

| Window | File | What it does | Mockup page |
|---|---|---|---|
| Main window (ribbon, palettes, viewport) | `src/main_window/hivewe.cpp`, `HiveWE.ui`, `flat_chrome.cpp` | Map viewport, sidebar palettes, docks | `main-window-flat-*` (approved) |
| Object Editor | `src/object_editor/object_editor.cpp`, `.ui` | QMainWindow + ads: left docks per type (Units, Items, Doodads, Destructibles, Abilities, Upgrades, Buffs) with tree, search, All/Custom chips, Reveal/Reset; central "Object Data" dock with summary, bookmarks, section filters, Field/Value table | `object-editor` |
| Object Editor popups | same file | Copy / Add object dialog (Free ID, name), Remove confirm, context menu (Add, Duplicate, Copy, Paste, Remove, Convert to destructible) | `small-dialogs` |
| Trigger Editor | `src/trigger_editor/trigger_editor.cpp`, `.ui`, `trigger_explorer.cpp`, `jass_editor.cpp` | QMainWindow + ads: Trigger Explorer tree (left), trigger tabs (GUI Events/Conditions/Actions tree, or JASS/Lua code editor, comments), Output dock (bottom), toolbar (Generate Script Ctrl+F10, Create Category / GUI Trigger / Jass Trigger / Comment / Variable) | `trigger-editor` |
| Variable editor, Search window | `variable_editor.ui`, `search_window.cpp` | Variable Name, Variable Type, Array, Size, Initial Value; Ctrl+F find box | `trigger-editor` |
| Model Editor | `src/model_editor/model_editor.cpp` | QMainWindow 1280x800, ads tab per open model (GL viewport), "Browse Models" button in the tab bar | `model-editor` |
| Browse Models dialog | `model_editor.cpp`, `object_editor/model_view.cpp` | Search Models, Preview Animation combo, category checks (Abilities ... Units, Map), GL grid, Path field, Open in Model Editor | `model-editor` |
| Settings | `src/menus/settings_editor.ui` | Tabs General / Testing / Script (theme, game data, HD, teen, comments; test launch args; custom tool) | `settings` |
| Unit Properties | `src/menus/unit_properties_dialog.cpp` | Tabs General (player, hit points, mana, level, attributes) and Items Dropped (drop mode, sets, items). No doodad sibling: doodads use only the Selection panel in the main window | `unit-properties` |
| Edit Drop Item | `src/menus/item_drop_editor.cpp` | Chance, random class / level, item | `unit-properties` |
| Map Settings ("Map Info") | `src/menus/map_info_editor.ui` | Tabs Description, Options, Size and Camera Bounds, Loading Screen, Preferences | `map-settings` |
| Scenario Properties | `src/menus/scenario_properties_editor.cpp` | Tabs Players (table) and Forces | `scenario-gameplay` |
| Gameplay Constants | `src/menus/gameplay_constants_editor.cpp` | Search plus a property table of the game constants | `scenario-gameplay` |
| Create New Map | `src/menus/new_map_dialog.cpp` | Map Name, Tileset, Width, Height, live summary, tileset preview | `new-map-file` |
| File menu / Recent Maps | `src/main_window/hivewe.cpp`, `flat_chrome.cpp` | 3 recent slots in a submenu (there is no start page or welcome screen) | `new-map-file` |
| Asset Manager | `src/asset_manager/asset_manager.cpp` | Stats, Rescan, filter (All files / Unused only), File/Type/Usages table, Select all unused, Delete | `tools-dialogs` |
| Map Protector | `src/map_protector/map_protector.cpp` | Source, Output file, hardening groups, Export Protected Map | `tools-dialogs` |
| Import Object Data | `src/menus/object_data_import_dialog.cpp` | Tabs Validation / New Objects / Conflicts, Proceed despite warnings | `tools-dialogs` |
| Item Tables | `src/menus/item_tables_editor.cpp` | Tables, Sets, Entries groups, Assign To Unit Type | `misc-dialogs` |
| Config (shortcut reference) | `src/menus/shortcut_config_dialog.cpp` | Description / Shortcut table | `misc-dialogs` |
| Tile Setter, Tile Pather | `src/menus/tile_setter.ui`, `tile_pather.ui` | Change Tileset (shift/remove/add tiles); Tile Pathing (unwalkable/unflyable/unbuildable, Replace/Add/Remove, Apply Retroactively) | `misc-dialogs` |

## Not mocked up, with reason

- Import Manager: `actionImport_Manager` exists in `HiveWE.ui` but nothing connects to it; no window exists (UNCERTAIN: planned feature).
- Tooltip Editor (`src/tooltip_editor/`) and WC3 colour dialog: rarely used, same widgets as `misc-dialogs`; not drawn.
- Map Protector progress and Item Drop editors share the dialog widgets drawn on `tools-dialogs` and `unit-properties`.
- Palettes, Minimap, Selection panel, Preview: part of the approved main window.
- Tile Picker, Region palette, Pathing palette: docks inside the main window.
- ImGui overlays (debug panel, `src/qt_imgui/`): rendered by the engine, not styled by QSS.

## Gaps and UNCERTAIN

- Exact tree grouping in the Trigger Explorer and the Object Editor trees is drawn from the model code classifiers (category, gui, script, variable, comment), not from a capture.
- Warcraft III icons (BLP/DDS) are shown as coloured placeholder tiles; the app draws the real game icons there.
- Native window title bars are drawn only so the pages read as windows; Qt does not style them.
