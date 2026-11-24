# Changelog

This is the changelog for the Lufion fork of HiveWE. Versions follow the fork's own numbering; `v0.20` is the first packaged release of the fork and builds on upstream HiveWE `v0.10`.

---

## [Unreleased]

### Bug Fixes
- **Stock campaign maps such as humanx03 open despite custom objects whose base unit no longer exists** — Maps with a leftover custom object based on a unit missing from the game data (for example n003 based on nbsh) no longer fail to open. HiveWE opens the map and shows one warning listing the skipped objects, including how many placed units were left out; those objects are dropped the next time the map is saved.
- **`HiveWE --resave` no longer reports success when custom objects were dropped** — If custom objects with an unknown base id were skipped on load and are therefore missing from the saved map, `--resave` now exits with code 5 instead of 0. A script compile failure (exit 3) still takes priority but now also reports the skipped objects.
- **A map that passes the open check but fails while loading shows an error instead of crashing** — If a map still fails during loading, HiveWE now shows a message naming the cause (for example a custom unit based on an id missing from the object data) and opens a blank map instead of crashing.
- **Open Map (MPQ) and Map Protector refuse archive entries that would write outside the target folder** — Entries with absolute paths, drive letters, `..` parts or Windows device names (CON, NUL, COM1 ...) now fail the unpack with an error instead of writing files elsewhere on disk. Maps whose archive actually holds such entry names are no longer opened or protected; import-list entries the archive does not contain are still skipped.
- **Change Tileset no longer leaves stale tile indices in undo or the terrain clipboard** — When the tile list changes (a tile removed, added or reordered), the undo history is cleared and the copied terrain selection is dropped, so Ctrl+Z or paste can no longer write a texture index that no longer exists into the map. Replacing only a tile's texture keeps both, and still marks the map as having unsaved changes.
- **Open Map (MPQ) and Map Protector no longer carry on after a file fails to extract** — If a file in a .w3x/.w3m archive cannot be extracted (damaged archive, unwritable target), HiveWE now shows which file failed and does not open the half-unpacked map; an optional file the archive simply lacks is still skipped quietly.
- **Saving doodads with short or long ids no longer corrupts war3map.doo** — Saving a map with a doodad whose id, skin id or item-drop id is shorter than 4 characters no longer corrupts war3map.doo; ids longer than 4 characters now refuse the save instead of writing a shifted file.
- **Startup map load failures show an error instead of crashing** — HiveWE now shows an "Opening map failed" error instead of silently crashing when the startup map can't be loaded because a data file is missing or damaged.
- **Object Editor New, Add and Copy dialogs only accept object ids of exactly 4 letters or digits** — Ids with non-ASCII characters, quotes, backslashes or spaces, which could break the generated map script, now keep the OK button disabled.
- **Browse Models animated tiles that failed to start are retried when the dialog is shown again** — Tiles no longer stay a still image after hiding and reopening the dialog, and dropping tiles no longer leaks them when the GL context cannot be made current.
- **A damaged .mdx model with a forged animation track count now fails to load with an error** — It no longer exhausts memory or crashes HiveWE.
- **Map Description and other map text fields show plain-text values and save your edits** — Map Properties and New Map fields (name, author, description, suggested players, player and force names) used to appear empty when the map file stored them as plain text, and edits to them were silently dropped. They now show the stored text and keep your edits.
- **Edited map text is stored as a trigger string** — Quotes and line breaks in these fields can no longer break the generated map script. New Map also stores its name and suggested-players text as trigger strings.
- **Change Tileset no longer crashes on map folders or imported tile textures with non-ASCII names** — Folder or file names the Windows code page cannot hold (for example Chinese or Cyrillic) now work in the Change Tileset dialog, and its import error messages show such paths correctly.
- **Change Tileset accepts tile textures with an upper-case extension** — Importing a tile texture such as `Tile.BLP` now works.
- **Maps in folders with non-ANSI names open, save and export correctly** — Opening, saving, Save As, exporting, recent maps and Test Map no longer crash or garble the path when the map folder name has characters outside the Windows code page (Cyrillic, Chinese, etc.), and the window title and map name show the real name.
- **Saving no longer fails halfway on imported files with non-ANSI names** — A file whose name has characters outside the Windows code page no longer aborts the save.
- **Object data export and import folder pickers handle non-ANSI folders** — Folders with such names can now be chosen.
- **HD (Reforged, v1800) models now load whole instead of being refused** — They open in the Model Editor and show thumbnails in Browse Models.
- **Model Editor: Save to MDX keeps the model's own version** — Models are no longer downgraded to v1200 on save, and a failed save shows a message instead of crashing.
- **Model Editor: a failed model open no longer throws, and "Open in Model Editor" opens the tab** — A model that cannot be opened now shows an error instead of an exception, and "Open in Model Editor" from the model browser now actually switches to the Model Editor tab.
- **Model Editor: failed Save to MDX and Edit MDL writes now warn, and non-ASCII folder paths work** — A write that fails shows a warning instead of failing silently, and models in folders with non-ASCII names can be opened and saved.
- **Edit in Object Editor no longer crashes when the browser's Custom filter hides the object** — The Object Editor browser now switches to All and selects the object.
- **Gameplay Constants has no blank gap above its first row, and Change Tileset spells "Additional Tiles" correctly** — The gap and the "Additonal Tiles" typo are gone.
- **Unit Properties and Gameplay Constants names display cleanly** — Player names in Unit Properties no longer show stray quote characters, and Gameplay Constants names are readable in the Flat Light theme and no longer show quote characters.
- **A theme that fails to load no longer stops HiveWE from starting** — HiveWE falls back to the Dark theme and shows a message.
- **A failed save no longer says "Map saved" or damages your map files** — If a save fails or is interrupted (disk full, read-only or locked file), the error is shown, and map files are written to a temporary file and swapped in so a failed write no longer truncates the existing good file. `--resave` exits with a non-zero code on these failures.
- **A failed map script step no longer puts a stale script into your map** — If the Lua temp file cannot be written, or JassHelper fails, times out or exits with an error, HiveWE keeps the map's previous `war3map.lua` / `war3map.j` instead of copying an old script (possibly from another map), and shows "Saved without a new map script" instead of "Map saved".
- **Map Protector reports a failed internal save and returns to your map folder** — The export shows the error, and the editor no longer stays pointed at a temporary folder.
- **New Trigger Editor variables no longer break the generated script** — Variables created in the Trigger Editor now default to type integer with clean flags, so the script no longer fails with "Undefined type udg_NewVariable". New variables also get unique names (NewVariable, NewVariable2, ...).
- **Doodads keep their skin when a map is loaded and saved** — A doodad that `war3map.doo` skins as a different type (for example FTtw shown as LTlt) used to lose that skin. The skin is now kept in `war3map.doo` and in the generated script, while newly placed doodads still use their own type as the skin.
- **Pressing OK in Scenario > Map Options no longer clears the map's global weather** — The weather list now shows every entry (the first one was missing and the entries had no usable ids), and the current weather is shown by name.
- **Map Options no longer clears the map's custom sound environment on OK** — The "Default" environment now also shows in the Custom Sound list.
- **The Map Info dialog shows and saves the water tint and fog colours correctly** — Red and blue were swapped before, and an unchanged dialog now keeps the colours' alpha byte.
- **Saved maps set the water colour when water tinting is on** — The generated map script now calls `SetWaterBaseColor` like the World Editor, so tinted water no longer falls back to the default colour in game. Not game-tested yet.
- **Start location priorities in the generated map script match the World Editor** — `SetStartLocPrio` and `SetEnemyStartLocPrio` are written in the World Editor's order with its values, and maps without priorities no longer get an empty `InitAllyPriorities`. Not game-tested yet.
- **Saving no longer closes HiveWE silently when object data has a field it cannot write** — The save is refused before any file is written, and a dialog names the object and field. Custom abilities losing their data on load, the usual cause, was already fixed earlier in this release.
- **Export > Binary package checks object data the same way** — A field that cannot be written now shows "Nothing was exported" instead of crashing and leaving a half-written folder.
- **Any other error during Save or Save As shows a "Saving failed" dialog** — The editor no longer closes, so you keep your open map and unsaved edits.
- **Map Protector restores the map folder after an object-data rewrite error** — A failed rewrite no longer leaves the folder half-protected.
- **LNI import: new custom abilities no longer rejected** — `validate_shadow_map` was calling `field_to_meta_id` with the custom object's own ID (e.g. `A060`), which is not yet in the SLK for brand-new objects. `AbilityMetaData.slk` keys most fields via `useSpecific` (e.g. `nameANfy`), so the code lookup failed and every such field was reported as "Unknown field", blocking the whole import. Fixed by using the parent (`oldid`) ID for field lookup on custom objects. Regression test added.
- **Scenario import dialog no longer appears when nothing changed** — `prompt_apply_scenario_import` now skips the dialog entirely when `compute_scenario_import_changes` returns empty. The Apply button was already disabled in this case, but the dialog still forced a confirmation click.
- **Maps with an unreadable format version are refused instead of crashing or corrupting data** — Opening a map HiveWE cannot read, such as a Warcraft III 3.0 map, used to crash the editor or load wrong data that a save would write back. HiveWE now shows a dialog naming the files and versions and leaves the open map and its unsaved edits intact; a refused .w3x writes nothing to your folders, and HiveWE also refuses to unpack over the folder of the map that is open. Warcraft III 3.0 maps now open (see New Features).
- **Saving a map no longer loses or corrupts map info data** — Map flag bits and force flag bits HiveWE does not recognize are now kept instead of dropped, and random unit tables are saved with their position count and keep their full size when a table ID is empty. Previously these could write a corrupted `war3map.w3i`.
- **Map Protector writes clean values for fields older maps do not store** — These fields are now written as 0 instead of garbage.
- **Map Protector refuses asset obfuscation on Warcraft III 3.0 maps** — It stops with a clear failure message instead of producing a damaged map.
- **Truncated or corrupt map files no longer crash the editor** — Opening a map with a damaged file (such as `war3map.w3i`, `war3map.w3e`, the doodad and unit files, triggers, regions, cameras, sounds, pathing or object data) now shows an error naming the file instead of crashing, and your open map keeps its unsaved edits. Invalid terrain offsets, out-of-range cliff or ground texture indices, and a pathing map too small for its terrain are refused the same way, and editing a map whose ground texture index equals its texture count no longer reads out of bounds in pathing. Not game-tested.
- **Change Tileset no longer loads placeholder ground textures in HD mode** — A doubled `_diffuse` suffix broke the lookup. SD tile textures now share the cache with the initial load instead of loading a duplicate copy.
- **Opening or creating another map after a Map Resize or Change Tileset no longer discards unsaved work silently** — Those actions cleared the undo history that the unsaved-changes prompt relied on, so HiveWE skipped the prompt. It now asks before replacing the map.
- **Change Tileset no longer reads past the tile table when a cell used a removed tile** — Maps with cells on a tile that the new tileset drops no longer risk a crash or garbage tiles.

- **Saving no longer changes a doodad's flag byte** — Doodads with flag state 3 were written back as 2, and flag bit 0x08 was dropped, on every save including v8 maps. Both are now preserved.
- **Damaged v12 or v13 doodad files are refused with a dialog** — Opening one shows an error and leaves your open map usable.
- **Units set to a random unit from a group keep their count on save** — The count was dropped, so the next load misread the unit file. Fixed for all unit file versions.
- **Empty item-set ids in unit files are saved at full size** — They were written as 0 bytes instead of 4, which shifted everything after them. Fixed for all unit file versions.
- **Saving a map with a unit or item id longer than 4 bytes is refused** — A dialog names the id and nothing is written. Before, the unit file was corrupted.
- **The Object Editor no longer accepts a 4-character id that is longer than 4 bytes** — Ids such as `hfoö` are rejected because they cannot be stored in map files.
- **Start locations in the generated map script are now numbered by position** — The script numbered start locations by player number instead of by position (0, 1, 2..., as the World Editor does), which was wrong for maps whose players are not numbered 0, 1, 2... such as Blizzard's Forgotten Hollow (players 1 and 10). Affects all map versions.
- **Trigger comparisons and arithmetic in the generated map script no longer write quoted operators** — Operators such as `"!="` were written with quotes, and in Lua "No unit"-style values were written as `null`. This made Lua scripts invalid, likely why the game refused HiveWE-saved Lua maps with GUI comparisons, and also affected JASS output. Operators are now written bare, and Lua uses `~=` and `nil`.
- **Lua map scripts saved by HiveWE are valid again for maps with custom-text triggers, odd trigger names or parameterless events** — Scripts were invalid or crashed at map start when a map had Lua code-library (custom text) triggers, trigger names with characters such as `'` or `=`, or events without parameters. HiveWE now skips custom-text triggers' globals and InitTrig calls in Lua like the World Editor, turns odd characters in trigger, region, camera and sound names into underscores, and no longer writes a trailing comma for events without parameters.
- **Variables and arrays in saved Lua maps get the World Editor's defaults** — Every array was declared with an empty-text default and every variable as nil, so maps comparing integer arrays crashed during map init (in game: black screen, music plays, no fade-in). Defaults are now per type (0, false, "", and nil or `{}` for handles). Confirmed in game on Blizzard's Forgotten Hollow with a hand-patched copy; the built fix is not game-tested yet.
- **Destructables and doodads that triggers refer to are assigned to their variable again** — When the creation number was below 1000, the generated script looked up the variable as "144" but declared it as "0144", so triggers using them found nothing (in Blizzard's Forgotten Hollow the start sequence stayed on a black screen). Affects all map versions, JASS and Lua.
- **Placed items that triggers refer to are linked in the saved map script** — Triggers using a placed item (`gg_item_` variables) got nothing in Lua and JASS, so for example the orbs at the first altar in Blizzard's Forgotten Hollow stayed visible instead of being hidden. A trigger reference whose unit or item type does not match the placed object is also skipped now, instead of writing an assignment to an undeclared variable in JASS.
- **Sounds in saved maps are set up the way the World Editor does it** — Dialogue voice lines keep their sound label, subtitles (speaker name and text keys) and facial animation, sound durations are read from the audio file instead of a wrong 0 or 1, channel and pitch are no longer forced to broken values on labelled sounds, and sound numbers are written as plain decimals. Before, voice lines could go silent, music could restart or play wrongly, and combat sounds could stay muted after a cinematic voice line (seen in Blizzard's Forgotten Hollow).
- **Saving a map no longer reports success when a file was not fully written** — If writing a map file is cut short or the file cannot be closed (for example when the disk is full), the save now fails instead of silently leaving a broken file.
- **Placed units with autocast abilities no longer break the saved map script** — The autocast order (for example Bloodlust set to autocast) was written as a list of characters, so Lua maps failed to load. Found in 61 stock maps by the new automated comparison. Not game-tested yet.
- **Lua GUI triggers converted to custom text run on map initialization again** — They are initialised and run like the World Editor does, and pure custom-script blocks no longer get an extra trigger object (for example "Cursed Flute" in Forgotten Hollow). This corrects a regression from an earlier fix in this release. Not game-tested yet.
- **Team count, game placement, race selection and forced start locations in the generated map script now match the World Editor** — The team count (`SetTeams`) used the number of forces, game placement was always written as "use map settings", and player races marked selectable also forced the start location. All of these are now set the way the World Editor does it, in both Lua and JASS. Not game-tested yet.
- **Placed units' team colour and gold mine amounts in the generated map script now match the World Editor** — The script now sets the team colour of placed units (taverns, mercenary camps, and units with a custom colour where the type allows it) and the gold amount of every gold mine, in both Lua and JASS. Before, both were dropped, so mines started with the default amount. Not game-tested yet.
- **Standard-definition models saved by newer Warcraft III versions that contain a camera now load in the Model Editor and Browse Models** — Many stock units (gnoll, bristleback, harpy witch, goblin land mine) failed to open with "Trying to read out of range of buffer" and now open normally.
- **Force "Share Vision" is read from and saved to the bit Warcraft III itself uses** — Stock and World Editor maps now show it ticked, and generated scripts include the vision calls. Vision ticked in earlier HiveWE builds is kept (the old 0x04 bit is read as vision and saved as 0x08). Maps saved by builds from 2026-08-09 until 2026-11-08 dropped stock vision (0x08) on save and may need Share Vision ticked again.
- **Model camera depth-of-field tracks are kept when a model is saved** — Saving a model in the Model Editor or rewriting it with the Map Protector used to drop the camera's depth-of-field tracks (IDUF, ELAF, PTSF), any unknown camera tracks and the camera variant byte. They are now preserved in .mdx output; converting a model to MDL still drops them.
- **Ctrl+Z after deleting or cutting doodads restores them again** — Undo restored nothing before, a regression from an earlier refactor.

#### Map Protector
- **Asset Path Obfuscation no longer fails on maps with a custom object whose base object is unknown** (for example humanx03's leftover unit on `nbsh`) — such an object is copied into the protected map's object data unchanged, and all other objects are still rewritten.
- **"Remove listfile" now really leaves `(listfile)` out of the protected map** — The option was ignored before and `(listfile)` was always written into the protected archive.
- **Map Protector no longer leaves later reads and writes pointed at its temp folder when a map's terrain file (war3map.w3e) is truncated.**

### New Features
- **Change Tileset shows a preview and mapping per tile and can import custom tiles** — Each tile has a preview and a mapping, and removed tiles are repainted with a replacement you choose; the blight index is remapped too. You can import a BLP or TGA texture as a custom tile, saved into the map folder as a texture override. Everything is applied only on OK, all-or-nothing. DDS is rejected because Warcraft III would find the stock BLP first.
- **Model Editor has a toolbar, animation timeline, inspector docks and status bar, styled for Flat Dark and Flat Light** — The toolbar offers Open, Browse Models, Wireframe, Reset Camera, Edit MDL, Save to MDX and a sequence picker. The timeline has play/pause, restart, scrub, loop and 0.5x/1x/2x speed, and the Sequences, Geosets/Materials (with per-geoset Show toggles) and Info (MDX version, drawn vertex and triangle counts) docks replace the old ImGui panels.
- **Model Editor has a Browse Models dialog, styled for Flat Dark and Flat Light** — The toolbar's Browse Models action opens a dialog with model categories and counts, search, a thumbnail grid (each thumbnail renders once and is cached) and a live preview with an animation choice and model info; HD models still show a placeholder tile because they do not load yet.
- **New "Flat Dark" and "Flat Light" themes (File > Settings > Theme)** — A modern flat look with the bundled Inter font, a compact menu bar and icon toolbar instead of the ribbon, the sidebar on the left with mode buttons, Minimap and the palette, Properties and Preview panels on the right, and a status bar (cursor position, tileset, map size, brush, saved state). The terrain palette uses Texture / Cliff / Deformation / Cell tabs, and all palettes scroll so the window fits small screens. The old Dark and Light themes are unchanged.
- **Flat themes: the Properties panel shows the selected unit or doodad** — A read-only panel lists type, owner with player colour, facing in degrees, hit points as a percentage with the absolute value, position, scale and other fields, and follows the selection live while you move, rotate or undo. With several objects selected it shows the first one plus a count. The Regions tab is still a placeholder.
- **Flat themes: more dialogs now match the theme** — The Settings, Unit Properties (including Edit Drop Item), Map Settings, Scenario Properties and Gameplay Constants dialogs follow Flat Dark and Flat Light instead of keeping the old look.
- **Flat themes: the Object Editor and more dialogs now match the theme** — The Object Editor, Create New Map, Asset Manager, Map Protector, Import Object Data, Item Tables, Config, Change Tileset and Tile Pathing dialogs follow Flat Dark and Flat Light.
- **Shortcut change: Ctrl+T now runs Test Map** — Ctrl+T used to toggle Wireframe; Wireframe moved to Ctrl+Shift+W.
- **New command-line options for automated testing** — `--qa-offscreen`, `--theme`, `--palette` and `--window-size` (see `--help`).
- **Command-line map resave (`HiveWE.exe --resave <in> <out>`)** — HiveWE loads a map folder and saves it to another folder without opening the editor. The exit code shows whether the save worked and whether the generated map script compiled (0 ok, 1 refused or failed, 2 wrong usage, 3 saved but the script did not compile, 4 no game data or OpenGL, 5 saved but custom objects with an unknown base id were skipped on load and are missing from the saved map). Used for automated comparisons against World Editor output.
- **Warcraft III 3.0 map info (`war3map.w3i` v39) can be read and saved** — HiveWE keeps the map's own version stamps when saving it.
- **Doodad files (`war3map.doo`) v12 and v13 can be read and saved** — The new doodad fields (group id, roll, pitch, lights) are kept, and a v12 or v13 file saves back as the same version. Roll, pitch and lights are not rendered or editable yet.
- **Unit files (`war3mapUnits.doo`) v12 and v13 can be read and saved** — These are the versions written by Warcraft III 1.32+ and Reforged. The version and subversion are kept on save.
- **Warcraft III 3.0 maps now open** — HiveWE reads and saves region files (`war3map.w3r` v7), keeping each region's camera blocker and minimap colour; a v7 file saves as v7 and older ones as v5. Camera files (`war3map.w3c` v3) are read and saved back unchanged. Damaged v7 or v3 files are refused with a dialog. Known gaps: some 3.0 models and textures log load errors, the new 3.0 camera script calls are not yet written to the map script, and this has not been tested in the game yet.
- **Model previews can be orbited with left-drag and start from the front of the model** — This applies to the Selection panel, the Model Editor and Browse Models, so a model no longer opens facing away from you.
- **Browse Models shows file sizes in MB** — You can see how large a model is before picking it.
- **Browse Models sidebar has "Models" and "VFX/SFX" filters** — Effect models can be separated from unit and doodad models.
- **Object Editor: double-clicking a model field opens the Browse Models picker** — The picker has a Path line, so typed paths and clearing the field still work, and it replaces the old model picker.
- **Browse Models tiles now animate** — Tiles in view play their Stand animation instead of showing still pictures, up to 12 at once, and pause while you scroll or when the window is inactive.

---

## v0.21

### New Features

#### Map Protector
- New **Map Protector** window (Config ribbon) that exports a hardened copy of the current map or an external `.w3x` / `.w3m` / folder. The source is never modified.
- MPQ hardening: remove listfile, remove attributes, per-file encryption, and optional junk-file injection.
- Trigger/script hardening: remove GUI trigger data (`war3map.wtg`), and optional trigger-string inlining (strips `war3map.wts` after resolving `TRIGSTR_*` in the script and map info).
- Optional metadata sanitization (author, description, loading-screen text, map name).
- Asset path obfuscation: rename custom imports to flat random-hex names and rewrite matching references in object data, map info, MDX internals, and script string literals, with a dangling-reference safety check before packing.

#### Object Data Export / Import
- Export Object Editor modifications (units, items, abilities, buffs, upgrades, destructibles, doodads) as a binary or human-readable text package from the Home ribbon.
- Import with validation and conflict preview, per-object overwrite controls, and transactional apply with rollback on failure.
- Also supports importing raw `war3map.w3*` files from another map folder.
- Newly created objects from an import now appear correctly in a new Object Editor tab.

#### Scenario Properties
- New **Players / Forces** editor (Map ribbon) matching World Editor fields: controller, race, fixed start location, color, and force alliance flags.
- Always shows all 24 player slots; inactive (None) rows are dimmed.
- Players/Forces can be exported/imported as `scenario.json` alongside Object Data packages, with a field-level change preview.

#### Asset Manager
- Reworked into a stronger unused-file cleanup tool: stats header, type column, All / Unused filter, and bulk delete to the recycle bin.
- Full MDX scan (not only Object Editor–reachable models), override detection for stock game paths, and case-insensitive used/unused matching.

### Bug Fixes
- Fixed opaque SD models rendering invisible (alpha test incorrectly applied to Filtermode None).
- Fixed black models caused by missing mipmaps on non-DDS (TGA/PNG) textures.
- Fixed tiling textures collapsing due to limited UV packing range (now half-float UVs).
- Fixed force share-vision bit loading and race lobby-selectability generation.
- Fixed `TRIGSTR` next-id tracking that could collide with newly created strings.
- Doodad and unit palettes now sort alphabetically.
- Fixed Object Editor left-panel search focus stealing.
- Opening listfile-less `.w3x` maps is more reliable (extracts core `war3map.*` files by name).
- Fixes for listfile-less archives in Map Protector (core-file extraction + `war3map.imp` recovery), StormLib fabricated-name packing crashes, and several object-data field types (`modelList`, `pathingTexture`, extensionless icon/model paths).
- Window title version updated to **0.21**.

---

## v0.20

First packaged release of the fork. Highlights since upstream HiveWE `v0.10`:

### New Features

#### Object Editor
- Complete visual redesign into a modern, cleaner style with a matching color scheme.
- New **Custom** folder on every tab that collects all custom object data.
- Folders are collapsed by default; single click opens the right-side data panel.
- Active object data sorted alphabetically; stacked values shown inline; object IDs shown next to names; selected-object data has proper column headers with full-row click/highlight.
- Arrow-key navigation that also updates the right-side settings panel.
- Folder search for the icon settings.
- Ability object improvements: fixed hotkey fields, multi-select Option field, and a Buffs field with searchable list.
- Copy/paste objects with Ctrl+C / Ctrl+V; F2 rename fixes; fix for duplicate objects syncing renames.
- Editor state (active tab, open/closed folders) remembered across sessions.
- Custom object images read and displayed correctly.
- **O** shortcut to open the editor.

#### Regions
- New Regions palette with colored regions (toggle with **R**; regions only shown while palette is open).
- Region movement wired into undo/redo with cursor feedback. Fixed incorrect rect coordinates.
- Overlapping regions prioritize the smaller region on click; double-clicking a region moves the camera to it.

#### Terrain & Palettes
- Palettes reworked into a sidebar with a full visual redesign.
- Terrain copy/paste.
- Smooth tool is now gradual instead of instantly flattening.
- Add-water tool accounts for differing terrain heights.
- Brush size can be changed with Shift+Scroll.

#### Doodads & Units
- Selected doodads highlighted; drag placement now requires movement.
- Scale doodads with Shift+Scroll (works with multiple selected).
- Doodad palette gained a custom-doodads tab; unit palette can filter by custom units.
- Item Drop Table window (under the Units tab and when double-clicking pre-placed units).

#### Sidebar, Preview & Navigation
- Doodad/unit preview in the sidebar; arrow keys switch the previewed model.
- Minimap moved into the sidebar and is toggleable (off by default).
- Player start locations shown.

#### Model Editor & Rendering
- Model editor browser added; view made much larger.
- Animation dropdown to choose which animation effects play.
- SD/HD editable mesh rendering, particle rendering and visibility fixes, full sequence tokenization, SD model grid view.

#### MDX / MDL
- MDX/MDL round-trip tests plus reader/writer fixes.
- MDX camera and texture animations added.
- Fixed PREM1 loading; removed MDL block comments.

#### Tooltip Editor
- New tooltip editor that reads values from existing objects and hides raw WC3 color strings, with color-picker UX improvements.

#### Maps & General
- New-map creation popup with tileset tile preview.
- Saving shows a fading confirmation message.
- Quick access to the last three opened maps under **Files**.
- Config tab listing all editor shortcuts, grouped by category.
- Building the project now launches it automatically and kills old instances.
- New application icon, header redesign.

### Bug Fixes
- Fixed a crash when scrolling through certain assets in the Object Editor.
- Fixed doodad Z placement that previously mismatched in-game and differed from the World Editor.
- Fixed water pathing for units.
- Fixed pre-placement click that fired immediately without requiring drag movement.

---

## v0.10 *(upstream HiveWE)*

### New Features
- Gameplay constants editor.
- Map size and camera bounds editing.
- Required animation names support.
- Asset manager (unused file finding).
- Multithreaded resource loading.
- Terrain converted from AoS to SoA layout for performance.

### Bug Fixes
- Fixed tracy firewall requirement.
- Fixed CASC future race condition.
- Fixed tileset changing.
- Fixed ability meta-field resolution.
- Fixed unit selection.
- Fixed doodad palette height decimal.
- Fixed OpenGL data corruption.
- Fixed ramp/cliff updating.
- Fixed blend mode 0 opaque rendering.
- Fixed `TRIGSTR` in stringList.
- Fixed incorrect object editor ID duplication.
- Optimized SLK loading, cliff mesh loading, ground height calculation, hierarchy file open.

---

## v0.9 *(upstream HiveWE)*

### Bug Fixes
- Removed superfluous disconnect.

---

## v0.8 *(upstream HiveWE)*

### Bug Fixes
- Fixed version numbers.

---

## v0.7 *(upstream HiveWE)*

### Bug Fixes
- Disabled script settings (temporary).

---

## v0.6 *(upstream HiveWE)*

### New Features
- Added bullet (projectile) support.

### Bug Fixes
- Fixed incorrect mouse coordinates when pointing into void.

---

## v0.5 *(upstream HiveWE)*

### Bug Fixes
- README update.

---

## v0.3 *(upstream HiveWE)*

### New Features
- Remembers previously set Warcraft III directory.

### Bug Fixes
- Updated readme.
