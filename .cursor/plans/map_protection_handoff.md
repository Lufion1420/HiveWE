# Map Protector — Session Handoff

**Date:** 2026-08-05
**Read this if you're picking up Map Protector work in another session.** Phase 1 and Phase 2 (per `map_protection_plan.md`) were already implemented before this session. This session found and fixed a real bug in the underlying MPQ-open path, then used that fix to solve a real correctness problem in the protector itself. Both are shipped and build-verified; the user manually confirmed both work end-to-end (see "Build status" below).

---

## 1. `.w3x` direct-open bug (prerequisite fix, not Map-Protector-specific)

**File:** `src/file_formats/mpq.ixx`

- **Root cause:** `MPQ::unpack()`'s wildcard enumeration (`SFileFindFirstFile(handle, "*", ...)`) depends on the archive's `(listfile)`. Without one — common on real-world maps, and specifically producible by Map Protector's own "Remove listfile" option — StormLib still enumerates every entry but gives unnamed ones fabricated names like `File00003784.blp`. `unpack()` was silently producing a folder missing `war3map.w3i` etc. under their real names, so "Open Map (MPQ)" failed on any listfile-less archive.
- **Fixes:**
  - `MPQ::unpack()` — guarded `find_handle` before dereferencing (was reading an uninitialized `SFILE_FIND_DATA` when `SFileFindFirstFile` found nothing).
  - Added `MPQ::extract_file(archived_name, disk_path)` — extracts one file by exact name, bypassing enumeration entirely. Works without a listfile since MPQ looks files up by name hash, not directory listing.
  - Fixed `MPQ::file_exists()` / `file_open()` — both ran the archive-internal name through `fs::weakly_canonical`, which resolves relative paths against the process's CWD instead of treating them as archive-relative strings. This was dead code (zero live callers before this session), but is now load-bearing for `extract_file()`.
- **`src/main_window/hivewe.cpp`:** `HiveWE::load_mpq()` now loops over `map->imports.blacklist` (the fixed set of core `war3map.*` filenames, already used for import bookkeeping in `src/base/imports.ixx`) and calls `mpq.extract_file()` for each, after the normal `mpq.unpack()` — so core files are always recovered under their real names regardless of listfile presence. Added `import Imports;` to `hivewe.cpp`.
- **Verified against a real map:** confirmed with a standalone StormLib probe (before touching code) against the user's `release.w3x` (Map Protector output: no listfile/attributes) that wildcard enumeration returns fabricated names but `SFileHasFile`/direct-name lookup resolves core files correctly. Re-verified by building and the user testing "Open Map (MPQ)" in the actual app.
- **Known limitation (expected, not a bug):** custom-imported assets (non-core filenames, e.g. `war3mapImported\Something.mdx`) still can't be recovered under their real relative path without a listfile — there's no fixed name list to fall back on for those. Not fixable in general; this is exactly what "Remove listfile" is supposed to achieve.

---

## 2. Map Protector: protect an external file/folder, not just the live in-memory map

**Files:** `src/map_protector/protection_pipeline.ixx`, `src/map_protector/map_protector.h`, `src/map_protector/map_protector.cpp`

**Problem found:** the existing pipeline (`run_sync_save_and_restore`) always calls `map->save(temp_dir)` — HiveWE's full save, which regenerates `war3map.j`/`war3map.lua` from HiveWE's own trigger/GUI state (`Map::save()` → `triggers.generate_map_script()`, `map.ixx:653`). For maps built externally (the user's case: a VS Code extension compiles the actual script), this silently clobbered the real compiled code. Protecting *any* map always broke its code, even though the map itself loaded and displayed fine.

**Fix:** added a second pipeline path in `protection_pipeline.ixx` that never touches `Map::load()`/`Map::save()`:

- `copy_source_folder(source_directory, temp_dir)` — plain recursive copy for folder-mode sources.
- `unpack_source_archive(source_file, temp_dir)` — unpacks via the fixed `mpq::MPQ::unpack()` plus the same known-filename fallback as `load_mpq()` (constructs a local `Imports{}` for `.blacklist` — doesn't touch the global `map`).
- `sanitize_metadata(temp_dir, options)` — patches `war3map.w3i` metadata (author/description/loading text/name) via a **standalone** `MapInfo` instance, not `map->info`. `MapInfo::save()` needs a tileset byte that `MapInfo::load()` itself discards (`map_info.ixx:234`, `reader.advance(1)`), so it's read directly from `war3map.w3e`'s header (4-byte `"W3E!"` magic, 4-byte version, 1-byte tileset — same layout `Terrain::load()` parses at `terrain.ixx:299-307`) instead of loading all of `Terrain`.
- `prepare_source_path(source, temp_dir, options)` — new **exported** entry point tying the three together. Must be called synchronously on the UI thread before backgrounding the pack step (same constraint as the existing `run_sync_save_and_restore`, since `sanitize_metadata` briefly redirects the global `hierarchy.map_directory`).
- New module imports added to `protection_pipeline.ixx`: `Imports`, `MapInfo`, `BinaryReader`, `MPQ`.
- `run_sync_save_and_restore()` (the full `Map::save()` path) is **unchanged** — still used when the user picks "currently loaded map" as source, which is correct there since that's explicitly protecting the live editor session.

**UI changes** in `map_protector.h`/`.cpp`:

- New "Source" `QGroupBox`, added **first** in `main_layout` (before the output-path row), with two `QRadioButton`s: **Currently loaded map** vs **Map file or folder:** (+ a `QLineEdit` and **Browse File...** / **Browse Folder...** buttons, enabled only when the second radio is checked).
- New members: `source_current_map_radio`, `source_external_radio`, `source_path_edit`, `source_browse_file_button`, `source_browse_folder_button`.
- New handlers: `on_source_browse_file_clicked()`, `on_source_browse_folder_clicked()`, `update_source_controls_enabled()`.
- `on_export_clicked()` now branches on which radio is checked: validates/builds a `source_path` accordingly, then calls `run_sync_save_and_restore()` or `prepare_source_path()`.
- `set_options_enabled()`'s widget list gained `source_current_map_radio`/`source_external_radio`, and it now calls `update_source_controls_enabled()` at the end — so the source path row's enabled state stays tied to the radio selection through the disable/re-enable cycle instead of being blanket-toggled.
- `load_settings()`/`save_settings()` gained two new `QSettings` keys under the `MapProtector` group: `useCurrentMap` (bool, defaults to `map && map->loaded`) and `sourcePath` (string).

**Heads up if you're actively editing this feature:** if you're mid-way modifying `ProtectionOptions`, `on_export_clicked()`, `set_options_enabled()`, `load_settings()`/`save_settings()`, or the constructor layout in `map_protector.cpp`, expect overlap — these are the exact touch points this session's change went through. Diff against `git log` for the commit(s) from this session before merging manually-written changes on top.

---

## 3. Plan doc is stale

`map_protection_plan.md` still says `Status: Planning — not yet implemented` and describes a `protection_pipeline.h`/`.cpp` split. Both are inaccurate: the actual implementation is a single `protection_pipeline.ixx` C++ module (matches this repo's module convention, not the plan's assumed `.h`/`.cpp` split), and Phase 1 + Phase 2 were both already done before this session. This session didn't rewrite the plan doc itself — just flagging so it isn't trusted at face value. Worth a proper pass to bring it in line with reality (current file layout, options actually implemented vs. still-aspirational ones like `obfuscate_object_ids`, `strip_trigger_strings`, `remove_script_source`, `strip_unused_fields` — none of those exist in the current `ProtectionOptions`).

---

## Build / test status

- Both changes build clean via `cmake --build --preset Release --target HiveWE` (only warning is a pre-existing, unrelated `#include "StormLib.h";` semicolon warning in `mpq.ixx`, not introduced this session).
- Not run through `ctest` this session.
- User manually verified in the real app: opened a real listfile-less `.w3x` successfully via "Open Map (MPQ)"; protected it via the new external-source path successfully; confirmed the protected output no longer opens in the stock World Editor and still runs correctly in-game.
