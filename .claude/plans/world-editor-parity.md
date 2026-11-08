# HiveWE vs base World Editor 3.0.1.24371: feature parity audit

Audit date 2026-11-08. Base World Editor version 3.0.1.24371. Repo HEAD 541e695. Every HiveWE status below comes from reading code; HiveWE was not run. The base WE side was first read from files and strings, then checked against the running World Editor (menus and dialog controls, tag RUN). Read-only audit: no build, no commit. Companion to `.claude/plans/wc3-3.0-compatibility.md`; its format items are cross-referenced as [FORMAT: name] and not re-filed.

## How to read this

Evidence tags for the base WE side:
- S:n = line n of UI/WorldEditStrings.txt (enUS, extracted from live CASC).
- L:Name = section in UI/WorldEditLayout.txt. It holds the dialog's control layout, so it proves the WINDOW and its controls.
- B:Name = RTTI class or literal string in `_retail_/x86_64/World Editor.exe` (read as bytes, not launched).
- D = patch notes, via `.claude/plans/wc3-3.0-compatibility.md`. MEM = my memory of the classic WE, not confirmed in files.
- RUN = observed in the running World Editor 3.0.1 on 2026-11-08, map NCOW_Spell_Dev.w3x. Menu trees were read with GetMenu/GetMenuItemInfo, windows were opened by posting the menu command IDs, controls were enumerated with EnumChildWindows. No keyboard input, no viewport edits, nothing saved. RUN beats the tags above where they disagree. Captures are not in the repo; the notes cite only what was seen. "Hidden controls" means enumerated but not visually captured.

"CMD ONLY" = strings prove the command, no layout section or class proves its window. Menu grouping before RUN came from string order and the WECOMMAND_ prefixes (for example "Advanced - Replace Tiles", S:10000-10135). RUN has since read the main window's menus: a string without a menu item there (Copy As Text, Delete Invalid Objects, and others below) exists in the exe but has no entry in the main window's menus. That does not prove it is unused elsewhere (module window menus other than Object Editor and Light Editor were not dumped).

HiveWE side:
- All paths are relative to src/.
- COMPLETE = I read the UI wiring and the save or effect path, and found no missing control against the WE evidence. It does not mean tested. HiveWE was not run.
- PARTIAL = says what is missing. MISSING = no UI found by grep.
- NO MENU = the running WE has no menu item for it (RUN), so it is not a HiveWE gap against the main window.
- Status totals (rows in sections 1-9, recounted 2026-11-08 after RUN): 143 rows: 26 COMPLETE, 49 PARTIAL, 61 MISSING, 6 NO MENU, 1 UNKNOWN. Before RUN: 141 rows: 26, 48, 66, 0, 1. Change: 5 rows MISSING to NO MENU; Height Map split out of "Camera Bounds" row (NO MENU) and Preserve Copied Object Height split out of "Reset Fixed Object Heights" row (PARTIAL).
- [INFERRED] marks a runtime consequence or a claim I reasoned out, not observed.
- Screenshots taken during the audit were temporary and not kept. They were used only to confirm the two Map Info tab facts noted below; no status rests on them alone.

Global caveat on every row: maps saved by the 3.0 WE do not load, and saves write 2.0.3-era formats. See [FORMAT: w3i v39], [FORMAT: doo v12/13], [FORMAT: units v12/13], [FORMAT: w3r v7], [FORMAT: w3c v3], [FORMAT: MDX SKIN], [FORMAT: _de layer], [FORMAT: stale meta overrides]. COMPLETE below means "complete for maps HiveWE can load".

## 1. Modules

RUN: the main window's Module menu lists Terrain Editor F3, Trigger Editor F4, Sound Editor F5, Object Editor F6, Campaign Editor F7, AI Editor F8, Object Manager F11, Asset Manager F12, Light Editor (no shortcut). Module windows are separate top-level windows with their own menu bars (File, Edit, Module, Window; Object Manager and Object Editor add View; Trigger Editor adds New and JassHelper). WE hides them on close rather than destroying them.

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Terrain Editor (main window) | S:1705, S:2654, B:CWEMainWindow | PARTIAL | Works as the main viewport. Gaps are itemised in sections 2-7. | main_window/hivewe.cpp, main_window/glwidget.cpp |
| Trigger Editor | S:1706, B:CScriptEditor, L:VariablesDialog | PARTIAL | GUI triggers are read-only; see section 8. | trigger_editor/trigger_editor.cpp:204, 301 |
| Sound Editor | S:1708, S:2807-2830, B:CSoundEditor, L:SoundPropsDialog, RUN F5 (tree Doodads and Units by tileset/race, list columns File Name, Variable Name, Length, play and stop buttons) | MISSING | No window. `Sounds::save` is empty and is never called from Map::save, so war3map.w3s stays as loaded. Sounds are read for script generation only. | base/sounds.ixx:97, base/map/map.ixx:386-458, base/triggers/map_script.cpp:411 |
| Object Editor | S:1709, S:2717-2805, B:CObjectEditor, L:ObjectEditor | PARTIAL | All seven tabs, save paths for base and skin files. Missing commands in section 9. | object_editor/object_editor.cpp:763-801, base/map/map.ixx:411-429 |
| Campaign Editor | S:1707, S:2832-2851, B:CCampaignEditor, L:CampaignEditor, CampaignPaneGeneral, CampaignPaneScreen, CampaignButtonDialog | MISSING | No code. `war3campaign` appears only in Map Protector comments. | map_protector/asset_obfuscation.ixx:295 |
| AI Editor | S:1712, S:2853-2876, B:CAIEditor, L:AIEditor, AIPaneGeneral and related | MISSING | No code, no .wai handling. | grep: no match in src |
| Object Manager | S:1710, S:2661, S:1221-1255, B:CObjectManager, CObjectManagerViewMapSummary, RUN F11: 10 tabs Map Summary, Units, Items, Doodads, Regions, Cameras, Triggers, Functions, Variables, Sounds; no Cross Reference tab (the earlier compatibility plan listed one from strings). Map Summary rows: map name, expansion required, map type, playable size, tileset, unit counts per player and Neutral, Items (max 1024), Doodads (max 130000), Regions, Cameras, Triggers, Variables, Sounds, custom object counts per type. Menu File/Edit/View/Module/Window | MISSING | No equivalent. Global search finds objects; Asset Manager lists file usage. No Map Summary or per-type lists. | object_editor/global_search.cpp, asset_manager/asset_manager.cpp |
| Import Manager / Asset Manager | S:1711, S:2662, L:ImportManager, AssetManager, ImportDialog, B:CAssetManager, RUN F12: it lists game files, not only map imports (status "Total Files: 4047/4047 (379,272/379,272 KB)"). Filters: text plus Mode (ignore, SD, HD, DE), Teen (ignore, No, Yes), Locale (ignore, Unknown, enUS, deDE, esES, frFR, itIT, koKR, plPL, ruRU, zhCN, zhTW, esMX, ptBR), Type (ignore, Other, Model, Image/Texture, Sound/Music, AI Script, AI Data, Text, Preload, Unknown). Columns File Name, Mode, Teen, Locale, Type, Size (KB), Full Path. Opening took about 15 s with the process not responding | PARTIAL | Has search, filter, Rescan, Select All Unused, Delete, Open in Object Editor. Missing Import File, Export File, Export All, Replace, Replace and Reload, Modify Properties (custom path). Imports are found by scanning the folder and written with a fixed flag. | asset_manager/asset_manager.cpp:261-329, 598, 624; base/imports.ixx:55-85 |
| Model Previewer | S:1713, S:2497, B:CWEModelPreview, WECOMMAND_MODULE_MODEL_PREVIEWER; RUN: not in the Module menu or any main-window menu | PARTIAL | HiveWE Model Editor opens models, has a sequence picker. v1400+ models hit [FORMAT: MDX SKIN]. The exe has the class and command, but no menu item opens it in this build; it may be reachable another way (not looked for). | model_editor/model_editor.cpp:55, 89; model_editor/model_editor_glwidget.cpp:201 |
| Dialogue Editor | S:2660, S:10949-10970, B:WESTRING_MODULE_CONVERSATION, "Conversation Editor", conversation.json. No layout section. RUN: no Module menu item or other main-window menu item | MISSING | Exists in exe strings but has no menu item here, so it is not reachable from the main window; it is not a practical gap. conversation.json is only blacklisted from imports. | base/imports.ixx:16 |
| Sound Set Editor | S:10893-10895, B:WECOMMAND_MODULE_SOUND_SET, "Sound Set Config", SoundSetManager art. No layout section. RUN: no Module menu item or other main-window menu item | MISSING | Exists in exe strings but has no menu item here. No HiveWE code. | grep: no match in src |
| Light Editor | B:CEnvironmentLightEditor, WESTRING_MODULE_LIGHT, "Environment Light Data", control panels CLightColorControlPanel etc. RUN: Module > Light Editor (command 1802), no shortcut. Separate window with menus File (map commands), Edit (Create New Unit Light, Create New Terrain Light, Refresh), Module, Window. Left tree: Terrain and Unit roots, each with Internal (23 stock DNC<Tileset>Terrain.mdl / DNC<Tileset>Unit.mdl, Ashenvale, Lordaeron, Dalaran, Dungeon, Felwood, Underground) and Imported (map-supplied light MDLs). Right side: Unit/Terrain/Both combo, Enable Custom Light, Export Current Values, Preview spin edit. Hidden controls: two Color R/G/B groups with keyframe control, three scalar Value editors, a Direction x/y picker. The root counts ("Unit (0)" with children) are unexplained | MISSING | The patch notes' "lighting editor" (D) is this window. Per-light panels were not reached (nothing selected), so property names are unknown. Data file still inferred: war3map.w3l [INFERRED: stock w3l names two .mdl light models; not shown in the UI]. | grep: no match in src |
| Object Group Editor | B:CObjectGroupEditor, CWorldObjectGroupPropertiesDialog, CWorldObjectGroupsManager; S:1920, S:1935, S:2203. RUN: no Module menu item or other main-window menu item | MISSING | NEW in this audit. war3map.w3grp is in the exe's file list, so the UNKNOWN in `.claude/plans/wc3-3.0-compatibility.md` is probably object group data, and doodad/unit group_id probably points at it [INFERRED]. The classes exist in the exe but no menu opens an editor here; it may be reached from object properties (not looked for). | grep: no match in src |
| Cinematic Editor | D only (wiki). B has CinematicPreviewAction only, no editor class or module string. RUN: no menu entry anywhere in the main window | UNKNOWN | Still unconfirmed as a feature, but it has no menu entry in this build. No class named so, so probably not a separate window [INFERRED]. | n/a |

## 2. File menu

RUN: the File menu has exactly the items below plus a recent-maps list and Exit; shortcuts Ctrl+N, O, W, S, F9 (Test Map). Object Editor and Light Editor File menus repeat the map commands.

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| New Map | S:2499, L:NewMapDialog | PARTIAL | Tileset, width, height. Missing cliff type, water and random height controls, and it starts from the bundled template, so that map's war3mapMap.blp and war3map.mmp carry over [INFERRED]. | main_window/hivewe.cpp:863, menus/new_map_dialog.cpp:171-185 |
| Open Map | S:2500, L:OpenDocumentDialog | PARTIAL | Folder and MPQ (MPQ unpacks to a folder first). 3.0 maps: [FORMAT: w3i v39]. | main_window/hivewe.cpp:634, 668 |
| Recent maps (HiveWE list) | MEM | COMPLETE | Read wiring and path check. | main_window/hivewe.cpp:204, 613 |
| Close Map | S:2501 | MISSING | No command. Replaced by opening another map. | main_ribbon.cpp:256-319 |
| Save Map | S:2502 | PARTIAL | Saves to a folder. Does not refresh minimap or shadows, and regenerated script loses data (row "Script generation" below). | main_window/hivewe.cpp:743, base/map/map.ixx:386 |
| Save Map As | S:2503 | PARTIAL | Folder only; .w3x through separate Export MPQ. | main_window/hivewe.cpp:762, 809 |
| Calculate Shadows and Save | S:2504, WECOMMAND_RESETSHADOWMAP | MISSING | shadow_map is loaded, resized, saved, never computed. war3map.shd goes stale after placing objects [INFERRED]. | base/shadow_map.ixx:28, 49; base/map/map.ixx:409 |
| Export Script | S:2505 | PARTIAL | "Generate Script" checks the script and writes it into the map folder; no export to a chosen file. | trigger_editor/trigger_editor.cpp:83-92 |
| Export Minimap | S:2506, L:ExportMinimapDialog | MISSING | Minimap only rendered in the viewport widget. war3mapMap.blp and war3map.mmp are never written. | base/terrain.ixx:865, base/imports.ixx:22, 40 |
| Export Strings / Import Strings | S:2507-2508, WECOMMAND_EXPORTSTRINGS | MISSING | wts round-trips but has no UI. | base/trigger_strings.ixx:61 |
| Preferences | S:2509, L:PrefsDialog, Prefs_General, Visual, Triggers, MapFile, TestMap, Video, Sound | PARTIAL | Theme, game data, HD, teen, comments, test options. Missing undo limit, autosave, grid colors, palette options, video, sound. The Script tab checkboxes and Custom Tool appear unwired: no setting for them is read or stored in settings_editor.cpp [INFERRED from grep]. | menus/settings_editor.cpp:44-83, menus/settings_editor.ui:304-428 |
| Configure Controls | S:2510, L:ConfigControlsDialog | PARTIAL | HiveWE "Config" is a read-only reference table; no rebinding. | menus/shortcut_config_dialog.cpp:21 |
| Test Map | S:2511, L:Prefs_TestMap | COMPLETE | Saves then launches with -loadfile and configured arguments. Launch on 3.0 not run. | main_window/hivewe.cpp:846 |
| Exit | S:2512 | COMPLETE | Prompts through closeEvent. | main_window/hivewe.cpp:228 |
| Script generation (what Save produces, not a menu item) | B war3map.j; stock Warchasers war3map.j:12421-12441, 7000; Silverpine war3map.lua:46977, 48820 | PARTIAL | HiveWE emits no InitTechTree, InitUpgrades, InitRandomGroups, WaygateSetDestination or SetUnitColor. random_type, waygate and custom_color round-trip in war3mapUnits.doo but nothing emits them. gold is never emitted (SetResourceAmount: MEM, no stock script proves it). custom_light_tileset and custom_sound_environment are ignored and line 722 reads TerrainLights for the unit lights. | base/triggers/map_script.cpp:161-263, 721-726, 833-865; base/units.ixx:215-226, 305 |

## 3. Edit menu

RUN: the Edit menu has Undo, Redo, Cut, Copy, Paste (Ctrl+X, C, V), Mirror Paste Vertically and Horizontally (Ctrl+NumPad + and -), Rotate Paste 90 CW and CCW (Ctrl+NumPad * and /), Clear (Delete), Select All (Ctrl+A), Select All Special (Ctrl+Shift+~), Nudge Selection submenu, View Selection submenu, Edit Properties (Enter). Not present: Copy As Text, mirror or rotate selection, Move Up/Down, Select Next/Previous. Those strings exist in the exe but have no Edit menu item, hence NO MENU below.

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Undo / Redo | S:2515-2516, WECOMMAND_MODULE_UNDO | PARTIAL | World edits have undo modules. Item Tables and Scenario Properties edits have no undo; whether WE undoes those is UNKNOWN. | main_window/hivewe.cpp:113, 130; base/world_undo_manager.ixx |
| Cut, Copy, Paste, Clear | S:2517-2525 | PARTIAL | Ctrl+X, C, V, Delete in terrain, doodad and unit brushes. No region copy. Keyboard only, no menu. | brush/brush.cpp:123-150, brush/region_brush.h:32-33 |
| Copy As Text | S:2519; RUN: no such Edit menu item | NO MENU | Was MISSING before RUN. String exists, no menu item in the main window. | grep: no match |
| Mirror and rotate paste | S:2521-2524, RUN: Edit menu items with Ctrl+NumPad + - * / | MISSING | Confirmed real. | grep flip, mirror in brush/: no match |
| Select All / Select All Special | S:2526-2527, RUN: Ctrl+A and Ctrl+Shift+~ | PARTIAL | A key in doodad and unit brushes; WE uses Ctrl+A. No Special dialog. | brush/doodad_brush.cpp:166, brush/unit_brush.cpp:71 |
| Edit Properties | S:2528 | PARTIAL | Double click opens units only; doodads do nothing. | main_window/glwidget.cpp:317, 327-331 |
| View Selection | S:2530 | PARTIAL | Region list double click centres the camera; nothing for units and doodads found. | menus/region_palette.cpp:108-125 |
| Cycle Main View | S:2531-2532, RUN: Edit > View Selection submenu, ~ and Shift+~ | MISSING | | grep: no match |
| View In Object Manager | S:2533 | MISSING | No Object Manager. | n/a |
| View In Object Editor | S:2534 | COMPLETE | "Edit in OE" on doodad and unit selection. | menus/doodad_palette.cpp:283, menus/unit_palette.cpp:86, 144 |
| Select In Tool Palette | S:2535 | COMPLETE | | menus/doodad_palette.cpp:287, menus/unit_palette.cpp:90 |
| Nudge submenu | S:2537-2553, RUN: Edit > Nudge Selection. Move by NumPad 4/6/8/2 and diagonals 7/1/9/3; Rotate Left/Right NumPad / and *; Scale Up/Down NumPad + and -; Scale XY Home/End; Scale Z Pg Up/Pg Down | PARTIAL | Move and scale through keys in the brushes. No menu; whether HiveWE has XY and Z scale keys is not confirmed. | brush/doodad_brush.cpp:130-133, 175-210; brush/unit_brush.cpp:54-57 |
| Mirror / rotate selection | S:2555-2559; RUN: no such Edit menu item (only Nudge Selection Rotate Left/Right, covered above) | NO MENU | Was MISSING before RUN. Doodads can rotate in place and take an angle; no 90/180 group rotate, no mirror. | brush/doodad_brush.cpp:349, 721 |
| Move Up/Down, Select Next/Previous in list | S:2561-2564; RUN: no such Edit menu item | NO MENU | Was MISSING before RUN. Strings exist, no item in the main window (module windows' Edit menus other than Object Editor and Light Editor not dumped). | grep: no match |

## 4. View menu

RUN: the View menu has, in order: Terrain submenu (Ctrl+T: Textured, Wireframe, Hidden), Foliage, Doodads, Units, Water (W), Blight (H), Shadows (S), Lighting (L), Weather (E), Sky (K), Fog Effects (F), Light Ranges, Bloom (Q), Ambient Occlusion (O), Grid submenu (G: None, Large, Medium, Small), Camera Bounds (B), Pathing Ground (P), Pathing Naval (N), Info Display (I), Regions (Ctrl+R), Camera Objects (Ctrl+M), Lock to Game View Settings (V), Lock to Game Camera (C), Letterbox Mode (X), Snap to Game Camera, Free Camera Mode (Alt+C), Preview DoF (Shift+C). There is no Height Map item, and Cameras is under Layer, not View.

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Wireframe | S:2567, RUN: View > Terrain submenu | COMPLETE | Toggle changes the polygon mode. | main_window/hivewe.cpp:158, base/map/map.ixx:538 |
| Doodads, Units, Water, Lighting, Pathing Ground | S:2572-2580 | COMPLETE | Toggles read in Map::render. | main_window/hivewe.cpp:151-156, base/map/map.ixx:540-580 |
| Textured, Hidden terrain, Terrain toggle | S:2566-2570, RUN: View > Terrain submenu (Textured checked, Wireframe, Hidden) | MISSING | No terrain visibility flag. | base/map/map.ixx:81-89 |
| Foliage, Info Display, Blight, Pathing Naval, Shadows, Weather, Sky, Fog Effects, Light Ranges | S:2571-2594, WECOMMAND_TOGGLE_SHOWLIGHTRANGES | MISSING | No flag for any. Blight paints but has no toggle. Rendering of sky, weather and fog in the viewport not found by grep. | base/map/map.ixx:81-89 |
| Grid (None, Large, Medium, Small) | S:2582-2586, RUN: View > Grid submenu, G key, None checked | MISSING | | grep: no match |
| Camera Bounds, Camera Objects | S:2587-2591, RUN: View items (B, Ctrl+M) | MISSING | Bounds drawn only as a preview in Map Size tab. | menus/map_info_editor.cpp:454-460 |
| Height Map | S:2587-2591; RUN: not in the View menu | NO MENU | Split from the row above after RUN; was MISSING. | n/a |
| Regions | S:2589 | PARTIAL | Drawn while the region brush is active; no toggle [INFERRED from RegionBrush render_brush]. | brush/region_brush.h:30-31 |
| Lock to Game View, Lock/Snap to Game Camera, Letterbox | S:2595-2598 | MISSING | | grep game_camera, letterbox: no match |
| Free Camera Mode, Preview DoF, Bloom, Ambient Occlusion | S:11149-11151, WECOMMAND_TOGGLE_FREECAMERAMODE. RUN: all four are View menu items: Bloom (Q), Ambient Occlusion (O, checked), Free Camera Mode (Alt+C), Preview DoF (Shift+C) | MISSING | | grep: no match |

## 5. Layer menu and palettes

Evidence for layers: S:2489, S:10151-10155 lists Terrain, Units, Doodads, Regions, Cameras. S:1716-1721 lists six palettes: Terrain, Pathing, Doodad, Unit, Region, Camera. RUN: the Layer menu has Terrain (T), Doodads (D), Units (U), Regions (R), Cameras (M); no Items layer. Window > New Palette offers Terrain, Units, Doodads, Regions, Cameras (no Pathing). There is no Item palette string. Items seem to live in the Unit palette (B:CUnitToolPalette, L:UnitItemPalette is for drops; placement MEM).

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Layer switch Terrain, Doodads, Units, Regions | S:10151-10154 | COMPLETE | T, D, U, R keys and ribbon. | main_window/hivewe.cpp:243-256 |
| Layer switch Cameras | S:10155 | MISSING | No camera palette. | n/a |
| Terrain palette: tile textures, blight | L:TilesetDialog palette, S:2570, B:CTerrainPalette, WorldEditData MiscBrushes | COMPLETE | Texture brush with blight button; saved through Terrain::save. | menus/terrain_palette.cpp:269, 634-640; base/terrain.ixx:542 |
| Terrain palette: cliffs, ramps, water levels, height brushes | WorldEditData CliffBrushes, HeightBrushes | COMPLETE | Raise, lower, plateau, ripple, smooth; cliff levels, deep and shallow water, ramp, cliff type buttons. | menus/terrain_palette.cpp:298-360, 421-441 |
| Terrain palette: boundary paint | WorldEditData MiscBrushes Nothing, Unnothing | PARTIAL | Add and remove boundary present. Alpha tile brush (3.0) absent; storage unknown in `.claude/plans/wc3-3.0-compatibility.md`. | menus/terrain_palette.cpp:393-398 |
| Terrain palette: Alpha brush | WorldEditData MiscBrushes Alpha, S:1802 | MISSING | NEW 3.0. | grep: no match |
| Pathing palette | S:1717, WorldEditData PathingBrushes | COMPLETE | Replace, add, remove, masks, size, import and export pathing (more than WE). Saved. | menus/pathing_palette.cpp:78-91, base/pathing_map.ixx:68 |
| Doodad palette | S:1718, B:CDoodadToolPalette | PARTIAL | List, variation, random rotation, placement. Random scale is on/off: Symmetric, Asymmetric, Z and XY modes (S:2626-2631) missing. Doodad lights and new 3.0 properties have no UI. | menus/doodad_palette.cpp:163-192, brush/doodad_brush.cpp:506 |
| Unit palette | S:1719, B:CUnitToolPalette | PARTIAL | Player filter, placement, selection. No random unit placement [INFERRED: no code writes random_type except default]. | menus/unit_palette.cpp, base/units.ixx:401 |
| Item placement | L:UnitItemPalette (drop picker), MEM for placement | MISSING | Palette lists units only and UnitBrush has no item path. `units.items` is loaded, rendered, saved and scripted, but nothing in brush/ touches it, and picking uses units.units only [INFERRED: placed items cannot be added, selected or moved]. | brush/unit_brush.cpp, base/render_manager.ixx:363, base/units.ixx:238, base/triggers/map_script.cpp:269 |
| Region palette | S:1720, B:CRegionPalette | PARTIAL | New, delete, move, resize, name, color. Weather and sound missing (next row). | menus/region_palette.cpp:64-65, 102-105; brush/region_brush.h:12-21 |
| Camera palette | S:1721, S:1669-1671, S:1698-1703, S:11148, B:CCameraPalette | MISSING | No palette, no create or edit. `GameCameras::save` is empty, so w3c is preserved but never changed. Cameras can only be deleted indirectly by map resize. | base/game_cameras.ixx:64, base/map/resize.cpp:119-135 |

### Placed object property dialogs

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Unit Properties | L:UnitPropertiesDialog, B:CUnitPropertiesDialog | PARTIAL | Has owner, facing, HP, mana, level, hero attributes, item drops with apply to all. Missing skin, acquisition range, gold, waygate active and region, custom color, abilities list, inventory slots, random unit controls. Fields round-trip in the file but cannot be edited. Multi-select not offered. | menus/unit_properties_dialog.cpp:289-290, 325-331, 346-367, 391-451, 625-698; base/units.ixx:48-62 |
| Doodad Properties | L:DoodadPropsDialog, B:CDoodadPropsDialog, S:750, S:11158-11167 | MISSING | No dialog. Sidebar edits angle, height, scale only. Variation, life, drops, color, skin, lights absent. `item_table_pointer` and `item_sets` round-trip and are scripted but have no UI. | main_window/glwidget.cpp:327-331, menus/doodad_palette.cpp:226-268, base/doodads.ixx:143-145 |
| Region Properties | L:RegionPropsDialog, B:CRegionPropertiesDialog | PARTIAL | Name and color in sidebar. No coordinate edits, no weather or sound control: `set_selected_weather_id` and `set_selected_ambient_id` exist and nothing calls them. Camera Blocker and Alpha Tile Minimap Color [FORMAT: w3r v7]. | brush/region_brush.cpp:73, 80; menus/region_palette.cpp:64-65 |
| Camera Properties | L:CameraPropsDialog, B:CCameraPropertiesDialog | MISSING | | n/a |

## 6. Scenario menu

RUN: the Scenario menu has Map Description, Map Options, Map Water, Map Size and Camera Bounds, Map Loading Screen, Map Preferences (no Map Prologue Screen), then Player, Ally Priorities, Force, Techtree, Ability and Upgrade Properties.
- Map Properties is ONE dialog (530x730) with 6 tabs: Description, Options, Water, Size and Camera Bounds, Loading Screen, Prefs. The first six menu items open it on the matching tab. No Prologue tab. Buttons: Reset <tab> to Defaults, OK, Cancel.
- Scenario Properties is ONE dialog (861x714) with 6 tabs: Players, Ally/Enemy Priorities, Forces, Techtree, Abilities, Upgrades. Player Properties and Force Properties open it on the matching tab. Only Players and Forces were captured.

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Map Description | S:2600, L:MP_Description, RUN: Name, Suggested Players, Description, Author, Expansion Required (Yes plus reasons), Map Version, Saved with Editor Version | PARTIAL | Name, suggested players, description, author, versions, saved to w3i. Missing Expansion Required display and Reset to Defaults. Ribbon opens correct tab. | menus/map_info_editor.cpp:19-25, 197-200; main_window/hivewe.cpp:233 |
| Map Options | S:2601, L:MP_Options, L:FogControls, RUN fields: Hide minimap in preview screens, Masked areas are partially visible, Show water waves on cliff shores and on rolling shores, Use item classification system, Use accurate probability for calculations, Use custom ability skin, Disable deny icon, Alpha Tile default minimap color (Set Color), Dynamic Minimap, Use Terrain Fog (6 styles: Linear, Exponential 1, Exponential 2, Height, New Exponential 1, New Exponential 2; Z Start, Z End, Density, Draw Fog Over Sky, Height Start/End, Linear Start/End, Max Opacity, Fog Color), Use Global Weather (48 entries), Custom Sound Environment (4, names show as WESTRING_SNDENV_* placeholders), Custom Light Environment (23 tilesets), Game Data Set (Default based on map melee status, Custom (1.01), Melee (Latest Patch)), Game Data Version (The Frozen Throne, Forsaken Kingdom), Script Language (JASS, Lua), Supported Modes SD/HD/DE, Force default/min/max camera zoom (3 values, 2500/2500/3000 here) | PARTIAL | Six flags, fog (3 styles), weather, sound, light, game data set, tint color. Missing: accurate probability, custom ability skin, disable deny icon (read and written, no UI), Dynamic Minimap, Game Data Version, Script Language, Supported Modes SD/HD/DE, camera zoom overrides, 3.0 fog styles and fields, Alpha minimap color. RIBBON BUG: the "Options" button opens index 2, "Size and Camera Bounds" (seen in a temporary screenshot, not kept). Water tint is read at line 79 and written at 256, but `water_tinting` is not in the script. Whether the game applies w3i fog and weather without script is UNKNOWN. | menus/map_info_editor.cpp:66-124, 222-257; main_window/hivewe.cpp:235; menus/map_info_editor.ui:122, 378; base/map_info.ixx:118-125 |
| Map Water | S:2602, L:MP_Water, RUN: Water tab exists in Map Properties (controls not captured) | MISSING | Tint is on Options tab; no HD Water Settings group. [FORMAT: w3i v39 fields]. | n/a |
| Map Size and Camera Bounds | S:2603, L:MP_Size | COMPLETE | Arrow controls, both bound types, reset, preview, map resize on OK. No direct ribbon button, reached through the misrouted Options button. | menus/map_info_editor.cpp:145-177, 260-310, 465-546; base/map/resize.cpp |
| Map Loading Screen | S:2604, L:MP_LoadScreen, RUN: Loading Screen tab (hidden controls: campaign combo 121 entries, imported model combo, race combo Human, Orc, Night Elf, Undead, Forsaken) | PARTIAL | Default, campaign and imported, title, subtitle, text. Missing Crest (S:1649). Imported list takes .mdx only. Campaign combo filled twice, so entries are duplicated. RIBBON BUG: button opens index 1, Options (seen in a temporary screenshot, not kept). | menus/map_info_editor.cpp:28-33, 35-42, 44-49, 202-215; main_window/hivewe.cpp:234 |
| Map Prologue Screen | S:2605, L:MP_Prologue; RUN: no Scenario menu item and no tab in Map Properties | NO MENU | Was MISSING before RUN. Fields round-trip with no UI in HiveWE; the real 3.0.1 WE shows no prologue UI either; code comment "Prologue?". | menus/map_info_editor.cpp:219, base/map_info.ixx:135-138 |
| Map Preferences | S:2606, L:MP_Prefs, RUN: Prefs tab (hidden controls: sky combo 24 entries incl. Arcane Sky; time-of-day combo Use Editor Preference, Not Fixed, Fixed - Dawn/Noon/Dusk/Midnight) | MISSING | Tab exists and holds only a "?" label. No sky, time of day, terrain limits, external data. | menus/map_info_editor.ui:1322-1336 |
| Player Properties | S:2607, S:182-186, L:SD_Players, S:11426, RUN: Players tab, 24 rows with columns # / Player Name / Color / Race / Controller / Fixed Start Location / HUD Skin, plus Reset Players to Defaults. Race options: Human, Orc, Undead, Night Elf, Selectable. HUD Skin options: Selected Race, Human, Orc, Undead, Night Elf, Forsaken (enabled only on User rows). Controller options (Player 5, currently Neutral): None, User, Computer, Neutral, Rescuable | PARTIAL | Name, color display, race, controller, fixed start, reset. Saved through MapInfo::save and scripted. Missing HUD Skin (3.0) and the Selectable race entry [check HiveWE's race list; not re-read]. CORRECTION: the earlier guess of a Forsaken race option is wrong; Forsaken appears only as a HUD Skin (and in the Loading Screen race combo). Controller "None" is written as neutral; WE has both Neutral and None (confirmed by RUN), so there is no way to pick Neutral vs an unused slot. RISK: opening the dialog pads to 24 slots and saves them, giving SetPlayers(24) and start-location indexes that count all slots [INFERRED]; compare a real WE save. | menus/scenario_properties_editor.cpp:161-192, 254-284, 332-338; base/map_info.ixx:456-470; base/triggers/map_script.cpp:471-509, 752 |
| Force Properties | S:2608, L:SD_Forces, RUN: Forces tab with Use Custom Forces, Fixed Player Settings, force tree, Add/Remove/Rename Force, grid Allied, Allied Victory, Share Vision, Share Unit Control, Share Adv. Unit Control | PARTIAL | Custom forces, fixed settings, tree with drag, add, remove, rename, five flags, saved and scripted. Missing only a Reset to Defaults (L:ScenarioDialog Reset); opening the dialog also edits the map. No undo. | menus/scenario_properties_editor.cpp:345-485, 547-637; base/map_info.ixx:470-479; base/triggers/map_script.cpp:603 |
| Ally Priorities | S:2609, L:SD_AllyPri, B:CAllyPrioritiesPane, RUN: "Ally/Enemy Priorities" tab (contents not captured) | MISSING | Data saved and scripted, flag exported in scenario data, no UI. | base/map_info.ixx:464-467, base/triggers/map_script.cpp:663, utilities/scenario_data_io.ixx:115 |
| Techtree Properties | S:2610, L:SD_Techtree, B:CTechtreePane, L:TechItemDialog, RUN: Techtree tab (contents not captured) | MISSING | available_tech round-trips but no UI and no script emission (stock emits it). | base/map_info.ixx:490-493, base/triggers/map_script.cpp (no match) |
| Ability Properties | S:2611, B:CAbilitiesPane, L:AbilityDialog | MISSING | Same. | same |
| Upgrade Properties | S:2612, L:SD_Upgrades, L:UpgradeDialog, B:CUpgradesPane | MISSING | available_upgrades round-trips, no UI, no script emission. | base/map_info.ixx:482-487 |
| Post Processing | S:11174-11175, S:11176-11209, L:PostProcessingDialog, B:CPostProcessingDialog, WECOMMAND_MODIFY_POSTPROCESSING, RUN: Advanced > Post Processing..., one dialog (514x596), no tabs. Enable Bloom (base intensity, base saturation, intensity, saturation, threshold, blur amount, blur sample count, plus portrait variants), Enable Tonemap (exposure), Override Ambient Occlusion Settings (radius, shadow multiplier, power, clamp, horizon angle threshold, fade out from and to, adaptive quality limit, sharpness, temporal super sampling angle and radius offset, detail shadow strength, portrait detail shadow strength, blur pass count, quality level) | MISSING | NEW 3.0. Storage is `war3mapPostProcessing.txt` (literal in the exe), which closes the UNKNOWN in `.claude/plans/wc3-3.0-compatibility.md`; format itself not inspected. Not in Imports::blacklist, so HiveWE treats it as an import. It sits in the Advanced menu, not Scenario (the section 6 placement is historical). | base/imports.ixx:15-52 |

## 7. Tools, Advanced, Window, Help

RUN menu contents. Tools: Selection Brush (Space), Brush Size submenu (1, 2, 3, 5, 8; Ctrl+1 to Ctrl+5), Brush Shape submenu (Circle Ctrl+[, Square Ctrl+]), Randomly Rotate Doodads (checked), Randomly Scale Doodads submenu (Off, Symmetric, Asymmetric, Asymmetric Z-axis only, Asymmetric XY-axis only), Reset Random Brush (Z), Set Uniform Doodad Heights (Y). Tools has NO Round, Plateau, Noise or Smooth entries; those WE strings are probably terrain palette tools (not verified). Advanced: Modify Tileset, Random Groups, Item Tables, Gameplay Constants, Game Interface, Post Processing, Adjust Map String IDs, Reset Height Field, Adjust Cliff Levels, Replace Tiles, Replace Cliff Type, Replace Doodads, Replace Units, View Entire Map, Reset Texture Variations, Cycle Random Groups Next/Previous (Ctrl+= and Ctrl+-), Auto Cycle Random Groups (Ctrl+G, checked), Enforce Water Height Limits (Ctrl+Shift+W), Enforce Camera Bounds (Ctrl+Shift+B, checked), Reset Fixed Object Heights (Ctrl+Shift+H), Preserve Copied Object Height (Ctrl+Shift+Z, checked). There is NO Debug menu: the top bar is File, Edit, View, Layer, Scenario, Tools, Advanced, Module, Window, Help. Window: New Palette submenu, Show Palettes (A), Toolbar, Minimap, Previewer, Brush List, then the open map. Help: Warcraft III World Editor Help, About (no License item).

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Selection Brush | S:2614 | COMPLETE | Ribbon toggle plus Space in each palette. | menus/terrain_palette.cpp:452, 535 |
| Reset Random Brush | S:2615, RUN: Tools, Z key, grayed here | MISSING | | grep: no match |
| Set Uniform Doodad Heights | S:2616 | COMPLETE | Group Height min, average, max. | menus/doodad_palette.cpp:268, 423-425 |
| Round, Plateau, Noise, Smooth brushes | S:2617-2620; RUN: no Tools menu items for these | COMPLETE | Correction: the strings are not Tools menu entries in 3.0.1; status kept because HiveWE has the equivalent terrain palette buttons, but the WE side may be palette tools only (unverified). Same as terrain palette row. | menus/terrain_palette.cpp:421-441 |
| Brush Size, Brush Shape | S:2621-2624, RUN: Tools submenus, sizes 1, 2, 3, 5, 8 (Ctrl+1 to 5), shapes Circle and Square (Ctrl+[ and ]) | COMPLETE | Slider, =, -, circle, square, plus diamond. | menus/terrain_palette.cpp:570, 587-595; brush/brush.cpp:126-129 |
| Randomly Rotate Doodads | S:2625 | COMPLETE | | menus/doodad_palette.cpp:166 |
| Randomly Scale Doodads modes | S:2626-2631, RUN: Tools submenu Off, Symmetric (default checked), Asymmetric, Asymmetric Z-axis only, Asymmetric XY-axis only | PARTIAL | On/off only. | menus/doodad_palette.cpp:174, brush/doodad_brush.cpp:506 |
| Modify Tileset | S:2633, L:TilesetDialog | PARTIAL | Add, remove, order, replace on remove. Missing Space Used and Left readouts and Use Custom [INFERRED from layout]. | menus/tile_setter.cpp:70-78, 150-155, 209-229 |
| Random Groups | S:2634, L:RandomGroupsDialog, RandomSetDialog, B:CRandomGroupsDialog | MISSING | random_unit_tables round-trips with no UI and no InitRandomGroups emission. | base/map_info.ixx:163, base/triggers/map_script.cpp:845 |
| Item Tables | S:2635, L:ItemTablesDialog, L:ItemTable, B:CItemTablesDialog | PARTIAL | Tables, sets, entries, drop editor with random classes, deletion clears references, saved and scripted. Missing Equipment class (S:11169, WESTRING_ITEMCLASS_EQUIPMENT) and tag and equipment type filters (L:UnitPropertiesDialog RandItemEquipmentTypeMenu, RandItemTagMenu, UnitItemPalette). Tables can be assigned to units, not doodads. | menus/item_tables_editor.cpp:174-208, menus/item_drop_editor.cpp:27-36, base/map_info.ixx:507-513, base/triggers/map_script.cpp:556-600 |
| Reset Height Field | S:2636, B:CResetHeightDialog | MISSING | | grep: no match |
| Adjust Cliff Levels | S:2637 | MISSING | CMD ONLY. | grep: no match |
| Replace Tiles | S:2638, L:ReplaceTilesDialog | PARTIAL | Only as a side effect of removing a tile from the tileset. | menus/tile_setter.cpp:219-224 |
| Replace Cliff Type | S:2639, L:ReplaceCliffTypeDialog | MISSING | | grep: no match |
| Replace Doodads | S:2640, L:ReplaceDoodadsDialog | MISSING | | grep: no match |
| Replace Units | S:2641, L:ReplaceUnitsDialog | MISSING | | grep: no match |
| Gameplay Constants | S:2642, L:MiscDataDialog, B:CMiscDataDialog | PARTIAL | Searchable table, saved to war3mapMisc.txt. Six 3.0 rows absent [FORMAT: stale meta overrides]. No reset all. | menus/gameplay_constants_editor.cpp:16-65, base/gameplay_constants.ixx:64-71, base/map/map.ixx:451 |
| Game Interface | S:2643, B:CSkinDialog, L:SkinDialog, S:821, RUN: Advanced > Game Interface, dialog 1022x675 with "Use Custom Game Interface" checkbox, Name/Value list (for example "Icon - Attack Type Chaos" = a .blp path), Edit Value, Reset Value, Display Values As Raw Data, OK, Cancel | MISSING | war3mapSkin.txt preserved, never edited. | base/imports.ixx:43 |
| View Entire Map | S:2644 | PARTIAL | Reset camera only. Full-map framing not confirmed. | main_window/hivewe.cpp:190 |
| Reset Texture Variations | S:2645 | MISSING | | grep: no match |
| Cycle / Auto Cycle Random Groups | S:2646-2648 | MISSING | | grep: no match |
| Enforce Water Height Limits | S:2649 | COMPLETE | | menus/terrain_palette.cpp:461, brush/terrain_brush.h:27 |
| Enforce Camera Bounds | S:2650 | MISSING | | grep enforce_camera: no match |
| Reset Fixed Object Heights | S:2651, RUN: Advanced, Ctrl+Shift+H | PARTIAL | "Update Doodad Z" covers part; not the same command [INFERRED]. | menus/terrain_palette.cpp:468 |
| Preserve Copied Object Height | S:2652, RUN: Advanced, Ctrl+Shift+Z, a checkable toggle (checked here) | PARTIAL | Split from the row above after RUN. Lock Height covers part; not the same command [INFERRED]. | menus/doodad_palette.cpp:182 |
| Delete Invalid Objects | S:2675; RUN: no Debug menu and no such item in any main-window menu | NO MENU | Was MISSING before RUN ("Debug menu grouping" was a wrong inference). | grep: no match |
| Adjust Map String IDs | S:2676, S:11210-11214, B:CMapStringsDialog | MISSING | | grep: no match |
| Validate Map Triggers | S:10861; RUN: no Debug menu; not in the main window menus (Trigger Editor menu items not dumped, so it may sit there) | PARTIAL | Generate Script reports JassHelper errors; not a dedicated check. | trigger_editor/trigger_editor.cpp:83, base/triggers/map_script.cpp:873-888 |
| New Palette, Show Palettes | S:2664-2665 | PARTIAL | One sidebar per palette, toggled. No extra floating copies. | main_window/hivewe.cpp:243-256, 1595-1647 |
| Toolbar | S:2666 | PARTIAL | Ribbon has no hide command found. | main_window/main_ribbon.cpp |
| Minimap | S:2667 | COMPLETE | Toggle, click to move. Display only; file output in section 2. | main_window/hivewe.cpp:160, 316 |
| Previewer | S:2668 | COMPLETE | Doodad and unit preview in sidebar. | main_window/hivewe.cpp:1629, 1638 |
| Brush List | S:2669 | PARTIAL | Lists are per palette; no separate toggle. | menus/doodad_palette.cpp, menus/unit_palette.cpp |
| Help, License, About | S:2671-2673, L:AboutDialog, L:WelcomeDialog | MISSING | Low value. | main_ribbon.cpp:256-319 |

## 8. Trigger Editor commands

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| New Category, Trigger, Comment, Custom Script | S:2696-2700, S:2325-2328 | PARTIAL | Created through toolbar and context menu. A new GUI trigger cannot get content (next row). | trigger_editor/trigger_explorer.cpp:36-57, 149-277 |
| New Event, Condition, Action, and editing them | S:2703-2705, L:none; B:CTriggerFunctionDialog, CTriggerParameterDialog family | MISSING | GUI trigger opens as a QTreeWidget filled by show_gui_trigger. No edit or context connection found; display only. Round-trip and conversion to script exist. Biggest functional gap. | trigger_editor/trigger_editor.cpp:200-206, 301-359; base/triggers/gui.cpp:415; base/triggers/triggers.ixx:825 |
| New Lua Script | S:2701, S:2329 | MISSING | No Lua trigger command; `info.lua` has no setter in UI. | base/map_info.ixx:262, trigger_editor/trigger_editor.cpp:87 |
| Variables dialog and New Global Variable | S:2692, S:2702, L:VariableDialog, VariablesDialog, B:CGlobalVariablesDialog | PARTIAL | Each variable opens as a tab. Type is free text, not a list of types. Initial value is free text. No combined dialog. Saved when the tab closes or at save. | trigger_editor/variable_editor.cpp:3-11, trigger_editor/trigger_editor.cpp:285-296, trigger_editor/trigger_explorer.cpp:245 |
| Enable Trigger, Initially On, Run on Init | S:2684-2685 | COMPLETE | | trigger_editor/trigger_explorer.cpp:105, 119, 133 |
| Enable Function | S:2686 | MISSING | Needs GUI editing. | n/a |
| Category is Comment | S:2687 | MISSING | | grep: no match |
| Convert to Custom Text | S:2688 | MISSING | Conversion code exists, no command. | base/triggers/gui.cpp:415 |
| Copy As Script | S:11155 | MISSING | Same. | n/a |
| Convert triggers to Lua | D | MISSING | No menu string found (`.claude/plans/wc3-3.0-compatibility.md`). | n/a |
| Reset Triggers, Import/Export Triggers | S:2678-2681 | MISSING | | grep: no match |
| Find | S:2689-2691, L:FindDialog | PARTIAL | Ctrl+F highlights in the open script tabs; no find next or previous found. | trigger_editor/trigger_editor.cpp:110-134, trigger_editor/search_window.cpp |
| Map Header / custom script editor | S:2700 | COMPLETE | Jass editor tab, saved to wct. | trigger_editor/trigger_editor.cpp:159-175, base/triggers/triggers.ixx:909-939 |
| JassHelper options | S:2706-2710, S:1662 | PARTIAL | Always runs clijasshelper --scriptonly in Jass mode. No enable, vJass, debug or optimizer toggles. | base/triggers/map_script.cpp:873-888 |

## 9. Object Editor commands

| Item | WE evidence | HiveWE | What is missing / notes | Code |
|---|---|---|---|---|
| Tabs: Units, Items, Doodads, Destructibles, Abilities, Upgrades, Buffs | S:320 onward, B:CObjectPane* | COMPLETE | Tabs and save paths for base and skin files. Unknown 3.0 fields: [FORMAT: stale meta overrides]. | object_editor/object_editor.cpp:763-801, base/map/map.ixx:411-429 |
| New, Rename, Delete Custom Object; Copy and Paste Object | S:2717-2719, S:2744-2745 | COMPLETE | Add, Duplicate, Copy, Paste, Remove, rename. | object_editor/object_editor.cpp:1156, 1248, 1316, 1392, 2270-2274 |
| Reset Selected Object, Reset Standard Objects | S:2720-2721 | MISSING | Only "Reset Filters". | object_editor/object_editor.cpp:1731 |
| Export, Import per object type | S:2722-2723 | MISSING | All-object bundle only. | main_window/hivewe.cpp:1311-1399 |
| Export All, Import All Object Data | S:2724-2725 | PARTIAL | Own package format plus "map folder" import; WE's .w3o not read [INFERRED: B lists .w3o, no match in src]. | main_window/hivewe.cpp:1311-1399, utilities/object_data_io.ixx |
| Modify Selected Field | S:2726, L:DataValueDialog, B:CDataValueDialog* | PARTIAL | Inline editors for many types; unknown types (equipment, tags etc.) fall through to a line edit [FORMAT: new enum types]. | models/single_model.cpp:594-661 |
| Reset Selected Field | S:2727 | MISSING | grep found no reset action in single_model or OE menu. | models/single_model.cpp |
| Auto Fill Levels | S:2728, L:AutoFillDialog, B:CAutoFillDialog | MISSING | grep: no match (only setAutoFillBackground). | n/a |
| Display Values As Raw Data | S:2729 | MISSING | Shows raw ID of selected field only. | object_editor/object_editor.cpp:2072 |
| Sort Objects By Name | S:2730 | MISSING | grep: no match. | n/a |
| Copy, Paste Field Value | S:2731-2732 | COMPLETE | Ctrl+C, Ctrl+V on field view. | object_editor/object_editor.cpp:1777-1780 |
| Find | S:2733-2735 | COMPLETE | Ctrl+F and global search. Find next not checked. | object_editor/object_editor.cpp:830-835 |

## 10. HiveWE-only tools (no WE row)

Tooltip Editor (tooltip_editor/), Map Protector (map_protector/), Import Heightmap (main_window/hivewe.cpp:1047), Change Tile Pathing (menus/tile_pather.cpp), Global Search (object_editor/global_search.cpp), Merge MDXs (menus/doodad_palette.cpp:297), Pathing import and export (menus/pathing_palette.cpp:78-82), object bookmarks and history (object_editor/object_editor.cpp:841-844, 997).

## 11. Gaps ranked by map maker need

Format and 3.0 load work in `.claude/plans/wc3-3.0-compatibility.md` comes first and is not ranked here: nothing below helps a user whose map does not open.

1. Regenerated script loses gameplay data: silent damage to existing maps on every save.
2. GUI trigger editing: most map makers script through it, today it is display only.
3. Placed object properties (units, doodads, regions) and item placement: daily editing, many fields unreachable.
4. Minimap and shadow output: every released map, lobby preview and shadows go stale.
5. Techtree, Abilities, Upgrades, Ally Priorities, Random Groups dialogs: standard for melee and RPG maps; needs 1.
6. Map Options, Prefs, Water completion and ribbon misrouting (the real WE has no Prologue tab): cheap, visible, and the routing is a plain bug.
7. Camera palette and w3c writer: all cinematics need cameras; no workaround but script.
8. Sound Editor and w3s writer: needed for any custom sound, large.
9. Asset Manager import, export, replace: folder copy works, but no path control.
10. Object Editor missing commands: Reset Field, Auto Fill and Raw Data are frequent time savers.
11. Replace tools (the real WE has no selection mirror or rotate), Reset Height, Reset Variations: terrain workflow.
12. Game Interface: UI reskins are common in custom maps, small since Gameplay Constants can be reused.
13. Object Manager (10 tabs incl. Map Summary; the real WE has no cross reference tab): power users; partly covered by search.
14. View toggles (grid, camera bounds, regions, sky, fog, weather, game camera): QoL, some need new rendering.
15. Export Strings, Script, Minimap; Map String IDs: translators and tool users.
16. Post Processing (Advanced menu), Alpha tiles, doodad lights, Light Editor (Module menu): new 3.0 only, few maps yet; depends on format work. Post Processing and Light Editor are confirmed reachable in the real WE.
17. AI Editor, Campaign Editor: niche, usable from the real WE alongside HiveWE.
18. Dialogue, Sound Set, Object Group editors: exist in the exe but have no menu item in the real WE main window; lowest need, drop unless a way to open them turns up.

## 12. Only the user can check in the real WE

Answered by RUN on 2026-11-08 (menus read, dialogs opened, nothing edited or saved):
- Module menu contents, and the placement of Post Processing (Advanced), Free Camera Mode, Preview DoF, Bloom and Ambient Occlusion (View). Light Editor is real; Object Group, Cinematic, Dialogue, Sound Set and Model Previewer have no menu item.
- Player 5's Controller dropdown: None, User, Computer, Neutral, Rescuable.

Still needs the user in the real WE (RUN did no edits and no exports; do each on a COPY of a map):
1. Unit Layer: set the Unit Palette owner to Neutral Passive, look for an Items entry in its dropdowns. That settles whether items need a palette of their own.
2. Scenario > Player Properties: set Players 5-24 to None, File > Export Script, read `SetPlayers(...)` in config and `DefineStartLocation` numbering.
3. Scenario > Map Options: turn on Use Terrain Fog and Use Global Weather, set Map Water tint, File > Export Script, search for SetTerrainFog, weather and SetWaterBaseColor. If absent, the game reads them from w3i and HiveWE is fine on that point.
4. Same export: place a gold mine with non-default gold, search for SetResourceAmount.

## 13. Not determined

- HiveWE runtime behaviour: HiveWE was not run for this audit. Status rests on code reading. The base WE side was run (RUN) for menus and a few dialogs only.
- Contents not captured in RUN: Map Properties Water, Size and Camera Bounds, Loading Screen and Prefs tabs; Scenario Properties Ally/Enemy Priorities, Techtree, Abilities, Upgrades tabs; Light Editor per-light panels and property names; menu items of the Trigger Editor, Sound Editor, Asset Manager and Object Manager windows.
- Whether Object Group, Dialogue, Sound Set, Model Previewer or Cinematic functions are reachable some other way (no main-window menu item opens them). Cinematic Editor existence stays unconfirmed.
- Whether Round, Plateau, Noise and Smooth are palette tools in the real WE (not menu items; not verified).
- Format of war3mapPostProcessing.txt, war3map.w3grp, war3map.w3l, war3map.soundassets. Purpose of the last three is INFERRED only from names; the Light Editor UI does not show its data file.
- Whether fog, weather and water tint apply from w3i alone.
- Whether Item Tables and Scenario edits are undoable in WE.
- Some rows are grep-absence based ("grep: no match"); a differently named implementation could exist, though none surfaced.
