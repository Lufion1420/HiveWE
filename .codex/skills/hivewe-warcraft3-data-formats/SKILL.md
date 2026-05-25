# hivewe-warcraft3-data-formats

## Purpose
Use this skill when editing Warcraft III map serialization, game-data loading, object data merges, trigger formats, or MDL/MDX parsing and writing.

## When to use this skill
- The task touches `war3map.*`, `war3mapSkin.*`, SLK/INI merges, CASC/MPQ access, MDL/MDX parsing, or generated map script output.
- A bug report mentions corrupted saves, load failures, missing objects, broken model round-trips, or version-specific format behavior.

## Relevant folders/files
- `src/base/map/map.ixx`
- `src/base/hierarchy.ixx`
- `src/base/binary_reader.ixx`
- `src/base/binary_writer.ixx`
- `src/base/triggers/`
- `src/file_formats/`
- `src/file_formats/mdx/`
- `src/object_editor/slk_conversions.ixx`
- `tests/binary_writer_test.cpp`
- `tests/modification_tables_test.cpp`
- `tests/mdl_reader_test.cpp`
- `data/test map/`
- `data/warcraft/`

## Workflow
1. Identify the exact format and owner:
   - Map folder data and load/save orchestration: `src/base/map/map.ixx`.
   - Asset lookup order: `src/base/hierarchy.ixx`.
   - Generic binary helpers: `BinaryReader`, `BinaryWriter`.
   - MDL/MDX: `src/file_formats/mdx/`.
   - Trigger serialization/script generation: `src/base/triggers/`.
2. Read the current load path and save path before editing one side only.
3. Preserve version handling and round-trip behavior.
4. Prefer the smallest fixture or map artifact that reproduces the issue.
5. If the format is partially understood, write the change as a minimal compatibility fix and mark assumptions that still need verification.
6. Add or update tests when changing deterministic parser/serializer logic.

## Coding rules
- Do not silently discard unknown data unless the current code already does that intentionally.
- Keep binary bounds checks and explicit error propagation intact.
- Preserve lookup order between overrides, local files, map files, and CASC unless the task is specifically to change that priority.
- Avoid "cleanup" refactors in parser code while fixing a compatibility bug.

## Verification
- Run the relevant doctest coverage.
- For MDL/MDX work, confirm round-trip tests still pass.
- For map serialization changes, load and save `data/test map/` and inspect the affected artifacts if feasible.
- If changing load/save order, verify both opening an existing map and saving it back out.

## Common mistakes to avoid
- Changing save logic without checking the corresponding load path.
- Assuming Warcraft III formats are forgiving; many editor compatibility bugs come from tiny ordering or field-size mistakes.
- Breaking `Map::load()` sequencing between game-data tables, map overrides, terrain/pathing, objects, triggers, and imports.
