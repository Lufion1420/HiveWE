# map-diff scripts

Tools for the differential sweep: load each stock WC3 map in HiveWE, save it, and compare the output with the World Editor original. Pure Python stdlib plus the CascLib/StormLib DLLs. Run them from your scratchpad with `python -I`, and never write into the stock root or a user map.

## extract_scripts.py: World Editor scripts from CASC

```
python -I extract_scripts.py <dll_dir> <stock_root> <out_root> [wc3_install]
```

- `dll_dir` holds `CascLib.dll` and `StormLib.dll`, plus `bz2.dll` and `zlib1.dll` if StormLib needs them. Use a copy in your scratchpad. Loading them from `build/Agent/Release` locks files that another agent's build needs.
- `stock_root` is the stock dump: `<id>/war3map.*` and `index.json` (`{id: "war3.w3mod:maps\\...\\x.w3x"}`). The dump does not contain the scripts.
- Stock maps are MPQ archives stored inside CASC (storage `<install>/:w3`). The script reads each archive into one temp file, one map at a time. It tries the `_locales\enus.w3mod:` copy first, then the base copy, and pulls out `war3map.lua`, `war3map.j` or `scripts\war3map.j` by name. It never calls `SFileFindFirstFile`: an earlier extractor built on it hung and grew to 20 GB.
- Output: `<out_root>/<id>/war3map.lua|.j`, plus `all.json` (every map), `subset.json` and `subset.tsv` (the 3.0 maps, w3i version 39). Columns are map, w3i_version, language, script, casc.
- Result on 2026-11-09 (466 maps): 417 JASS, 49 Lua. Of the 191 v39 maps, 188 are JASS and 3 are Lua: 0239 Forgotten Hollow, 0240 Silverpine Sprint, 0430 Gnollwood S2. Every script came from the base archive. None needed the enUS copy.

## sdiff.py: per-function script diff

```
python -I sdiff.py <orig> <new> [--json OUT] [--top N] [--max-diff LINES] [--float-digits D] [--strict]
```

- The language is detected from the extension or from `endfunction`. Lua goes through a small tokenizer that tracks block depth (`function/if/do/repeat` … `end/until`), so user code with nested or anonymous functions and top-level `local` tables splits correctly. JASS is split line by line (`globals`/`endglobals`, `function`/`endfunction`).
- Normalization: whitespace, comments, `;`, numbers (hex and octal to decimal, floats rounded to D decimals, default 3, trailing zeros dropped) and string quote style. By default it also drops parens around a whole condition, return value or assigned value, turns `local x = nil` into `local x`, and turns the World Editor's `i = 0 / while true do / if ((i > N)) then break end / … / i = i + 1 / end` into HiveWE's `for i = 0 , N do … end`. `--strict` turns these three off.
- Report: globals missing (orig only), extra (new only) and changed. Functions missing, extra, reordered-only (same lines in another order), and changed. Changed functions come sorted by size and carry a unified diff of the normalized lines. Loose top-level statements are matched by content.
- Output: a text summary on stdout, and the full result with `--json`. Exit code 0 means identical after normalization, 1 means differences, 2 means an I/O error.
- Sanity checks: a self-compare is IDENTICAL on all three stock Lua maps and on `0001` human01 (JASS). A planted global removal in JASS is reported.
- Two limits. Function bodies are compared line by line, so a statement split across lines differently counts as a change. Renamed helper functions (for example WE `Trig_X_Func003C` against HiveWE `Trig_X_HiveWE0`) show up as missing + extra plus a changed caller.

## Resaving user and warcraft-vscode maps

- `HiveWE.exe --resave <in> <out>` accepts folder maps directly (verified 2026-11-09 on a 382 MB folder map, 3.5 s). For a packed `.w3x`, first extract read-only with StormLib via `(listfile)` plus known names, never `SFileFindFirstFile`.
- sdiff.py cannot split a warcraft-vscode minified single-line `war3map.lua` bundle. Extract the bundled `origwar3map.lua` module (the editor-generated part) and compare that with `sim/simdiff.py`.
- warcraft-vscode projects: `warcraft.json` `mapdir` names the folder map. Packed output is `.build/release.w3x` and `.build/war3map.lua` (merged script). User code may wrap `InitGlobals`, `InitCustomTriggers` and `RunInitializationTriggers`, so HiveWE's Lua output must keep those as global functions.

### Forgotten Hollow (stock 0239 vs HiveWE save), 2026-11-09

Missing globals `gg_item_ergg_0000` … `gg_item_ergw_0003` (and the matching `CreateAllItems` lines). `InitSounds` lacks the `SetSoundParamsFromLabel` / facial-animation setters. `main` lacks `SetHDWaterParamsEx`, and its `SetDayNightModels` passes the terrain model as the unit model, with `/` instead of `\`. The World Editor writes `CreateNeutralPassiveBuildings` / `CreatePlayerBuildings` / `CreatePlayerUnits`, and HiveWE inlines them. `InitCustomPlayerSlots` lacks `SetPlayerRaceSkin`. `config` has `SetTeams(1)` where the World Editor has `SetTeams(2)`. `InitAllyPriorities` differs. Camera fields are written with more decimals (90.51 vs 90.5).
