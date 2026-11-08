# ui-capture

PowerShell helpers for driving and capturing the running HiveWE without SendInput. Used by `designer` ("Comparing to the running app" in `agents/designer.md`). No path is hardcoded.

- `capture-lib.ps1`: dot-source for `Cap-Window`, `Get-MyWindows` (PrintWindow capture, per-monitor DPI aware; also exposes `GetDpiForWindow` as `[W]::GetDpiForWindow`).
- `ui-act.ps1`: UIA `select|invoke|toggle|expand|list` by Name. Run as its own process (Invoke blocks on modal dialogs).
- `open-capture.ps1`: run an action detached, capture each new window, close with WM_CLOSE. `-Retry` only for idempotent actions.
- `ads-tab.ps1`: list or invoke ADS dock tabs by index (Invoke may not switch; use `post-click.ps1`).
- `post-click.ps1`: posted WM_LBUTTON click on a UIA element, no focus needed.
- `uia-dump.ps1`, `uia-window.ps1`: dump the UIA tree of a process or one window.

Captures are written to a `captures\` folder next to the scripts. Copy the scripts into your scratchpad and run them there so captures stay out of the repo. Launch HiveWE only from a sandbox working directory (see `skills/hivewe-build-and-debug`).
