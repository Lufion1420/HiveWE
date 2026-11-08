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

- As of 2026-11-08 HiveWE tracks formats through 2.0.3a, plus a w3i v39 parser (commit 3a5c6ed). doo v12/v13 has a reader and writer. The rest of 3.0 (units 12/13, w3r 7, w3c 3, MDX v1800) is not supported. Details: `.claude/plans/wc3-3.0-compatibility.md`.
- 3.0 maps are refused with a dialog, not crashed on, through doo, units, w3r and w3c. `src/base/map/map_format_check.ixx` defines every accepted format version set once, and `map_archive_check.ixx` checks archives before unpacking. When a new format version gets a parser, add it to the set.
- The header preflight only protects against wrong versions. `src/base/map/map_body_check.ixx` validates bodies through the real parsers before teardown (`reset_map_session()`); a new accepted version must be added there too. Throws after validation passes are tracked in the ticket "Map load can still throw after body validation passes".
- Stock sweeps stop at the version refusal, so a `check_map_for_opening` sweep says nothing about the bodies of version-refused maps (191 of 466 stock maps, doo v12/v13). All 191 have doo 12/13, units 12/13, w3r 7 and w3c 3, so none opens until all of doo, units, w3r and w3c are supported, and the readable-parts pass is needed until then.
- `Doodad::state_from_flags` still clamps flag state 3 to 2 for the `state` enum, but `Doodad::file_flags` keeps the raw byte and `flags()` writes it back, so state 3 and bit 0x08 survive saves; a changed state still takes effect. Flag values 3 and 7 occur on 146,036 stock doodads.
- `group_id` is exactly 0xFFFFFFFF on all 546,722 stock v12/13 doodads and doodad `unk` is -1 on all 169,423 v13 doodads. One fresh 3.0.1 World Editor save (2026-11-08, editor 7003, game 3.0.1.24371) with two user-placed doodads also has group_id 0xFFFFFFFF, unk -1, roll 0, pitch 0, no lights, flag 0x02. That is one map. The meaning of group_id and unk is unknown, so non-default values are preserved.
- doo save policy: v7 and v8 save as (8,11), v12 as (12,11), v13 as (13,11), a new map as (8,11). This keeps the w3i and doo pairings seen in stock. The new-map case relies on `data/test map/war3map.doo` being v7 or v8; a test guards that.
- A doodad without the fixed-Z flag gets its z re-derived from terrain on save, so byte-exact round-trip checks need fixed-Z doodads or must exclude z (QA: 10.0 became 0.0).
- `Doodads::read_records` resizes without clearing, so reusing a `Doodads` object keeps per-record fields the new version does not read. Production always uses a fresh `Map`.
- Terrain offset is an input to every placed-object position, so a finite-position check on raw unit and doodad coordinates does not stop NaN reaching `interpolated_height`. `std::clamp` passes NaN through.
- `Terrain::change_tileset` leaves undo actions and the terrain clipboard holding stale ground indices, so "HiveWE never writes X" must be checked through undo and clipboard replay, not just the brush. A w3e ground index equal to the ground count can therefore come from HiveWE itself, and is accepted on load.
- `TerrainRect` inherits `QRect`, so `right()` and `bottom()` are inclusive. Check this before mirroring terrain loops.
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

- `tests/binary_writer_test.cpp`, `tests/modification_tables_test.cpp`, `tests/mdl_reader_test.cpp`, `tests/map_format_check_test.cpp`, `tests/map_body_check_test.cpp`.
- The stock sweeps in `tests/map_body_check_test.cpp` pass silently unless `HIVEWE_STOCK_BODY_ROOT` and `HIVEWE_BODY_CHECK_CASC` are set. Confirm the printed "stock maps checked" line.
- The stock v39 sweep in `tests/map_format_check_test.cpp` passes silently when `HIVEWE_STOCK_W3I_ROOT` is unset. Confirm the "stock v39 files checked: N" message.
- `Doodads::load` and `Units::load` can be tested without GL if the `Terrain` is heap-allocated and never destroyed (its destructor calls GL). Both readers refuse unknown versions before using it. `new Terrain()` has a 0 offset because `Terrain` has no user-provided constructor, so it is value-initialized.
- A sweep check that a real file was parsed must read the checked copy, not the source folder, or a stubbed copy still passes.
- doctest `SUBCASE` inside a `for` loop is unreliable, so looped refusal tests use plain blocks.
