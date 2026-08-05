# Map Protection Tool — Implementation Plan

**Project:** HiveWE Fork
**Feature:** Map Protection Window (standalone export-only tool, opens from main ribbon)
**Status:** Implemented — Phase 1 and the safe half of Phase 2 (see "What shipped vs. what's deferred" below). Verified in-game: protected output no longer opens in the stock World Editor and still plays correctly.
**Last updated:** 2026-08-05

For a session-by-session account of *how* this got built (bugs found, why certain approaches were chosen), see `map_protection_handoff.md`. This doc describes the current, as-built state.

---

## Overview

A **Map Protector** window (ribbon: Config section) that lets the user configure a set of protection transforms, then exports either the currently-loaded map or an arbitrary external map file/folder as a new, separate `.w3x`. The source is never modified. The protected output runs in Warcraft III but is resistant to being opened, decompiled, or edited in world editors.

---

## Architecture (as built)

Diverges from the original draft in two ways worth knowing before editing this feature:

- **File layout**: `protection_pipeline.ixx` is a single C++20 module, not a `.h`/`.cpp` split — matches this repo's module convention (see `.cursor/rules/hivewe-cpp-modules.mdc`).
- **Source selection**: the window has a "Source" group with two mutually exclusive options:
  - **Currently loaded map** — routes through `run_sync_save_and_restore()`, which calls the live `map->save(temp_dir)` (HiveWE's full save, regenerating triggers/script from HiveWE's own state). Correct for protecting the live editor session.
  - **Map file or folder** (external) — routes through `prepare_source_path()`, which copies a folder-mode source or unpacks a `.w3x`/`.w3m` source directly into the temp folder **without ever loading it into HiveWE's `Map` object**. This exists because routing everything through `Map::save()` would silently regenerate `war3map.j`/`war3map.lua` from HiveWE's own trigger state, corrupting maps built by external tooling (e.g. a compiled Lua script from another build pipeline). Metadata sanitization for this path uses a standalone `MapInfo` instance instead of `map->info`.

Both paths converge on the same `run_async_pack()` step (background thread, StormLib packing).

### Files
```
src/map_protector/
  map_protector.h            — QMainWindow subclass (the window)
  map_protector.cpp          — UI logic, settings, export trigger
  protection_pipeline.ixx    — ProtectionOptions + both save paths + packing (single module)
```

### Prerequisite fix: opening real `.w3x` archives

`HiveWE::load_mpq()` (`hivewe.cpp`) and `MPQ::unpack()` (`src/file_formats/mpq.ixx`) previously failed silently on archives without a `(listfile)`: StormLib's wildcard enumeration still finds every entry, but unnamed ones come back under fabricated names (`File00001234.blp`) instead of the real ones, so the unpacked folder ended up missing `war3map.w3i` and failed to load. This mattered directly for Map Protector, since its own "Remove listfile" option produces exactly this kind of archive — there was no way to open/verify a protected map's own output. Fixed by also fetching the fixed set of core `war3map.*` files directly by name via the new `MPQ::extract_file()` (MPQ looks files up by name hash, not directory listing, so this works with or without a listfile). `unpack_source_archive()` in `protection_pipeline.ixx` reuses this same fix for the external-source path.

Known limitation (expected, not a bug): custom-imported assets under non-core names (e.g. `war3mapImported\Something.mdx`) still can't be recovered under their real relative path from a listfile-less archive — there's no fixed name list to fall back on for arbitrary imports. This is exactly what removing the listfile is supposed to achieve.

---

## UI (as built)

Window title: **Map Protector**, `QMainWindow`, resizes from 700×560.

1. **Source** — `Currently loaded map` / `Map file or folder:` radio buttons; the latter enables a path field + `Browse File...` / `Browse Folder...` buttons.
2. **Output file** — path field + `Browse...`. Defaults next to the source map's own folder (not the app-wide "last opened directory" setting, which can point at an unrelated/stale location).
3. **MPQ Archive Hardening** — Remove listfile (default ON), Remove attributes file (ON), Encrypt MPQ files (ON), Inject junk files (OFF) + junk file count spinner (default 50, enabled only when the checkbox is on).
4. **Trigger / Script Hardening** — Remove GUI trigger data (ON) — deletes `war3map.wtg`.
5. **Metadata Sanitization** — Clear map author / description / loading screen text / normalize map name (all OFF by default).
6. **Action bar** — status label, indeterminate progress bar, `Export Protected Map` button.

All options persist via `QSettings` (group `"MapProtector"`).

---

## `ProtectionOptions` (as built, `protection_pipeline.ixx`)

```cpp
struct ProtectionOptions {
    // MPQ archive hardening
    bool remove_listfile = true;
    bool remove_attributes = true;
    bool encrypt_files = true;
    bool inject_junk_files = false;
    int junk_file_count = 50;

    // Trigger hardening
    bool remove_gui_triggers = true;

    // Metadata sanitization
    bool clear_author = false;
    bool clear_description = false;
    bool clear_loading_text = false;
    bool normalize_name = false;
};
```

## Export flow (as built)

```
User clicks "Export Protected Map"
  → MapProtector::on_export_clicked()
  → Validate source (loaded map, or existing external path) and output path
  → Disable options, show progress bar
  → Synchronously, on the UI thread:
        run_sync_save_and_restore(temp_dir, options)   [current map]
        or prepare_source_path(source, temp_dir, options)  [external source]
  → On success, background via QThread::create():
        run_async_pack(temp_dir, output_path, options)
  → QMetaObject::invokeMethod(..., Qt::QueuedConnection) back to
    MapProtector::on_export_finished(result) on the UI thread
  → Re-enable options, hide progress bar, update status label
```

The sync-save step *must* run on the UI thread before backgrounding: both `run_sync_save_and_restore()` and `prepare_source_path()`'s metadata step briefly redirect the global `hierarchy.map_directory`, and `Map::save()`/`MapInfo::save()` aren't safe to call off the UI thread. Only the packing step (pure file I/O on the temp folder) is backgrounded.

---

## What shipped vs. what's deferred

### Implemented (Phase 1 + safe half of Phase 2)
- Remove listfile / remove attributes
- Remove GUI trigger data (`war3map.wtg`)
- Metadata clearing (author / description / loading text / name) — via a standalone `MapInfo` instance for external sources, so this never requires loading the source into HiveWE's `Map` object
- Per-file MPQ encryption (`MPQ_FILE_ENCRYPTED`) — the same technique real-world map protectors use; WC3 decrypts per-file-encrypted archive contents transparently
- Junk file injection (configurable count, random name/extension/content)
- Protecting either the live in-editor map or an arbitrary external `.w3x`/`.w3m`/folder

### Deliberately deferred — not in `ProtectionOptions`, no UI
- **`strip_trigger_strings`** (inline `war3map.wts` TRIGSTR placeholders into the generated script, then strip the string table) — blocked on: no JASS/Lua string-literal escaper exists anywhere in this codebase, and script generation currently passes TRIGSTR placeholders through untouched. Naive substitution without correctly escaping quotes/newlines/backslashes in trigger text would corrupt the generated script and produce a map that fails to load.
- **`strip_unused_fields`** (drop object-data fields the World Editor uses but the engine doesn't read at runtime) — blocked on: no such classification exists anywhere in this codebase or in WC3's shipped meta files. The `useHero`/`useUnit`/`useItem`/`useBuilding` SLK columns only indicate which editor tab a field appears on, not whether the engine reads it. Misclassifying even one field risks silently breaking unit/item/ability behavior with no safety net.
- **`remove_script_source`** (delete `war3map.wct`) — not implemented, not currently planned.

### Phase 3 (optional, complex — unchanged from original assessment, not started)
- JASS/Lua variable name obfuscation (would need a proper tokenizer — see `src/trigger_editor/jass_tokenizer.h`)
- Custom object ID remapping (would need to update every cross-reference in triggers/initialization code)

---

## Verified

- **Build**: `cmake --build --preset Release` (app + tests) — clean, no errors. Two pre-existing, unrelated `AutoMoc` warnings (`tooltip_editor`) are the only warnings.
- **Tests**: same pre-existing baseline (23 passed / 5 failed — unrelated failures in `object_data_io_test`/`mdl_reader_test`, predate this feature) — no regression.
- **In-game**: a map protected via Map Protector (a) no longer opens in the stock World Editor, (b) plays correctly in Warcraft III.
- Not yet specifically tested: encryption + junk files in combination with an *external-source* (non-loaded-map) export; only the currently-loaded-map path and the basic external-source path have both been confirmed in-game so far.

---

## Key references (as built)

| Concept | Location |
|---|---|
| Window class | `src/map_protector/map_protector.h` / `.cpp` |
| Pipeline module | `src/map_protector/protection_pipeline.ixx` |
| Ribbon button | `src/main_window/main_ribbon.h` / `.cpp` (`map_protector`, Config section) |
| Ribbon → window wiring | `src/main_window/hivewe.cpp`, `HiveWE::HiveWE()` constructor |
| MPQ wrapper (`open`/`unpack`/`extract_file`/`file_exists`) | `src/file_formats/mpq.ixx` |
| Existing "Open Map (MPQ)" flow (reused fix) | `HiveWE::load_mpq()`, `src/main_window/hivewe.cpp` |
| Full HiveWE save | `Map::save()`, `src/base/map/map.ixx` |
| Map metadata format | `MapInfo`, `src/base/map_info.ixx` |
| `war3map.w3e` header layout (tileset byte) | `Terrain::load()`, `src/base/terrain.ixx` (~line 296-308) |
| Core map filename list (reused for listfile-independent extraction) | `Imports::blacklist`, `src/base/imports.ixx` |
| WindowHandler | `src/base/window_handler.ixx` |
