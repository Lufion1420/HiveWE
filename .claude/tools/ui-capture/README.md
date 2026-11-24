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
- Allowed since 2026-11-09 on sandbox map copies to check that HiveWE output loads (launch, load, capture, read errors), after the orchestrator has told the user in chat. Never save and never touch real maps: with a map open, Test Map also saves it. Never post it.

## Launching the real editor and game (verified 2026-11-09)

- World Editor: run `"World Editor.exe" -launch -loadfile "<map>"` from `_retail_\x86_64`. Without `-launch` it exits and starts Battle.net.
- WE menu IDs for `WM_COMMAND`: Layer > Regions 771, Layer > Cameras 772, Close Map 64189. The Tool Palette window lists region and camera names.
- Launching `Warcraft III.exe` directly normally shows a login screen the user cannot use. `Battle.net.exe --exec="launch W3"` reaches the logged-in menu, but the game ignores posted clicks. For load checks see "Real game load check (3.0.1)" below.
- For the user's in-game check, put the map in `Documents\Warcraft III\Maps\<named subfolder>`. The game lists maps by internal name, so subfolders tell copies apart. Clean up afterwards and verify by listing the folder: QA once left `hwe_qa_*.w3x` copies in `Documents\Warcraft III\Maps` while reporting them removed.

## Real game load check (3.0.1)

Announce the run in chat first ("do not click or type in the window"); sandbox copies only (guardrails "Verification without the user").

- World Editor route (verified 2026-11-24): `World Editor.exe -launch -loadfile "<map folder>"` from `C:\Program Files (x86)\Warcraft III\_retail_\x86_64`. UIA and PrintWindow work on it. Never save, never Test Map. Posted mouse messages do NOT click in the game main menu.
- Game route (used successfully 2026-11-24; the user pointed to it): WarMash `d:\Stuff\Warcraft_3_All_Assets\Projects\WarMash\.claude\tools\wc3ref\run_reference.ps1 -NoInject -Foreground -ForceKeysAfterSec 40`. It runs `Warcraft III.exe -launch -loadfile <packed .w3x> -window -nowfpause` with the saved in-game Battle.net login (no login = stuck at the login screen). The game picks a random race. A map folder must first be packed into a `.w3x` with StormLib `SFileCreateArchive` + `SFileAddFileEx` (MopaqPack-rs cannot create from nothing). For a JASS map pass `-NoStartSec 0 -StallSec 0`. The run ends by timeout (about 150 s); the map is reached after about 36 s.
- The harness guardrail still says launch via Battle.net, never the exe directly; the user allowed the WarMash direct launch for the 2026-11-24 run. Needs the user to confirm it as the standing method.

## Driving file-dialog flows (verified 2026-11-08, qa-verifier on HiveWE)

- Launch with a sandbox working directory (a full copy of `data/`, see `skills/hivewe-build-and-debug`).
- The ribbon header "No map loaded" label is stale at startup; check the window title or saved files instead.
- The Regions palette toggle via UIA checks the box, but the palette is not visible to UIA or capture (needs an in-process hook). Invoke the File button through UIA in a detached process.
- Qt popups are not in UIA (message boxes are reachable by HWND, see the next bullet). Find their HWNDs with `EnumWindows` filtered by process ID, then post `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP` to the popup HWND.
- Posted client coordinates map to capture pixels at about x1.25 on a 125% DPI display, with a title-bar offset on dialogs. Hover does not render in `PrintWindow`. Calibrate per run; coordinates depend on DPI and the recent-map list.
- Native dialogs: fill with `WM_SETTEXT` (Open File edit id 1148, folder picker edit id 1152), then `BM_CLICK` on OK (id 1). Dismiss with `WM_CLOSE`.
- Qt dialogs: Alt+letter via `WM_SYSKEYDOWN` did nothing; a posted `WM_KEYDOWN` VK_RETURN accepted Create New Map.
- `QMessageBox` windows from normal (non-hook) paths are drivable offscreen: find the window by process ID and class `Qt6102QWindowIcon`, `AutomationElement.FromHandle`, then Invoke the OK button. The startup map load in `src/main.cpp` shows "Opening map failed" this way (try/catch around `map->load`, exit code 1).
- Qt dialogs are reachable only through `AutomationElement.FromHandle`, not by top-level name lookup.
- For an edit you can read back, toggle a checkbox such as Melee map in Map Description. Name, author and description edits may be dropped (ticket "Map Description drops edits when the name, author or description is a literal"). Needs verification: that this applies to `data/test map`.
- A UIA walk of the whole main window can exceed 60 seconds. Run it detached or scope to a dialog HWND.
- Never name a PowerShell function `H`, `R` or `Cls`: they are built-in aliases (`R` is `Invoke-History`), so the call silently runs something else.
- Map Protector opens via `post-click.ps1`, not `ui-act.ps1` invoke. The ribbon Save button plus the `ribbonMapMeta` text confirm which map path the editor is on.
- Posted-message brush strokes only draw a brush preview and do not register in `world_undo`, so they cannot create a tracked unsaved edit. Brush, copy/paste and undo checks need an in-process test from `cpp-dev`, not the user (guardrails, 2026-11-09).
- Qt File popups close when focus shifts, so invoke File and post the click in the same script.
- UIA can Invoke ribbon buttons only on the selected tab, so select the tab first with `ui-act.ps1 select -Type TabItem`. Map Protector is on Home, Item Tables is on Units.
- A `Start-Process` argument list needs the whole string quoted, or names with spaces are silently dropped.
- Qt combo popups ignore posted mouse and key messages and UIA select, so a combo choice cannot be committed without real input.
- No `world_undo`-tracked edit was reachable without real mouse input on `data/test map`: it has no doodads and Ctrl+A skips `sloc`. Map Options, Modify Map Bounds does reach `Map::resize` without the mouse.
- Map Protector's saved settings can point at the user's real map. Reset the Source and Output fields and read them back before exporting.
- The ribbon Export Objects is a QToolButton with InstantPopup, so UIA Invoke does nothing. Open it with `post-click.ps1`, then post a click to the popup HWND (class `Qt6102QWindowPopupDropShadowSaveBits`) at client (60,14) for the first item.
- The folder picker defaults to the user's real map path: always set edit 1152. Empty `recentMaps` first and restore it afterwards (safety section below).
- UIA does not expose `ColorButton` swatch values. Read them by sampling `PrintWindow` capture pixels at the element's bounding rect, scaled by capture size divided by window rect.
- `ui-act.ps1` works on Qt dialogs (select, toggle, invoke OK); `post-click.ps1` fails on them because it finds the dialog by name.
- A Qt QComboBox exposes only the Invoke, Value and ExpandCollapse patterns. The Value pattern returns the displayed text, so it confirms a selection; popup items cannot be listed or picked.
- `data/test map` w3i (v33) has weather_id at byte offset 0xB3. Patching `RAlr` there in a sandbox copy gives a weather test input (no stock map with RAlr was found).
- MPQ test archives can be built with `StormLib.dll` through ctypes.
- `ui-act.ps1` accepts `-Like` on the Trigger Editor toolbar; the Create Variable button's UIA name is `actionCreateVariable`. A variable's type is not exposed through UIA (the explorer tree shows names only), so verify it from the saved wtg or script. The Output pane (QPlainTextEdit) returned no text through UIA.

### Safety: recent maps (incident 2026-11-08)

The File popup lists recent maps directly below Open Map (MPQ). A calibration click hit a recent entry and loaded the user's real map folder into the test instance. Before driving the File popup:

1. Export `HKCU\Software\HiveWE\HiveWE` (QSettings org and app; not `HKCU\Software\HiveWE`, which an emptied run left untouched while the popup showed real maps), then empty its `recentMaps` value so a mis-click cannot open a real map. Read the value back and confirm it is empty before driving the File popup. In PowerShell 5.1 `reg add /d ""` drops the empty argument and blocks on a prompt. Use `[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey(path, $true).SetValue('recentMaps',[string[]]@(),'MultiString')`.
2. Restore the export at the end, also when the run fails.

All helpers must refuse any HWND not owned by the started process ID. Never use SendInput or SendKeys.

Captures are written to a `captures\` folder next to the scripts. Copy the scripts into your scratchpad and run them there so captures stay out of the repo. Launch HiveWE only from a sandbox working directory (see `skills/hivewe-build-and-debug`).

`wins.ps1` (copied from a session scratchpad 2026-11-09): window-listing helper used for the UIA-only open/save flow (ribbon Open, Open Map Directory edit 1152 via WM_SETTEXT, button id 1). Needs verification that it still runs unchanged.

## Offscreen QA runs (user request 2026-11-22: test windows must not disturb the user)

- Launch HiveWE only with `--qa-offscreen --theme "<name>" --palette <terrain|doodads|units|pathing|regions> --window-size WxH`. `--qa-dump-layout <file>` writes widget widths and the markers `OVER_VIEWPORT` and `DEEPEST_OFFENDER`.
- Start it with .NET `ProcessStartInfo` (`UseShellExecute=false`, `CreateNoWindow=true`). `Start-Process` on the console-subsystem exe pops a console window that takes foreground; `-WindowStyle Hidden` leaves the Qt window unpainted.
- No UIA Toggle or Invoke on the window (UIA Toggle activates even an off-screen no-activate window), no `SetForegroundWindow`, no F5 theme reload: restart with another `--theme`.
- Sample `GetForegroundWindow` after launch to prove there was no focus steal.
- Wait at least 8 s after a resize before `PrintWindow`, and retry blank (about 20 KB) captures. Use a tall `--window-size` (for example 1500x1700) to see palette controls below the fold.
- Under `--qa-offscreen`, theme-failure messages go to stderr. A minimized Qt window reports a 199x34 rect.
- Before copying the exe or snapshotting the registry, check for a leftover sandbox `HiveWE.exe` by path. The sandbox `data/themes` is a copy: copy changed themes in before recapturing. Capture hooks (`--qa-open-dialog <name>`, `--qa-dialog-tab <n>`; both need `--qa-offscreen`) must not write QSettings under `--qa-offscreen`. For long captures use `dshot.ps1` (a scratchpad script, not in the repo) with `run_in_background`.
- `data/test map` has no units except the start location (`sloc`) and no doodads (`war3map.doo` is 24 bytes, header only). For doodad save checks, swap `tests/fixtures/resave/skinned_doodads.doo` in as `war3map.doo` in a scratch copy. `--qa-select-first` (needs `--qa-offscreen`, exits 2 without it; use with `--palette units|doodads`) adds a sample footman (`hfoo`) or `LTlt` doodad to the in-memory map and selects it, so selection-dependent UI can be captured. Never saved.
- `--qa-open-dialog <name>` (needs `--qa-offscreen`, exits 2 without it): opens one dialog or tool window with sample data after the map loads, so no clicks are needed. Names: `settings`, `unit-properties` (adds an in-memory Archmage `Hamg` with two item sets), `item-drop` (Edit Drop Item, opened from the event loop), `map-settings`, `scenario`, `gameplay-constants`, `new-map`, `object-editor`, `asset-manager`, `map-protector`, `item-tables`, `config`, `tile-setter`, `tile-pather`; `object-editor` opens the details of stock unit `hfoo` after 2 s (via `open_by_id`; `select_id` crashed offscreen with an access violation, cause not found); `import-object-data` opens with an empty package. `map-protector` shows the Source/Output paths saved in the registry, so a capture can contain a real map path: check before committing. `--qa-dialog-tab <n>` selects tab n (0-based) of the window's first `QTabWidget`. Nothing is accepted, so nothing is saved. Dialogs are parented to the main window (centred off screen) and parentless windows are moved off screen by the `NoActivateFilter`. Capture the dialog HWND, not the main window: filter `Get-MyWindows` for a title that is not `HiveWE 0.21` (the Map Settings title is "Map Settings").
- `--qa-open-dialog model-editor --qa-model-editor-script "<steps>" --qa-script-out <file> [--qa-model <path>]` (needs `--qa-offscreen`; exits 2 on an empty out path, 3 on a failed open): drives the Model Editor in-process. Steps are in `src/model_editor/model_editor.cpp`. `save-mdx:<name.mdx>` runs the Save to MDX write path (`ModelEditor::write_active_mdx`, the modal file dialog is not drivable offscreen) into a plain `.mdx` file name beside the `--qa-script-out` file, re-reads and re-parses it with `mdx::MDX`, and logs `PASS|FAIL` comparing geoset, sequence and bone counts with the open model.
- Never combine `--resave` with `--qa-offscreen`: the run hangs. Use `--resave` alone (no window is shown).
- Dialog captures do not need the 1.3 GB `build/Agent/Release`: use a sandbox folder with only the exe, the DLLs and a copy of `data/` (about 43 MB) plus the `platforms/`, `styles/` and `imageformats/` folders from `build/Agent/Release` and `data\warcraft`; without them you get a "no Qt platform plugin" box or crash 0xC0000409 about 3 s after start.
- `--qa-change-tileset <out> --qa-tile-texture <blp|tga> --qa-script-out <file>` (needs `--qa-offscreen`; exit 0 only if all checks PASS): grows the startup map to 20 tiles, imports the texture on a kept and an added tile, runs the dialog OK path after Save-As to `<out>`, saves, reloads and writes PASS/FAIL lines. Run from a COPIED sandbox working dir and re-copy `data/test map` before EACH run: `build/Agent/data` is a symlink to the repo's `data/`, so an uncopied run would write into it.
- `--qa-doodad-undo-check --palette doodads --qa-offscreen [--qa-script-out <file>]` (exits 2 without `--qa-offscreen` and `--palette`; exit 0 only if all checks PASS): adds three `LTlt` doodads in memory, selects them in the `DoodadBrush`, and checks `delete_selection` (count, empty selection), undo (same ids, positions, creation numbers), redo, a second undo, and `cut_selection` plus undo. Never saves, so it can run from `build/Agent` or a sandbox. Verified RED when the `DoodadDeleteAction::doodads` fill loop in `delete_selection` is commented out (undo checks FAIL).
- `--qa-refusal-checks --qa-offscreen` (exit 0 pass, 1 fail, 2 missing offscreen, 3 watchdog): runs 5 items: the 5-byte id save refusals, undo after a refused map load, and item 5, a failed `Map::load` after the body check leaves a blank Untitled map. The hook copies `data/test map` inside the sandbox: re-copy `data/` before each run, or it runs against a leftover map. Run from a sandbox working directory: exe, DLLs, `platforms/`, `styles/`, `imageformats/` and a copy of `data/` (no `HiveWE_lib.lib`). Read the real exit code via `ProcessStartInfo`; `robocopy` exit 1 means success.
- `--qa-object-new-check --qa-offscreen` (same exit codes and sandbox as above): checks that a `hfoö` id keeps the Add dialog's OK disabled and creates nothing (`ObjectEditor::build_new_object_dialog`).
- Hooks `--qa-open-dialog tile-setter-changes` (opens the Change Tileset dialog) and `tile-setter-limits` (with `--qa-script-out <file not ending .png>` prints the OK/Remove enabled states and quits).
- `--qa-open-dialog browse-models --qa-offscreen --qa-script-out <file not ending .png>`: opens the Browse Models dialog in-process (exit 2 on bad args). `from_disk=0` in the hook (disk thumbnail cache is off offscreen).
- Browse Models soak and leak env vars (read in `src/model_editor/browse_models_dialog.cpp`, only on the `browse-models` hook path; set them in the `ProcessStartInfo` environment):
  - `HIVEWE_QA_HOLD_SECONDS=<n>`: after the thumbnail checks, keep the dialog open and animating for n seconds, logging `INFO [soak] t=.. working_set=.. private=.. tiles=.. frames=.. resources=..` every 5 s. Clamped to 0..3600; unset or 0 skips the hold.
  - `HIVEWE_QA_REOPEN_CYCLES=<n>`: number of close-and-reopen cycles in the reopen leak check that runs after the hold. Clamped to 1..100; default 5 when unset (a non-numeric value reads as 0 and becomes 1). Logs `PASS|FAIL [reopen]` lines: working set growth under 50 MB over all cycles, and cached resources not growing after the first cycle.
  - Example (PowerShell, then read the `--qa-script-out` file): `$psi.Environment["HIVEWE_QA_HOLD_SECONDS"]="120"; $psi.Environment["HIVEWE_QA_REOPEN_CYCLES"]="20"; $psi.Arguments='--qa-offscreen --qa-open-dialog browse-models --qa-script-out "<scratch>\bm.log"'`. Budget wall-clock for hold plus cycles (each cycle takes at least about 2 s); use `run_in_background`.
- Thumbnail and bulk-render code facts: `ResourceManager` keeps textures until `clear()`, so bulk renderers evict with `snapshot_keys()` / `evict_unused_since()` (main thread, thumbnail GL context current). `hive::unordered_map` erase: collect keys first, then erase. Use `QString::toLower` and the u8 helpers for non-ASCII paths (`fs::path::string()` uses the ANSI code page on MSVC). `SkeletalModelInstance` needs about 3 `update()` calls on a fresh skeleton before world matrices settle.
- In-process QA hook rules: an unexposed (offscreen) `QOpenGLWidget` stops its `frameSwapped` repaint loop and `repaint()` does not call `paintGL`; `grabFramebuffer()` does, so pump frames with it. File arguments in script steps need their own path check (a plain `.png` name beside the out file). Never use a modal `QMessageBox` on an offscreen hook path (it hangs): return the error through an out-parameter.
- `shot.ps1`: do not pipe it into `Select-Object` (the pipeline stops before the capture and the old PNG stays; check PNG timestamps). Call `& shot.ps1 ...` once per command. Capture launch exit codes with `WaitForExit(30000)` and no Kill fallback.
- Background subagents cannot be prompted, so `.claude/settings.local.json` needs allow rules for the Agent-preset cmake/ctest and `build.lock` commands, or they get PowerShell denials. Compound commands containing a kill are denied. It allows only simple commands starting with `cmake --preset Agent`, `cmake --build --preset Agent`, `ctest --preset Agent`, `New-Item`/`Remove-Item`/`Test-Path` on `build.lock`/`HOLD`, and `*shot.ps1*`. Chained one-liners are denied (a qa-verifier run got COULD-NOT-RUN), so briefs must tell QA to send single commands or use a script named `*shot.ps1` (for example `hookshot.ps1 -Mode pos|neg|badmodel`).
