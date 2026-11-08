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

- As of 2026-11-08 HiveWE tracks formats through 2.0.3a, plus a w3i v39 parser (commit 3a5c6ed). The rest of 3.0 (doo and units 12/13, w3r 7, w3c 3, MDX v1800) is not supported. Details: `.claude/plans/wc3-3.0-compatibility.md`.
- 3.0 maps are refused with a dialog, not crashed on, through doo, units, w3r and w3c. `src/base/map/map_format_check.ixx` defines every accepted format version set once, and `map_archive_check.ixx` checks archives before unpacking. When a new format version gets a parser, add it to the set.
- The header preflight only protects against wrong versions. Each version added to an accepted set turns its damaged bodies from a clean refusal into a `Map::load` crash, until bodies are validated before `reset_map_session()`.
- Readers failed two ways on unknown versions: a crash (w3i, doo, units, via `std::bad_alloc`) or a silent misread that a save writes back (w3r). The preflight now covers both, but "no crash" still does not mean safe.
- Stock maps are in CASC under `war3.w3mod:maps\...`, for example `war3.w3mod:maps\forsakenkingdom\scenario\(1)forgottenhollow.w3x` (w3i 39, editor 7003), and can be read byte-exact through CascLib. A map is 3.0 when `war3map.w3i` starts with u32 39.
- `Map::load` ignores the bool results of `Terrain::load` and `PathingMap::load`, so a false return is not a safe path. For w3e, `interpolated_height` then indexes empty corner vectors. For wpm, the map stays 0 by 0 and a save overwrites the real `war3map.wpm` with an empty one. `new Map()` value-initializes, so unset scalar members are 0, not garbage.
- `Doodads::load` zeroes `Doodad::auto_increment`, `Unit()` increments `Unit::auto_increment`, and `Triggers::load` zeroes `Trigger::next_id`. The readers therefore cannot be dry-run against an open map without saving and restoring these statics.
- From a Python replay of HiveWE's read order (not the C++ code) over the 466 stock 3.0.1 maps: 67 of the 465 wct files that parsed have trailing zeros, and units use `random_type` -1 and 641 with no payload. Neither is damage.
- StormLib: `SFileCreateArchive` always writes a `(listfile)` regardless of `MPQ_CREATE_LISTFILE`; only `SFileCreateArchive2` with `dwFileFlags1 = 0` omits it. `MPQ_OPEN_READ_ONLY` does not change the read and extract paths.
- The live game install on this machine is found through `find_warcraft_directory` at `C:/Program Files (x86)/Warcraft III`. The registry's Blizzard GamePath/Program/InstallPath values under `F:\` are stale.

## w3i facts (strict parse of 466 stock maps, 2026-11-08)

- Force flag: stock shared vision is 0x08. HiveWE treats it as 0x04 (ticket filed: "Forces: shared vision uses the wrong flag bit").
- v39 player `hud_skin` exists only when editor_version >= 6140. Samples: 6117 and 6127 have none; 6140, 7000, 7003 have it; 6128-6139 have no samples. A wrong gate still reaches EOF on most maps with names shifted, so test by comparing names, not EOF.

## Tests

- `tests/binary_writer_test.cpp`, `tests/modification_tables_test.cpp`, `tests/mdl_reader_test.cpp`, `tests/map_format_check_test.cpp`.
- The stock v39 sweep in `tests/map_format_check_test.cpp` passes silently when `HIVEWE_STOCK_W3I_ROOT` is unset. Confirm the "stock v39 files checked: N" message.
- `Doodads::load` and `Units::load` can be tested without GL if the `Terrain` is heap-allocated and never destroyed (its destructor calls GL). Both readers refuse unknown versions before using it.
