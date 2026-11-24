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
- `resave_smoke` already runs inside `ctest --preset Agent`.
- `.ixx` files are CRLF in the working copy, so `git apply` of saved diffs fails; use Edit.

## Performance

- `Map::load()` has no `std::async`. The async work is in `src/base/stock_object_data.ixx`, which rethrows through `.get()` on the main thread.
- Large SLK/meta merges affect startup — avoid redundant work.
