# Changelog

This is the changelog for the Lufion fork of HiveWE. Versions follow the fork's
own numbering; `v0.20` is the first packaged release of the fork and builds on
upstream HiveWE `v0.10`.

## v0.20

First packaged release of the fork. Highlights since upstream HiveWE `v0.10`:

### Object Editor
- Complete visual redesign into a modern, cleaner style with a matching color scheme.
- New **Custom** folder on every tab that collects all custom object data, for easier work in heavily customized maps.
- Folders are collapsed by default; a single click opens the right-side data panel instead of spawning new tabs.
- Active object data is sorted alphabetically, stacked values are shown inline on one line, object IDs are shown next to names, and selected-object data has proper column headers with full-row click/highlight.
- Arrow-key navigation that also updates the right-side settings panel.
- Folder search for the icon settings.
- Ability object improvements: fixed hotkey fields, multi-select Option field, and a Buffs field that lets you add/remove buffs from a searchable list.
- Copy/paste objects with Ctrl+C / Ctrl+V, F2 rename fixes, and a fix for duplicate objects syncing renames.
- Fewer clicks needed to open value fields such as ability target type.
- Editor state (active tab, open/closed folders) is now remembered.
- Custom object images are read and displayed correctly.
- Fixed a crash when scrolling through certain assets, and added the **O** shortcut to open the editor.

### Regions
- New Regions palette with colored regions (toggle with **R**; regions are only shown while the palette is open).
- Region movement is wired into undo/redo, with cursor feedback for drag/move.
- Overlapping regions prioritize the smaller region on click; double-clicking a region in the list moves the camera to it. Fixed incorrect rect coordinates.

### Terrain & Palettes
- Palettes reworked into a sidebar with a full visual redesign.
- Terrain copy/paste (use with caution).
- Smooth tool is now gradual instead of instantly flattening.
- The add-water tool accounts for differing terrain heights.
- Brush size can be changed with Shift+Scroll.

### Doodads & Units
- Selected doodads are highlighted; drag placement now requires movement and no longer fires immediately, and the pre-placement click helper was removed.
- Scale doodads with Shift+Scroll (works with multiple selected).
- Doodad palette gained a custom-doodads tab; the unit palette can filter by custom units.
- Item Drop Table window (under the Units tab and when double-clicking pre-placed units).
- Fixed doodad Z placement that previously mismatched in-game and differed from the World Editor.
- Fixed water pathing for units.

### Sidebar, Preview & Navigation
- Doodad/unit preview in the sidebar; arrow keys switch the previewed model.
- Minimap moved into the sidebar and is toggleable (off by default).
- Reworked panel/sidebar behavior: selection mode is on by default when switching panels, and editors are brought to the foreground when clicked.
- Player start locations are now shown.

### Model Editor & Rendering
- Added a model editor browser and made the model editor view much larger.
- Animation dropdown to choose which animation effects play, plus improved model-preview sequence selection.
- SD/HD editable mesh rendering, particle rendering and visibility fixes, full sequence tokenization, and an SD model grid view.

### MDX / MDL
- MDX/MDL round-trip tests plus reader/writer fixes.
- Added MDX camera and texture animations.
- Fixed PREM1 loading and removed MDL block comments.

### Tooltip Editor
- New tooltip editor that reads values from existing objects and hides the raw WC3 color strings for clarity, with color-picker UX improvements.

### Maps & General
- New-map creation popup that previews the selected tileset's tiles, similar to the default World Editor.
- Saving shows a fading confirmation message.
- Quick access to the last three opened maps under **Files**.
- Config tab listing all editor shortcuts, grouped by category.
- Building the project now launches it automatically (and kills old instances) to speed up iteration.
- New application icon, header redesign, and assorted visual/coordinate fixes in the debug view.

### Running
Download the zip, extract it, and run `HiveWE.exe`. A Warcraft III installation is required.
