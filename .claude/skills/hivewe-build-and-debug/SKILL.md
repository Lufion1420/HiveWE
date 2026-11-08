---
name: hivewe-build-and-debug
description: >-
  Guides HiveWE local build, test, and debug on Windows with CMake presets and vcpkg.
  Use when diagnosing compile/link errors, preset issues, dependency failures, or
  Tracy profiling — or when asked to build or test.
---

# HiveWE Build and Debug

## Key files

`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `overlay-ports/`, `.github/workflows/compile.yml`, `tests/`

## Workflow

1. Prerequisites: VS 2022 17.14+, `VCPKG_ROOT` set.
2. Agents build and test **only with the `Agent` preset** (`build/Agent`, no auto-launch):
   - `cmake --preset Agent` / `cmake --build --preset Agent`
   - `ctest --preset Agent`
3. **Never build `Release` as an agent:** its auto-run step force-kills a running `HiveWE.exe`. One build at a time. Rules: `.claude/rules/harness-guardrails.md`.
4. `Debug` (runtime issues) and `ReleaseTracing` (Tracy profiling) are for the user to run. Pasted build errors from the user: fix those first.
5. Dependency failures → inspect `vcpkg.json` and `overlay-ports/` before source changes.
6. VS open-folder: post-build `data/` symlink may need Administrator.
7. `LNK1104` on `HiveWE.exe` → often a running editor holding the lock.

## Running HiveWE and CI

- `src/main.cpp` always loads `data/test map/` relative to the working directory (line 154) and writes `data/tools/` (lines 108-111). **Never launch HiveWE with the repo as working directory.** Use a sandbox folder with a copy of `data/`.
- To test any map without UI driving, replace the sandbox's `data/test map/` with the unpacked map. A stock 3.0 map (e.g. `(1)ForgottenHollow`) used to crash HiveWE (0xc0000409) about 2 seconds after start; it now shows the refusal dialog and exits with code 1 (see `rules/hivewe-file-formats.md`).
- Crash debugging: the `Agent` preset produces no PDB. `cdbX64.exe` from the WindowsApps alias works, and `.exr -1` names the C++ exception class.
- Use PowerShell for `reg`: Git Bash rewrites `HKCU\Software\HiveWE` into an invalid key.
- `CloseMainWindow` can be swallowed by an open ribbon popup, so confirm the process really exited.
- QSettings lives in `HKCU\Software\HiveWE` and holds the user's real layout and recent maps, so running HiveWE changes it. `reg export` before, `reg import` after.
- CI (`.github/workflows/compile.yml`) builds and tests on every push to `main` and publishes nothing. New pushes cancel running ones. On a cold vcpkg cache, Configure CMake took about an hour on 2026-11-08.

## Verification checklist

- [ ] Reconfigure after CMake/dependency edits
- [ ] Narrowest test first, then full preset if feasible
- [ ] Runtime/UI: the user launches the app and opens affected workflow
- [ ] If the user pastes errors: interpret them, give a targeted next fix

## Common mistakes

- Custom CMake flags when a preset already encodes the config.
- Forgetting vcpkg overlay ports.
- Assuming Linux/macOS primary workflow — docs and CI are Windows-focused.

## Decision timeline

One line per ICE or overlay-port decision: date, what failed, what was decided and why. Check here before reopening one.

- 2026-11-08: MSVC LTCG hit an ICE on `HiveWE_tests`; IPO is off for that target.
