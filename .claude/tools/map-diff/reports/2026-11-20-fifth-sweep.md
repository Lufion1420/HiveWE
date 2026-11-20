# Fifth differential sweep, 2026-11-20

Check of the water colour and ally/start-location priority batch (Asana 1220369557367551) on HEAD 2727caa plus the uncommitted batch. Same 237 maps and tooling as report 4 (`2026-11-20-fourth-sweep.md`), with an exact-argument call comparison added to `sweep/one.py`. The 78 kinds were still not rebuilt.

## Run

- `ctest --preset Agent`: 265 of 265 passed.
- 49 ok (all Lua), 185 script_compile_error, 3 refused (0046, 0133, 0144): same as report 4. Peak memory 466 MB, 614 s.
- Lua, kinds 8 and 9: 0 remaining. PASS. All 49 maps match by name and arguments: SetWaterBaseColor 16 = 16, SetStartLocPrioCount 266 = 266, SetStartLocPrio 555 = 555, SetEnemyStartLocPrioCount 93 = 93, SetEnemyStartLocPrio 332 = 332, InitAllyPriorities present in 49 of 49 on both sides. These no longer appear in simdiff net differences (report 4 had them in 16, 15, 15 and 2 maps).
- JASS, 185 generated scripts: PASS. Water 12 = 12, count 742 = 742, prio 2974 = 2974, enemy count 31 = 31, enemy prio 57 = 57, InitAllyPriorities 154 of 154.
- Unchanged: SetUnitColor 244 = 244 (Lua), SetResourceAmount 577 = 577 (Lua) and 392 = 392 (JASS). JASS colour: only map 0070's 16 lines missing, as before.
- simdiff: 0 new warnings or errors. fdiff counts equal report 4.
- `input.lua` clash: none. All 185 JASS scripts are distinct, and none is NCOW's.

## NCOW (scratchpad copy)

- `war3map.lua` went from 41634 to 41060 bytes: the 20 `SetStartLocPrioCount(n, 0)` lines are removed. Left: counts for players 0, 3 and 6 and 7 `SetStartLocPrio` lines (for example `SetStartLocPrio(0, 0, 6, MAP_LOC_PRIO_HIGH)`). `config` still calls `InitAllyPriorities`. No water tinting.
- Originals untouched.

## Reviewer follow-ups (open)

- A range guard for `1u << internal_number`.
- A test of the `main()` order and of `config` omitting `InitAllyPriorities`.
