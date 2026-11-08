# Changelog

This is the changelog for the Lufion fork of HiveWE. Versions follow the fork's own numbering; `v0.20` is the first packaged release of the fork and builds on upstream HiveWE `v0.10`.

---

## [Unreleased]

### Bug Fixes
- **LNI import: new custom abilities no longer rejected** — `validate_shadow_map` was calling `field_to_meta_id` with the custom object's own ID (e.g. `A060`), which is not yet in the SLK for brand-new objects. `AbilityMetaData.slk` keys most fields via `useSpecific` (e.g. `nameANfy`), so the code lookup failed and every such field was reported as "Unknown field", blocking the whole import. Fixed by using the parent (`oldid`) ID for field lookup on custom objects. Regression test added.
- **Scenario import dialog no longer appears when nothing changed** — `prompt_apply_scenario_import` now skips the dialog entirely when `compute_scenario_import_changes` returns empty. The Apply button was already disabled in this case, but the dialog still forced a confirmation click.
- **Maps with an unreadable format version are refused instead of crashing or corrupting data** — Opening a map HiveWE cannot read, such as a Warcraft III 3.0 map, used to crash the editor or load wrong data that a save would write back. HiveWE now shows a dialog naming the files and versions and leaves the open map and its unsaved edits intact; a refused .w3x writes nothing to your folders, and HiveWE also refuses to unpack over the folder of the map that is open. Opening 3.0 maps is not supported yet.
- **Saving a map no longer loses or corrupts map info data** — Map flag bits and force flag bits HiveWE does not recognize are now kept instead of dropped, and random unit tables are saved with their position count and keep their full size when a table ID is empty. Previously these could write a corrupted `war3map.w3i`.
- **Map Protector writes clean values for fields older maps do not store** — These fields are now written as 0 instead of garbage.
- **Map Protector refuses asset obfuscation on Warcraft III 3.0 maps** — It stops with a clear failure message instead of producing a damaged map.

### New Features
- **Warcraft III 3.0 map info (`war3map.w3i` v39) can be read and saved** — HiveWE keeps the map's own version stamps when saving it. Real 3.0 maps are still refused on open until their doodad, unit, region and camera files are supported.

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
