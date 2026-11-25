# Eleventh differential sweep, 2026-11-25
Kept script for maps with missing trigger data (Asana 1220882874361280), uncommitted tree on 9c1c141. Tier FULL, all 466 stock maps, 2179 s (tenth: 1126 s).
- 237-map subset: 49 ok, 188 script_compile_error. Other 229: 212 ok, 16 script_compile_error, 1 refused (0456, w3a A029 datab1 no meta id). No status changes.
- Maps counted as "script kept": 0 of 466 (all have wtg and wct). Kept-script save warnings: 0.
- All 237 full fdiff reports identical to tenth. Colour 341->325 (lost 16), amount 970=970, prio 5186=5186. Lua 49: sim_rc 1 on all in both, 0 new sim errors. No tool errors.
- Triggers: 465 compared, 0 normalised diffs, 0 moved text; wct equal 463, wtg byte-equal 259, same as tenth.
- Error text differs on 14 maps with error differences outside paths, all explained by pjass "Maybe you meant" suggestions (0 unexplained). "cannot be written" 0, "Unsaved changes check:" 0.
- Peak per map 470 MB (tenth 482); per-map delta median +2, mean +7 MB.
- Wall: subset per-map delta median +0.8 s, mean +1.4 s. CPU load read 99% after the sweep with none of QA's processes running. A/B on 7 maps, 2 runs each, previous build 46.0 s vs this build 43.3 s: slowdown is the machine, not the change.
- Monitor: HiveWE 467 MB, python 314 MB, 3931 samples, 0 foreground, 0 on-screen windows, no kills or timeouts.
- Baseline for next time: C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\c85be5c8-e658-4d4f-963a-a25e71743bd5\scratchpad\qav\sweep\w4\work (scripts in ...\qav\sweep\code, runner ...\qav\sweeprun.ps1; recreate the stock and stock_scripts junctions and box_sweep first). May vanish.
