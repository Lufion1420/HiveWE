# Sixth differential sweep, 2026-11-22

Check of the doodad skin id fix (Asana 1220369827454184) on HEAD b25f32d plus uncommitted doodad.ixx, doodad_brush.cpp, tests. Same 237 maps and tooling as report 5 (w4 one.py with the prio check), plus a copy of each fdiff JSON.

## Run
- `ctest --preset Agent`: 276 of 276 passed; resave_smoke Passed (not skipped), with the SKINNED_DOO fixture argument.
- 49 ok (all Lua), 185 script_compile_error, 3 refused (0046, 0133, 0144): identical per map to report 5. Peak memory 468 MB (report 5: 466), 607 s.
## Doodad skin kind (war3map.doo)
- fdiff doo diffs, all maps: 119375 (report 5) -> 116706 = 2669 fewer, in 19 maps (0097, 0129, 0140, 0149, 0408, 0409, 0411, 0416, 0417, 0418, 0419, 0423, 0431, 0436, 0441, 0443, 0446, 0452, 0453). 2669 is exactly the number of doodads in the stock war3map.doo of these maps whose skin differs from the id.
- Remaining `skin` diffs in war3map.doo: 0 (was 2669 in this subset; the task's 24 maps / 11,991 includes maps outside the 237 subset, not re-run). No other fdiff file changed status or count in any map.
- Remaining doo diffs are only doodads[].x / .y float noise (58832 / 57870), as before.
## Scripts (Lua first)
- Lua 49 maps: colour 244 = 244, amount 577 = 577, prio counts equal; sd lost/gain per function identical to report 5 in all 237 maps; simdiff net natives identical (0 changed names), 0 new warnings or errors.
- JASS 185: sd identical to report 5; colour lost 16 (map 0070) as before.
- No new kinds, no regressions.
## BlzCreateDestructableZWithSkin skin argument (open question)
- HiveWE emits `script.four_cc(i.skin_id)` (map_script.cpp:373), i.e. the doo skin, now preserved on load.
- Not answerable from the data: in all 2754 `Create*Destructable*Skin*` calls of the 468 stock scripts the WE skin argument equals the base id (0 with skin != id), and the 2669 skinned doodads in the subset have no destructable call in either the WE script or HiveWE's. Extra run of map 0380 (FTtw as LTlt): ok, no destructable calls in either script.
- Needs a map with a skinned destructable made in the real World Editor to compare.