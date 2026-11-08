---
description: HiveWE build, test, and dependency workflow on Windows
---

# HiveWE Build Policy

Windows-first: Visual Studio 2022 17.14+, `VCPKG_ROOT`, overlay ports in `overlay-ports/`.

## Agent behavior

- Agents may build and test, **only with the `Agent` preset** (`build/Agent`, no auto-launch): `cmake --preset Agent`, `cmake --build --preset Agent`, `ctest --preset Agent`. See `.claude/rules/harness-guardrails.md`.
- **Never build `Release` as an agent:** with `HIVEWE_AUTO_RUN_AFTER_BUILD=ON` it force-kills a running `HiveWE.exe`, losing unsaved map work. `.claude/settings.json` denies it.
- One build at a time: agents share `build/Agent`.
- Prefer fixing compile issues from pasted build errors first when the user pastes them.

## Presets

`Debug`, `Release`, `ReleaseTracing` (Tracy profiling) are for the user. `Agent` is Release settings for automated builds.

## Dependency / CI notes

- Custom vcpkg overlay ports are intentional patches — inspect before changing source.
- CI reference: `.github/workflows/compile.yml`.
- VS open-folder builds may need Administrator (post-build `data/` symlink).
- `LNK1104` on `HiveWE.exe` is often a running editor holding the file lock.

## Verification

- Reconfigure after CMake or dependency changes.
- Narrowest test first, then full preset if feasible.
- For runtime/UI changes, the user launches the built app manually; `qa-verifier` hands over exact steps.
