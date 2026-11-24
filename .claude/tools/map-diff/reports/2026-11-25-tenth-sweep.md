# Tenth differential sweep, 2026-11-25
Unsaved-changes prompt (1220329518098180), tree on a6bba0d plus this change. Tier FULL, all 466 stock maps, 1126 s (ninth: 1101 s).
- 237-map subset: 49 ok, 188 script_compile_error. Other 229: 212 ok, 16 script_compile_error, 1 refused (0456, w3a A029 datab1 no meta id). Same as ninth, no status changes.
- All 237 full fdiff reports identical to ninth (folder paths ignored).
- Colour 341->325 (lost 16), amount 970=970, prio 5186=5186, same as ninth. Lua 49: sim_rc 1 on all in both, 0 new sim errors. No tool errors.
- Triggers: 465 compared, 0 normalised diffs, 0 moved text; wct equal 463, wtg byte-equal 259, same as ninth.
- Error text differs on some maps only in pjass "Maybe you meant" suggestions (0 unexplained).
- Resave logs (466 stderr, 466 stdout): "cannot be written" 0, "Unsaved changes check:" 0.
- Peak per map 482 MB (ninth 457); per-map delta median 0, mean -1 MB (sampling noise). Subset wall delta median 0 s.
- Monitor: HiveWE 487 MB, python 233 MB, never foreground (1937 samples), no kills or timeouts.
- Method: ninth's one.py, drive_all.py and trigcmp.py with the exe pointed at a sandbox copy; stock data via junctions (removed after).
