# hivewe-build-and-debug

## Purpose
Use this skill for local build, test, and debugging workflows in HiveWE. The repo is Windows-first, uses CMake presets, `vcpkg`, Qt 6, and modern C++ modules.

## When to use this skill
- You need to build or test after making changes.
- You are diagnosing compile errors, linker errors, missing dependencies, or preset issues.
- You are working on performance-sensitive code and need the profiling path.

## Relevant folders/files
- `README.md`
- `CMakeLists.txt`
- `CMakePresets.json`
- `vcpkg.json`
- `overlay-ports/`
- `.github/workflows/compile.yml`
- `tests/`

## Workflow
1. Check build prerequisites:
   - Visual Studio 2022 17.14+.
   - `VCPKG_ROOT` set.
2. Prefer repo presets instead of ad hoc CMake flags:
   - `cmake --preset Release`
   - `cmake --build --preset Release`
3. Run tests with:
   - `ctest --preset Release`
4. For debugging runtime issues, use `Debug`.
5. For performance work, use `ReleaseTracing` and keep Tracy-related behavior intact.
6. If dependency resolution fails, inspect `vcpkg.json` and `overlay-ports/` before changing source code.
7. If Visual Studio folder-open builds behave differently, remember the post-build `data` symlink requirement from the README.

## Coding rules
- Do not change preset structure or dependency declarations unless the task actually requires it.
- Treat overlay ports as intentional patches, not incidental build clutter.
- Keep CI parity in mind; `.github/workflows/compile.yml` is the clean reference path.

## Verification
- Reconfigure after changing CMake or dependency files.
- Run the narrowest useful test first, then the full preset test run if feasible.
- For runtime/UI changes, launch the built app and open the affected editor or map workflow.

## Common mistakes to avoid
- Building with custom flags when a preset already encodes the intended configuration.
- Forgetting that this repo depends on `vcpkg` overlay ports.
- Assuming Linux/macOS is the primary workflow; current docs and CI are Windows-focused.
