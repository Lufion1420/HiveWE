---
name: hivewe-warcraft3-data-formats
description: >-
  Guides Warcraft III map serialization, SLK/INI merges, CASC/MPQ, MDL/MDX, and trigger formats.
  Use for corrupted saves, load failures, round-trip bugs, or war3map/war3mapSkin editing.
---

# HiveWE Warcraft III Data Formats

## When to use

`war3map.*`, `war3mapSkin.*`, SLK/INI merges, CASC/MPQ, MDL/MDX, generated map script output, version-specific format bugs.

## Owner map

| Concern | Location |
|---------|----------|
| Load/save orchestration | `src/base/map/map.ixx` |
| Asset lookup order | `src/base/hierarchy.ixx` |
| Binary helpers | `BinaryReader`, `BinaryWriter` |
| MDL/MDX | `src/file_formats/mdx/` |
| Triggers/script | `src/base/triggers/` |
| Object SLK merges | `src/object_editor/slk_conversions.ixx` |

Fixtures: `data/test map/`, `data/warcraft/`. Tests: `tests/binary_writer_test.cpp`, `tests/modification_tables_test.cpp`, `tests/mdl_reader_test.cpp`.

## Workflow

1. Read **both** load and save paths before editing one side.
2. Preserve version handling and round-trip behavior.
3. Smallest fixture/map artifact that reproduces the issue.
4. Minimal compatibility fix; mark uncertain assumptions `Needs verification`.
5. Add/update tests for deterministic parser/serializer changes.

## Verification checklist

- [ ] Relevant doctest coverage passes
- [ ] MDL/MDX round-trip tests pass
- [ ] `data/test map/` load + save smoke test
- [ ] Load/save order changes: verify open existing map AND save back out

## Common mistakes

- Save-only changes without checking load path.
- Breaking `Map::load()` sequencing (SLK, overrides, terrain, objects, triggers, imports).
- "Cleanup" refactors while fixing compatibility bugs.

## Differential tools and stock data (2026-11-09)

- `.claude/tools/map-diff/` (see the READMEs): `files/fdiff.py` (file diff with independent parsers, `allow.json` lists accepted differences), `scripts/sdiff.py` and `extract_scripts.py` (per-function Lua/JASS diff; extracted stock scripts are described in `scripts/README.md`), `sim/simdiff.py` (Lua native-call trace diff). Run a sweep of HiveWE resave against the World Editor original before hand-checking in game.
- Sweep workflow: `sweep/resave.py` (HiveWE `--resave` over the maps) -> `fdiff.py` / `sdiff.py` / `simdiff.py` -> report under `.claude/tools/map-diff/reports/` (first: `2026-11-09-first-sweep.md`). Pass a high max-diffs (e.g. 20000) to sdiff/fdiff, because the 300 default hides wtg diffs. Resaves cannot run in parallel (shared script paths).
- Stock maps are MPQs inside CASC (open storage `<install>/:w3`); their scripts are in the base archive. Of 466 stock maps, 191 are 3.0 (w3i v39) and 49 use Lua; only 3 of the 3.0 maps are Lua: 0239 Forgotten Hollow, 0240 Silverpine Sprint, 0430 Gnollwood S2.
- Simulator: Lua 5.3 via lupa seeds string hashing per process, so `pairs`/`next` order varies; logs need sorted iteration and stubbed `os.clock`/`os.time`. HiveWE writes full float32 where the World Editor writes 1 decimal, so numeric diffs need about 0.1 tolerance.
- Script generator priority (user, 2026-11-09): the user's maps are Lua-only. Prioritize Lua; JASS output only must not break.
