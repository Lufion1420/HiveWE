# Designs

HTML/CSS mockups of the HiveWE UI, rendered to PNG headlessly. Pencil (`.pen`) was dropped on 2026-11-22 (reason in `.claude/rules/harness-guardrails.md`, "Designs").

```
designs/
  current/      As-built baseline: real-app screenshots taken with .claude/tools/ui-capture/ (refill when next updated)
  proposals/    Mockups for planned work, one folder per feature
    <name>/
      index.html (or several pages)
      tokens.css      One token file (CSS custom properties) for the Dark and Light themes
      icons/*.svg     The same SVG files the app will ship (qtsvg)
      renders/*.png   Rendered output the user approves
  archive/pencil/   Old Pencil baseline (.pen plus assets); encrypted, never Read or Grep
```

## Process

1. Direction mockups go in `proposals/<name>/` and are edited as plain text.
2. Gate 1: the user approves the PNGs in `renders/` (direction pick).
3. Gate 2: the first `cpp-dev` step of an approved UI change is a real Qt prototype (the actual window with the theme QSS applied), screenshotted with `.claude/tools/ui-capture/`. The user approves that screenshot before the rest of the UI work proceeds.
4. After a change ships, refresh the affected screenshots in `current/`.
- Frames and pages are named `<Window> / <State>` (file name or page title).

## QSS-expressible CSS only

Everything must be buildable with Qt Widgets and `data/themes/*.qss`:
- No box-shadow, blur or filter, backdrop, transforms, transitions or animation, and no CSS grid tricks Qt layouts cannot do.
- `border-radius` only where Qt stylesheets support it; gradients only as linear gradients that map to `qlineargradient`.
- Fonts: the bundled UI font plus system fallback.
- Icons: SVG files, the same ones the app will ship.

## Rendering (headless, no window)

Use the Playwright-installed Chromium (or `msedge`); the Python and npm playwright packages are not installed and must not be required:

```
%LOCALAPPDATA%\ms-playwright\chromium-<newest>\chrome-win\chrome.exe --headless --disable-gpu --hide-scrollbars --window-size=W,H --screenshot=<abs png> file:///<abs html>
```

Use absolute paths. Add `--virtual-time-budget=8000` so web fonts load before the screenshot. Pick the newest `chromium-*` folder. Needs verification: the exact folder name on a given machine.
