# Map Protection Tool — Implementation Plan

**Project:** HiveWE Fork
**Feature:** Map Protection Window (standalone export-only tool, opens from main ribbon)
**Status:** Implemented — Phase 1, all of Phase 2 except object-field stripping (see "What shipped vs. what's deferred" below). Verified in-game: protected output no longer opens in the stock World Editor and still plays correctly. `strip_trigger_strings`'s first real in-game test (this session) found and fixed a real bug — see "Session notes" below — and still needs a fresh in-game re-test with the fix applied before it's fully trusted.
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
- **Not yet in-game tested**: `strip_trigger_strings` with this session's fix applied (see "Session notes" below) — needs a fresh real-client run before it's fully trusted.

---

## Session notes (2026-08-05, second session)

First real in-game test of `strip_trigger_strings` surfaced a bug: the map's protected name and loading screen text both went blank in-game, even with every "Metadata Sanitization" checkbox left unchecked.

- **Root cause:** confirmed directly from the user's actual map's on-disk `war3map.w3i` — `loading_screen_text` was literally the string `"TRIGSTR_001"` and `loading_screen_title` was `"TRIGSTR_002"`. These `war3map.w3i` fields aren't always literal text; the World Editor writes a `TRIGSTR_XXX` placeholder when a field is set via a localized/custom-text string picker, resolved against `war3map.wts` at display time — same mechanism as script string literals. `strip_trigger_strings_step()` deleted `war3map.wts` after patching only `war3map.j`/`war3map.lua`, never touching `war3map.w3i`, so any TRIGSTR-based `name`/`author`/`description`/loading-screen field in it went dangling. This is a separate code path from "Metadata Sanitization", which is why toggling those checkboxes had no effect.
- **Fix:** added `inline_map_info_trigger_strings()` to `protection_pipeline.ixx` — resolves TRIGSTR references in `war3map.w3i`'s `name`/`author`/`description`/`loading_screen_text`/`loading_screen_title`/`loading_screen_subtitle` fields (via a standalone `MapInfo` instance, same tileset-byte-from-`war3map.w3e` technique as `sanitize_metadata()`) using the same parsed `war3map.wts` table already built for script patching. Called from `strip_trigger_strings_step()` right before `war3map.wts` is deleted; fails (aborts, doesn't delete wts) on a dangling reference, same as the script-patching path.
- **Separately found and fixed:** the output filename always defaulted to the *currently loaded* HiveWE map's name (`map->name`), even when protecting an unrelated external file/folder — and once any export had ever run, the field was pre-filled from a persisted `QSettings` value, so `populate_default_output_path()`'s empty-field check silently skipped recomputing it forever after. Fixed in `map_protector.cpp`/`.h`: added `default_output_base_name()` (derives from the actual selected source — `source_path_edit`'s file/folder stem for an external source, `map->name` only when "currently loaded map" is selected), stopped persisting `outputPath` in `QSettings`, and wired `populate_default_output_path()` to re-run on source-radio/source-path changes (tracked via a new `auto_generated_output_path` member so a manually-typed path is never clobbered).
- **Not changed this session:** full filename obfuscation (renaming every packed file, including imports, to garbage names the way classic-era map protectors did) — discussed with the user but not implemented. See note below.

### Asset path obfuscation — Phase 0 + Phase 1 shipped (2026-08-05, third session)

Full plan lives at the session's plan-mode output (see chat history); scope decisions confirmed with the user: renamed files flatten into the archive root, random-hex names, object-data path fields found via the generic `type == "model"/"icon"` meta classification, sound files excluded from v1 (`Sounds::save()` is a no-op stub and `Sounds::load()` even discards some fields today — separate, unrelated work), `war3mapSkin.txt` excluded (already in `Imports::blacklist`, HiveWE doesn't parse it).

Shipped this session:
- `bool ProtectionOptions::obfuscate_asset_paths` + a new "Asset Path Obfuscation" checkbox/group in `map_protector.cpp`, wired through all 4 existing touch points (`collect_options()`, `set_options_enabled()`, `load_settings()`/`save_settings()`).
- New module `src/map_protector/asset_obfuscation.ixx`: `asset_match_key()` (mirrors `unused_files.cpp`'s `match_key` lambda), `is_stock_override_path()` (mirrors the same file's override-root check), `RenameCandidate` struct, and `enumerate_rename_candidates(temp_dir, never_rename_file_names, is_stock_asset)` — walks `temp_dir`, excludes blacklisted/stock-override/stock-resolving files, generates unique flat random-hex names preserving extension. `is_stock_asset` is an injectable predicate (production passes `hierarchy.game_file_exists`) so the function stays testable without touching the global `Hierarchy` singleton.
- `run_async_pack()` calls `enumerate_rename_candidates()` when the option is enabled (proves the module wiring/build end-to-end) but currently always fails the export with an explanatory message — no rewrite/rename logic exists yet, so actually renaming files now would ship a map with dangling references. This is intentional, not a placeholder bug.
- 6 new doctest cases in `tests/asset_obfuscation_test.cpp` (registered in `tests/CMakeLists.txt`) covering blacklist/override/stock-asset exclusion and unique-flat-name generation. Full suite: same pre-existing 5 failures in `object_data_io_test.cpp` (predate this work), no new failures.

**Architectural question raised and resolved:** Phase 2 (object-data SLK field rewriting) needs a fully set-up template+meta SLK per category, which previously only existed inline inside `Map::load()`. The user's real workflow requires Asset Path Obfuscation to work on an externally-compiled `.w3x` (from a VS Code extension), not just the currently-loaded map — HiveWE also can't reliably open `.w3x` directly today (forces folder-expansion), so the external-source protection path is not optional for them. Decided: **(b) extract a shared loader**, not (a) current-map-only or (c) a second divergent loader.

**Shipped:** new module `src/base/stock_object_data.ixx` (`stock_object_data::load() -> Tables`, one `{data, meta}` `TableSet` per category: units/items/doodads/destructibles/abilities/upgrades/buffs, plus `unit_editor_data` for the Object Editor's UI-only dropdown data). `Map::load()` (`src/base/map/map.ixx`) now calls this and move-assigns the results into the existing global `units_slk`/`units_meta_slk`/etc. (from the `Globals` module) exactly as before — pure code motion, not a logic change. Verified faithful (not just "builds"): every string literal (130) and every `.merge()`/`.add_column()`/`.set_shadow_data()`/`.substitute()`/`.build_meta_map()` call (111), extracted via `grep`, matches the pre-refactor code exactly in count and order — the only diff is the variable qualification (`units_meta_slk` → `tables.units.meta`). Full test suite: same pre-existing 5 failures in `object_data_io_test.cpp`, no new failures. **Not verified:** an actual in-app "open a map, check Object Editor" pass — no GUI automation tooling exists in this environment for a native Qt desktop app (no pywinauto/FlaUI harness, no CLI flag to auto-open a map). Recommend a quick manual check (open any map, confirm unit/item/ability data still populates normally in the Object Editor) before relying on this further, though the diff-based verification above is a stronger correctness signal than a screenshot would have been.

Phase 2 itself (actually walking `stock_object_data::load()`'s tables to classify `model`/`icon` fields and rewrite them) is still not built — this session only lands the prerequisite both protection paths can share.

**Phase 2 shipped** (same session, after the user manually verified the `stock_object_data` extraction above by opening a real map and checking the Object Editor/saving worked normally):

- `rewrite_object_data_file()` (exported from `asset_obfuscation.ixx`, not file-local, specifically so it's unit-testable against small hand-built SLKs without needing real game data through Hierarchy) rewrites `model`/`icon`-typed fields in a single object-data modification file: loads it via `extract_modification_shadow_map_path()`, classifies every field via `field_to_meta_id()` + the meta SLK's `type` column (same mechanism `src/models/table_model.ixx`'s `field_type()` already uses for the live Object Editor UI), rewrites any whose value match-keys to a candidate, and re-saves via `shadow_map_to_slk()` + `save_modification_file()` only if something actually changed.
- `rewrite_object_data_references()` orchestrates this across all 7 categories + their `war3mapSkin.*` counterparts, loading its own independent copy of the stock tables via `stock_object_data::load()` (not the live map's) and redirecting `hierarchy.map_directory` to `temp_dir` for the duration, same pattern as `sanitize_metadata()`.
- 3 new tests (hand-built meta/template SLKs + `build_modification_file_buffer()`/`extract_modification_shadow_map_path()` round-trip, no real game data needed): a model-typed field gets rewritten while a same-value string-typed field on the same object doesn't (proves type-gating works, not just presence-matching), a non-matching value is left untouched, and a missing file is a no-op success. Full suite: 32 passed, same pre-existing 5 failures, no regressions.
- **Deliberately not wired into `run_async_pack()` yet.** Until Phase 6 (verification + the actual physical file rename) exists, enabling this would rewrite object-data fields to point at names that don't exist on disk yet, which is worse than doing nothing - the "not implemented, aborts the export" message from Phase 0/1 stays in place until the full enumerate → rewrite-all-reference-kinds → verify → rename chain can run atomically in one pipeline step.

**Phase 3 shipped** (`war3map.w3i`): `rewrite_map_info_references()` rewrites `loading_screen_model`/`prologue_screen_model` if either matches a candidate, reusing the exact `MapInfo` load/tileset-byte/save pattern already implemented twice in `protection_pipeline.ixx`. No dedicated automated test - this is a thin reuse of an already-proven pattern (validated in production by this session's earlier TRIGSTR-in-w3i fix), and the fields it touches (`loading_screen_model`/`prologue_screen_model`) aren't exercised by the object-data test fixtures already in place; covered instead by Phase 6's eventual end-to-end verification.

**Phase 4 shipped** (MDX-internal paths): `rewrite_mdx_references()` walks every `.mdx` under `temp_dir` directly via plain file I/O (no `Hierarchy` needed - these are always loose files already physically present in `temp_dir`), rewrites `Texture::file_name`/`Attachment::path`/`ParticleEmitter1::path` fields that match a candidate, and re-serializes via `to_mdx(model.version)` - explicitly passing the model's own parsed version rather than `to_mdx()`'s `LATEST_MDX_VERSION` default, so an untouched model doesn't get its version silently bumped as a side effect of an unrelated rename elsewhere in the same file. Deliberately does not call `MDX::validate()` (that's for editor-authored models being exported and can restructure the model, e.g. add a bone if none exist - out of scope for a pure rename pass). 2 new tests reusing the `minimal_v1000.mdl` fixture already shared with `mdl_reader_test.cpp` (it ships one texture, `Textures/Stone.blp`): a matching texture gets rewritten and the version is preserved; a non-matching candidate leaves the file byte-for-byte untouched. Full suite: 34 passed, same pre-existing 5 failures, no regressions.

**Phase 5 shipped** (script literals): `rewrite_script_asset_references()` (`protection_pipeline.ixx`, since `find_string_literals()`/`escape_script_string()` already live there and can't be imported into `AssetObfuscation` without a circular module dependency) reuses the same JASS/Lua-aware literal scanner built for TRIGSTR inlining, but unlike TRIGSTR resolution a non-matching literal isn't an error - most string literals aren't paths. A new `unescape_script_string()` (inverse of `escape_script_string()`) was needed and is the one genuinely new/risky piece of this phase: a literal's raw source content is still escaped (a real path separator appears as the two-character sequence `\\`), so comparing it against a candidate's `match_key` without unescaping first would mismatch every backslash. 5 new tests cover the escape/unescape round-trip specifically (including the exact "escaped double-backslash must resolve to one real backslash and still match-key correctly" property), a matching literal getting rewritten while an unrelated literal is left alone, no-match leaving the file untouched, and a commented-out reference correctly *not* being rewritten (proves comment-skipping still works for this use).

**Phase 6 shipped** (verification safety net + physical rename + full wiring): `verify_no_dangling_text_references()` re-scans every `.txt`/`.ini`/`.j`/`.lua` file under `temp_dir` after all four rewrite phases have run, for any leftover occurrence of a candidate's *original* path - deliberately scoped to text-like files only, not a blind byte-scan of every binary asset (binary asset content was already exhaustively covered by the structured MDX phase or simply can't reference another file's path; scanning every byte of every multi-MB import on a large map for no realistic gain was rejected). This is what catches `war3mapSkin.txt` specifically - it can legitimately hold path overrides but nothing in this codebase parses it, so it's excluded from the candidate pool and never rewritten, meaning a reference to a renamed file living only there would otherwise go undetected. `apply_renames()` physically renames every candidate via `fs::rename`, and must run strictly last. `run_asset_obfuscation()` in `protection_pipeline.ixx` orchestrates all of it - enumerate → object data → w3i → MDX → script → verify → rename - short-circuiting on the first failure (originally written as a range-for over a pre-built list of results, which was caught and fixed before landing: that shape evaluates every step regardless of an earlier failure, since the initializer list itself is built eagerly before the loop's first `success` check ever runs). Wired into `run_async_pack()`, replacing the "not implemented" stub from Phase 0/1; runs before `strip_trigger_strings_step()` since both steps can touch `war3map.j`/`war3map.lua`. UI tooltip updated to drop the "not yet implemented" wording. 5 new tests for `verify_no_dangling_text_references()`/`apply_renames()` (text-format leak caught, binary files correctly ignored even when bytes coincidentally match, physical rename works, partial-rename-on-failure behavior documented). Full suite: 44 passed, same pre-existing 5 failures, no regressions. `HiveWE.exe` also rebuilds clean.

**Not covered by an automated test**: `run_asset_obfuscation()`'s full end-to-end orchestration. It transitively calls `stock_object_data::load()` (via `rewrite_object_data_references()`), which needs real stock game data (`Units/UnitData.slk` etc.) resolved through `Hierarchy` - `tests/test_main.cpp` never initializes `Hierarchy` with a real game path, so this can only be exercised by the real `HiveWE.exe` app (which does initialize it), not the test binary. Every phase this orchestrates *is* independently unit-tested against hand-built fixtures that don't need real game data - what's untested is specifically the "does it correctly chain all five phases against a real map's real stock data" integration, which needs a real in-app run.

## Status: critical bug found and fixed via real-map testing (2026-08-06, fourth session)

First real in-app test against the user's actual map (`NCOW_Arena.w3x`, protected via the external-source/"Map file or folder" path) found total breakage: black loading screen, every custom UI element showing as a missing-texture green box. Ladik's MPQ Editor confirmed files were genuinely renamed and core files kept their real names, so the rename/pack mechanics themselves weren't in question - something upstream of the four rewrite phases was wrong.

**Root cause, confirmed by directly inspecting the protected archive** (extracted via a throwaway `mpq::MPQ` test, not guessed): `unpack_source_archive()` - the function that unpacks an external `.w3x` source into `temp_dir` before any Asset Path Obfuscation runs - has **no `(listfile)`** in `NCOW_Arena.w3x` (confirmed: 4014/4014 unpacked files came back under StormLib-fabricated names like `File00001234.blp`, not their real names). This is a *pre-existing* gap, already documented in this same doc's "Prerequisite fix" section from an earlier session ("custom-imported assets under non-core names... still can't be recovered... this is exactly what removing the listfile is supposed to achieve") - but that session only worked around it for the fixed `Imports::blacklist` set of ~29 core filenames. Every custom asset (thousands of them) was extracted under a garbage name with zero relation to what the script/object-data/models actually reference. `enumerate_rename_candidates()` then built candidates from *those* garbage names, so nothing in the four rewrite phases ever matched a real reference - meanwhile the real-named assets those references depend on had already been scattered under meaningless names and then renamed *again*, orphaning them completely. Not a bug in the rewrite logic itself (verified independently via 33 passing unit tests) - every candidate it was ever given was already disconnected from reality before Asset Path Obfuscation started.

**Fix:** `war3map.imp` - the standard WC3 import manifest (written by the World Editor, HiveWE, and evidently the user's own external build pipeline too) - already lists every custom import's real path. Confirmed present and populated in the user's archive (168KB, 3982 entries) via the same diagnostic test. `unpack_source_archive()` now parses it (`parse_import_manifest_paths()`, matching `Imports::save()`'s binary format exactly: `u32` version, `u32` count, then per-entry `u8` flag + null-terminated path) and re-extracts every listed file by its real name via the existing listfile-independent `MPQ::extract_file()` (name-hash lookup, doesn't need a listfile). Verified directly against the real broken case: 3981/3982 manifest entries recovered by real name, including the specific texture (`UI\NCOW_UI_BarRedRiffled.blp`) confirmed missing in-game. This fixes the *whole* external-source protection path, not just Asset Path Obfuscation - any Map Protector option using a listfile-less source `.w3x` was silently at risk of this before.

**Known, accepted leftover**: the stale fabricated-name copies from the original wildcard unpack aren't deleted (no mapping exists from fabricated name back to real name - that pairing isn't exposed by `MPQ::unpack()`). Harmless (never referenced by anything once the real-named copy exists alongside it) but bloats the temp folder and, transitively, candidate count. Worth a follow-up if it turns out to matter in practice.

**Separate, still-open issue** (not investigated this session, confirmed pre-existing and unrelated to Asset Path Obfuscation): exporting this same map with *every* Map Protector option off still fails with `There was an error adding 'File00000000.xxx' to the protected archive (error code 87)`. Reproduces with a bare-minimum export, so it's not an interaction with any specific option. Given the fabricated-name pattern in the error, likely related to the same listfile-less-source territory (a StormLib-enumerated entry StormLib itself can't cleanly re-add), but not yet root-caused - needs its own investigation.

**Next before trusting Asset Path Obfuscation on a real map:**
1. Re-run the exact failing case (`NCOW_Arena.w3x`, "Rename imported files to random names" on) with this fix and confirm the loading screen and custom UI render correctly in-game.
2. In-game: full playability pass - a model with attachments, a custom-icon unit, a doodad, a script special-effect.
3. Ladik's MPQ Editor: confirm renamed files still don't resolve to recognizable names (already confirmed once, should still hold).
4. Combined with `strip_trigger_strings`/`encrypt_files`/`inject_junk_files` together.
5. Separately: root-cause and fix the error-87 bare-minimum-export crash.

### On classic-era file renaming/obfuscation (discussed, not implemented)

The user asked how old map protectors renamed every file (including BLPs) to things like `File002591294921` while keeping the map playable. Mechanically: it's possible because everything WC3 loads by path — model paths in `war3map.doo`, unit/item art paths in `war3unit.doo`/object data, terrain textures in `war3map.w3e`, icon/portrait paths in the SLK-derived meta tables, etc. — is just a string, and the engine doesn't care what that string looks like as long as every reference to a given asset is rewritten to the *same* new name consistently. The catch: WC3 itself hardcodes the small set of `war3map.*`/`war3campaign.*` core filenames it looks up directly by name (these can never be renamed), but everything else — every custom import — is fair game if (and only if) every cross-reference to it, across every binary format that can mention a path, is rewritten in lockstep. That's a large, error-prone surface (this repo's Phase 3 notes already flag "custom object ID remapping" as similarly complex/deferred, for the same reason: many places to update, one miss silently breaks something). Not started; would need its own scoped pass through every format that stores an asset path before it's safe to build.

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
