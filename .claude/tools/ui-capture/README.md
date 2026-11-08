# ui-capture

PowerShell helpers for driving and capturing the running HiveWE without SendInput. Used by `designer` ("Comparing to the running app" in `agents/designer.md`). No path is hardcoded.

- `capture-lib.ps1`: dot-source for `Cap-Window`, `Get-MyWindows` (PrintWindow capture, per-monitor DPI aware; also exposes `GetDpiForWindow` as `[W]::GetDpiForWindow`).
- `ui-act.ps1`: UIA `select|invoke|toggle|expand|list` by Name. Run as its own process (Invoke blocks on modal dialogs).
- `open-capture.ps1`: run an action detached, capture each new window, close with WM_CLOSE. `-Retry` only for idempotent actions.
- `ads-tab.ps1`: list or invoke ADS dock tabs by index (Invoke may not switch; use `post-click.ps1`).
- `post-click.ps1`: posted WM_LBUTTON click on a UIA element, no focus needed.
- `uia-dump.ps1`, `uia-window.ps1`: dump the UIA tree of a process or one window.

## Inspecting the real World Editor (verified 2026-11-08, WE 3.0.1)

- Its native menu bar is invisible to UIA (the main window is missing from the UIA top-level list) but readable across processes with `GetMenu`/`GetMenuItemInfo`. Do not combine `MIIM_TYPE` with `MIIM_STRING` in one mask: the call fails silently. Use `MIIM_ID|MIIM_STATE|MIIM_SUBMENU|MIIM_FTYPE|MIIM_STRING`.
- The menu is not `MNS_NOTIFYBYPOS`, so `PostMessage(hwnd, WM_COMMAND, wID, 0)` opens the item. `WM_CLOSE` on a module window hides it instead of destroying it. Dialogs close with `BM_CLICK` on Cancel.
- Asset Manager (F12) takes about 15 s to appear and the process reports not responding meanwhile; wait, do not kill it.
- Only with explicit user permission, and never save: with a map open, Test Map also saves it. Do not post it.

## Driving file-dialog flows (verified 2026-11-08, qa-verifier on HiveWE)

- Launch with a sandbox working directory (a copy of `data/`). Invoke the File button through UIA in a detached process.
- Qt popups and message boxes are not in UIA. Find their HWNDs with `EnumWindows` filtered by process ID, then post `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP` to the popup HWND.
- Posted client coordinates map to capture pixels at about x1.25 on a 125% DPI display, with a title-bar offset on dialogs. Hover does not render in `PrintWindow`. Calibrate per run; coordinates depend on DPI and the recent-map list.
- Native dialogs: fill with `WM_SETTEXT` (Open File edit id 1148, folder picker edit id 1152), then `BM_CLICK` on OK (id 1). Dismiss with `WM_CLOSE`.
- Qt dialogs: Alt+letter via `WM_SYSKEYDOWN` did nothing; a posted `WM_KEYDOWN` VK_RETURN accepted Create New Map.
- Qt dialogs are reachable only through `AutomationElement.FromHandle`, not by top-level name lookup.
- For an edit you can read back, toggle a checkbox such as Melee map in Map Description. Name, author and description edits may be dropped (ticket "Map Description drops edits when the name, author or description is a literal"). Needs verification: that this applies to `data/test map`.
- A UIA walk of the whole main window can exceed 60 seconds. Run it detached or scope to a dialog HWND.
- Never name a PowerShell function `H` or `Cls`: they are built-in aliases, so the call silently runs something else.
- Posted-message brush strokes only draw a brush preview and do not register in `world_undo`, so they cannot create a tracked unsaved edit. Brush, copy/paste and undo checks need real mouse input.
- Qt File popups close when focus shifts, so invoke File and post the click in the same script.
- UIA can Invoke ribbon buttons only on the selected tab, so select the tab first with `ui-act.ps1 select -Type TabItem`. Map Protector is on Home, Item Tables is on Units.
- A `Start-Process` argument list needs the whole string quoted, or names with spaces are silently dropped.
- Qt combo popups ignore posted mouse and key messages and UIA select, so a combo choice cannot be committed without real input.
- No `world_undo`-tracked edit was reachable without real mouse input on `data/test map`: it has no doodads and Ctrl+A skips `sloc`. Map Options, Modify Map Bounds does reach `Map::resize` without the mouse.
- Map Protector's saved settings can point at the user's real map. Reset the Source and Output fields and read them back before exporting.
- MPQ test archives can be built with `StormLib.dll` through ctypes.

### Safety: recent maps (incident 2026-11-08)

The File popup lists recent maps directly below Open Map (MPQ). A calibration click hit a recent entry and loaded the user's real map folder into the test instance. Before driving the File popup:

1. Export `HKCU\Software\HiveWE\HiveWE` (QSettings org and app; not `HKCU\Software\HiveWE`, which an emptied run left untouched while the popup showed real maps), then empty its `recentMaps` value so a mis-click cannot open a real map. Read the value back and confirm it is empty before driving the File popup. In PowerShell 5.1 `reg add /d ""` drops the empty argument and blocks on a prompt. Use `[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey(path, $true).SetValue('recentMaps',[string[]]@(),'MultiString')`.
2. Restore the export at the end, also when the run fails.

All helpers must refuse any HWND not owned by the started process ID. Never use SendInput or SendKeys.

Captures are written to a `captures\` folder next to the scripts. Copy the scripts into your scratchpad and run them there so captures stay out of the repo. Launch HiveWE only from a sandbox working directory (see `skills/hivewe-build-and-debug`).
