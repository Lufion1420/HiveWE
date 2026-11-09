# Third differential sweep, 2026-11-09

Same 237 stock 3.0.1 maps, same harness and classifier as report 2 (`2026-11-09-second-sweep.md`), run on the uncommitted fix for kinds 3, 4, 5 (HEAD 0b59035 plus working-tree changes to `map_script.cpp`, `triggers.ixx`, new `tests/map_settings_script_test.cpp`). One map per process, `resave.py` timeouts (300 s, 2048 MB cap). The classifier is the report 2 scratchpad script, reused unchanged.

## Run

- Build: `cmake --build --preset Agent` ok. `ctest --preset Agent`: 246 of 246 passed (includes `resave_smoke`).
- Results: 49 ok (all Lua), 185 script_compile_error, 3 refused (0046, 0133, 0144): identical to report 2. No timeout, crash or leftover process.
- Simulator, 49 Lua maps: 0 new errors, 0 new warnings (same as report 2).

## Delta vs report 2

- Gone: kind 3 SetTeams (222 maps, Lua 49 -> 0), kind 4 SetGamePlacement (48, Lua 47 -> 0), kind 5 SetPlayerRaceSelectable (47, Lua 47 -> 0).
- Every other kind has the same map count and Lua count as in report 2. No new kinds.
- ForcePlayerStartLocation (now only when w3i dword bit 0x1): no remaining diff in the classifier's slot-setup bucket.
- Spot checks of `config()` / slot lines against the World Editor script (WE script vs HiveWE output):
  - 0407 (Lua): `SetPlayers(2) SetTeams(2) SetGamePlacement(TEAMS_TOGETHER)`, `SetPlayerRaceSelectable(Player(0/1), true)`: identical.
  - 0239 (Lua): `SetTeams(2)`, `USE_MAP_SETTINGS`, `SetPlayerRaceSelectable(Player(1), false)`, `Player(10) false`: identical.
  - 0454 (JASS, compared in the pre-jasshelper script): `SetPlayers(12) SetTeams(12) TEAMS_TOGETHER`, `ForcePlayerStartLocation(Player(0), 0)` plus `SetPlayerRaceSelectable(.., false)`: identical.
  - 0043 (JASS): `SetPlayers(1) SetTeams(1) USE_MAP_SETTINGS`, `SetPlayerRaceSelectable(Player(0), true)`: identical.

## NCOW (`NCOW_Spell_Dev.w3x` folder map, fresh scratchpad copy)

- `HiveWE.exe --resave`: exit 0, 2.9 s. War3map.lua differs from the original on exactly 2 lines (line 857-858, `config`):
  - `SetTeams(4)` -> `SetTeams(24)`
  - `SetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)` -> `SetGamePlacement(MAP_PLACEMENT_TEAMS_TOGETHER)`
  - `SetPlayers(24)` unchanged; no `InitCustomPlayerSlots` line changed.
- Previously (report 2) this map's script was a fixed point, because its `config` had been written by the old HiveWE (4 forces, USE_MAP_SETTINGS). The two changed lines are the intended new rule (team count = player count, placement by human slot count, 466 of 466 stock maps). Not game-tested.
- Originals: size and mtime of all 4083 files (folder map plus `.build/`) compared with the baseline taken before report 2: 0 differences.
