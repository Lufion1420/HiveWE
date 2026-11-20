# map-diff/sweep

Scripts of the differential sweep (fourth sweep, 2026-11-20). Not run since being copied here; Needs verification that they run unchanged. Read `.claude/rules/harness-guardrails.md` first: one map per process, wall-clock timeouts, no `SFileFindFirstFile`, kill only your own processes.

## Layout and environment

- `S` (env var, or argv) is a scratchpad root holding `stock/<id>/` (unpacked stock maps), `stock_scripts/<id>/` (the WE script) and `w4/` (this folder's scripts' working dir: `dll/` with `StormLib.dll` and `CascLib.dll`, `lupa/`, `subset.json`, `work/`). Copy the DLLs from a scratchpad, never from `build/Agent`.
- `R` (env var) is the repo root (`one.py`).
- `WC3_INSTALL` (env var, default `C:/Program Files (x86)/Warcraft III`) is the install `enum.py`, `ext1.py` and `gettd.py` read through CASC. `one.py` also uses `%LOCALAPPDATA%\Temp\input.lua`.
- Script paths inside the scripts assume the `w4/` working directory name; rename it consistently or edit the `'/w4'` constant.

## Order of a sweep

1. `enum.py <dll_dir> <out.json> <mask>`: list CASC map paths (`maps_all.json` is the sorted list `ext_all.py` reads).
2. `gettd.py <dll_dir> <out>`: extract the game's `triggerdata.txt` (fdiff `--triggerdata`, expected at `stock/TriggerData.txt`).
3. `ext_all.py <S> <w4>` runs `ext1.py` per map (known names plus `(listfile)` entries, never `SFileFindFirstFile`) to fill `stock/` and `stock_scripts/`. `mpq_extract.py <dll_dir> <mpq> <outdir>` extracts from a single archive.
4. `subset2.py <S> <subset.json>` (or older `subset.py <S>`): choose the maps (v39 or Lua scripts); prints counts.
5. `drive4.py` (needs `S`): loops `one.py <id> <lang>` over the subset with a timeout and skips finished ids. `one.py` calls `sweep/resave.py` (HiveWE `--resave`), `files/fdiff.py`, `scripts/sdiff.py`, and `sim/simdiff.py` for Lua maps, and counts `SetUnitColor` and `SetResourceAmount`. Writes `w4/work/res/<id>.json`.
6. `agg.py <S>`: aggregate the results (status, peak memory, colour/gold counts, fdiff histogram, sdiff lost-line heads).
7. `cfgdiff.py <S>`: compare `config`/`main`/slot functions of a folder map before (`rel_in/`) and after (`rel_out/`) a resave (NCOW check).
8. `teams.py <S> <repo>/.claude/tools/map-diff` (subset) or `teams_all.py` (all 466): per-map w3i flags, human count, SetTeams and placement from the WE script.

The 78-kind classifier from reports 1-3 is not in the repo and was not rebuilt.

`%TEMP%/input.lua` is shared by every HiveWE process: see `../scripts/README.md`.
