# Map Protection Tool — Implementation Plan

**Project:** HiveWE Fork
**Feature:** Map Protection Window (standalone export-only tool, opens from main ribbon)
**Status:** Implemented — Phase 1, all of Phase 2 except object-field stripping (see "What shipped vs. what's deferred" below). Verified in-game: protected output no longer opens in the stock World Editor and still plays correctly. `strip_trigger_strings` specifically is algorithm-tested (see "Verified") but **not yet in-game tested** — do that before trusting it on a real map.
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
4. **Trigger / Script Hardening** — Remove GUI trigger data (ON) — deletes `war3map.wtg`. Strip trigger strings (OFF) — inlines `war3map.wts` text into the script, then deletes `war3map.wts`.
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
    bool strip_trigger_strings = false;

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

### Implemented (Phase 1 + Phase 2 except object-field stripping)
- Remove listfile / remove attributes
- Remove GUI trigger data (`war3map.wtg`)
- **Strip trigger strings** — inlines `war3map.wts` TRIGSTR references directly into `war3map.j`/`war3map.lua`, then deletes `war3map.wts`. Implementation notes:
  - Researched (not guessed) how WC3 actually resolves TRIGSTR: a string value is recognized as a reference if it *starts with* `TRIGSTR_` followed by digits — trailing characters after the digits are accepted but ignored by the engine (e.g. `TRIGSTR_7abc` still resolves to trigger string #7). Matching is on that prefix+digits rule, not a stricter exact-string match.
  - `find_string_literals()` scans the script text for `"..."` literals, skipping both JASS (`//`, `/* */`) and Lua (`--`, `--[[ ]]`) comment styles so a quote inside a comment can't desynchronize detection of literals after it. Respects `\"`/`\\` so an escaped quote doesn't end a literal early.
  - `escape_script_string()` handles the shared JASS/Lua escape set needed here: `\\`, `\"`, `\n` (WC3 itself uses the literal 2-character sequence `|n`, not a real newline byte, for in-tooltip line breaks — but trigger string *text* can still legitimately contain a real newline byte if a user pasted multi-line text into a WTS field, so this is handled, not assumed away).
  - Fails the whole export (does not silently ship a partially-resolved script) if a detected reference has no matching `war3map.wts` entry — a dangling reference would otherwise become unrecoverable text once the string table is deleted.
  - **Scope limitation, disclosed in the UI tooltip**: this only patches the script. Object data fields (e.g. a very long custom unit tooltip) can *independently* store a `TRIGSTR_XXX` reference too — those are not patched, since doing so would require loading the full SLK/meta tables the external-source path deliberately avoids touching. If a map relies on this, that specific field will show raw `TRIGSTR_034`-style text in-game after stripping.
  - All of the above (padding normalization, comment-skipping for both languages, escaping, and the fail-on-dangling-reference behavior) was validated with a standalone 22-case test against realistic sample script text before shipping — see git history for `protection_pipeline.ixx` around the commit that added this. **Not yet confirmed in a real Warcraft III client** — do that before trusting it on a real map.
- Metadata clearing (author / description / loading text / name) — via a standalone `MapInfo` instance for external sources, so this never requires loading the source into HiveWE's `Map` object
- Per-file MPQ encryption (`MPQ_FILE_ENCRYPTED`) — the same technique real-world map protectors use; WC3 decrypts per-file-encrypted archive contents transparently
- Junk file injection (configurable count, random name/extension/content)
- Protecting either the live in-editor map or an arbitrary external `.w3x`/`.w3m`/folder

### Deliberately deferred — not in `ProtectionOptions`, no UI
- **`strip_unused_fields`** (drop object-data fields the World Editor uses but the engine doesn't read at runtime) — blocked on: no such classification exists anywhere in this codebase, in WC3's shipped meta files, or (checked via web research) in any community source found so far. The `useHero`/`useUnit`/`useItem`/`useBuilding` SLK columns only indicate which editor tab a field appears on, not whether the engine reads it. Misclassifying even one field risks silently breaking unit/item/ability behavior with no safety net. Revisit only if a genuinely authoritative field-usage list turns up.
- **`remove_script_source`** (delete `war3map.wct`) — not implemented, not currently planned.

### Phase 3 (optional, complex — unchanged from original assessment, not started)
- JASS/Lua variable name obfuscation (would need a proper tokenizer — see `src/trigger_editor/jass_tokenizer.h`)
- Custom object ID remapping (would need to update every cross-reference in triggers/initialization code)

---

## Verified

- **Build**: `cmake --build --preset Release` (app + tests) — clean, no errors. Two pre-existing, unrelated `AutoMoc` warnings (`tooltip_editor`) are the only warnings.
- **Tests**: same pre-existing baseline (23 passed / 5 failed — unrelated failures in `object_data_io_test`/`mdl_reader_test`, predate this feature) — no regression.
- **In-game**: a map protected via Map Protector (a) no longer opens in the stock World Editor, (b) plays correctly in Warcraft III. Confirmed for the external-source path specifically with encryption + junk files enabled together.
- **Not yet in-game tested**: `strip_trigger_strings` (algorithm-validated standalone, see above, but not run through a real client on a map that actually uses TRIGSTR references).

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
| Trigger string table format (mirrored, not reused directly) | `TriggerStrings`, `src/base/trigger_strings.ixx` |
| Trigger-string inlining logic | `strip_trigger_strings_step()` and helpers, `src/map_protector/protection_pipeline.ixx` |
| WindowHandler | `src/base/window_handler.ixx` |
