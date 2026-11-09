# Second differential sweep, 2026-11-09

HiveWE (`build/Agent` exe, HEAD 89e4485, contains 5eba4ca: autocast orders as string literals, Lua converted-trigger init by the wtg classifier) loaded and saved the same 237 stock 3.0.1 maps as report 1 (191 w3i-v39 maps plus all 49 Lua maps). Each output was compared with the World Editor original by `fdiff.py` (`--max-diffs 20000`), `sdiff.py` and, for Lua, `simdiff.py`. One map per process, sequentially, with `resave.py` timeouts (300 s, 2048 MB cap).

## Run

| maps | ok | script_compile_error | refused / load failed | timeout | crash |
|---|---|---|---|---|---|
| 237 (49 Lua, 188 JASS) | 49 (all 49 Lua) | 185 (185 of 188 JASS) | 3 (0046, 0133, 0144) | 0 | 0 |

- Build: `cmake --build --preset Agent` succeeded; `ctest --preset Agent`: 239 of 239 passed (includes the new autocast test and `resave_smoke`).
- Time: about 10 minutes in total (resave+diffs 525 s summed over maps). Peak HiveWE working set 431 MB (report 1: 320 MB); fdiff/sdiff/sim stayed small. No process was left running.
- JASS maps use HiveWE's pre-jasshelper script (`%TEMP%/input.lua`, copied right after each resave) as in report 1, so JASS script kinds are from that file. Compile counts now come from the stderr error list of each resave; they are lower bounds (jasshelper stops listing) and are not comparable one to one with report 1.
- Same three non-loading maps as report 1: 0046 and 0144 (`ankerl::unordered_dense::map::at(): key not found` after doodad loading), 0133 (undead02, `war3map.wtg` damaged / read out of range).
- Not run: the World Editor and the game. Severity stays a judgement from the diff.
- The classifier is a throwaway script in the scratchpad, not in the repo. 18 of report 1's 76 kinds were not re-bucketed (marked `n/r` in the table; they are cosmetic, harness or sdiff artifacts, plus 3 one-map wtg/wts/compile kinds). Counts of the other kinds can drift by method (see Delta).

## Kinds 1 and 2: both gone

- Kind 1 (autocast order char list): 61 maps -> 0, Lua 4 -> 0. No `['"', 'b', ...]` list in any generated script, and 0410, 0421, 0433, 0449 now load in the simulator (0 simulator load errors, 0 new warnings or errors across all 49 Lua maps).
- Kind 2 (Lua custom-text trigger never initialised): 2 -> 0, Lua 1 -> 0. 0240 `InitTrig_BeginMap` and `ConditionalTriggerExecute(gg_trg_BeginMap)` are now emitted; no `InitTrig_` is lost in any Lua map. Side effects gone with it: kind 18 (0240 initially-on list) Lua 1 -> 0, kind 20 (Cursed_Flute / duplicate InitTrig_BeginMap) Lua 2 -> 0, kind 22 (disabled-trigger globals) Lua 1 -> 0.

## Next fixes (kind numbers as in report 1)

| fix | kinds | Lua maps | exact diff (WE -> HiveWE) |
|---|---|---|---|
| SetTeams | 3 | 49 | `SetTeams(2)` -> `SetTeams(1)` (0239); `SetTeams(14)` -> 1 (0240) |
| SetGamePlacement + SetPlayerRaceSelectable | 4, 5 | 47 | `SetGamePlacement(MAP_PLACEMENT_TEAMS_TOGETHER)` -> `USE_MAP_SETTINGS`; `SetPlayerRaceSelectable(Player(0), true)` -> `false` (0407) |
| SetUnitColor + SetResourceAmount | 6, 7 | 47 | `SetUnitColor(u, ConvertPlayerColor(n))` lost (0407: 3); `SetResourceAmount(u, 12500)` lost (0407: 4) |
| SetWaterBaseColor + ally priorities | 8, 9 | 16, 15 | `SetWaterBaseColor(128,128,255,255)` lost (0407); `SetEnemyStartLocPrioCount(1,1)` vs HiveWE start-loc calls (0239) |
| doodad skin ids | 10 | 15 | `doodads[6288].skin LTlt -> FTtw` (0408) |

New: kind 78 (waygate destination) below.

## Delta vs report 1

- Gone: kinds 1 (61 maps, Lua 4) and 2 (2 maps, Lua 1).
- Lua-affecting kinds unchanged in count: 3, 4, 5, 8, 10, 11, 12, 13, 14, 16, 21, 23, 24, 25. Kinds 6 and 7 rise from 44/43 to 47/47 Lua maps because the four maps that used to fail to load in the simulator (kind 1) are now compared; this is not a regression.
- Count moves with no related code change, so classification drift and not behaviour change, except as noted: kind 9 JASS 169 -> 146 (Lua 15 -> 15), 15 (random groups) 2 -> 1 (0430 no longer shows it in the script diff; 0240 does), 17 JASS 82 -> 80, 36 (destructable pitch/roll) Lua 44 -> 1 (sim confirms only 0239 loses `BlzCreateDestructableWithSkinPitchRollColor`; report 1's 44 was too broad), 47 77 -> 70, 50 6 -> 5.
- Compile kinds rise (51: 101 -> 149 by log, 184 maps contain `call ReturnAction` in the script; 52: 25 -> 34; 53: 3 -> 7; 54: 3 -> 5) because they are now counted from the full stderr list rather than the first error.
- New kinds: 78 waygate destination (Lua 0446, plus about 17 JASS maps that call it in the WE script, unconfirmed per map). `compile: other` (0081 `Syntax Error, unexpected: ")"`) is report 1's kind 57 (`not ()`), not new.
- Run results unchanged: same 49 ok, 185 script_compile_error, 3 refused.

## Kinds, ranked

`n/r` = not re-bucketed in this sweep. Columns show report 1 -> report 2.

| # | kind | maps (r1 -> r2) | Lua (r1 -> r2) | severity | origin | known | status |
|---|---|---|---|---|---|---|---|
| 1 | script: preplaced-unit ability order emitted as a char list | 61 -> 0 | 4 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) (2fc3208 'Use more std::string_view') |  | GONE |
| 2 | script: Lua custom-text trigger never initialised (InitTrig_/RunInit call dropped) | 2 -> 0 | 1 -> 0 | breaks-in-game | fork (d611820 emits_trigger_identifier) |  | GONE |
| 3 | script: SetTeams count | 222 -> 222 | 49 -> 49 | changes-gameplay | upstream HiveWE (stijnherfst) | known | same |
| 4 | script: SetGamePlacement always USE_MAP_SETTINGS | 48 -> 48 | 47 -> 47 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 5 | script: melee player slots not race-selectable | 47 -> 47 | 47 -> 47 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 6 | script: preplaced unit custom colour (SetUnitColor) missing | 44 -> 47 | 44 -> 47 | changes-gameplay | upstream HiveWE (stijnherfst) |  | changed |
| 7 | script: gold mine SetResourceAmount missing | 43 -> 47 | 43 -> 47 | changes-gameplay | upstream HiveWE (stijnherfst) |  | changed |
| 8 | script: SetWaterBaseColor missing | 28 -> 28 | 16 -> 16 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 9 | script: ally priorities wrong (SetEnemyStartLocPrio) | 169 -> 146 | 15 -> 15 | changes-gameplay | upstream HiveWE (stijnherfst) |  | changed |
| 10 | doodad skin id replaced | 19 -> 19 | 15 -> 15 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 11 | script: SetTerrainFogEx missing | 43 -> 43 | 11 -> 11 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 12 | script: map-wide weather effect missing | 12 -> 12 | 6 -> 6 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 13 | script: NewSoundEnvironment from tileset instead of w3i | 14 -> 14 | 4 -> 4 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 14 | imp: _Locales entries dropped | 187 -> 187 | 2 -> 2 | changes-gameplay | upstream HiveWE (stijnherfst) | known | same |
| 15 | script: random groups (InitRandomGroups) missing | 2 -> 1 | 2 -> 1 | changes-gameplay | upstream HiveWE (stijnherfst) |  | changed |
| 16 | script: InitUpgrades / InitTechTree not generated | 154 -> 154 | 1 -> 1 | changes-gameplay | upstream HiveWE (stijnherfst) |  | same |
| 17 | script: force shared vision (SetPlayerAllianceStateVisionBJ) missing | 82 -> 80 | 1 -> 1 | changes-gameplay | upstream HiveWE (stijnherfst) | known | changed |
| 18 | script: initially-on trigger list differs (RunInitializationTriggers) | 4 -> 4 | 1 -> 0 | changes-gameplay | upstream HiveWE (stijnherfst) |  | changed |
| 19 | script: player slot setup differs (other) | 49 -> n/r | 3 -> n/r | unknown | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 20 | script: Cursed_Flute-style extra trigger (gg_trg_/InitTrig_ for disabled or custom-text trigger) | 8 -> 6 | 2 -> 0 | unknown | fork | known | changed |
| 21 | script: camera dest position / field values differ | 181 -> 181 | 1 -> 1 | unknown | fork/upstream HiveWE (stijnherfst) |  | same |
| 22 | script: gg_trg_ globals of disabled/comment triggers not declared | 136 -> 136 | 1 -> 0 | unknown | upstream HiveWE (stijnherfst) |  | changed |
| 23 | script: extra/missing destructable globals | 5 -> 5 | 1 -> 1 | unknown | upstream HiveWE (stijnherfst) |  | same |
| 24 | script: preplaced units inlined into CreateAllUnits (WE per-player functions) | 234 -> 234 | 49 -> 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 25 | script: SetDayNightModels unit model + slashes | 234 -> 234 | 49 -> 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known | same |
| 26 | doodad/unit x/y float bits | 234 -> 234 | 49 -> 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) (moved in fork) |  | same |
| 27 | Misc.txt: lowercase keys / BOM / LF | 234 -> 234 | 49 -> 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known | same |
| 28 | script: local/global initialiser style (= null, = CreateGroup()) | 234 -> n/r | 49 -> n/r | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 29 | wtg: header element counts and deleted-id lists zeroed | 233 -> 233 | 49 -> 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 30 | object data: empty tables written where WE wrote none | 231 -> 231 | 49 -> 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 31 | script: item-drop helper functions renamed | 184 -> 184 | 48 -> 48 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 32 | script: empty Create*/Init* functions emitted and called from main | 169 -> 169 | 48 -> 48 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 33 | script: other sound differences | 55 -> n/r | 47 -> n/r | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 34 | w3e: v11 upgraded to v12 (corners rewritten) | 46 -> 46 | 46 -> 46 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 35 | w3i: field changes | 46 -> 46 | 46 -> 46 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 36 | script: destructables lose pitch/roll/vertex colour (3.0 natives) | 184 -> 141 | 44 -> 1 | cosmetic-or-equivalent | new in 3.0 |  | changed |
| 37 | w3e: custom_tileset flag 0 -> 1 | 127 -> 127 | 17 -> 17 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) (gvremena 6fd24c3) |  | same |
| 38 | script: SetHDWaterParamsEx missing | 188 -> 188 | 3 -> 3 | cosmetic-or-equivalent | new in 3.0 | known | same |
| 39 | object data: skin fields moved from war3mapSkin.* into base files | 182 -> 182 | 3 -> 3 | cosmetic-or-equivalent | fork (moved from upstream modification_tables) | known | same |
| 40 | object data: end token id -> 0 | 146 -> 146 | 3 -> 3 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known | same |
| 41 | script: SetPlayerRaceSkin missing | 60 -> 60 | 3 -> 3 | cosmetic-or-equivalent | new in 3.0 | known | same |
| 42 | script: GUI helper functions renamed (FuncNNN -> HiveWEn) | 187 -> 187 | 2 -> 2 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 43 | wtg: call has_params 1 -> 0 on zero-arg calls | 186 -> 186 | 2 -> 2 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 44 | wts: // comments dropped | 170 -> 170 | 2 -> 2 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known | same |
| 45 | script: 3.0 camera fields missing (BlzCameraSetupSetCameraType etc.) | 181 -> 181 | 1 -> 1 | cosmetic-or-equivalent | new in 3.0 | known | same |
| 46 | wct: entry count / trailing empty entries / header differ | 114 -> 114 | 1 -> 1 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 47 | script: placed item coordinates printed with more decimals | 77 -> 70 | 1 -> 1 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | changed |
| 48 | script: InitGlobals local i | 76 -> n/r | 1 -> n/r | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 49 | script: item colour BlzSetItemColor / gg_item_ globals | 29 -> 29 | 1 -> 1 | cosmetic-or-equivalent | new in 3.0 | known | same |
| 50 | script: dialogue TRIGSTR keys differ (SetDialogueTextKey/SpeakerNameKey) | 6 -> 5 | 1 -> 1 | cosmetic-or-equivalent | fork (2fe340c) |  | changed |
| 51 | compile: Skip Remaining Actions emitted as call ReturnAction() | 101 -> 149 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  | changed |
| 52 | compile: nested helper function (unexpected takes) | 25 -> 34 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known | changed |
| 53 | compile: rawcode-typed variable initial value unquoted | 3 -> 7 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  | changed |
| 54 | compile: string initial value not escaped | 3 -> 5 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known | changed |
| 55 | resave: load failed / refused | 3 -> 3 | 0 -> 0 | breaks-in-game | unknown |  | same |
| 56 | compile: duplicate region names declared twice | 2 -> 2 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  | same |
| 57 | compile: empty condition `not ()` | 1 -> n/r | 0 -> n/r | breaks-in-game | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 58 | compile: garbage variable name (human05 wtg misread) | 1 -> 1 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known | same |
| 59 | wtg: written file does not parse | 1 -> 1 | 0 -> 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known | same |
| 60 | wtg: variables misread (human05 unknown-list header) | 1 -> n/r | 0 -> n/r | breaks-in-game | upstream HiveWE (stijnherfst) | known | not re-bucketed |
| 61 | script: InitSounds setters missing (SetSoundDuration/Channel/Distances/ConeAngles/Pitch) | 185 -> 185 | 0 -> 0 | changes-gameplay | fork (2fe340c generate_sounds) | known | same |
| 62 | script: region ambient sounds (SetSoundPosition/RegisterStackedSound) missing | 98 -> 98 | 0 -> 0 | changes-gameplay | fork/upstream (generate_regions:372-404 mixed) |  | same |
| 63 | script: gg_unit_ global missing | 10 -> 10 | 0 -> 0 | unknown | upstream HiveWE (stijnherfst) | known | same |
| 64 | script: trigger names with leading spaces trimmed (gg_trg___X -> gg_trg_X) | 6 -> 6 | 0 -> 0 | unknown | upstream HiveWE (stijnherfst) |  | same |
| 65 | wts: strings renumbered/added/removed | 1 -> n/r | 0 -> n/r | unknown | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 66 | wtg: elements removed/added | 1 -> n/r | 0 -> n/r | unknown | upstream HiveWE (stijnherfst) |  | not re-bucketed |
| 67 | script: InitTrig_ order differs (reordered only) | 173 -> 173 | 0 -> 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 68 | object data: original-object record with no mods dropped | 34 -> 34 | 0 -> 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 69 | script: item SetItemColor missing (classic items) | 3 -> 3 | 0 -> 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known | same |
| 70 | object data: file version changed | 1 -> 1 | 0 -> 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  | same |
| 71 | lua/j bytes differ (expected; see script kinds) | 49 -> n/r | 49 -> n/r | cosmetic-or-equivalent | - |  | not re-bucketed |
| 72 | imp: written where WE had none | 47 -> n/r | 47 -> n/r | cosmetic-or-equivalent | harness artifact |  | not re-bucketed |
| 73 | sim: call-count noise from moved code (Player/CreateTrigger) | 45 -> n/r | 45 -> n/r | cosmetic-or-equivalent | tool artifact |  | not re-bucketed |
| 74 | imp: .done marker added (harness) | 187 -> n/r | 2 -> n/r | cosmetic-or-equivalent | harness artifact |  | not re-bucketed |
| 75 | imp: entries for files absent from the dump dropped (harness) | 187 -> n/r | 2 -> n/r | cosmetic-or-equivalent | harness artifact |  | not re-bucketed |
| 76 | script: nested-helper split artifacts in sdiff | 34 -> n/r | 0 -> n/r | cosmetic-or-equivalent | tool artifact |  | not re-bucketed |
| 77 | compile: other | new -> 1 | new -> 0 | unknown | upstream HiveWE (stijnherfst) | | NEW |
| 78 | script: waygate destination (WaygateSetDestination/WaygateActivate) missing | new -> 1 | new -> 1 | changes-gameplay | upstream HiveWE (stijnherfst) | | NEW |
## Details

Examples are the exact normalized diff lines (`function: line`); WE side is the line starting with `-` in sdiff, which is what HiveWE lost. Responsible locations are from report 1 and were not re-verified, except kind 78.

### 1. script: preplaced-unit ability order emitted as a char list

- Maps: 0 (Lua 0: ). Severity: breaks-in-game. Known: no. Responsible: src/base/triggers/map_script.cpp generate_units:225/230 `"\"" + order_on + "\""` on a string_view formats as a char list

### 2. script: Lua custom-text trigger never initialised (InitTrig_/RunInit call dropped)

- Maps: 0 (Lua 0: ). Severity: breaks-in-game. Known: no. Responsible: src/base/triggers/triggers.ixx:512 emits_trigger_identifier() skips every Lua trigger with non-empty custom_text, in InitCustomTriggers and globals; RunInitializationTriggers also drops them

### 3. script: SetTeams count

- Maps: 222 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: changes-gameplay. Known: yes. Responsible: src/base/triggers/map_script.cpp:875
- `0239 (Lua)`: `config: SetTeams ( 2 )`
- `0240 (Lua)`: `config: SetTeams ( 14 )`
- `0407 (Lua)`: `config: SetTeams ( 2 )`
- `0001`: `config: call SetTeams ( 5 )`
- `0002`: `config: call SetTeams ( 7 )`

### 4. script: SetGamePlacement always USE_MAP_SETTINGS

- Maps: 48 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp:876 generate_map_configuration hard-codes it
- `0407 (Lua)`: `config: SetGamePlacement ( MAP_PLACEMENT_TEAMS_TOGETHER )`
- `0408 (Lua)`: `config: SetGamePlacement ( MAP_PLACEMENT_TEAMS_TOGETHER )`
- `0409 (Lua)`: `config: SetGamePlacement ( MAP_PLACEMENT_TEAMS_TOGETHER )`
- `0465`: `config: call SetGamePlacement ( MAP_PLACEMENT_TEAMS_TOGETHER )`

### 5. script: melee player slots not race-selectable

- Maps: 47 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_players:618 uses race==selectable; WE writes true when w3i flags lack 0x20/0x40 (no custom forces)
- `0407 (Lua)`: `InitCustomPlayerSlots: SetPlayerRaceSelectable ( Player ( 0 ) , true )`
- `0408 (Lua)`: `InitCustomPlayerSlots: SetPlayerRaceSelectable ( Player ( 0 ) , true )`
- `0409 (Lua)`: `InitCustomPlayerSlots: SetPlayerRaceSelectable ( Player ( 0 ) , true )`

### 6. script: preplaced unit custom colour (SetUnitColor) missing

- Maps: 47 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_units: Unit::custom_color never emitted
- `0407 (Lua)`: net 3 lost: `SetUnitColor(<unit@CreateNeutralPassiveBuildings#6>,<playercolor@CreateNeutralPassiveBuildings#1>)`
- `0408 (Lua)`: net 2 lost: `SetUnitColor(<unit@CreateNeutralPassiveBuildings#5>,<playercolor@CreateNeutralPassiveBuildings#1>)`
- `0409 (Lua)`: net 2 lost: `SetUnitColor(<unit@CreateNeutralPassiveBuildings#15>,<playercolor@CreateNeutralPassiveBuildings#1>)`

### 7. script: gold mine SetResourceAmount missing

- Maps: 47 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_units: Unit::gold is read (units.ixx:329) but never emitted
- `0407 (Lua)`: net 4 lost: `SetResourceAmount(<unit@CreateNeutralPassiveBuildings#1>,12500)`
- `0408 (Lua)`: net 6 lost: `SetResourceAmount(<unit@CreateNeutralPassiveBuildings#1>,12000)`
- `0409 (Lua)`: net 5 lost: `SetResourceAmount(<unit@CreateNeutralPassiveBuildings#1>,12500)`

### 8. script: SetWaterBaseColor missing

- Maps: 28 (Lua 16: 0407, 0408, 0411, 0413, 0414, 0415, 0418, 0420). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_main (w3i water colour not emitted)
- `0407 (Lua)`: `main: SetWaterBaseColor ( 128 , 128 , 255 , 255 )`
- `0408 (Lua)`: `main: SetWaterBaseColor ( 0 , 255 , 230 , 255 )`
- `0411 (Lua)`: `main: SetWaterBaseColor ( 155 , 255 , 255 , 255 )`
- `0044`: `main: call SetWaterBaseColor ( 0 , 128 , 255 , 255 )`
- `0045`: `main: call SetWaterBaseColor ( 0 , 128 , 255 , 255 )`

### 9. script: ally priorities wrong (SetEnemyStartLocPrio)

- Maps: 146 (Lua 15: 0239, 0240, 0418, 0423, 0424, 0427, 0428, 0429). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_ally_priorities:791-825
- `0239 (Lua)`: `InitAllyPriorities: SetEnemyStartLocPrioCount ( 1 , 1 )`
- `0240 (Lua)`: `InitAllyPriorities: SetEnemyStartLocPrioCount ( 0 , 1 )`
- `0418 (Lua)`: `InitAllyPriorities: SetEnemyStartLocPrioCount ( 0 , 3 )`
- `0001`: `InitAllyPriorities: call SetStartLocPrioCount ( 0 , 2 )`
- `0003`: `InitAllyPriorities: call SetStartLocPrioCount ( 1 , 2 )`

### 10. doodad skin id replaced

- Maps: 19 (Lua 15: 0408, 0409, 0411, 0416, 0417, 0418, 0419, 0423). Severity: changes-gameplay. Known: no. Responsible: src/base/doodad.ixx:118 Doodad::init sets skin_id = id (called from Doodads::create)
- `0408 (Lua)`: `war3map.doo doodads[6288].skin: LTlt -> FTtw`
- `0409 (Lua)`: `war3map.doo doodads[2693].skin: JSax -> SWt0`
- `0411 (Lua)`: `war3map.doo doodads[4055].skin: YSaw -> APct`
- `0097`: `war3map.doo doodads[4].skin: LT09 -> LT01`
- `0129`: `war3map.doo doodads[630].skin: BRrk -> LRrk`

### 11. script: SetTerrainFogEx missing

- Maps: 43 (Lua 11: 0413, 0415, 0418, 0419, 0420, 0422, 0425, 0426). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_main (w3i fog not emitted)
- `0413 (Lua)`: `main: SetTerrainFogEx ( 0 , 3000 , 5000 , 0.5 , 1 , 1 , 1 )`
- `0415 (Lua)`: `main: SetTerrainFogEx ( 0 , 3000 , 5000 , 0.6 , 0.435 , 0.145 , 0.514 )`
- `0418 (Lua)`: `main: SetTerrainFogEx ( 0 , 3000 , 5000 , 0.25 , 0 , 0.251 , 0.251 )`
- `0049`: `main: call SetTerrainFogExV ( 0 , 1000 , 8000 , 0 , 0 , 0 , 10000 , 10000 , 0.502 , 0 , 0 )`
- `0051`: `main: call SetTerrainFogExV ( 0 , 1000 , 11000 , 0 , 0 , 0 , 10000 , 10000 , 0.902 , 0.902 , 0.902 )`

### 12. script: map-wide weather effect missing

- Maps: 12 (Lua 6: 0407, 0410, 0422, 0433, 0442, 0449). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_main (w3i weather_id not emitted)
- `0407 (Lua)`: `main: we = AddWeatherEffect ( Rect ( - 6144 , - 6144 , 6144 , 6144 ) , FourCC ( "RAlr" ) )`
- `0410 (Lua)`: `main: we = AddWeatherEffect ( Rect ( - 8192 , - 6144 , 8192 , 10240 ) , FourCC ( "RAlr" ) )`
- `0422 (Lua)`: `main: we = AddWeatherEffect ( Rect ( - 8192 , - 8192 , 8192 , 8192 ) , FourCC ( "RLlr" ) )`
- `0059`: `main: set we = AddWeatherEffect ( Rect ( - 14336 , - 5120 , 10240 , 7168 ) , 'RLlr' )`
- `0060`: `main: set we = AddWeatherEffect ( Rect ( - 14336 , - 8192 , 10240 , 8192 ) , 'RLlr' )`

### 13. script: NewSoundEnvironment from tileset instead of w3i

- Maps: 14 (Lua 4: 0240, 0415, 0420, 0445). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp:847 generate_main reads world_edit_data SoundEnvironment by tileset
- `0240 (Lua)`: `main: NewSoundEnvironment ( "lake" )`
- `0415 (Lua)`: `main: NewSoundEnvironment ( "lake" )`
- `0420 (Lua)`: `main: NewSoundEnvironment ( "lake" )`
- `0056`: `main: call NewSoundEnvironment ( "Dungeon" )`
- `0090`: `main: call NewSoundEnvironment ( "Dungeon" )`

### 14. imp: _Locales entries dropped

- Maps: 187 (Lua 2: 0239, 0240). Severity: changes-gameplay. Known: yes. Responsible: src/base/imports.ixx save rebuilds from disk
- `0239 (Lua)`: `war3map.imp entries[_locales\dede.w3mod\war3map.wts]: {'flag': 29, 'path': '_Locales/deDE.w3mod/war3map. -> <absent>`
- `0240 (Lua)`: `war3map.imp entries[_locales\dede.w3mod\war3map.wts]: {'flag': 13, 'path': '_Locales/deDE.w3mod/war3map. -> <absent>`
- `0001`: `war3map.imp entries[_locales\dede.w3mod\war3map.wts]: {'flag': 25, 'path': '_Locales/deDE.w3mod/war3map. -> <absent>`
- `0002`: `war3map.imp entries[_locales\dede.w3mod\war3map.wts]: {'flag': 25, 'path': '_Locales/deDE.w3mod/war3map. -> <absent>`

### 15. script: random groups (InitRandomGroups) missing

- Maps: 1 (Lua 1: 0240). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp (no InitRandomGroups generator; gg_rg_ globals missing)
- `0240 (Lua)`: `InitRandomGroups missing`

### 16. script: InitUpgrades / InitTechTree not generated

- Maps: 154 (Lua 1: 0240). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp (no generator for w3i upgrades/tech availability)
- `0240 (Lua)`: `main: InitTechTree ( )`
- `0001`: `main: call InitUpgrades ( )`
- `0002`: `main: call InitUpgrades ( )`

### 17. script: force shared vision (SetPlayerAllianceStateVisionBJ) missing

- Maps: 80 (Lua 1: 0240). Severity: changes-gameplay. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_custom_teams:731-790 (flag 0x04 vs 0x08)
- `0240 (Lua)`: `InitCustomTeams: SetPlayerAllianceStateVisionBJ ( Player ( 0 ) , Player ( 2 ) , true )`
- `0002`: `InitCustomTeams: call SetPlayerAllianceStateVisionBJ ( Player ( 0 ) , Player ( 3 ) , true )`
- `0003`: `InitCustomTeams: call SetPlayerAllianceStateVisionBJ ( Player ( 0 ) , Player ( 1 ) , true )`

### 18. script: initially-on trigger list differs (RunInitializationTriggers)

- Maps: 4 (Lua 0: ). Severity: changes-gameplay. Known: no. Responsible: gui.cpp initialization_triggers
- `0017`: `RunInitializationTriggers: call ConditionalTriggerExecute ( gg_trg_The_Dragons )`
- `0102`: `RunInitializationTriggers: call ConditionalTriggerExecute ( gg_trg_Map_Initilization )`

### 20. script: Cursed_Flute-style extra trigger (gg_trg_/InitTrig_ for disabled or custom-text trigger)

- Maps: 6 (Lua 0: ). Severity: unknown. Known: yes. Responsible: triggers.ixx emits_trigger_identifier
- `0001`: `extra InitTrig_Coward_Meets_Grunt,InitTrig_Coward_Turns_Corner,InitTrig_Defend,InitTrig_Ensnaree_Escapes,InitTrig_Ensnaree_Runs,InitTrig_Ensnarer_Attacked,I`
- `0002`: `extra InitTrig_Disable_Leaderboard,InitTrig_FarmCancelled,InitTrig_FarmCompletes,InitTrig_FarmDestroyed,InitTrig_FootmanPopsOut,InitTrig_Horses_Leave,InitTr`

### 21. script: camera dest position / field values differ

- Maps: 181 (Lua 1: 0239). Severity: unknown. Known: no. Responsible: src/base/triggers/map_script.cpp generate_cameras:405-443 / w3c v3 reader
- `0239 (Lua)`: `CreateCameras: CameraSetupSetField ( gg_cam_Camera_001 , CAMERA_FIELD_ROTATION , 90.5 , 0 )`
- `0001`: `CreateCameras: call CameraSetupSetField ( gg_cam_Close_Arthas , CAMERA_FIELD_ROTATION , 83.7 , 0 )`
- `0002`: `CreateCameras: call CameraSetupSetField ( gg_cam_Blademaster01A , CAMERA_FIELD_ROTATION , 81.9 , 0 )`

### 22. script: gg_trg_ globals of disabled/comment triggers not declared

- Maps: 136 (Lua 0: ). Severity: unknown. Known: no. Responsible: src/base/triggers/map_script.cpp generate_global_variables:78 skips disabled/comment triggers
- `0001`: `missing gg_trg_If_Gerard_Dies`
- `0002`: `missing gg_trg___Disable_Leaderboard`

### 23. script: extra/missing destructable globals

- Maps: 5 (Lua 1: 0239). Severity: unknown. Known: no. Responsible: src/base/triggers/map_script.cpp gg_dest_ scan
- `0239 (Lua)`: `gg_dest_BUnE_1220`
- `0056`: `gg_dest_ATg3_0032`
- `0102`: `gg_dest_LOcg_1355`

### 24. script: preplaced units inlined into CreateAllUnits (WE per-player functions)

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_units
- `0239 (Lua)`: `missing CreateBuildingsForPlayer10,CreateNeutralPassiveBuildings,CreatePlayerBuildings,CreatePlayerUnits`
- `0240 (Lua)`: `missing CreateNeutralPassive,CreatePlayerBuildings,CreatePlayerUnits,CreateUnitsForPlayer21,CreateUnitsForPlayer4`
- `0407 (Lua)`: `missing CreateNeutralHostile,CreateNeutralPassive,CreateNeutralPassiveBuildings,CreatePlayerBuildings,CreatePlayerUnits`
- `0001`: `missing CreateBuildingsForPlayer2,CreateBuildingsForPlayer9,CreateNeutralHostile,CreateNeutralPassive,CreateNeutralPassiveBuildi`
- `0002`: `missing CreateBuildingsForPlayer0,CreateBuildingsForPlayer1,CreateBuildingsForPlayer2,CreateNeutralHostile,CreateNeutralPassive,`

### 25. script: SetDayNightModels unit model + slashes

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/base/triggers/map_script.cpp:843-845 (unit_lights reads TerrainLights)
- `0239 (Lua)`: `main: SetDayNightModels ( "Environment\\DNC\\DNCUnderground\\DNCUndergroundTerrain\\DNCUndergroundTerrain.mdl" , "Environment\\DNC\\DNCUnderground\\DNCUndergroundUnit\\DNCUnder`
- `0240 (Lua)`: `main: SetDayNightModels ( "Environment\\DNC\\DNCLordaeron\\DNCLordaeronTerrain\\DNCLordaeronTerrain.mdl" , "02fbcd95-156c-4d08-8869-77d787397e2f.mdl" )`
- `0407 (Lua)`: `main: SetDayNightModels ( "Environment\\DNC\\DNCLordaeron\\DNCLordaeronTerrain\\DNCLordaeronTerrain.mdl" , "Environment\\DNC\\DNCLordaeron\\DNCLordaeronUnit\\DNCLordaeronUnit.m`
- `0001`: `main: call SetDayNightModels ( "Environment\\DNC\\DNCLordaeron\\DNCLordaeronTerrain\\DNCLordaeronTerrain.mdl" , "Environment\\DNC\\DNCLordaeron\\DNCLordaeronUnit\\DNCLordaeronU`
- `0002`: `main: call SetDayNightModels ( "Environment\\DNC\\DNCLordaeron\\DNCLordaeronTerrain\\DNCLordaeronTerrain.mdl" , "Environment\\DNC\\DNCLordaeron\\DNCLordaeronUnit\\DNCLordaeronU`

### 26. doodad/unit x/y float bits

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Known: no. Responsible: doodads.ixx:127/227, units.ixx:289/413 (/128 then *128 + offset)
- `0239 (Lua)`: `war3map.doo doodads[144].y: 11834.5302734375 -> 11834.53125`
- `0240 (Lua)`: `war3map.doo doodads[9137].x: -3910.8525390625 -> -3910.8515625`
- `0407 (Lua)`: `war3map.doo doodads[1581].x: 1471.842529296875 -> 1471.8427734375`
- `0001`: `war3map.doo doodads[2010].y: 5217.51611328125 -> 5217.515625`
- `0002`: `war3map.doo doodads[662].x: 2741.56396484375 -> 2741.564453125`

### 27. Misc.txt: lowercase keys / BOM / LF

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Known: yes. Responsible: gameplay_constants.ixx save
- `0239 (Lua)`: `war3mapMisc.txt bom: False -> True`
- `0240 (Lua)`: `war3mapMisc.txt bom: False -> True`
- `0407 (Lua)`: `war3mapMisc.txt <extra>: None -> None`
- `0001`: `war3mapMisc.txt bom: False -> True`
- `0002`: `war3mapMisc.txt bom: False -> True`

### 29. wtg: header element counts and deleted-id lists zeroed

- Maps: 233 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Known: no. Responsible: triggers.ixx:983-1003 Triggers::save writes 0 for every count/list
- `0239 (Lua)`: `war3map.wtg counts.maps: 1 -> 0`
- `0240 (Lua)`: `war3map.wtg counts.maps: 1 -> 0`
- `0407 (Lua)`: `war3map.wtg counts.maps: 1 -> 0`
- `0001`: `war3map.wtg counts.maps: 1 -> 0`
- `0002`: `war3map.wtg counts.maps: 1 -> 0`

### 30. object data: empty tables written where WE wrote none

- Maps: 231 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Known: no. Responsible: Map::save writes every object table
- `0239 (Lua)`: `extra war3map.w3h,war3map.w3q,war3mapSkin.w3h,war3mapSkin.w3q`
- `0240 (Lua)`: `extra war3map.w3h,war3map.w3q,war3mapSkin.w3h,war3mapSkin.w3q`
- `0407 (Lua)`: `extra war3map.w3a,war3map.w3b,war3map.w3d,war3map.w3h,war3map.w3q,war3map.w3t,war3map.w3u,war3mapSkin.w3a,war3mapSkin.w3b,war3mapSkin.w3d,war3mapSkin.w3h,war3mapSkin.w3q,war3mapSkin.w3t,war3mapSkin.w3u`
- `0001`: `extra war3map.w3a,war3map.w3b,war3map.w3d,war3map.w3h,war3map.w3q,war3map.w3t,war3mapSkin.w3a,war3mapSkin.w3b,war3mapSkin.w3d,war3mapSkin.w3h,war3mapSkin.w3q,war3mapSkin.w3t`
- `0002`: `extra war3map.w3a,war3map.w3b,war3map.w3d,war3map.w3h,war3map.w3q,war3map.w3t,war3mapSkin.w3a,war3mapSkin.w3b,war3mapSkin.w3d,war3mapSkin.w3h,war3mapSkin.w3q,war3mapSkin.w3t`

### 31. script: item-drop helper functions renamed

- Maps: 184 (Lua 48: 0240, 0407, 0408, 0409, 0410, 0411, 0412, 0413). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_item_tables
- `0240 (Lua)`: `ItemTable000000_DropItems missing; extra []`
- `0407 (Lua)`: `Unit000001_DropItems missing; extra ['UnitItemDrops_20']`
- `0408 (Lua)`: `Unit000004_DropItems missing; extra ['UnitItemDrops_10']`
- `0001`: `Unit000025_DropItems missing; extra ['UnitItemDrops_12']`
- `0002`: `Unit000023_DropItems missing; extra ['UnitItemDrops_148']`

### 32. script: empty Create*/Init* functions emitted and called from main

- Maps: 169 (Lua 48: 0240, 0407, 0408, 0409, 0410, 0411, 0412, 0413). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_main
- `0240 (Lua)`: `extra CreateAllDestructables,CreateAllItems,CreateCameras`
- `0407 (Lua)`: `extra CreateAllDestructables,CreateAllItems,CreateCameras,CreateRegions,InitSounds`
- `0408 (Lua)`: `extra CreateAllDestructables,CreateAllItems,CreateCameras,CreateRegions,InitSounds`
- `0001`: `extra CreateAllItems`
- `0002`: `extra CreateAllItems`

### 34. w3e: v11 upgraded to v12 (corners rewritten)

- Maps: 46 (Lua 46: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Known: no. Responsible: terrain.ixx save write_version
- `0407 (Lua)`: `war3map.w3e version: 11 -> 12`
- `0408 (Lua)`: `war3map.w3e version: 11 -> 12`
- `0409 (Lua)`: `war3map.w3e version: 11 -> 12`

### 35. w3i: field changes

- Maps: 46 (Lua 46: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/map_info.ixx:193 write_version 33 for pre-39 maps
- `0407 (Lua)`: `war3map.w3i version: 31 -> 33`
- `0408 (Lua)`: `war3map.w3i version: 31 -> 33`
- `0409 (Lua)`: `war3map.w3i version: 31 -> 33`

### 36. script: destructables lose pitch/roll/vertex colour (3.0 natives)

- Maps: 141 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_destructables:302-370 (writes BlzCreateDestructableZWithSkin)
- `0239 (Lua)`: `CreateAllDestructables: gg_dest_B000_0144 = BlzCreateDestructableWithSkinPitchRollColor ( FourCC ( "B000" ) , - 13635 , 11834.5 , 173 , 0 , 0 , 1 , 0 , FourCC ( "B000" ) , ConvertPlayerColor ( 2`
- `0001`: `CreateAllDestructables: set gg_dest_LOcg_1482 = BlzCreateDestructableWithSkinPitchRoll ( 'LOcg' , 704 , - 4096 , 209.589 , 0 , 0 , 0.93 , 0 , 'LOcg' )`
- `0002`: `CreateAllDestructables: set gg_dest_LTcr_2866 = BlzCreateDestructableWithSkinPitchRoll ( 'LTcr' , - 4608 , - 3264 , 0 , 0 , 0 , 1.2 , 1 , 'LTcr' )`

### 37. w3e: custom_tileset flag 0 -> 1

- Maps: 127 (Lua 17: 0407, 0409, 0412, 0422, 0426, 0427, 0428, 0430). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/terrain.ixx:547 writes 1
- `0407 (Lua)`: `war3map.w3e custom_tileset: 0 -> 1`
- `0409 (Lua)`: `war3map.w3e custom_tileset: 0 -> 1`
- `0412 (Lua)`: `war3map.w3e custom_tileset: 0 -> 1`
- `0002`: `war3map.w3e custom_tileset: 0 -> 1`
- `0003`: `war3map.w3e custom_tileset: 0 -> 1`

### 38. script: SetHDWaterParamsEx missing

- Maps: 188 (Lua 3: 0239, 0240, 0430). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_main
- `0239 (Lua)`: `main: SetHDWaterParamsEx ( 0 , 0 , 0 , false , 20 , 0 , 100 , 10 , 0 , 10 , 100 , 100 )`
- `0240 (Lua)`: `main: SetHDWaterParamsEx ( 50 , 50 , 75 , true , 20 , 0 , 80 , 50 , 0 , 100 , 100 , 25 )`
- `0430 (Lua)`: `main: SetHDWaterParamsEx ( 0 , 0 , 0 , false , 20 , 0 , 100 , 10 , 0 , 50 , 100 , 100 )`
- `0001`: `main: call SetHDWaterParamsEx ( 0 , 0 , 0 , false , 20 , 0 , 100 , 10 , 0 , 10 , 100 , 100 )`
- `0002`: `main: call SetHDWaterParamsEx ( 0 , 0 , 0 , false , 20 , 0 , 100 , 10 , 0 , 10 , 100 , 100 )`

### 39. object data: skin fields moved from war3mapSkin.* into base files

- Maps: 182 (Lua 3: 0239, 0240, 0411). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/utilities/modification_tables.ixx:357
- `0239 (Lua)`: `war3mapSkin.w3a custom[AIpz,Agrt].sets[0].mods[aefs,0,0]: {'field': 'aefs', 'type': 3, 'level': 0, 'data_poi -> <absent>`
- `0240 (Lua)`: `war3mapSkin.w3b original[FTtw,].sets[0].mods[bmas,0,0]: {'field': 'bmas', 'type': 2, 'value': '3.5', 'end' -> <absent>`
- `0411 (Lua)`: `war3mapSkin.w3u original[nbsm,].sets[0].mods[uabs,0,0]: {'field': 'uabs', 'type': 3, 'value': 'Avul', 'end -> <absent>`
- `0001`: `war3mapSkin.w3u custom[nvlk,nC00].sets[0].mods[unam,0,0]: {'field': 'unam', 'type': 3, 'value': 'TRIGSTR_219 -> <absent>`
- `0002`: `war3mapSkin.w3u original[Obla,].sets[0].mods[upro,0,0]: {'field': 'upro', 'type': 3, 'value': 'TRIGSTR_565 -> <absent>`

### 40. object data: end token id -> 0

- Maps: 146 (Lua 3: 0239, 0240, 0411). Severity: cosmetic-or-equivalent. Known: yes. Responsible: modification_tables.ixx save
- `0239 (Lua)`: `war3map.w3a original[AIrc,].sets[0].mods[Ircd,1,1].end: AIrc ->     `
- `0240 (Lua)`: `war3map.w3a original[Aihn,].sets[0].mods[areq,0,0].end: Aihn ->     `
- `0411 (Lua)`: `war3map.w3u original[nbsm,].sets[0].mods[uabi,0,0].end: nbsm ->     `
- `0014`: `war3map.w3u original[Etyr,].sets[0].mods[uabi,0,0].end: Etyr ->     `
- `0035`: `war3map.w3a original[AUds,].sets[0].mods[aite,0,0].end: AUds ->     `

### 41. script: SetPlayerRaceSkin missing

- Maps: 60 (Lua 3: 0239, 0240, 0430). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_players
- `0239 (Lua)`: `InitCustomPlayerSlots: SetPlayerRaceSkin ( Player ( 1 ) , RACE_PREF_FORSAKEN )`
- `0240 (Lua)`: `InitCustomPlayerSlots: SetPlayerRaceSkin ( Player ( 0 ) , RACE_PREF_USER_SELECTABLE )`
- `0430 (Lua)`: `InitCustomPlayerSlots: SetPlayerRaceSkin ( Player ( 0 ) , RACE_PREF_USER_SELECTABLE )`
- `0007`: `InitCustomPlayerSlots: call SetPlayerRaceSkin ( Player ( 0 ) , RACE_PREF_USER_SELECTABLE )`
- `0011`: `InitCustomPlayerSlots: call SetPlayerRaceSkin ( Player ( 1 ) , RACE_PREF_USER_SELECTABLE )`

### 42. script: GUI helper functions renamed (FuncNNN -> HiveWEn)

- Maps: 187 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Known: no. Responsible: gui.cpp generate_function_name
- `0239 (Lua)`: `Trig_Next_Level_Prep_Func003C`
- `0240 (Lua)`: `Trig_EndMap_Func001C`
- `0001`: `Trig_Delete_Uther_Func001001`
- `0002`: `Trig_BarracksConstructed_Queue_Func007001`

### 43. wtg: call has_params 1 -> 0 on zero-arg calls

- Maps: 186 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Known: no. Responsible: Triggers::save ECA parameter writer
- `0239 (Lua)`: `war3map.wtg elements[8,50331651].ecas[1].params[1].call.has_params: 1 -> 0`
- `0240 (Lua)`: `war3map.wtg elements[8,50332328].ecas[1].params[0].call.has_params: 1 -> 0`
- `0001`: `war3map.wtg elements[8,50331648].ecas[113].params[1].call.params[0].call.has_params: 1 -> 0`
- `0002`: `war3map.wtg elements[8,50331648].ecas[31].params[1].call.params[0].call.has_params: 1 -> 0`

### 44. wts: // comments dropped

- Maps: 170 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Known: yes. Responsible: trigger_strings.ixx:61 save
- `0239 (Lua)`: `war3map.wts strings[14].comment: // Destructibles: B000 (3D Point), Name (Name) -> `
- `0240 (Lua)`: `war3map.wts strings[2161].comment: // Doodads: Dmcp (Roadmarker Center Point), Name ( -> `
- `0001`: `war3map.wts strings[3].comment: // Units: nC08 (Amy Diane), Name (Name) -> `
- `0002`: `war3map.wts strings[47].comment: // Units: hC00 (Dragon Hunter), Name (Name) -> `

### 45. script: 3.0 camera fields missing (BlzCameraSetupSetCameraType etc.)

- Maps: 181 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_cameras
- `0239 (Lua)`: `CreateCameras: BlzCameraSetupSetCameraType ( gg_cam_Camera_001 , 0 )`
- `0001`: `CreateCameras: call BlzCameraSetupSetCameraType ( gg_cam_Close_Arthas , 0 )`
- `0002`: `CreateCameras: call BlzCameraSetupSetCameraType ( gg_cam_Blademaster01A , 0 )`

### 46. wct: entry count / trailing empty entries / header differ

- Maps: 114 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Known: no. Responsible: triggers.ixx save_scripts
- `0239 (Lua)`: `war3map.wct header_script:   -> `
- `0044`: `war3map.wct trailing_empty_entries: 134 -> 128`
- `0045`: `war3map.wct trailing_empty_entries: 131 -> 125`

### 47. script: placed item coordinates printed with more decimals

- Maps: 70 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_items
- `0239 (Lua)`: `CreateAllItems: gg_item_ergg_0000 = BlzCreateItemWithSkin ( FourCC ( "ergg" ) , - 13861.6 , 12732.5 , FourCC ( "ergg" ) )`
- `0004`: `CreateAllItems: call BlzCreateItemWithSkin ( 'pman' , - 4932 , 2671.3 , 'pman' )`
- `0014`: `CreateAllItems: set gg_item_gcel_0023 = BlzCreateItemWithSkin ( 'gcel' , - 1396.3 , - 175.1 , 'gcel' )`

### 49. script: item colour BlzSetItemColor / gg_item_ globals

- Maps: 29 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_items
- `0239 (Lua)`: `CreateAllItems: BlzSetItemColor ( gg_item_ergg_0000 , ConvertPlayerColor ( 6 ) )`
- `0044`: `CreateAllItems: call BlzSetItemColor ( i , ConvertPlayerColor ( 0 ) )`
- `0052`: `CreateAllItems: call BlzSetItemColor ( i , ConvertPlayerColor ( 0 ) )`

### 50. script: dialogue TRIGSTR keys differ (SetDialogueTextKey/SpeakerNameKey)

- Maps: 5 (Lua 1: 0240). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_sounds
- `0240 (Lua)`: `InitSounds: SetDialogueSpeakerNameKey ( gg_snd_A1M4_C010_010_GLIX , "TRIGSTR_2829" )`
- `0147`: `InitSounds: call SetDialogueTextKey ( gg_snd_A05Kael01 , "TRIGSTR_272" )`
- `0156`: `InitSounds: call SetDialogueTextKey ( gg_snd_S04Tyrande27 , "TRIGSTR_58" )`

### 51. compile: Skip Remaining Actions emitted as call ReturnAction()

- Maps: 149 (Lua 0: ). Severity: breaks-in-game. Known: no. Responsible: src/base/triggers/gui.cpp convert_eca_to_script: no special case for ReturnAction (WE writes `return`)
- `0003`: `Line 690:  Undeclared function ReturnAction`
- `0005`: `Line 1724:  Undeclared function ReturnAction`

### 52. compile: nested helper function (unexpected takes)

- Maps: 34 (Lua 0: ). Severity: breaks-in-game. Known: yes. Responsible: gui.cpp helper generation (known ticket)
- `0001`: `Line 3384: Syntax Error, unexpected: "takes"?`
- `0002`: `Line 2259: Syntax Error, unexpected: "takes"?`

### 53. compile: rawcode-typed variable initial value unquoted

- Maps: 7 (Lua 0: ). Severity: breaks-in-game. Known: no. Responsible: src/base/triggers/map_script.cpp generate_init_global_variables:115-125 (rawcode types fall through to the raw value)
- `0037`: `Line 1529:  Undeclared variable gg_trg___Load_Arthas`
- `0048`: `Line 721:  Undeclared variable hfoo`

### 54. compile: string initial value not escaped

- Maps: 5 (Lua 0: ). Severity: breaks-in-game. Known: yes. Responsible: src/base/triggers/map_script.cpp:119 generate_init_global_variables
- `0110`: `Line 686:  Invalid escape character sequence`
- `0127`: `Line 577:  Invalid escape character sequence`

### 55. resave: load failed / refused

- Maps: 3 (Lua 0: ). Severity: breaks-in-game. Known: no. Responsible: Map::load between doodads and units (w3u/w3t mods or Units::load); undead02 refused at wtg preflight
- `0046`: `Resave failed while loading: ankerl::unordered_dense::map::at(): key not found`
- `0133`: `Resave refused at open: This map cannot be opened because war3map.wtg is damaged or incomplete.`

### 56. compile: duplicate region names declared twice

- Maps: 2 (Lua 0: ). Severity: breaks-in-game. Known: no. Responsible: src/base/triggers/map_script.cpp generate_global_variables:59 (no de-duplication of gg_rct_ names)
- `0116`: `Line 296:  Symbol gg_rct_InitialHuntress03C multiply defined`
- `0147`: `Line 253:  Symbol gg_rct_Intro_Kael_StartB multiply defined`

### 58. compile: garbage variable name (human05 wtg misread)

- Maps: 1 (Lua 0: ). Severity: breaks-in-game. Known: yes. Responsible: triggers.ixx load_version_31 (fixed 3 ints after deleted-id lists)
- `0102`: `Line 2: Invalid identifier name: udg_�`

### 59. wtg: written file does not parse

- Maps: 1 (Lua 0: ). Severity: breaks-in-game. Known: yes. Responsible: Triggers::save after the misread
- `0102`: `wtg parse_failure`

### 61. script: InitSounds setters missing (SetSoundDuration/Channel/Distances/ConeAngles/Pitch)

- Maps: 185 (Lua 0: ). Severity: changes-gameplay. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_sounds:444-570
- `0001`: `InitSounds: call SetSoundDuration ( gg_snd_BanditYesAttack2 , 1216 )`
- `0002`: `InitSounds: call SetSoundDuration ( gg_snd_BlacksmithWhat1 , 3059 )`

### 62. script: region ambient sounds (SetSoundPosition/RegisterStackedSound) missing

- Maps: 98 (Lua 0: ). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_regions
- `0001`: `CreateRegions: call SetSoundPosition ( gg_snd_WaterStreamLoop1 , 1568 , - 1248 , - 195.7 )`
- `0003`: `CreateRegions: call SetSoundPosition ( gg_snd_CityScapeMagicRunesLoop1 , - 2464 , - 736 , 768 )`

### 63. script: gg_unit_ global missing

- Maps: 10 (Lua 0: ). Severity: unknown. Known: yes. Responsible: src/base/triggers/map_script.cpp:938 fixed 4-char gg_unit scan
- `0029`: `missing gg_unit_nrvd_0054`
- `0038`: `missing gg_unit_negt_0066`

### 64. script: trigger names with leading spaces trimmed (gg_trg___X -> gg_trg_X)

- Maps: 6 (Lua 0: ). Severity: unknown. Known: no. Responsible: src/base/triggers/triggers.ixx:520 prefixed_script_identifier trims (upstream trimmed too)
- `0001`: `InitTrig___Coward_Meets_Grunt`
- `0002`: `InitTrig___Disable_Leaderboard`

### 67. script: InitTrig_ order differs (reordered only)

- Maps: 173 (Lua 0: ). Severity: cosmetic-or-equivalent. Known: no. Responsible: src/base/triggers/map_script.cpp generate_trigger_initialization
- `0001`: `reordered [{'name': 'InitTrig_Camp_is_entered', 'lines': 7}, {'name': 'InitTrig_Intro_Cancel', 'lines': 6}]`
- `0002`: `reordered [{'name': 'InitTrig_Blademaster_Cancel', 'lines': 6}, {'name': 'InitTrig_HINTMilitia', 'lines': 7}]`

### 68. object data: original-object record with no mods dropped

- Maps: 34 (Lua 0: ). Severity: cosmetic-or-equivalent. Known: no. Responsible: modification_tables.ixx save skips objects without changed fields
- `0004`: `war3map.w3u original[nenf,]: {'base_id': 'nenf', 'new_id': '\x00\x00\x00\x00',  -> <absent>`
- `0082`: `war3map.w3a original[AUdd,]: {'base_id': 'AUdd', 'new_id': '\x00\x00\x00\x00',  -> <absent>`

### 69. script: item SetItemColor missing (classic items)

- Maps: 3 (Lua 0: ). Severity: cosmetic-or-equivalent. Known: yes. Responsible: src/base/triggers/map_script.cpp generate_items
- `0127`: `CreateAllItems: call SetItemColor ( i , ConvertPlayerColor ( 0 ) )`
- `0130`: `CreateAllItems: call SetItemColor ( i , ConvertPlayerColor ( 0 ) )`

### 70. object data: file version changed

- Maps: 1 (Lua 0: ). Severity: cosmetic-or-equivalent. Known: no. Responsible: mod_table_write_version
- `0107`: `war3map.w3b version: 2 -> 3`

### 77. compile: other

- Maps: 1 (Lua 0: ). Severity: unknown. Known: no. Responsible: see report 1 kind 57 (`not ()` empty condition, 0081).
- `0081`: `Line 4951: Syntax Error, unexpected: ")"?`

### 78. script: waygate destination (WaygateSetDestination/WaygateActivate) missing

- Maps: 1 confirmed (Lua 1: 0446; about 17 more JASS maps call it in the WE script). Severity: changes-gameplay. Known: no. Responsible: src/base/triggers/map_script.cpp generate_units never emits waygate destinations; Unit::waygate (units.ixx:73, read at :371, written at :472) holds the target region index. Without it the waygates do nothing in game. Counting: Lua 0446 confirmed by the simulator (12 calls lost, 6 waygates); JASS count is every map whose WE script calls WaygateSetDestination (18 in total, 17 JASS), not confirmed per map
- `0446 (Lua)`: net 6 lost: `WaygateSetDestination(<unit@CreateNeutralPassiveBuildings#14>,8608.000,-4192.000)`

## NCOW round-trip (user's own Lua map)

Source `NCOW-Arena/NCOW_Spell_Dev.w3x` is a folder map (4080 files, 382 MB). Everything below ran on scratchpad copies. Before and after, path, size and mtime of all 4083 files under the folder and `.build/` (folder map 4080 + `release.w3x`, `war3map.lua`, `packlist.json`) were compared with a baseline: 0 differences. Newest file in the folder map is still `2amfbody.blp`, 2026-08-26 14:17:40 (folder directory 2026-08-26 00:32:27).

- `HiveWE.exe --resave <folder> <out>` accepts a folder map directly (it only accepts folders; a `.w3x` must be extracted first). Result `ok`, 3.5 s, peak 497 MB, exit 0.
- fdiff (32 files): 29 identical, 2 byte-identical unparsed (`war3map.lua`, `war3mapMap.blp`), 1 differs. `war3map.lua`, `war3map.w3i`, `w3u/w3t/w3a/w3b/w3d/w3h/w3q`, `war3mapSkin.*`, `doo`, `war3mapUnits.doo`, `w3e`, `wtg`, `wts`, `wct`, `Misc.txt` are all identical. The one diff: `war3map.imp` count 4046 -> 4047, entry `Images\Spells\Pein\2amfbody.blp` (flag 13) added.
- sdiff of `war3map.lua`: IDENTICAL after normalization (20/20 functions, 15/15 globals). simdiff: IDENTICAL (1766 logged calls each, 0 warnings, 0 errors).
- Diff kinds: only one, `imp: file on disk but missing from war3map.imp is added`. NOT in the stock sweep (stock dumps hold no imported files, so report 1 only saw dropped entries, kinds 74/75). Cause: `2amfbody.blp` was added to the folder at 14:17:40, after the imp list was written at 00:59:52; HiveWE rebuilds the list from disk. Benign (it makes the file an import).
- None of the stock-sweep Lua kinds (3-13) appear. Reason, an inference: the folder's `war3map.lua` was already generated by HiveWE (HiveWE rewrites it byte for byte), so the round trip is a fixed point. It shows no WE-versus-HiveWE script differences for this map and no value for kinds 3-13; those would show when the map is opened in the real World Editor.
- `.build/release.w3x`: read-only StormLib open of a scratchpad copy, files read by name only (no `SFileFindFirstFile`). Of the 32 `war3map*` files in the folder, 31 are byte-identical in the archive and the 32nd is `war3map.lua`, which equals `.build/war3map.lua` (same SHA-256). So the packed map's data files are the folder map's; only the script differs.
- `.build/war3map.lua` (1.38 MB) is one minified bundle: user modules registered as `a['Bootstrap/Core.lua']=function(_ENV,...)...`, the editor script as module `origwar3map.lua` (38,232 chars, loaded with `dofile('origwar3map.lua')`), then `main` is wrapped in `xpcall(... dofile('main.lua'))`. HiveWE cannot read that bundle, so the check was: extract the `origwar3map.lua` module from the build, and run simdiff against HiveWE's resave of (folder map + the bundle as war3map.lua). Result: IDENTICAL (1766 calls, 0 warnings). So the user's merged build would not change from a HiveWE save of the editor part. sdiff cannot compare minified Lua (it reports all 20 functions as extra), a tool limit. A HiveWE save of the folder replaces `war3map.lua` with the editor part only, as for any folder map; the user code lives in `src/` and is merged by warcraft-vscode at build time.
- User code against editor-generated functions (grep of `src/` and `scripts/`, `.lua`): nothing defines or reassigns `main`, `config`, `CreateAllUnits`, `InitCustomTeams`, `InitCustomPlayerSlots`, `InitAllyPriorities`, `CreateRegions`, `InitSounds` or `InitTrig_*`. Hooks that wrap generated functions, which need them to stay global functions with these names (HiveWE's output keeps them): `src/Custom-Systems/LuaSystem.lua:41-72` wraps `RunInitializationTriggers`, `InitCustomTriggers` and `InitGlobals` (`local old = X; X = function ...`), and `src/Libraries/WarcraftStudioCode.lua:1263-1268` wraps `InitGlobals` and `InitCustomTriggers`. Several character files declare `local function InitGlobals()` (local, no clash).
- Not run: the in-game load of the packed map, and the stock-sweep fix kinds against NCOW (they cannot show, see above).
