# Seventh differential sweep, 2026-11-22

Check of Asana 1220330960297388 (map save can report success after a failed or truncated write): first round. Tier FULL, 237 maps, w4 one.py, compared with report 6.
- ctest 293/293. 49 ok (Lua), 185 script_compile_error (JASS), 3 refused (0046, 0133, 0144), identical per map to report 6; 0 differences (status, colour, amount, prio, sdiff, simdiff, fdiff incl. full JSON minus orig/new paths). 619 s, peak 467 MB/map.
- 49 Lua out folders (1524 files) byte-identical to report 6; no *.tmp.
- NCOW_Spell_Dev.w3x copy: only war3map.lua differs (41634 -> 41060 bytes, 20 SetStartLocPrioCount(n, 0) lines removed), same as report 5.
- GUI: Map Protector export (opened via post-click.ps1) OK, 18795 bytes; ribbon Save keeps the sandbox path.
