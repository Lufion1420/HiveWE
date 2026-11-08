---
description: Warcraft III map serialization, binary formats, and load/save invariants
paths:
  - "src/file_formats/**/*"
  - "src/base/map/**/*"
  - "src/base/triggers/**/*"
---

# HiveWE File Formats

WC3 compatibility is fragile — tiny ordering or field-size mistakes break saves.

## Owners

- Map load/save orchestration: `src/base/map/map.ixx` (`Map::load()` / `Map::save()` sequencing).
- Asset lookup: `src/base/hierarchy.ixx`.
- Binary helpers: `BinaryReader`, `BinaryWriter` in `src/base/`.
- MDL/MDX: `src/file_formats/mdx/`.
- Triggers/script: `src/base/triggers/`.
- Object SLK merges: `src/object_editor/slk_conversions.ixx`.

## Rules

- Read both load and save paths before editing one side.
- Preserve round-trip: MDL/MDX, SLK merges, W3* files, generated scripts.
- Do not silently discard unknown data unless existing code does intentionally.
- Keep bounds checks and explicit error propagation.
- Preserve Hierarchy lookup order unless task is specifically to change it.
- No "cleanup" refactors while fixing compatibility bugs.
- Mark uncertain format rules `Needs verification`; test with `data/test map/` or fixtures.

## Game version support

- As of 2026-11-08 HiveWE tracks formats through 2.0.3a only. 3.0 maps (w3i 39, doo and units 12/13, w3r 7, w3c 3, MDX v1800) are not supported. Details: `.claude/plans/wc3-3.0-compatibility.md`.
- The live game install on this machine is found through `find_warcraft_directory` at `C:/Program Files (x86)/Warcraft III`. The registry's Blizzard GamePath/Program/InstallPath values under `F:\` are stale.

## Tests

- `tests/binary_writer_test.cpp`, `tests/modification_tables_test.cpp`, `tests/mdl_reader_test.cpp`.
