# HiveWE (Fork)

This is a personal fork of [stijnherfst/HiveWE](https://github.com/stijnherfst/HiveWE).

---

## Changes in this fork

### Regions
Regions are a new feature added to this fork, including a full Region Palette for managing them.
- Regions can be colored for visual distinction
- Regions are only visible when the Region Palette is open
- Double-clicking a region in the list moves the camera to it
- Layered regions prioritize the smaller region on click
- Region movement supports undo/redo
- Cursor feedback is shown for drag and move actions on regions
- Player Start Locations are supported as a region type

### Object Editor
- Object Editor objects and data are now ordered alphabetically
- Active object data is sorted alphabetically; stacked values are listed on one line for a cleaner look
- Added a Custom Folder on every tab that holds all custom object data for better UX in highly custom maps
- OE folders are now closed by default
- OE state (active tab, open/closed folders) is now saved and restored between sessions
- Clicking on an object (e.g. a unit) switches the active tab instead of opening a new one
- Added proper column headers for the selected object data panel; entire row is now clickable and highlighted
- Object editor now reads and displays custom images correctly
- Reduced the number of clicks required for multi-click value fields in the abilities tab (e.g. target type)
- Editors such as the Object Editor and Asset Manager are now unminimized and brought to the foreground when reopened

#### Modern redesign (new)
The Object Editor received a full visual and UX overhaul into a much more modern style:
- Reworked left browser with a proper header per object type: icon, live object counts (e.g. `x shown · y total`), search, and `All` / `Custom` mode chips
- Custom tree delegate: object raw IDs are shown muted behind the object names, custom objects get a badge, and folder rows are calmer with cleaner spacing/icon sizing
- New right-side object summary card with a large icon, Raw ID, Parent, and Base Object, plus quick actions (Copy ID, Copy Name, Open Parent, Bookmark/Unbookmark, Modified Fields)
- The right-side summary header is sticky and stays visible while scrolling
- Field search plus `Modified` and `Core` filters, a section filter dropdown, and a one-click section chip strip (All, Art, Data, Stats, Text, etc.)
- Live field counts (e.g. `24 of 163 fields`) and styled empty-state panels with a Clear Filters action when filters hide everything
- Object navigation history with Back / Forward actions (`Alt + Left` / `Alt + Right`) and a Reveal In Browser action
- Bookmarks for objects, shown directly under the summary
- Browser and inspector filter state (search, custom-only, section, Modified, Core) now persists across reopening the editor
- Ability "insights"/help content rendered as a proper card with title, tag pills, and wrapped description text
- OE color scheme adjusted to fit the new style

#### Editing & UX
- Arrow key movement through the object tree now updates the right-side settings panel in sync; tree focus is preserved so keyboard navigation keeps working
- `Tab` switches focus between the object tree and the details pane; inactive details selection is rendered with a grey highlight
- The selected object field is remembered across object switches and restored when available; details scroll position is preserved when switching objects
- Field-name clicks no longer immediately open editors or force scroll recentering
- `Ctrl + C` / `Ctrl + V` to copy/paste selected object settings (type-compatible) and to copy/paste whole objects (e.g. abilities), including an ID prompt, duplicate-ID blocking, and a copied-name suffix
- `F2` to rename object entries inline directly in the left browser (no longer visually clipped)
- New custom object IDs now advance sequentially and reuse deleted gaps instead of using random IDs
- `TRIGSTR_*` names for existing map objects now resolve and display correctly, and trigger-string-backed names are preserved on edit
- Fixed duplicate objects syncing their renames
- Ability Buffs field now behaves like the Unit/Hero ability field — buffs are added/removed by picking from a searchable list of existing buffs
- Ability Option field properly handles existing values and supports selecting multiple ones
- Fixed the hotkey fields on the ability object
- Added a folder search for the Object Editor icon settings
- Fixed a crash that occurred when scrolling through certain map assets in the Object Editor

### Tooltip Editor (new)
A dedicated Tooltip Editor with a template manager for working with WC3 tooltip strings:
- Reads and pre-fills the correct values from existing objects' data
- WC3 color codes are syntax-highlighted in the input fields, with the `|cff…`/`|r` codes themselves muted to grey for clarity
- Color toolbar with preset colors and a custom color picker; selecting text and clicking a color wraps it in `|cff… … |r`, otherwise the code is inserted at the cursor
- Custom colors persist between sessions and appear as reusable color-swatch buttons
- Template manager supports creating, applying, and favoriting templates (up to 5), with inline quick-apply buttons shown next to the tooltip field
- Combined tip + ubertip preview with a divider; real newlines preview correctly

### Item Drops (new)
- Double-clicking a preplaced unit opens a small unit window
- An Item Drop Table window is available under the `Units` tab and when double-clicking preplaced units

### Model Editor
- Model Editor window is now much larger and more visible
- Added an animation dropdown to set which animation is played by placed effects

### Palettes & Sidebar
- Palettes reworked visually into a sidebar layout
- Unit Palette now has a filter for custom-created units
- Doodad Palette now has a custom tab showing only custom-created doodads
- Fixed a visual bug with the Doodad Palette dropdown

#### Sidebar redesign (new)
- All palettes (Doodads, Units, Terrain, Pathing, Regions) now share a single, cohesive right-side sidebar shell with consistent dark-panel styling for line edits, combo boxes, list views, and sliders
- Only one palette is active at a time; mode buttons are wrapped and more readable
- In-panel `ACTIVE SELECTION` / `ACTIVE REGION` summary cards added to the Doodads, Units, and Regions palettes; they update for no selection, mixed selection, and individual entries (including region bounds)
- Terrain and Pathing palettes reorganized into clearer labeled sections (Texture Paint, Cliff Tools, Height Tools, Water/Boundaries, etc.) with synced brush-size sliders
- A live preview for the selected doodad/unit is shown in the sidebar
- The minimap was moved into the sidebar; it can be toggled there and is off by default
- Arrow key navigation switches the model preview shown in the sidebar

### Header
- Header redesigned for a cleaner, more modern look

### Map Creation (new)
- Added a Map Creation function with a proper popup like the default World Editor
- The new-map popup shows the actual tiles from the selected tileset

### Doodads
- Placing a doodad no longer shows an annoying click-to-confirm helper before placement
- Doodads can be scaled using `Shift + Scroll`; works with multiple doodads selected at once
- Selected doodads are now highlighted visually
- Doodad drag placement now requires actual mouse movement instead of placing immediately
- Fixed doodad Z placement in the editor, which previously caused a mismatch in-game and differed from the original World Editor

### Terrain
- Terrain Copy/Paste functionality added (use with caution)
- Brush size can be increased or decreased with `Shift + Scroll`
- Fixed spacing issues with the terrain window/palette
- The smooth tool now flattens more gradually instead of instantly
- Water placement via the Add Water tool now accounts for different terrain heights
- Fixed a top-left corner visual color bug

### Fixes
- Fixed water pathing for units

### General UX / UI
- When switching panels, selection mode is now active by default
- Reworked Panel/Sidebar behavior to be more UX-conformant
- Saving the map now shows a fading on-screen message for clarity
- Last 3 opened maps are listed under `File` for quick access
- Debug view mode cleaned up; removed unimportant debug settings and added a display value for proper WC3 coordinates

### Shortcuts
- `R` — toggle the Region Palette
- `O` — open the Object Editor
- `Shift + Scroll` — increase/decrease brush size (terrain)
- `Shift + Scroll` — scale selected doodad(s)
- Object Editor: `Ctrl + C` / `Ctrl + V` — copy/paste selected object settings or whole objects
- Object Editor: `F2` — rename the selected object entry inline
- Object Editor: `Alt + Left` / `Alt + Right` — navigate Back / Forward through opened objects
- Object Editor: `Tab` — switch focus between the object tree and the details pane
- Object Editor: `Ctrl + F` — focus the left browser search; `Ctrl + Shift + F` — focus the right-side field search
- Added a Config tab listing all usable shortcuts in the editor
- Config window commands further categorized for clarity
- Descriptions on hover added for some header items under `View`, including shortcut hints

### Build Process
- Attempts to automatically launch the project after building and kills any previously running instance first (note: automatic launch is currently not working as intended — the old instance is killed but starting the new one fails with an error)

---

## AI-assisted development

Cursor agent guidance lives in [`AGENTS.md`](AGENTS.md) (index) and [`.cursor/`](.cursor/) (rules, skills, plans). Scoped rules load automatically when editing matching files; invoke skills explicitly for subsystem workflows.
