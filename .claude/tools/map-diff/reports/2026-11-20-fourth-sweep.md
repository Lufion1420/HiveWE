# Fourth differential sweep, 2026-11-20

Check of the SetUnitColor / SetResourceAmount fix (efc379b) on HEAD ee787ee, Agent exe. One map per process.

## Setup differences from reports 1-3

- The stock root was gone. QA rebuilt it from CASC: 466 maps, same ids. Subset: 237 maps (49 Lua, 191 v39).
- The old kind classifier and `drive.py` were gone. Substitute (now in `sweep/`): resave, fdiff and sdiff on every map, simdiff on the Lua maps, plus direct counts of `SetUnitColor` and `SetResourceAmount`. **The 78 kinds were NOT rebuilt**, so "no new kinds" is weaker than in reports 2 and 3.

## Run

- 49 ok (all Lua), 185 script_compile_error, 3 refused (0046, 0133, 0144): same as report 3. No timeout. Peak memory 463 MB, 640 s.
- Kinds 6 and 7 in Lua: 0 remaining. PASS. SetUnitColor 244 = 244, SetResourceAmount 577 = 577.
- JASS: PASS. The only loss is map 0070's 16 `ConvertPlayerColor(0)` lines (accepted stale WE script). Gold 392 = 392. Map 0102 shows 26 vs 0: its known misread wtg.
- No new kinds (classifier limit above). `RandomDist*` now shows in two Lua maps (0240, 0430), one before; QA reads it as the same kind.
- simdiff, 49 Lua maps: 0 new warnings or errors.
- Artifact, map 0049: `%TEMP%/input.lua` is shared by every HiveWE process. A user save of NCOW at 17:11 contaminated one run; it was clean rerun alone.

## NCOW

- The user saved the folder map at 17:11:13 with the new build (expected). In `.build\release.w3x`, `config` differs only in `SetTeams(4)` -> `SetTeams(24)` and `USE_MAP_SETTINGS` -> `TEAMS_TOGETHER`. The `.build` files are unchanged.

## Add-on: w3i flags 0x20/0x40 and SetTeams/placement (466 stock WE scripts)

- Placement is `TEAMS_TOGETHER` with more than one human slot, else `USE_MAP_SETTINGS`; `SetTeams` equals the player count. 0 counterexamples, and flags 0x20/0x40 make no difference.
- 0x20 set with 2+ humans: 10 maps (0454-0465), all `TEAMS_TOGETHER`, `SetTeams` = player count, not force count (0465 writes 5 with 2 forces).
- 0x20 set with 1 human: 195 maps, all `USE_MAP_SETTINGS`.
- NCOW's old `SetTeams(4)` / `USE_MAP_SETTINGS` were HiveWE's own pre-abad589 output, not WE output.
- No stock map has 24 slots with 4 humans, so NCOW's new `SetTeams(24)` rests on the rule, not on a matching map.
