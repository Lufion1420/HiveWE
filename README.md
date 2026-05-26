# HiveWE (Fork)

This is a personal fork of [stijnherfst/HiveWE](https://github.com/stijnherfst/HiveWE).

---

## Changes in this fork

### Regions
- Added Regions and a Region Palette
- Regions are now colored for visual distinction
- Regions are only visible when the Region Palette is open
- Double-clicking a region in the list moves the camera to it
- Layered regions now prioritize the smaller region on click
- Region movement is wired into the undo/redo system
- Added cursor feedback for drag and move actions on regions
- Player Start Locations implemented as a region type
- `R` shortcut added to toggle the Region Palette

### Object Editor
- `O` shortcut added to open the Object Editor
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

### Model Editor
- Model Editor window is now much larger and more visible
- Added an animation dropdown to set which animation is played by placed effects

### Palettes & Sidebar
- Palettes reworked visually into a sidebar layout
- Unit Palette now has a filter for custom-created units
- Doodad Palette now has a custom tab showing only custom-created doodads
- Fixed a visual bug with the Doodad Palette dropdown

### Doodads
- Placing a doodad no longer shows an annoying click-to-confirm helper before placement
- Doodads can be scaled using `Shift + Scroll`; works with multiple doodads selected at once

### Terrain
- Terrain Copy/Paste functionality added (use with caution)
- Brush size can be increased or decreased with `Shift + Scroll`
- Fixed spacing issues with the terrain window/palette

### General UX / UI
- When switching panels, selection mode is now active by default
- Reworked Panel/Sidebar behavior to be more UX-conformant
- Saving the map now shows a fading on-screen message for clarity
- Last 3 opened maps are listed under `File` for quick access
- Descriptions on hover added for some header items under `View`, including shortcut hints
- Debug view mode cleaned up; removed unimportant debug settings and added a display value for proper WC3 coordinates

### Config & Shortcuts
- Added a Config tab listing all usable shortcuts in the editor
- Config window commands further categorized for clarity

### Build Process
- Building the project now automatically launches it; any previously running instances are killed first
