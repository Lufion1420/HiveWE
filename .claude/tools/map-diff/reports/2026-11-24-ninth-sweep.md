# Ninth differential sweep, 2026-11-24

Trigger load/save fix (commit f176390). All 466 stock maps, 1101 s.

## Run
- The 237-map subset of report 6: 49 ok, 188 script_compile_error.
- The other 229 maps: 212 ok, 16 script_compile_error, 1 refused save (0456: w3a A029 field datab1 has no meta id; object data, not triggers).
- Against the sixth sweep: 0046, 0133 and 0144 moved from exit 1 to script_compile_error; 0380 left the subset (ok).

## Triggers
- Trigger text and count differences: 0 on the 465 saved maps.
- war3map.wct byte-equal on 463; 0239 and 0458 differ by a lone NUL.
