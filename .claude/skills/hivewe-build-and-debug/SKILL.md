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
