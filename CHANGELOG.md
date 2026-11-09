# Changelog

This is the changelog for the Lufion fork of HiveWE. Versions follow the fork's own numbering; `v0.20` is the first packaged release of the fork and builds on upstream HiveWE `v0.10`.

---

## [Unreleased]

### Bug Fixes
- **LNI import: new custom abilities no longer rejected** — `validate_shadow_map` was calling `field_to_meta_id` with the custom object's own ID (e.g. `A060`), which is not yet in the SLK for brand-new objects. `AbilityMetaData.slk` keys most fields via `useSpecific` (e.g. `nameANfy`), so the code lookup failed and every such field was reported as "Unknown field", blocking the whole import. Fixed by using the parent (`oldid`) ID for field lookup on custom objects. Regression test added.
- **Scenario import dialog no longer appears when nothing changed** — `prompt_apply_scenario_import` now skips the dialog entirely when `compute_scenario_import_changes` returns empty. The Apply button was already disabled in this case, but the dialog still forced a confirmation click.
- **Maps with an unreadable format version are refused instead of crashing or corrupting data** — Opening a map HiveWE cannot read, such as a Warcraft III 3.0 map, used to crash the editor or load wrong data that a save would write back. HiveWE now shows a dialog naming the files and versions and leaves the open map and its unsaved edits intact; a refused .w3x writes nothing to your folders, and HiveWE also refuses to unpack over the folder of the map that is open. Warcraft III 3.0 maps now open (see New Features).
- **Saving a map no longer loses or corrupts map info data** — Map flag bits and force flag bits HiveWE does not recognize are now kept instead of dropped, and random unit tables are saved with their position count and keep their full size when a table ID is empty. Previously these could write a corrupted `war3map.w3i`.
- **Map Protector writes clean values for fields older maps do not store** — These fields are now written as 0 instead of garbage.
- **Map Protector refuses asset obfuscation on Warcraft III 3.0 maps** — It stops with a clear failure message instead of producing a damaged map.
- **Truncated or corrupt map files no longer crash the editor** — Opening a map with a damaged file (such as `war3map.w3i`, `war3map.w3e`, the doodad and unit files, triggers, regions, cameras, sounds, pathing or object data) now shows an error naming the file instead of crashing, and your open map keeps its unsaved edits. Invalid terrain offsets, out-of-range cliff or ground texture indices, and a pathing map too small for its terrain are refused the same way, and editing a map whose ground texture index equals its texture count no longer reads out of bounds in pathing. Not game-tested.

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

### New Features
- **Command-line map resave (`HiveWE.exe --resave <in> <out>`)** — HiveWE loads a map folder and saves it to another folder without opening the editor. The exit code shows whether the save worked and whether the generated map script compiled (0 ok, 1 refused or failed, 2 wrong usage, 3 saved but the script did not compile, 4 no game data or OpenGL). Used for automated comparisons against World Editor output.
- **Warcraft III 3.0 map info (`war3map.w3i` v39) can be read and saved** — HiveWE keeps the map's own version stamps when saving it.
- **Doodad files (`war3map.doo`) v12 and v13 can be read and saved** — The new doodad fields (group id, roll, pitch, lights) are kept, and a v12 or v13 file saves back as the same version. Roll, pitch and lights are not rendered or editable yet.
- **Unit files (`war3mapUnits.doo`) v12 and v13 can be read and saved** — These are the versions written by Warcraft III 1.32+ and Reforged. The version and subversion are kept on save.
- **Warcraft III 3.0 maps now open** — HiveWE reads and saves region files (`war3map.w3r` v7), keeping each region's camera blocker and minimap colour; a v7 file saves as v7 and older ones as v5. Camera files (`war3map.w3c` v3) are read and saved back unchanged. Damaged v7 or v3 files are refused with a dialog. Known gaps: some 3.0 models and textures log load errors, the new 3.0 camera script calls are not yet written to the map script, and this has not been tested in the game yet.

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
