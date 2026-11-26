---
description: C++ module conventions, naming, globals, and error handling in HiveWE
paths:
  - "**/*.ixx"
---

# HiveWE C++ Modules

Primary implementation units are `.ixx` modules; Qt UI stays in `.cpp`/`.h`.

## Style

- Tabs, width 4, line limit 140 (`.clang-format`, `tabs.editorconfig`).
- `snake_case` functions/fields/files; `PascalCase` types.
- Imports by logical name: `import Map;`, `import BinaryReader;`, `import Terrain;`.
- Do not auto-sort includes.
- `src/resources/texture.ixx` must keep its `#include` global module fragment, not `import std`: Qt `.cpp` files that include std headers import it (C2374 in `mocs_compilation`).

## Globals

Intentional globals: `map`, `camera`, `hierarchy`, `resource_manager`, `gl_thread_pool`. Do not add new globals casually.

`GLThreadPool::submit` futures come from a `packaged_task`, so unlike `std::async` futures they do NOT wait in their destructor. A loop over them that can throw or return early must use the exported `gl_thread_pool` `wait_for_all` (as `Doodads::create` and `Units::create` do), or workers keep running against freed state.

## Error handling by layer

- UI flows: `QMessageBox`.
- Parsing/resources: `std::expected<..., std::string>`.
- Low-level binary: exceptions on invalid bounds / I/O failure.
- Diagnostics: `std::println`.
- `Map` cannot be constructed in tests (`RenderManager` calls `init_gl`). Map-level checks must be free functions over the globals. `Terrain` needs GL too, so Change Tileset logic that tests cover lives in the GL-free `src/base/tileset_change.ixx` (tests in `tests/tileset_change_test.cpp`). Tests never construct `QApplication` or `QWidget`.
- Assertions are invariants in parsers — do not replace with silent fallbacks unless user-facing.

## Paths and Windows pitfalls

- On MSVC, `fs::path::string()`/`generic_string()` throw for characters outside the system code page (CJK/Cyrillic folders). In `src` use `wstring()`/`generic_wstring()`/`u8string()` and `QString::fromStdWString`. `toStdString()` into an `fs::path` garbles the same way: use `toStdWString()`. A fix must cover the whole chain (every caller and dialog on the path, and file names inside the map folder, for example the `Imports::save` listing). A Unicode-path test only goes RED on a non-UTF-8 code page.
- An exception escaping a Qt slot ends the program. When fixing one such throw, grep the whole call chain (Qt picker, labels, `utilities.ixx` error text), not only the named function.
- `path.extension() == ".blp"` is case-sensitive: fold case first.
- On MSVC, `fs::status`/`symlink_status` with an `error_code` set it for a missing path too (`fs::exists` clears it): treat `file_type::not_found` as no error.
- A multi-step delete of a set that code later recognises (for example a backup set) renames it first, so a partial delete cannot look complete.
- `resave_smoke` already runs inside `ctest --preset Agent`. Its cases in `tests/resave_smoke.cmake` share one sandbox: a case that plants a fixture must restore the original file before the next case.
- `.ixx` line endings are not uniform: most are CRLF in the working copy (so `git apply` of saved diffs fails; use Edit), but some, such as `units.ixx`, are LF. Check with `git ls-files --eol` before a scripted rewrite.
- `asset_obfuscation.ixx` has no `u8`/`u32` aliases: write `uint8_t`/`uint32_t`.

## Performance

- `Map::load()` has no `std::async`. The async work is in `src/base/stock_object_data.ixx`, which rethrows through `.get()` on the main thread.
- Large SLK/meta merges affect startup — avoid redundant work.
