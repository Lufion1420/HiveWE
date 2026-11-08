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

## Known gap (2026-11-08)

HiveWE's File ribbon popup menu does not appear in the process's UIA tree, so `qa-verifier` could not drive file-dialog flows this way. A method might still exist.

Captures are written to a `captures\` folder next to the scripts. Copy the scripts into your scratchpad and run them there so captures stay out of the repo. Launch HiveWE only from a sandbox working directory (see `skills/hivewe-build-and-debug`).
