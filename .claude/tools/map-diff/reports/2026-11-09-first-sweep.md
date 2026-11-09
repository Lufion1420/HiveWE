# First differential sweep, 2026-11-09

HiveWE (`build/Agent` exe, commit dc9763a, `--resave`) loaded and saved 237 stock 3.0.1 maps: the 191 w3i-v39 maps plus all 49 Lua maps (3 overlap). Each output was compared with the World Editor original by `fdiff.py` (files), `sdiff.py` (script) and, for Lua, `simdiff.py` (simulated natives). Raw per-map JSON stays in the scratchpad; this file and `2026-11-09-first-sweep.json` are the summary.

## Run

| maps | ok | script_compile_error | refused / load failed | timeout | crash |
|---|---|---|---|---|---|
| 237 (49 Lua, 188 JASS) | 49 (all 49 Lua) | 185 (185 of 188 JASS) | 3 | 0 | 0 |

- Time: 13m40s resave+diff sweep, 8m23s JASS generated-script pass, 2m18s untruncated fdiff rerun (24m21s total). Peak HiveWE working set about 320 MB; fdiff/sdiff/sim stayed under 20 MB. No process was left running.
- Every JASS map but 0046/0133/0144 hit `script_compile_error`, so HiveWE kept the WE `war3map.j`. A second pass resaved each of them again and copied HiveWE's pre-jasshelper script (`%TEMP%/input.lua`) right after the process exited; script kinds for JASS maps come from that file. jasshelper stops at the first syntax error, so the `compile:` counts for syntax errors are lower bounds.
- fdiff was rerun with `--max-diffs 20000` because the default 300 truncated war3map.wtg on 138 maps and war3map.doo on 80.
- Not run: the World Editor and the game itself. Severity is a judgement from the diff, not an in-game test.

## Kinds, ranked

Lua-affecting kinds first, then by severity. `known` = on the brief's ticket list. Harness and tool artifacts are at the end.

| # | kind | maps | Lua | severity | origin | known |
|---|---|---|---|---|---|---|
| 1 | script: preplaced-unit ability order emitted as a char list | 61 | 4 | breaks-in-game | upstream HiveWE (stijnherfst) (2fc3208 'Use more std::string_view') |  |
| 2 | script: Lua custom-text trigger never initialised (InitTrig_/RunInit call dropped) | 2 | 1 | breaks-in-game | fork (d611820 emits_trigger_identifier) |  |
| 3 | script: SetTeams count | 222 | 49 | changes-gameplay | upstream HiveWE (stijnherfst) | known |
| 4 | script: SetGamePlacement always USE_MAP_SETTINGS | 48 | 47 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 5 | script: melee player slots not race-selectable | 47 | 47 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 6 | script: preplaced unit custom colour (SetUnitColor) missing | 44 | 44 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 7 | script: gold mine SetResourceAmount missing | 43 | 43 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 8 | script: SetWaterBaseColor missing | 28 | 16 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 9 | script: ally priorities wrong (SetEnemyStartLocPrio) | 169 | 15 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 10 | doodad skin id replaced | 19 | 15 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 11 | script: SetTerrainFogEx missing | 43 | 11 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 12 | script: map-wide weather effect missing | 12 | 6 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 13 | script: NewSoundEnvironment from tileset instead of w3i | 14 | 4 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 14 | imp: _Locales entries dropped | 187 | 2 | changes-gameplay | upstream HiveWE (stijnherfst) | known |
| 15 | script: random groups (InitRandomGroups) missing | 2 | 2 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 16 | script: InitUpgrades / InitTechTree not generated | 154 | 1 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 17 | script: force shared vision (SetPlayerAllianceStateVisionBJ) missing | 82 | 1 | changes-gameplay | upstream HiveWE (stijnherfst) | known |
| 18 | script: initially-on trigger list differs (RunInitializationTriggers) | 4 | 1 | changes-gameplay | upstream HiveWE (stijnherfst) |  |
| 19 | script: player slot setup differs (other) | 49 | 3 | unknown | upstream HiveWE (stijnherfst) |  |
| 20 | script: Cursed_Flute-style extra trigger (gg_trg_/InitTrig_ for disabled or custom-text trigger) | 8 | 2 | unknown | fork | known |
| 21 | script: camera dest position / field values differ | 181 | 1 | unknown | fork/upstream HiveWE (stijnherfst) |  |
| 22 | script: gg_trg_ globals of disabled/comment triggers not declared | 136 | 1 | unknown | upstream HiveWE (stijnherfst) |  |
| 23 | script: extra/missing destructable globals | 5 | 1 | unknown | upstream HiveWE (stijnherfst) |  |
| 24 | script: preplaced units inlined into CreateAllUnits (WE per-player functions) | 234 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 25 | script: SetDayNightModels unit model + slashes | 234 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known |
| 26 | doodad/unit x/y float bits | 234 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) (moved in fork) |  |
| 27 | Misc.txt: lowercase keys / BOM / LF | 234 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known |
| 28 | script: local/global initialiser style (= null, = CreateGroup()) | 234 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 29 | wtg: header element counts and deleted-id lists zeroed | 233 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 30 | object data: empty tables written where WE wrote none | 231 | 49 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 31 | script: item-drop helper functions renamed | 184 | 48 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 32 | script: empty Create*/Init* functions emitted and called from main | 169 | 48 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 33 | script: other sound differences | 55 | 47 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 34 | w3e: v11 upgraded to v12 (corners rewritten) | 46 | 46 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 35 | w3i: field changes | 46 | 46 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 36 | script: destructables lose pitch/roll/vertex colour (3.0 natives) | 184 | 44 | cosmetic-or-equivalent | new in 3.0 |  |
| 37 | w3e: custom_tileset flag 0 -> 1 | 127 | 17 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) (gvremena 6fd24c3) |  |
| 38 | script: SetHDWaterParamsEx missing | 188 | 3 | cosmetic-or-equivalent | new in 3.0 | known |
| 39 | object data: skin fields moved from war3mapSkin.* into base files | 182 | 3 | cosmetic-or-equivalent | fork (moved from upstream modification_tables) | known |
| 40 | object data: end token id -> 0 | 146 | 3 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known |
| 41 | script: SetPlayerRaceSkin missing | 60 | 3 | cosmetic-or-equivalent | new in 3.0 | known |
| 42 | script: GUI helper functions renamed (FuncNNN -> HiveWEn) | 187 | 2 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 43 | wtg: call has_params 1 -> 0 on zero-arg calls | 186 | 2 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 44 | wts: // comments dropped | 170 | 2 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known |
| 45 | script: 3.0 camera fields missing (BlzCameraSetupSetCameraType etc.) | 181 | 1 | cosmetic-or-equivalent | new in 3.0 | known |
| 46 | wct: entry count / trailing empty entries / header differ | 114 | 1 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 47 | script: placed item coordinates printed with more decimals | 77 | 1 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 48 | script: InitGlobals local i | 76 | 1 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 49 | script: item colour BlzSetItemColor / gg_item_ globals | 29 | 1 | cosmetic-or-equivalent | new in 3.0 | known |
| 50 | script: dialogue TRIGSTR keys differ (SetDialogueTextKey/SpeakerNameKey) | 6 | 1 | cosmetic-or-equivalent | fork (2fe340c) |  |
| 51 | compile: Skip Remaining Actions emitted as call ReturnAction() | 101 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  |
| 52 | compile: nested helper function (unexpected takes) | 25 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known |
| 53 | compile: rawcode-typed variable initial value unquoted | 3 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  |
| 54 | compile: string initial value not escaped | 3 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known |
| 55 | resave: load failed / refused | 3 | 0 | breaks-in-game | unknown |  |
| 56 | compile: duplicate region names declared twice | 2 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  |
| 57 | compile: empty condition `not ()` | 1 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) |  |
| 58 | compile: garbage variable name (human05 wtg misread) | 1 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known |
| 59 | wtg: written file does not parse | 1 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known |
| 60 | wtg: variables misread (human05 unknown-list header) | 1 | 0 | breaks-in-game | upstream HiveWE (stijnherfst) | known |
| 61 | script: InitSounds setters missing (SetSoundDuration/Channel/Distances/ConeAngles/Pitch) | 185 | 0 | changes-gameplay | fork (2fe340c generate_sounds) | known |
| 62 | script: region ambient sounds (SetSoundPosition/RegisterStackedSound) missing | 98 | 0 | changes-gameplay | fork/upstream (generate_regions:372-404 mixed) |  |
| 63 | script: gg_unit_ global missing | 10 | 0 | unknown | upstream HiveWE (stijnherfst) | known |
| 64 | script: trigger names with leading spaces trimmed (gg_trg___X -> gg_trg_X) | 6 | 0 | unknown | upstream HiveWE (stijnherfst) |  |
| 65 | wts: strings renumbered/added/removed | 1 | 0 | unknown | upstream HiveWE (stijnherfst) |  |
| 66 | wtg: elements removed/added | 1 | 0 | unknown | upstream HiveWE (stijnherfst) |  |
| 67 | script: InitTrig_ order differs (reordered only) | 173 | 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 68 | object data: original-object record with no mods dropped | 34 | 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 69 | script: item SetItemColor missing (classic items) | 3 | 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) | known |
| 70 | object data: file version changed | 1 | 0 | cosmetic-or-equivalent | upstream HiveWE (stijnherfst) |  |
| 71 | lua/j bytes differ (expected; see script kinds) | 49 | 49 | cosmetic-or-equivalent | - |  |
| 72 | imp: written where WE had none | 47 | 47 | cosmetic-or-equivalent | harness artifact |  |
| 73 | sim: call-count noise from moved code (Player/CreateTrigger) | 45 | 45 | cosmetic-or-equivalent | tool artifact |  |
| 74 | imp: .done marker added (harness) | 187 | 2 | cosmetic-or-equivalent | harness artifact |  |
| 75 | imp: entries for files absent from the dump dropped (harness) | 187 | 2 | cosmetic-or-equivalent | harness artifact |  |
| 76 | script: nested-helper split artifacts in sdiff | 34 | 0 | cosmetic-or-equivalent | tool artifact |  |

## Details

### 1. script: preplaced-unit ability order emitted as a char list

- Maps: 61 (Lua 4: 0410, 0421, 0433, 0449). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst) (2fc3208 'Use more std::string_view').
- Responsible: src/base/triggers/map_script.cpp generate_units:225/230 `"\"" + order_on + "\""` on a string_view formats as a char list
- `0410 (Lua)`: `IssueImmediateOrder(u, ['"', 'b', 'l', 'o', 'o', 'd', 'l', 'u', 's', 't', 'o', 'f', 'f', '"'])` -> sim: LOAD ERROR war3map.lua:777 unexpected symbol near '['; the whole script fails to load
- `0003 (JASS)`: same line in JASS -> jasshelper `Syntax Error, unexpected: "["`
- Note: Merged with the compile kind of the same cause. Any preplaced unit with an autocast ability set on/off triggers it.

### 2. script: Lua custom-text trigger never initialised (InitTrig_/RunInit call dropped)

- Maps: 2 (Lua 1: 0240). Severity: breaks-in-game. Origin: fork (d611820 emits_trigger_identifier).
- Responsible: src/base/triggers/triggers.ixx:512 emits_trigger_identifier() skips every Lua trigger with non-empty custom_text, in InitCustomTriggers and globals; RunInitializationTriggers also drops them
- `0240 (Lua)`: WE: `InitTrig_BeginMap()` + `ConditionalTriggerExecute(gg_trg_BeginMap)`; HiveWE: neither. Sim: BeginMap never created or run (68 StringIdentity calls lost)
- `0102 (JASS)`: `-call ConditionalTriggerExecute(gg_trg_Map_Initilization)` (human05, wtg misread)
- Note: The 'Lua WE emits no gg_trg_/InitTrig_ for custom-text triggers' rule holds for 0239 but not 0240, where the WE emits both. Needs verification which trigger flag the WE uses.

### 3. script: SetTeams count

- Maps: 222 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/triggers/map_script.cpp:875
- `0239 (Lua)`: `SetTeams(2)` -> `SetTeams(1)`
- `0444 (Lua)`: 8 -> 1

### 4. script: SetGamePlacement always USE_MAP_SETTINGS

- Maps: 48 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp:876 generate_map_configuration hard-codes it
- `0407 (Lua)`: WE `SetGamePlacement(MAP_PLACEMENT_TEAMS_TOGETHER)`
- `0444 (Lua)`: same

### 5. script: melee player slots not race-selectable

- Maps: 47 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_players:618 uses race==selectable; WE writes true when w3i flags lack 0x20/0x40 (no custom forces)
- `0407 (Lua)`: WE `SetPlayerRaceSelectable(Player(0), true)`, HiveWE `false`
- `0444 (Lua)`: 8 slots true -> false
- Note: w3i flag check over all 237 maps: flags&0x60==0 -> WE true + TEAMS_TOGETHER (47 maps); 0x60 -> false (190).

### 6. script: preplaced unit custom colour (SetUnitColor) missing

- Maps: 44 (Lua 44: 0240, 0407, 0408, 0409, 0411, 0412, 0413, 0414). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_units: Unit::custom_color never emitted
- `0407 (Lua)`: `SetUnitColor(u, ConvertPlayerColor(9))` lost
- `0408 (Lua)`: 2 lost

### 7. script: gold mine SetResourceAmount missing

- Maps: 43 (Lua 43: 0407, 0408, 0409, 0411, 0412, 0413, 0414, 0415). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_units: Unit::gold is read (units.ixx:329) but never emitted
- `0407 (Lua)`: `SetResourceAmount(u, 12500)` x4 lost
- `0444 (Lua)`: `SetResourceAmount(u, 15000)` lost -> mine falls back to the unit default

### 8. script: SetWaterBaseColor missing

- Maps: 28 (Lua 16: 0407, 0408, 0411, 0413, 0414, 0415, 0418, 0420). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_main (w3i water colour not emitted)
- `0407 (Lua)`: `SetWaterBaseColor(128,128,255,255)` lost
- `0044`: `SetWaterBaseColor(0,128,255,255)`
- Note: Visual.

### 9. script: ally priorities wrong (SetEnemyStartLocPrio)

- Maps: 169 (Lua 15: 0239, 0240, 0418, 0423, 0424, 0427, 0428, 0429). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_ally_priorities:791-825
- `0239 (Lua)`: WE `SetEnemyStartLocPrioCount(1,1)` + `SetEnemyStartLocPrio(1,0,0,LOW)`; HiveWE `SetStartLocPrioCount(0,0)`, `(1,0)`
- `0240 (Lua)`: 70 calls removed, 5 added
- Note: Affects melee start-location placement.

### 10. doodad skin id replaced

- Maps: 19 (Lua 15: 0408, 0409, 0411, 0416, 0417, 0418, 0419, 0423). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/doodad.ixx:118 Doodad::init sets skin_id = id (called from Doodads::create)
- `0408 (Lua)`: doodads[6288].skin LTlt -> FTtw
- `0129`: doodads[630].skin BRrk -> LRrk
- Note: Visible model swap for re-skinned doodads/destructables.

### 11. script: SetTerrainFogEx missing

- Maps: 43 (Lua 11: 0413, 0415, 0418, 0419, 0420, 0422, 0425, 0426). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_main (w3i fog not emitted)
- `0413 (Lua)`: `SetTerrainFogEx(0, 3000, 5000, 0.5, 1, 1, 1)` lost
- `0049`: `SetTerrainFogExV(...)` lost
- Note: Visual/visibility.

### 12. script: map-wide weather effect missing

- Maps: 12 (Lua 6: 0407, 0410, 0422, 0433, 0442, 0449). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_main (w3i weather_id not emitted)
- `0407 (Lua)`: `AddWeatherEffect(Rect(-6144,-6144,6144,6144), 'RAlr')` + Enable lost
- `0059`: `AddWeatherEffect(Rect(...), 'RLlr')`
- Note: Visual.

### 13. script: NewSoundEnvironment from tileset instead of w3i

- Maps: 14 (Lua 4: 0240, 0415, 0420, 0445). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp:847 generate_main reads world_edit_data SoundEnvironment by tileset
- `0240 (Lua)`: WE `NewSoundEnvironment("lake")`, HiveWE `"Default"`
- `0056`: WE `"Dungeon"`, HiveWE `"Default"`
- Note: Audio.

### 14. imp: _Locales entries dropped

- Maps: 187 (Lua 2: 0239, 0240). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/imports.ixx save rebuilds from disk
- `0001`: `_Locales/deDE.w3mod/war3map.wts` (flag 25) dropped

### 15. script: random groups (InitRandomGroups) missing

- Maps: 2 (Lua 2: 0240, 0430). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp (no InitRandomGroups generator; gg_rg_ globals missing)
- `0240 (Lua)`: `RandomDistReset/AddItem/Choose` x4 groups lost, gg_rg_000..003 missing
- `0430 (Lua)`: random-group unit in CreateNeutralPassiveBuildings
- Note: NCOW does not use random groups.

### 16. script: InitUpgrades / InitTechTree not generated

- Maps: 154 (Lua 1: 0240). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp (no generator for w3i upgrades/tech availability)
- `0240 (Lua)`: `InitTechTree_Player1` missing
- `0001`: `call InitUpgrades()` missing from main
- Note: NCOW does not use tech tree/upgrades.

### 17. script: force shared vision (SetPlayerAllianceStateVisionBJ) missing

- Maps: 82 (Lua 1: 0240). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/triggers/map_script.cpp generate_custom_teams:731-790 (flag 0x04 vs 0x08)
- `0240 (Lua)`: 156 `SetPlayerAllianceStateVisionBJ(...)` lost
- `0002`: `SetPlayerAllianceStateVisionBJ(Player(0), Player(3), true)` lost
- Note: Ticketed as 'Forces: shared vision uses the wrong flag bit' (not on the brief's list).

### 18. script: initially-on trigger list differs (RunInitializationTriggers)

- Maps: 4 (Lua 1: 0240). Severity: changes-gameplay. Origin: upstream HiveWE (stijnherfst).
- Responsible: gui.cpp initialization_triggers
- `0017`: extra `ConditionalTriggerExecute(gg_trg_The_Dragons)`
- `0240 (Lua)`: 2 missing (see Lua custom-text kind)

### 19. script: player slot setup differs (other)

- Maps: 49 (Lua 3: 0239, 0240, 0430). Severity: unknown. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_players/custom_teams
- `0002`: InitAllyPriorities body differs

### 20. script: Cursed_Flute-style extra trigger (gg_trg_/InitTrig_ for disabled or custom-text trigger)

- Maps: 8 (Lua 2: 0239, 0240). Severity: unknown. Origin: fork. Known.
- Responsible: triggers.ixx emits_trigger_identifier
- `0239 (Lua)`: extra `InitTrig_Cursed_Flute()`
- `0240 (Lua)`: duplicate InitTrig_BeginMap

### 21. script: camera dest position / field values differ

- Maps: 181 (Lua 1: 0239). Severity: unknown. Origin: fork/upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_cameras:405-443 / w3c v3 reader
- `0239 (Lua)`: `CameraSetupSetDestPosition(cam, -180.1, 1723.4, 0)` -> `(126.593, 141.464, 0)` (not float noise)
- `0001`: rotation 83.7 written with more decimals
- Note: Mostly float precision, but the 0239 target position is wrong. Needs verification (w3c v3 field order?).

### 22. script: gg_trg_ globals of disabled/comment triggers not declared

- Maps: 136 (Lua 1: 0240). Severity: unknown. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_global_variables:78 skips disabled/comment triggers
- `0016`: WE declares `gg_trg_DEBUG_Sentinels = null`
- `0065`: `gg_trg__________Default_Heroes_________u`
- Note: Breaks only custom script that references a disabled trigger.

### 23. script: extra/missing destructable globals

- Maps: 5 (Lua 1: 0239). Severity: unknown. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp gg_dest_ scan
- `0239 (Lua)`: extra `gg_dest_BUnE_1220`
- `0056`: gg_dest_ZTg1_2415

### 24. script: preplaced units inlined into CreateAllUnits (WE per-player functions)

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_units
- `0239 (Lua)`: WE CreateNeutralPassiveBuildings/CreatePlayerUnits; HiveWE one CreateAllUnits
- Note: Same calls, different functions.

### 25. script: SetDayNightModels unit model + slashes

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/triggers/map_script.cpp:843-845 (unit_lights reads TerrainLights)
- `0239 (Lua)`: unit model = terrain model, `/` instead of `\`
- Note: Visual (unit lighting).

### 26. doodad/unit x/y float bits

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst) (moved in fork).
- Responsible: doodads.ixx:127/227, units.ixx:289/413 (/128 then *128 + offset)
- `0001`: units[1].y 1702.8778 -> 1702.8779
- Note: Expected; sub-millimetre.

### 27. Misc.txt: lowercase keys / BOM / LF

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: gameplay_constants.ixx save
- `0001`: BOM added, CRLF -> LF, `ItemStackingEnabled` -> `itemstackingenabled`

### 28. script: local/global initialiser style (= null, = CreateGroup())

- Maps: 234 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp
- `0001`: `local unit u = null`; `group udg_X = CreateGroup()` in globals

### 29. wtg: header element counts and deleted-id lists zeroed

- Maps: 233 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: triggers.ixx:983-1003 Triggers::save writes 0 for every count/list
- `0239 (Lua)`: counts.triggers 96 -> 0, deleted-id lists emptied
- Note: World Editor tolerance unknown; Needs verification.

### 30. object data: empty tables written where WE wrote none

- Maps: 231 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: Map::save writes every object table
- `0239 (Lua)`: war3map.w3h, war3map.w3q, war3mapSkin.w3h/w3q added

### 31. script: item-drop helper functions renamed

- Maps: 184 (Lua 48: 0240, 0407, 0408, 0409, 0410, 0411, 0412, 0413). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_item_tables
- `0407 (Lua)`: Unit000043_DropItems -> UnitItemDrops_62; Doodad003525_DropItems -> DoodadItemDrops_423

### 32. script: empty Create*/Init* functions emitted and called from main

- Maps: 169 (Lua 48: 0240, 0407, 0408, 0409, 0410, 0411, 0412, 0413). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_main
- `0407 (Lua)`: empty CreateAllItems/CreateCameras/InitSounds added

### 33. script: other sound differences

- Maps: 55 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp
- `0407 (Lua)`: empty InitSounds() emitted and called

### 34. w3e: v11 upgraded to v12 (corners rewritten)

- Maps: 46 (Lua 46: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: terrain.ixx save write_version
- `0407 (Lua)`: version 11 -> 12

### 35. w3i: field changes

- Maps: 46 (Lua 46: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/map_info.ixx:193 write_version 33 for pre-39 maps
- `0407 (Lua)`: version 31 -> 33

### 36. script: destructables lose pitch/roll/vertex colour (3.0 natives)

- Maps: 184 (Lua 44: 0239, 0407, 0408, 0409, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Origin: new in 3.0.
- Responsible: src/base/triggers/map_script.cpp generate_destructables:302-370 (writes BlzCreateDestructableZWithSkin)
- `0239 (Lua)`: WE `BlzCreateDestructableWithSkinPitchRollColor(..., ConvertPlayerColor(24))` + `SetDestructableVertexColor(d,255,255,255,255)`; HiveWE `BlzCreateDestructableZWithSkin(...)`
- `0001`: dead destructables: WE `BlzCreateDeadDestructableZWithSkinPitchRoll`, HiveWE create + `SetDestructableLife(d, 0*life)`
- Note: Default values here (0 pitch/roll, white, colour 24) are equivalent; non-default pitch/roll/vertex colour is lost (Needs verification how many stock objects use them).

### 37. w3e: custom_tileset flag 0 -> 1

- Maps: 127 (Lua 17: 0407, 0409, 0412, 0422, 0426, 0427, 0428, 0430). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst) (gvremena 6fd24c3).
- Responsible: src/base/terrain.ixx:547 writes 1
- `0407 (Lua)`: custom_tileset 0 -> 1

### 38. script: SetHDWaterParamsEx missing

- Maps: 188 (Lua 3: 0239, 0240, 0430). Severity: cosmetic-or-equivalent. Origin: new in 3.0. Known.
- Responsible: src/base/triggers/map_script.cpp generate_main
- `0239 (Lua)`: `SetHDWaterParamsEx(0,0,0,false,20,0,100,10,0,10,100,100)` lost

### 39. object data: skin fields moved from war3mapSkin.* into base files

- Maps: 182 (Lua 3: 0239, 0240, 0411). Severity: cosmetic-or-equivalent. Origin: fork (moved from upstream modification_tables). Known.
- Responsible: src/utilities/modification_tables.ixx:357
- `0239 (Lua)`: war3mapSkin.w3u `unam`/`usca` moved into war3map.w3u

### 40. object data: end token id -> 0

- Maps: 146 (Lua 3: 0239, 0240, 0411). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: modification_tables.ixx save
- `0014`: original[Etyr].mods[uabi].end Etyr -> 0

### 41. script: SetPlayerRaceSkin missing

- Maps: 60 (Lua 3: 0239, 0240, 0430). Severity: cosmetic-or-equivalent. Origin: new in 3.0. Known.
- Responsible: src/base/triggers/map_script.cpp generate_players
- `0239 (Lua)`: `SetPlayerRaceSkin(Player(1), RACE_PREF_FORSAKEN)` lost

### 42. script: GUI helper functions renamed (FuncNNN -> HiveWEn)

- Maps: 187 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: gui.cpp generate_function_name
- `0239 (Lua)`: Trig_Next_Level_Prep_Func003C -> Trig_Next_Level_Prep_HiveWE0

### 43. wtg: call has_params 1 -> 0 on zero-arg calls

- Maps: 186 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: Triggers::save ECA parameter writer
- `0001`: ecas[182].params[1].call.has_params 1 -> 0 (call has no params)

### 44. wts: // comments dropped

- Maps: 170 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: trigger_strings.ixx:61 save
- `0001`: `// Units: nC08 (Amy Diane), Name (Name)` dropped

### 45. script: 3.0 camera fields missing (BlzCameraSetupSetCameraType etc.)

- Maps: 181 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Origin: new in 3.0. Known.
- Responsible: src/base/triggers/map_script.cpp generate_cameras
- `0239 (Lua)`: `BlzCameraSetupSetCameraType(gg_cam_Camera_001, 0)` + DEPTH_OF_FIELD fields lost
- Note: Ticket '[3.0] Emit 3.0 calls in the script generator'.

### 46. wct: entry count / trailing empty entries / header differ

- Maps: 114 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: triggers.ixx save_scripts
- `0239 (Lua)`: one empty text added
- `0054`: trailing empty entries 175 -> 174

### 47. script: placed item coordinates printed with more decimals

- Maps: 77 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_items
- `0004`: -4932, 2671.3 -> -4931.989, 2671.312

### 48. script: InitGlobals local i

- Maps: 76 (Lua 1: 0240). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_init_global_variables
- `0001`: loop bounds/`set i = 0` differ

### 49. script: item colour BlzSetItemColor / gg_item_ globals

- Maps: 29 (Lua 1: 0239). Severity: cosmetic-or-equivalent. Origin: new in 3.0. Known.
- Responsible: src/base/triggers/map_script.cpp generate_items
- `0239 (Lua)`: `BlzSetItemColor(gg_item_ergg_0000, ConvertPlayerColor(6))` lost

### 50. script: dialogue TRIGSTR keys differ (SetDialogueTextKey/SpeakerNameKey)

- Maps: 6 (Lua 1: 0240). Severity: cosmetic-or-equivalent. Origin: fork (2fe340c).
- Responsible: src/base/triggers/map_script.cpp generate_sounds
- `0240 (Lua)`: 23 extra SetDialogueTextKey/SpeakerNameKey
- `D13 map`: WE `SetDialogueTextKey(..., "TRIGSTR_-1")` dropped

### 51. compile: Skip Remaining Actions emitted as call ReturnAction()

- Maps: 101 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/gui.cpp convert_eca_to_script: no special case for ReturnAction (WE writes `return`)
- `0005`: `call ReturnAction()` (line 1724) -> `Undeclared function ReturnAction`
- `0009`: same, 12 more in one map
- Note: JASS compile error; in Lua it would be a call to an undefined global at runtime (no stock Lua map uses the action).

### 52. compile: nested helper function (unexpected takes)

- Maps: 25 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: gui.cpp helper generation (known ticket)
- `0001`: `function Trig_Recruit_Queue_HiveWE74 takes nothing returns boolean` inside another function
- Note: jasshelper stops at the first syntax error, so the count is a lower bound.

### 53. compile: rawcode-typed variable initial value unquoted

- Maps: 3 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_init_global_variables:115-125 (rawcode types fall through to the raw value)
- `0048`: `set udg_WaveUnitTypes[i]=hfoo` -> Undeclared variable hfoo
- `0096`: `set udg_Sammy=Npbm`
- Note: In Lua this becomes `= hfoo` (nil) silently.

### 54. compile: string initial value not escaped

- Maps: 3 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/triggers/map_script.cpp:119 generate_init_global_variables
- `0127`: `set udg_AttackMarkerModelPath="Abilities\Spells\..."` -> Invalid escape character sequence

### 55. resave: load failed / refused

- Maps: 3 (Lua 0). Severity: breaks-in-game. Origin: unknown.
- Responsible: Map::load between doodads and units (w3u/w3t mods or Units::load); undead02 refused at wtg preflight
- `0046 humanx03 (classic)`: `ankerl::unordered_dense::map::at(): key not found` after Doodad loading
- `0144 humanx03 (reforged)`: same
- Note: 0046/0144 are a throw after body validation (ticket 'Map load can still throw after body validation passes'); undead02 is known.

### 56. compile: duplicate region names declared twice

- Maps: 2 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_global_variables:59 (no de-duplication of gg_rct_ names)
- `0116`: `rect gg_rct_InitialHuntress03C = null` declared at lines 206 and 296 -> Symbol multiply defined
- `0147`: gg_rct_Intro_Kael_StartB
- Note: Lua would silently reuse the global.

### 57. compile: empty condition `not ()`

- Maps: 1 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst).
- Responsible: gui.cpp condition conversion with an empty sub-condition
- `0081`: `if (not ()) then`

### 58. compile: garbage variable name (human05 wtg misread)

- Maps: 1 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: triggers.ixx load_version_31 (fixed 3 ints after deleted-id lists)
- `0102`: `array udg_\x95` -> Invalid identifier name
- Note: Same root cause as the wtg-count known ticket.

### 59. wtg: written file does not parse

- Maps: 1 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: Triggers::save after the misread
- `0102`: new wtg: count 149 at offset 84 cannot fit
- Note: Known (human05).

### 60. wtg: variables misread (human05 unknown-list header)

- Maps: 1 (Lua 0). Severity: breaks-in-game. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: triggers.ixx load_version_31
- `0102`: udg_Arthas, udg_Caravan_Marker missing; garbage `udg_` = null added
- Note: Known: wtg count field (human05/undead02).

### 61. script: InitSounds setters missing (SetSoundDuration/Channel/Distances/ConeAngles/Pitch)

- Maps: 185 (Lua 0). Severity: changes-gameplay. Origin: fork (2fe340c generate_sounds). Known.
- Responsible: src/base/triggers/map_script.cpp generate_sounds:444-570
- `0001`: WE `SetSoundDuration(gg_snd_BanditYesAttack2, 1216)` + `SetSoundChannel(..., 0)`; HiveWE none, extra `SetSoundVolume(..., 127)`
- `0002`: `SetSoundDuration(gg_snd_BlacksmithWhat1, 3059)`
- Note: Labelled-sound SLK setters are known. Duration lost for .flac campaign audio (Needs verification: lookup handles Ogg only). 0 Lua maps in stock, but any Lua map with such sounds is affected.

### 62. script: region ambient sounds (SetSoundPosition/RegisterStackedSound) missing

- Maps: 98 (Lua 0). Severity: changes-gameplay. Origin: fork/upstream (generate_regions:372-404 mixed).
- Responsible: src/base/triggers/map_script.cpp generate_regions
- `0001`: `SetSoundPosition(gg_snd_WaterStreamLoop1, 1568, -1248, -195.7)` + `RegisterStackedSound(..., true, 704, 576)` lost
- Note: Region-attached ambient sounds stay silent.

### 63. script: gg_unit_ global missing

- Maps: 10 (Lua 0). Severity: unknown. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/triggers/map_script.cpp:938 fixed 4-char gg_unit scan
- `0029`: gg_unit_nrvd_0054 missing
- `0044`: gg_unit_nwiz_0133
- Note: Likely the known ≥10000/scan issue; Needs verification per map.

### 64. script: trigger names with leading spaces trimmed (gg_trg___X -> gg_trg_X)

- Maps: 6 (Lua 0: 0239, 0240). Severity: unknown. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/triggers.ixx:520 prefixed_script_identifier trims (upstream trimmed too)
- `0001`: WE `InitTrig___GruntSurprise_Attacked`, HiveWE `InitTrig_GruntSurprise_Attacked`
- `0002`: `InitTrig___Load_Arthas`
- Note: Exact pairs found in 6 maps. Breaks only custom script that names the WE identifier. The regex bucket (73 maps) also holds other InitTrig_ adds/drops.

### 65. wts: strings renumbered/added/removed

- Maps: 1 (Lua 0). Severity: unknown. Origin: upstream HiveWE (stijnherfst).
- Responsible: trigger_strings.ixx
- `0096`: string 6 with a newline in its comment re-keyed as 'Name (Name)'
- Note: Multi-line comment parse.

### 66. wtg: elements removed/added

- Maps: 1 (Lua 0). Severity: unknown. Origin: upstream HiveWE (stijnherfst).
- Responsible: Triggers::save
- `0128`: element classifier 64 id 100663415 (empty name) dropped

### 67. script: InitTrig_ order differs (reordered only)

- Maps: 173 (Lua 0). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: src/base/triggers/map_script.cpp generate_trigger_initialization
- `0001`: same InitTrig_ calls, other order

### 68. object data: original-object record with no mods dropped

- Maps: 34 (Lua 0). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: modification_tables.ixx save skips objects without changed fields
- `0004`: original[nenf] (1 set, no mods) dropped
- `0097`: w3d original[SLt0]
- Note: Needs verification that the record really had no modifications.

### 69. script: item SetItemColor missing (classic items)

- Maps: 3 (Lua 0). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst). Known.
- Responsible: src/base/triggers/map_script.cpp generate_items
- `0127`: `SetItemColor(i, ConvertPlayerColor(0))` lost
- Note: Same family as BlzSetItemColor.

### 70. object data: file version changed

- Maps: 1 (Lua 0). Severity: cosmetic-or-equivalent. Origin: upstream HiveWE (stijnherfst).
- Responsible: mod_table_write_version
- `0107`: w3b version 2 -> 3

### 71. lua/j bytes differ (expected; see script kinds)

- Maps: 49 (Lua 49: 0239, 0240, 0407, 0408, 0409, 0410, 0411, 0412). Severity: cosmetic-or-equivalent. Origin: -.
- Responsible: -
- `0239`: war3map.lua: 
- `0240`: war3map.lua: 

### 72. imp: written where WE had none

- Maps: 47 (Lua 47: 0407, 0408, 0409, 0410, 0411, 0412, 0413, 0414). Severity: cosmetic-or-equivalent. Origin: harness artifact.
- Responsible: imports.ixx always writes war3map.imp
- `0407 (Lua)`: war3map.imp added (only `.done`)

### 73. sim: call-count noise from moved code (Player/CreateTrigger)

- Maps: 45 (Lua 45: 0239, 0240, 0407, 0408, 0409, 0411, 0412, 0413). Severity: cosmetic-or-equivalent. Origin: tool artifact.
- Responsible: simdiff per-context alignment
- `0240 (Lua)`: Player() +124 net

### 74. imp: .done marker added (harness)

- Maps: 187 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Origin: harness artifact.
- Responsible: the dump's `.done` file is picked up as an import
- `0001`: entry `.done` flag 13 added

### 75. imp: entries for files absent from the dump dropped (harness)

- Maps: 187 (Lua 2: 0239, 0240). Severity: cosmetic-or-equivalent. Origin: harness artifact.
- Responsible: imports.ixx (rebuilds imp from files on disk; the stock dump has no imported files)
- `0001`: count 13 -> 1
- Note: Not a HiveWE bug as such: the dump lacks the imported files.

### 76. script: nested-helper split artifacts in sdiff

- Maps: 34 (Lua 0). Severity: cosmetic-or-equivalent. Origin: tool artifact.
- Responsible: sdiff line splitter on nested functions
- `0001`: stray `endfunction`

## What the tools could not handle

- 46 Lua maps have w3i v31 (1.31-1.35) and were upgraded to w3i 33 / w3e 12 on save; fdiff reports the version change and every w3e corner as changed bytes, so terrain equality was not checked field by field for them.
- sdiff splits JASS line by line; HiveWE's nested helper functions (the known compile error) produce stray `endfunction` toplevel entries in 34 maps.
- simdiff aligns calls per function context. Code that moved between functions (units inlined into CreateAllUnits) shows as missing + extra; kinds above use per-native net counts to separate real losses (SetResourceAmount, SetUnitColor) from moves.
- 4 Lua maps (0410, 0421, 0433, 0449) did not load in the simulator (the char-list order bug), so nothing else was compared for them at runtime.
- 0240 (Silverpine Sprint) shows large handle-order noise (ALICE library, UnitHasItemOfTypeBJ) after the lost BeginMap trigger; it was not analysed further.
- The pre-jasshelper capture reads the shared `%TEMP%/input.lua`; a HiveWE the user runs at the same time would write the same file. The mtime check only catches a missing write.
- The stock dump has no imported files, so import-list differences are mostly harness artifacts.
