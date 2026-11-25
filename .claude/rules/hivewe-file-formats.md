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
- Which map files are referenced (Asset Manager): `src/base/map/asset_usage.ixx` does the matching; `Map::get_file_usage` (`unused_files.cpp`) hands it the in-memory state.

## Rules

- Read both load and save paths before editing one side.
- Preserve round-trip: MDL/MDX, SLK merges, W3* files, generated scripts.
- Do not silently discard unknown data unless existing code does intentionally.
- Keep bounds checks and explicit error propagation.
- `slk::SLK` (`src/file_formats/slk.ixx`): column 0 (the row key, e.g. unitID, effectID) lives only in `index_to_row` / `row_headers`, never in `base_data`, so `data("<first column name>", row)` silently returns "". Data row indexes start at 0 (header not indexed), so `for (i = 1; ...)` skips the first data row. Take ids from `index_to_row.at(i)`.
- Preserve Hierarchy lookup order unless task is specifically to change it.
- No "cleanup" refactors while fixing compatibility bugs.
- Mark uncertain format rules `Needs verification`; test with `data/test map/` or fixtures.
- Object data writers (`modification_tables.ixx`): `write_modification_property` throws on a field with no meta id (it used to `exit(0)`). Every caller must handle the throw: `Map::save` (through the preflight `find_unwritable_object_data`), `export_binary_object_data`, the Map Protector `rewrite_object_data_file`, and `protection_pipeline`'s `map->save`.
- Unknown field ids are dropped on load. A custom object's useSpecific fields resolve only through its row's `code`, so a row lost while copying shows up as a save failure, not a load failure.
- To force a refused save in a test: `anam` with data_pointer=1 gives column `namea`, which has no meta id (`tests/fixtures/resave/unwritable_field.w3a`).

## Game version support

- As of 2026-11-08 HiveWE tracks formats through 2.0.3a, plus a w3i v39 parser (commit 3a5c6ed). doo v12/v13 and units v12/v13 have a reader and writer. w3r 7 and w3c 3 are supported since 2026-11-09 (accepted w3r {2,3,5,7}, w3c {0,3} in `map_format_check.ixx`). MDX: see "MDX reader (2026-11-23)".
- w3r v7 saves as v7, other versions as v5. `GameCameras::save()` is empty, so the w3c file is kept as is and camera deletions by resize are lost on save for every version (pre-existing). The 3.0 World Editor writes w3r v7 and w3c v3 even for empty maps, and its `war3map.j` emits `CAMERA_FIELD_DEPTH_OF_FIELD_DISTANCE`/`_SCALE`, `CAMERA_FIELD_ZABSOLUTE` and `BlzCameraSetupSetCameraType` for v3 cameras (our script generator does not yet: ticket "[3.0] Emit 3.0 calls in the script generator").
- Units v12/v13 (`Units::load`/`save`): version and subversion are kept on save (`format_version`, `format_subversion`); older files save as (8,11). On v12+ the reader refuses leftover bytes, truncation, a `war3map.w3i` game version below 1.32, ids with bytes after the first NUL, and a non-zero third trailer value (save writes 0 there). `group_id` and the first two trailer values are opaque and preserved. A subversion 9 file is promoted to 11 on save only while some unit holds a non-default item table pointer or str/agi/int.
- `Units::load` replaces every stored unit z with `Terrain::interpolated_height` (`Unit::update_terrain_height`), with no version check. Measured in `(1)silverpinesprint.w3x`: 12 start locations are stored at z -767 on terrain at 791.75. Whether the game reads the stored z: Needs verification.
- `UnitBrush::place_clipboard` (`src/brush/unit_brush.cpp`) copies the whole `Unit` (`add_unit(i)`), then only resets `creation_number` and position. Every opaque per-record field (`group_id`, trailer) is therefore duplicated on paste unless the brush resets it. What `group_id` means is unknown.
- Known gap: `save_modification_table` (`src/utilities/modification_tables.ixx`, used for the object tables such as `war3map.w3u` and `war3map.w3t`) writes custom object ids with `write_string` and no length check, unlike `Units::write_id`. Details: `.claude/plans/wc3-3.0-compatibility.md`.
- Unsupported format versions are refused with a dialog, not crashed on. `src/base/map/map_format_check.ixx` defines every accepted format version set once, and `map_archive_check.ixx` checks archives before unpacking. When a new format version gets a parser, add it to the set.
- The header preflight only protects against wrong versions. `src/base/map/map_body_check.ixx` validates bodies through the real parsers before teardown (`reset_map_session()`); a new accepted version must be added there too. Throws after validation passes are tracked in the ticket "Map load can still throw after body validation passes". The body check reads object data only for layout, so other `Map::load` throws surface after teardown. Since 462d751 such a failure shows a dialog naming the cause and opens a blank template map (path cleared, name Untitled; `QApplication::exit(1)` if the template fails too). An unknown base id no longer takes that path (see the next entry). Known gap: the blank map keeps `hierarchy.map_directory` on `data/test map` until Save As, so anything writing through `hierarchy.map_file_write` before then (for example Change Tileset texture overrides) could write into the shipped template (Needs verification).
- Unknown base id (since 2026-11-24): `load_modification_table` skips the custom object and all its field changes (`slk::set_shadow_data` on a row missing from `base_data` still creates a `shadow_data` entry, so the field changes must be skipped too). `Map` keeps `skipped_objects`/`skipped_placed_units`, `HiveWE::load_map` shows one `QMessageBox::warning`, and `Units::load` takes `ignored_ids` so placed records of a skipped custom unit are dropped. Skipped objects are not written on save (no raw-record preservation, filed). `--resave` exits 5 after a successful save when objects were skipped (3, script compile failed, wins); `.claude/tools/map-diff/sweep/resave.py` keeps its own exit-code table.
- Map Protector's `rewrite_object_data_file` rewrites an object-data file wholesale from the shadow map, so anything load/extract skipped must be carried over as raw bytes or it is lost. Done for unknown-base custom objects (`RawCustomObject`, `build_modification_file_buffer_with_raw`); output is v3 layout. `RawCustomObject.bytes` always holds the version-3 layout. `verify_no_dangling_text_references` scans object-data files as raw bytes, so it also sees paths inside raw-kept objects.
- `save_modification_table(..., skin=true)` writes empty field lists and is correct only for `Map::save`'s own skin output. Rewrites of an existing `war3mapSkin.*` use skin=false, or its fields are wiped.
- In C++ tests with `optional_ints`, a nonzero data pointer renames the field column (`file` becomes `filea`).
- Never write C++ containing backslash literals through a bash heredoc (it eats them, C4129); use Edit or Write.
- `war3mapUnits.doo`: a record is a unit only if it has a `units_slk` row, else an item. Dropping object-data rows changes that sorting (placed items of dropped .w3t items are kept).
- "Missing from HiveWE's loaded tables" is not "missing from the game": `Map::load` ignores the game data sets `custom_v0`/`custom_v1`/`melee_v0`. Any drop keyed on a base-id lookup needs a visible warning.
- In `slk::SLK`, `add_row` fills `row_headers` but not `base_data`; `copy_row` needs `base_data`, so a row added that way cannot be copied.
- Calling `MapInfo::load`/`save` with `hierarchy.map_directory` redirected needs an RAII guard that restores it on every exit: `BinaryReader` throws `out_of_range` on a short file (a w3e under 4 bytes in Map Protector, bf69845). Map Protector's asset-obfuscation and TRIGSTR steps still change the global `map_directory` from the export background thread (`map_read_override` is thread-local and covers reads only).
- Stock sweep (2026-11-24): the body check accepts all 466 stock maps (asserted in `tests/map_body_check_test.cpp`). The readable-parts pass there has nothing to do and the test asserts it stays at 0; it matters again when a new version is refused.
- `Doodad::state_from_flags` still clamps flag state 3 to 2 for the `state` enum, but `Doodad::file_flags` keeps the raw byte and `flags()` writes it back, so state 3 and bit 0x08 survive saves; a changed state still takes effect. Flag values 3 and 7 occur on 146,036 stock doodads.
- `group_id` is exactly 0xFFFFFFFF on all 546,722 stock v12/13 doodads and doodad `unk` is -1 on all 169,423 v13 doodads. One fresh 3.0.1 World Editor save (2026-11-08, editor 7003, game 3.0.1.24371) with two user-placed doodads also has group_id 0xFFFFFFFF, unk -1, roll 0, pitch 0, no lights, flag 0x02. That is one map. The meaning of group_id and unk is unknown, so non-default values are preserved.
- doo save policy: v7 and v8 save as (8,11), v12 as (12,11), v13 as (13,11), a new map as (8,11). This keeps the w3i and doo pairings seen in stock. The new-map case relies on `data/test map/war3map.doo` being v7 or v8; a test guards that.
- A doodad without the fixed-Z flag gets its z re-derived from terrain on save, so byte-exact round-trip checks need fixed-Z doodads or must exclude z (QA: 10.0 became 0.0).
- `Doodads::create` runs `Doodad::init` on every loaded doodad, so any per-instance field `init` assigns is overwritten after load: keep file-backed fields out of `init`. The cheapest test of that path is `tests/resave_smoke.cmake` with a fixed-Z .doo fixture (whole-file .doo equality works there).
- `Doodads::read_records` resizes without clearing, so reusing a `Doodads` object keeps per-record fields the new version does not read. Production always uses a fresh `Map`.
- A byte-exact claim for placed objects must be tested through `Units::load` or the doodad equivalent with a non-zero terrain offset. Load re-derives z from terrain, splits units from items, and x and y lose float bits through the offset, so `read_records`-only round trips prove the writer only.
- `BinaryReader::read_string` cuts at the first NUL, so a fixed 4-byte id written with `write_string` can lose bytes. Every id field in a binary doo/units writer goes through `Doodads::write_id` / `Units::write_id` (NUL-padded to the fixed width, throws when too long); `write_string` writes the raw length and is safe only for magic numbers.
- Terrain offset is an input to every placed-object position, so a finite-position check on raw unit and doodad coordinates does not stop NaN reaching `interpolated_height`. `std::clamp` passes NaN through.
- `Terrain::change_tileset` leaves undo actions and the terrain clipboard holding stale ground indices, so "HiveWE never writes X" must be checked through undo and clipboard replay, not just the brush. A w3e ground index equal to the ground count can therefore come from HiveWE itself, and is accepted on load.
- `Terrain::tileset` (char) is the map's tileset letter, read and written in `war3map.w3e`; `tileset_ids` is the tile-id list. Change Tileset (`src/menus/tile_setter.cpp`) changes only `tileset_ids`, never the letter, so blight and water follow the map's letter. New Map (`hivewe.cpp`, near line 1039) sets the letter before `change_tileset`. Do not "fix" blight to use another tileset.
- `TerrainRect` inherits `QRect`, so `right()` and `bottom()` are inclusive. Check this before mirroring terrain loops.
- Readers failed two ways on unknown versions: a crash (w3i, doo, units, via `std::bad_alloc`) or a silent misread that a save writes back (w3r). The preflight now covers both, but "no crash" still does not mean safe.
- Stock maps are in CASC under `war3.w3mod:maps\...`, for example `war3.w3mod:maps\forsakenkingdom\scenario\(1)forgottenhollow.w3x` (w3i 39, editor 7003), and can be read byte-exact through CascLib. A map is 3.0 when `war3map.w3i` starts with u32 39.
- `Map::load` ignores the bool results of `Terrain::load` and `PathingMap::load`, so a false return is not a safe path. For w3e, `interpolated_height` then indexes empty corner vectors. For wpm, the map stays 0 by 0 and a save overwrites the real `war3map.wpm` with an empty one. `new Map()` value-initializes, so unset scalar members are 0, not garbage.
- `Doodads::load` zeroes `Doodad::auto_increment`, `Unit()` increments `Unit::auto_increment`, and `Triggers::load` zeroes `Trigger::next_id`. The readers therefore cannot be dry-run against an open map without saving and restoring these statics.
- From a Python replay over the stock 3.0.1 maps: units use `random_type` -1 and 641 with no payload. That is not damage.
- StormLib (verified 2026-11-24): `SFileCreateArchive` always forces `dwFileFlags1 = MPQ_FILE_DEFAULT_INTERNAL` (`SFileCreateArchive.cpp:102`) and so writes a `(listfile)` regardless of `MPQ_CREATE_LISTFILE`; only `SFileCreateArchive2` with `dwFileFlags1 = 0` omits it (`run_async_pack` in `src/map_protector/protection_pipeline.ixx` does this). `SFileCompactArchive` still works without a listfile because the names stay in memory in the same session. `MPQ_OPEN_READ_ONLY` does not change the read and extract paths.
- The live game install on this machine is found through `find_warcraft_directory` at `C:/Program Files (x86)/Warcraft III`. The registry's Blizzard GamePath/Program/InstallPath values under `F:\` are stale.

## Generated map script must match the World Editor (verified 2026-11-09)

Checks: `.claude/tools/script-check/`.

- Start locations: `DefineStartLocation` index is the position in the w3i players list, not the player number.
- TriggerData preset values keep their quotes after INI parsing. Operators must be written unquoted; Lua uses `~=` and `nil`.
- Custom-text triggers (verified on stock 0239 and 0240, 2026-11-09): emission depends on the wtg classifier. 32 (script element) gets no `gg_trg_`, no `InitTrig_` call and no init call. 8 (GUI trigger converted to custom text) gets `gg_trg_`, the `InitTrig_` call and, if run-on-init, the init call. In Lua the World Editor writes an empty `function InitTrig_X() end` before the custom text. `Trigger::classifier` defaults to GUI, so script-trigger tests must set it. JASS `ConditionalTriggerExecute` for converted run-on-init triggers: Needs verification (no stock example).
- The World Editor does not trim leading spaces in trigger names (`"  Defend"` becomes `gg_trg___Defend`).
- `SetGamePlacement` (466/466 stock): `MAP_PLACEMENT_TEAMS_TOGETHER` when more than one player has type human (w3i type 1), else `MAP_PLACEMENT_USE_MAP_SETTINGS`. It does not follow w3i flags: the 466-map check (2026-11-20) found w3i 0x20/0x40 make no difference, with 0 counterexamples.
- `SetTeams(n)` uses n = number of players (same as `SetPlayers`), not number of forces (466/466).
- The per-player w3i dword HiveWE calls `fixed_start_position` is a flag set: 0x1 = fixed start location (`ForcePlayerStartLocation` emitted exactly when set), 0x2 = race selectable. `SetPlayerRaceSelectable` is true when `!(w3i flags & 0x20)` or the slot is race-selectable (race 0 / dword bit 0x2). In stock maps w3i flags 0x20 and 0x40 are always set together; using 0x20 alone follows War3Net (spec, not stock-verified).
- Known bug (ticketed): `scenario_properties_editor.cpp` and `scenario_data_io.ixx` still write `fixed_start_position` as 0/1, which drops bit 0x2.
- `SetResourceAmount(u, gold)` is written for every placed unit whose type's abilList contains `Agld`, or type `ugol`; also for 12500 (466 stock maps, 77,921 units).
- `SetUnitColor`: if UnitUI `customTeamColor` (utcc) == 1 and the placed unit's custom colour != -1, use that colour; else if `teamColor` (utco) != -1, `ConvertPlayerColor(utco)`; else none. Per-unit call order: SetHeroLevel, WaygateSetDestination, WaygateActivate, SetUnitColor, SelectHeroSkill, item-drop trigger (War3Net's order is wrong here).
- The World Editor creates `ugol` with `CreateBlightedGoldmine(p, x, y, face)`, not `BlzCreateUnitWithSkin` (40 JASS maps).
- `InitAllyPriorities` is omitted by the World Editor when no player has a priority on another (463/466; 3 unexplained, Needs verification).
- `generate_map_script` writes fixed shared paths (`%TEMP%/input.lua`, `data/tools/war3map.j`), so saves and resaves cannot run in parallel. On a script compile error, HiveWE's generated script can be taken from `%TEMP%/input.lua` right after the resave.
- Save pitfall (Asana 1220330960297388, open): the Lua step writes `%TEMP%/input.lua` unchecked (`src/base/triggers/map_script.cpp`). A failed open makes HiveWE copy the previous save's script, possibly another map's, into war3map.lua and still report success. Map Protector's `save_restoring_state` (`src/map_protector/protection_pipeline.ixx`) catches a throwing `map->save` and restores state.
- `MapScriptWriter::call` formats float args with `"{}"` (can produce exponents), and `string_view` concatenations can format as char ranges. Pre-format arguments as `std::string`.
- `MapScriptWriter::four_cc` writes object ids into the generated script unescaped. An id validator (new-object dialogs accept exactly 4 ASCII letters or digits) must be checked against that, and against maps already on disk: `id_fits` stays length-only so existing maps still save.
- Identifiers: every non-`[A-Za-z0-9_]` becomes `_`, trim first, and use the same helper at declaration and use.
- Lua globals: `__jarray(0)`, `__jarray(false)` or `__jarray("")` by base type, `{}` for handle arrays; scalars `0`, `false`, `""`, `nil`.
- `gg_unit_` and `gg_dest_` numbers are zero-padded to 4 (`{:0>4}`). Declared names come from scanning trigger text, created names from placed objects. Any padding, length or type difference leaves a global undeclared (JASS) or nil (Lua). Known open: unit numbers of 10000 and above are truncated by the fixed 4-char scan.
- Test seam: `generate_*` and `MapScriptWriter` are private to `Triggers`. Export a narrow helper declared in `triggers.ixx` and defined in the `module Triggers;` unit (example: `generate_variable_script`); no `Map` is needed.
- Test setup: `ini::INI::set_whole_data` supplies TriggerTypes/TriggerTypeDefaults (a missing section makes `data()` throw). `const T x;` fails with C2737 when only some members have default initialisers. `data/test map` is a JASS map, so `resave_smoke` produces no war3map.lua and Lua save output is covered only by unit tests.
- WC3 3.0.1 writes no Lua runtime errors to `Documents\Warcraft III\Logs\War3Log.txt`, and Lua errors in `UITimer` callbacks fail silently. Use the simulator in `script-check`.

## w3i facts (strict parse of 466 stock maps, 2026-11-08)

- Force flags: 0x01 allied, 0x02 allied victory, 0x04 unknown, 0x08 shared vision, 0x10 shared unit control, 0x20 advanced unit control. `MapInfo::load` reads `share_vision = flags & 0x0C` (legacy HiveWE 0x04 migrates) and `unknown_flags = flags & ~0x3F`; save writes vision only as 0x08. Force flags are decoded only in `MapInfo::load`/`save`. History: upstream read 0x08 but wrote 0x04; fork commit 81e6432 changed the read to 0x04. So any 0x04 in a user map means HiveWE wrote vision on; stock maps never set it.
- Builds from 81e6432 (2026-08-09) to before 3a5c6ed (2026-11-08) dropped force bit 0x08 on save (no `unknown_flags` yet); not recoverable.
- v39 player `hud_skin` exists only when editor_version >= 6140. Samples: 6117 and 6127 have none; 6140, 7000, 7003 have it; 6128-6139 have no samples. A wrong gate still reaches EOF on most maps with names shifted, so test by comparing names, not EOF.

- `TriggerStrings::string()`/`set_string()` (`src/base/trigger_strings.ixx`) is the choke point for w3i, force and player name fields and is testable without `Map` or Qt. A w3i literal must be a `TRIGSTR_` key or go through `script_string_literal` before reaching script output. `TriggerStrings::save()` always writes padded `STRING 001`.
- Map Info water and fog colours are stored B,G,R,A. Fog confirmed by `SetTerrainFogEx` on stock map 0415 (2026-11-20).

- A w3i combo filled from an SLK must not assume the id column is the first SLK column (EnvironmentType in Weather.slk and EnvironmentSounds.slk is column 1 only); read ids from row keys and include the Default row. String combos need a byte-identity test for non-UTF-8 stored ids, or an unchanged OK rewrites them.
- Any CASC file can be extracted with `.claude/tools/map-diff/sweep/gettd.py`; it needs all vcpkg bin DLLs next to `CascLib.dll` in a scratchpad copy.

## MDX reader (2026-11-23)

- All shipped 3.0.1 CASC models are v1800 and `MDX::load` accepts any VERS, so v1800 models load, SD and HD.
- GEOS SKIN at v1400+: the count is the number of u16 elements, 8 per vertex (4 bone indices into MATS, then 4 weights summing 255); measured on 10 HD stock models. Below 1400 it is u8. The 1400 boundary is cited from upstream HiveWE, Needs verification (no stock v1300-v1700 files).
- `Geoset::skin` is `std::vector<uint16_t>` in memory. GPU upload packs to u8 via `MDX::skin_as_u8_weights`, which refuses indices >255 and non-8-per-vertex. The writer for versions <1400 throws on entries >255. `MDX::merge_with` resets skin to zeros+255, so the doodad merge export cannot exceed u8.
- Model Editor Save MDX writes at the model's own version. `to_mdx()` callers without a version get `LATEST_MDX_VERSION` (1200).
- Camera entry first dword = 24-bit size (low) + variant byte (top; 3 in all 3.0 files). `write_CAMS` writes `size | variant<<24` (variant preserved; throws if size > 0xFFFFFF with variant != 0). All 5229 stock cameras are variant 3 at v1800; local v800 files are variant 0. `decode_camera_word` in `src/file_formats/mdx/mdx.ixx` is the single place to switch; variants 1 and 2 are still refused (layout unknown).
- Camera depth-of-field tracks IDUF, ELAF, PTSF (f32; ASCII tags, MSVC literals `'FUDI'`, `'FALE'`, `'FSTP'`; always before KCTR/KCRL/KTTR in stock files) are parsed and preserved. Unknown camera tracks are kept as raw bytes tag..end (KCVS is absent from all 15,089 stock models). Write order: IDUF, ELAF, PTSF, raw tracks, KCTR, KCRL, KTTR, so a raw track that was after KCTR in the source moves before it. The MDL reader/writer still lose these tracks. `TrackHeader::save` writes nothing for an empty (0-key) track.
- Line endings: `mdx_reader.cpp` is LF; `mdx.ixx` and `mdx_writer.cpp` are CRLF. Use Edit, not PowerShell here-string rewrites.
- Tracks do not describe their value size; the tag decides it. Chunk readers bound loops at reader_pos+size taken before the size dword, 4 bytes short: harmless for well-formed files.
- `Track::operator==` cannot compare none/linear tracks (tangents uninitialized). Refusal tests use `CHECK_THROWS_AS(..., std::runtime_error)`, not bare `CHECK_THROWS`.
- Env-gated stock test: set `HIVEWE_MDX_DIR` for the "stock models" case in `tests/mdx_camera_test.cpp` (ctest #253, the stock MDX sweep). Without it the case passes with 0 assertions, and no stock `.mdx` are extracted locally. A QA brief for an MDX parser change must supply an extraction folder if a real-model sweep is wanted.
- `BinaryReader::read_count(smallest_record)` (`src/base/binary_reader.ixx`) is the guard for any u32 count that drives an allocation; `TrackHeader` in the MDX reader uses it. A refusal test proves early refusal only with a huge count, or by checking `reader.position` after the throw: a small forged count throws the same `out_of_range` before the fix.
- LITE layout: v1300+ adds a u32 shadow_casting after `type`, plus shadow_casting_start/end floats after shadow_intensity and tracks KLSS/KLSE; v1600+ adds quadratic_falloff, linear_falloff, damping floats and tracks KLQF/KLLF/KLDA. The DILG chunk (geoset ids, `gliders`) is kept. Tracks not valid at the file's version are kept raw. Best 3.0 MDX source: WhiteoutLib `docs/MDX_FILE_FORMAT_SPECIFICATION.md`.
- `MDX::validate` adds a bone and renumbers node ids, so byte-exact node tests need ids that survive it.
- `consume_u32` in `mdl_reader` wraps negative MDL ints: convert via `int32_t` where the field is signed or float (negative attenuation).
- `mdx::TrackTag` is not exported, so tests write track bytes by hand.
- Known gap (filed): `mdx_reader.cpp` near line 750 calls `read_vector<float>(reader.read<uint32_t>() * 12)`, which multiplies in 32 bits and can wrap past the bounds check.

## Tests

- `tests/binary_writer_test.cpp`, `tests/modification_tables_test.cpp`, `tests/mdl_reader_test.cpp`, `tests/map_format_check_test.cpp`, `tests/map_body_check_test.cpp`.
- The stock sweeps in `tests/map_body_check_test.cpp` pass silently unless `HIVEWE_STOCK_BODY_ROOT` and `HIVEWE_BODY_CHECK_CASC` are set. Confirm the printed "stock maps checked" line.
- The stock v39 sweep in `tests/map_format_check_test.cpp` passes silently when `HIVEWE_STOCK_W3I_ROOT` is unset. Confirm the "stock v39 files checked: N" message. `HiveWE_tests.exe -tc="*stock v39*"` with the variable set runs that sweep alone.
- `Doodads::load` and `Units::load` can be tested without GL if the `Terrain` is heap-allocated and never destroyed (its destructor calls GL). Both readers refuse unknown versions before using it. `new Terrain()` has a 0 offset because `Terrain` has no user-provided constructor, so it is value-initialized.
- Script-generator tests: `world_edit_data.data()` throws when a section is missing, so a `generate_main` test must add the TerrainLights, SoundEnvironment, DayAmbience and NightAmbience sections. The seams `generate_main_script` and `generate_map_configuration_script` exist for script tests. `generate_map_configuration_script` contains no `InitCustomTeams` or alliance calls; those come from `generate_custom_teams`, tested via `generate_custom_teams_script`.
- `ctest` runs each doctest case in its own process. Stock tests report pass with 0 assertions when their root variable is unset, so check the printed counts, not the pass status.
- `stock_object_data::load()` needs `world_edit_strings` and `world_edit_game_strings` loaded, and reads `data/overrides` relative to the working directory, which under ctest is not the repository root.
- Trigger round-trip sweep: set `HIVEWE_TRIGGER_ROUNDTRIP_ROOT` (it also checks cut wct files). wtg/wct round-trip checks must compare custom text per trigger id on maps with non-empty text (stock 0239, 0104, 0136; human05 and undead02 have none).
- doctest `-tc` filters split on commas.
- A test that uses `std::string` or `std::map` with module imports defines `DOCTEST_CONFIG_USE_STD_HEADERS` before including doctest, else C2027.
- Never end a `TEST_CASE` name with a backslash: ctest reports it Not Run.
- A sweep check that a real file was parsed must read the checked copy, not the source folder, or a stubbed copy still passes.
- doctest `SUBCASE` inside a `for` loop is unreliable, so looped refusal tests use plain blocks.

## Sounds, script globals, writers (verified 2026-11-09)

- w3s: 2^32 (0x4F800000) for pitch and distances, and -1 for channel and volume, mean "default"; the World Editor writes no setter for them. Duration is not stored in w3s; the World Editor computes it from the audio file (Ogg: last granule * 1000 / sample rate). 3.0 dialogue audio exists only under `_locales\enus.w3mod` in CASC.
- The script generator binds `gg_unit_`/`gg_dest_`/`gg_item_` globals from trigger text. The declared type comes from the trigger text and the assigned type from the placed object; they must match (enforced for items and units).
- HiveWE's object-data writer puts 0 in the end token where the World Editor writes the object id. The wts writer drops `//` comments (found by fdiff on Forgotten Hollow).
- StormLib: never enumerate with `SFileFindFirstFile` on these maps (hang, 20 GB RAM); see `harness-guardrails.md`.

## Triggers: wtg and wct (verified 2026-11-24)

- 1.31+ wtg starts with eight lists `{count, deleted_n, ids}` (element types 1..128), then u32 `game_version` (2). All 466 stock maps write (1, 0) in list 1. HiveWE writes counts back as read, so they go stale after edits; whether the World Editor minds: Needs verification.
- `Triggers::load` is strict: bad magic, unknown version, 1.31 subformat other than 7, trailing bytes or an unknown element classifier refuse the map, as does a wct with unmatched entries or non-zero trailing bytes. An unknown function name alone does not.
- 1.31+ wct: one entry per trigger element whose classifier is not 16 (commented-out GUI triggers included), in wtg element order. All 466 stock files end exactly there (the trailing zeros 66 of them show in older HiveWE's order are those entries).
- Files older HiveWE saved have lists 1-7 all empty and a category-grouped wct without `is_comment` triggers; only they get that layout (with the normal one as fallback). `save()` therefore never writes lists 1-7 all empty. A layout fallback that expects fewer entries also accepts truncated files, so trigger sweeps must include cut-file checks.
- World Editor 1.31-1.33 maps that omit wct entries would now be refused: Needs verification that any exist.
- New element ids come from `Triggers::new_element_id` (skips deleted ids). Editor code creates through `make_new_trigger_category` / `make_new_trigger` / `make_new_trigger_variable` and deletes or moves through `Triggers::remove_element` / `set_element_parent`.
- Both wtg variable readers assign every `TriggerVariable` field, so default member values on that struct affect only editor-created variables. `make_new_trigger_variable` sets type "integer", unknown=1 (unknown=1 Needs verification against a World Editor-written wtg).

## Maps with missing trigger data (2026-11-25)

- A map with a script but without `war3map.wtg` or without `war3map.wct` keeps its script: HiveWE writes no wtg, wct or script and turns trigger editing off (switch: `keeps_existing_script` in `src/base/map/script_preservation.ixx`). A missing wct alone counts because it also holds the map header code (`global_jass`), not only custom-text triggers.
- A script is any of `map_script_file_names` (`war3map.j`/`.lua`, root or `scripts\`). Any new script reader or writer must loop that list. Whether the game reads `scripts\`: Needs verification.

## MPQ extraction (2026-11-24)

- `MPQ::extract_file` returns `ExtractResult {extracted, absent, failed}`: it checks `file_exists` first, and a name the archive holds but that is unsafe (`mpq::is_safe_archive_name`) is `failed`, as is an `unpack` entry that would write outside the target. Open Map (MPQ) and Map Protector refuse such archives and name the failed files (in CHANGELOG).
- Inside methods with a local named `mpq`, write `decltype(mpq)::ExtractResult`. doctest `CHECK(enum_class)` compiles silently, so when changing a bool return, grep the tests for old callers.
- `prepare_source_path` is the exported seam for testing Map Protector's archive unpack.
- Known gaps (filed): `.string()` narrow paths in `extract_file`/`unpack` on CJK code pages; `war3map.imp` names written to `temp_dir/name` are validated but import-parser edge cases are unreviewed.

## Asset reference scan (2026-11-25)

- A false "used" is harmless; a false "unused" gets the file deleted. When in doubt, count it as used (an unreadable source does).
- File lists name every file, so they are never searched: `war3map.imp`, `(listfile)`, and the root `.git/` and `.svn/` folders.
- New in-memory state that can name files must be passed in by `Map::get_file_usage`.
- Audio extensions are one family (wav, mp3, flac, ogg), and a path may have no extension: the user's NCOW Lua passes sound paths that way. Reforged CASC stores stock audio as `.ogg` while maps override it with `.wav` at the same path (Needs verification against CASC).

## Save temp files and folder listings (verified 2026-11-22)

- Any loop that lists a map folder to copy, list or pack files must skip `is_map_write_temp_file` (`*.hivewe-tmp`). `Imports::save` builds `war3map.imp` from a folder listing, so a stray temp file becomes an import. Current loops: `Imports::save`, `export_mpq`, `run_async_pack`, `copy_source_folder`, the `Map::save` Save-As copy, `find_file_usage`.
