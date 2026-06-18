# Map Protection Tool — Implementation Plan

**Project:** HiveWE Fork  
**Feature:** Map Protection Window (standalone export-only tool, opens from main ribbon)  
**Status:** Planning — not yet implemented  
**Last updated:** 2026-05-29

---

## Overview

A new **Map Protector** window that lets the user configure a set of protection transforms, then exports the currently-loaded map as a new `.w3x` file. The original map file is never touched. The protected output runs in Warcraft 3 but is resistant to being opened, decompiled, or edited in world editors.

The feature is additive — it lives entirely in new files and hooks into two existing extension points:
1. The **ribbon** (adds a button that opens the window)
2. The **export MPQ pipeline** (`export_mpq()` in `hivewe.cpp`) (reused/extended for the protected export)

---

## Architecture Fit

### How existing editors are opened (the pattern to follow)
- A `QRibbonButton` is declared in `src/main_window/main_ribbon.h`
- In `HiveWE::HiveWE()` (`src/main_window/hivewe.cpp` ~line 207) a lambda connects the button click to `window_handler.create_or_raise<EditorType>()`
- The editor class inherits `QMainWindow`
- `WindowHandler` (in `src/base/window_handler.ixx`) manages lifetime and focus

### How maps are exported (the pipeline to hook into)
- `HiveWE::export_mpq()` (`src/main_window/hivewe.cpp` ~line 696) saves map data to a temp folder, then packs it into an MPQ using StormLib directly
- The protection tool will call `map->save(temp_path)` the same way, then run its transforms on the resulting folder before calling its own MPQ packing step
- StormLib is already available via `src/file_formats/mpq.ixx`

---

## New Files to Create

```
src/
  map_protector/
    map_protector.h          — QMainWindow subclass (the window)
    map_protector.cpp        — UI logic, settings, export trigger
    protection_pipeline.h    — ProtectionPipeline struct + ProtectionOptions struct
    protection_pipeline.cpp  — All transform logic (pure functions on temp folder / MPQ)
```

---

## Files to Modify

| File | Change |
|---|---|
| `src/main_window/main_ribbon.h` | Add `QRibbonButton* map_protector` pointer (~line 62) |
| `src/main_window/main_ribbon.cpp` | Instantiate and label the button in the ribbon |
| `src/main_window/hivewe.h` | Forward-declare MapProtector |
| `src/main_window/hivewe.cpp` | Connect ribbon button → `window_handler.create_or_raise<MapProtector>()` (~line 207 area) |
| `CMakeLists.txt` | Add new .cpp sources to the target |

---

## UI Design — MapProtector Window

Window title: **Map Protector**  
Window class: `MapProtector : public QMainWindow`  
Size: ~700 × 560 px (fixed or minimum)  
Layout: single central widget, `QVBoxLayout`

### Top section — Output path
```
[ Output file:  ______________________________ ] [ Browse... ]
  (defaults to <map_name>_protected.w3x next to original)
```

### Middle section — Protection options (grouped QGroupBoxes)

#### Group 1 — MPQ Archive Hardening
| Option | Default | Description shown in tooltip |
|---|---|---|
| Remove listfile | ✓ ON | Deletes `(listfile)` from the MPQ so editors cannot enumerate files |
| Remove attributes file | ✓ ON | Deletes `(attributes)` (MD5 hash table) used by some tools |
| Encrypt MPQ files | ✓ ON | Applies StormLib per-file encryption (`MPQ_FILE_ENCRYPTED`) to all files |
| Inject junk files | OFF | Adds N dummy files with random names and content to confuse deprotectors |
| Junk file count | 50 | (spinner, only enabled when inject junk is ON) |

#### Group 2 — Trigger / Script Hardening
| Option | Default | Description |
|---|---|---|
| Remove GUI trigger data | ✓ ON | Deletes `war3map.wtg` (trigger tree). Map runs on compiled script, editor shows nothing |
| Remove custom script source | OFF | Also removes `war3map.wct`. CAUTION: map must have a working compiled script already |
| Strip trigger string references | OFF | Removes human-readable string keys from `war3map.wts`; replaces with raw values in script |

#### Group 3 — Object Data Hardening
| Option | Default | Description |
|---|---|---|
| Strip unused object fields | ✓ ON | Removes editor-only metadata fields that are not read by WC3 at runtime |
| Obfuscate custom object IDs | OFF | Remaps all custom unit/ability/item IDs to random 4-char IDs (complex — Phase 2) |

#### Group 4 — Metadata Sanitization
| Option | Default | Description |
|---|---|---|
| Clear map author | OFF | Blanks author field in `war3map.w3i` |
| Clear map description | OFF | Blanks description field |
| Clear loading screen text | OFF | Blanks loading screen tip/hint text |
| Normalize map name | OFF | Replaces map name with a generic placeholder |

### Bottom section — Action bar
```
[ ℹ️  Status: Ready ]                    [ Export Protected Map ]
```
- Status label updates during export (e.g. "Saving map data...", "Applying transforms...", "Packing MPQ...", "Done — saved to X")
- Export button disabled when no map is loaded or output path is empty
- A `QProgressBar` (indeterminate) shows while export runs (runs in background thread via `QThread` or `QtConcurrent::run`)

---

## Protection Pipeline — Technical Implementation

### `ProtectionOptions` struct (`protection_pipeline.h`)
```cpp
struct ProtectionOptions {
    // MPQ hardening
    bool remove_listfile       = true;
    bool remove_attributes     = true;
    bool encrypt_files         = true;
    bool inject_junk_files     = false;
    int  junk_file_count       = 50;

    // Trigger/script
    bool remove_gui_triggers   = true;
    bool remove_script_source  = false;
    bool strip_trigger_strings = false;

    // Object data
    bool strip_unused_fields   = true;
    bool obfuscate_object_ids  = false; // Phase 2

    // Metadata
    bool clear_author          = false;
    bool clear_description     = false;
    bool clear_loading_text    = false;
    bool normalize_name        = false;

    std::filesystem::path output_path;
};
```

### `ProtectionPipeline` — ordered steps (`protection_pipeline.cpp`)

All transforms operate on a **temporary folder** of unpacked map files (same as the normal save pipeline uses), before MPQ packing.

**Step 1 — Save map to temp folder**
- Call `map->save(temp_folder)` (same call as normal export)
- This produces all `war3map.*` files in the temp folder

**Step 2 — Metadata sanitization** (operates on `war3map.w3i` binary)
- Read the file, zero out author/description/loading text fields based on options
- Write back

**Step 3 — Trigger/script hardening** (operates on files in temp folder)
- `remove_gui_triggers`: delete `war3map.wtg` from temp folder
- `remove_script_source`: delete `war3map.wct` from temp folder
- `strip_trigger_strings`: parse `war3map.wts`, inline string values directly into `war3map.j`/`war3map.lua`, then delete/empty `war3map.wts`

**Step 4 — Object data strip** (operates on `war3map.w3u`, `.w3t`, `.w3a`, etc.)
- Iterate modification blocks, drop any field IDs that are editor-only (known list)
- Write back

**Step 5 — Inject junk files** (operates on temp folder)
- Generate N files with random names (e.g. `_junk_a3f2.blp`) and random binary content
- Add them to the folder so they get packed into the MPQ

**Step 6 — Pack MPQ**
- Use StormLib directly (same as `HiveWE::export_mpq()`) to create the `.w3x`
- Use `MPQ_CREATE_LISTFILE` = 0 if `remove_listfile` is true (do NOT pass `MPQ_CREATE_LISTFILE` flag)
- Use `MPQ_CREATE_ATTRIBUTES` = 0 if `remove_attributes` is true
- For each file added: if `encrypt_files`, add `MPQ_FILE_ENCRYPTED` flag to `SFileAddFileEx()` call

**Step 7 — Cleanup**
- Delete temp folder
- Emit signal to update status label with final output path

---

## Integration Points — Exact Code Locations

### Adding the ribbon button

**`src/main_window/main_ribbon.h`** — Add in the public section near line 57:
```cpp
QRibbonButton* map_protector;
```

**`src/main_window/main_ribbon.cpp`** — Instantiate alongside other tool buttons.

**`src/main_window/hivewe.cpp`** — In the constructor (~line 207), add:
```cpp
connect(ui->ribbon->map_protector, &QRibbonButton::clicked, [this]() {
    bool created = false;
    const auto editor = window_handler.create_or_raise<MapProtector>(nullptr, created);
    if (created) {
        editor->set_map(map.get()); // pass map pointer so it can call map->save()
    }
});
```

### Passing the map to the window
- `MapProtector` holds a `Map*` (non-owning pointer)
- Export button is only enabled when `map != nullptr`
- Connect to HiveWE's `map_opened` / `map_closed` signals (check if these exist; if not, wire up manually in the connect block above)

---

## Export Flow (sequence)

```
User clicks "Export Protected Map"
  → MapProtector::on_export_clicked()
  → Validate: output path set? map loaded?
  → Disable button, show progress bar
  → QtConcurrent::run([this]() {
        ProtectionPipeline::run(map, options, temp_dir);
    })
  → on_pipeline_finished(result):
        hide progress bar
        enable button
        update status label ("Done — saved to X" or error message)
```

---

## Phase Plan

### Phase 1 — Core (implement first)
- Window skeleton (QMainWindow, layout, all option widgets)
- Options struct + serialization (save/restore with QSettings)
- Export pipeline: save to temp → pack MPQ (no transforms yet, just working end-to-end)
- Remove listfile + remove attributes (trivial — just don't pass flags)
- Remove GUI triggers (delete `war3map.wtg`)
- Clear metadata fields
- Ribbon button integration

### Phase 2 — Hardening
- Per-file MPQ encryption (`MPQ_FILE_ENCRYPTED`)
- Junk file injection
- Strip trigger strings (inline into script)
- Strip unused object data fields

### Phase 3 — Advanced (optional, complex)
- JASS/Lua variable name obfuscation (requires a proper tokenizer — see `src/trigger_editor/jass_tokenizer.h`)
- Custom object ID remapping (requires updating all cross-references in triggers)

---

## Notes & Constraints

- **WC3 compatibility must be verified after each protection option is implemented.** Test the exported map in WC3 1.31+ and Reforged before calling a feature done.
- `MPQ_FILE_ENCRYPTED` on the script file (`war3map.j` / `war3map.lua`) must be verified — WC3's loader should handle per-file encryption since it's part of the MPQ spec, but confirm.
- `remove_script_source` is dangerous: only expose it if the map already has a working compiled script (`war3map.j` present). Guard this in the UI with a warning dialog.
- `obfuscate_object_ids` (Phase 3) requires updating every reference to the old IDs in triggers and initialization code — very high effort and high breakage risk.
- The temp folder used for export should be unique per-run (use `QTemporaryDir`) to avoid collisions if the user runs multiple exports.
- All file I/O in the export pipeline should happen on a background thread (`QtConcurrent::run`) — never block the UI thread.

---

## Key References in Codebase

| Concept | Location |
|---|---|
| Window open pattern | `src/main_window/hivewe.cpp` ~line 207 |
| MPQ wrapper | `src/file_formats/mpq.ixx` lines 84–195 |
| Export MPQ (full pipeline) | `src/main_window/hivewe.cpp` ~line 696 |
| Map save | `src/base/map/map.ixx` ~line 593 |
| Map data structures | `src/base/map/map.ixx` lines 53–93 |
| Ribbon button declarations | `src/main_window/main_ribbon.h` ~line 57 |
| WindowHandler | `src/base/window_handler.ixx` |
| JASS tokenizer (for Phase 3) | `src/trigger_editor/jass_tokenizer.h` |
| ObjectEditor (window template) | `src/object_editor/object_editor.h` |
