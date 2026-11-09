# map-diff/sim — Lua differential sweep

Runs the World Editor's `war3map.lua` and HiveWE's saved `war3map.lua` for the same map in a Lua 5.3 simulator, logs every native call, and diffs the two logs. Lua maps only (no JASS). Derived from `../../script-check/` (pre/impl/ev.lua, sim2.py), with the Forgotten Hollow parts removed.

Setup: `pip install --target <scratch>\lupa lupa`, then pass `--lupa <scratch>\lupa` (or set `LUPA_PATH`). Natives and globals are stubbed from `data/tools/common.j` + `blizzard.j` (override with `sim.py --jdir`). Run from your scratchpad, on copies of maps.

```
python simdiff.py <orig/war3map.lua> <new/war3map.lua> --w3i orig/war3map.w3i [--w3i-new new/war3map.w3i]
       --lupa <dir> --out <dir> [--time 60] [--wall 90] [--mem 1024] [--tol 0.1] [--top 25]
```
Exit code 0 = identical, 1 = different, 2 = a simulation failed. Writes `orig.log`, `new.log`, `diff.json` to `--out`. Simulations run one at a time.

## Files
- `sim.py <script> <out.log>` — one simulation. Loads the chunk, wraps every global function defined in `war3map.lua`, runs `config`, `main`, then drains timers/trigger sleeps/coroutines up to `--time` simulated seconds.
- `rt.lua` — scheduler, handles, type checks, caps, deterministic `pairs`/`next`, undefined-global detection. `impl.lua` — stateful natives (triggers, timers, rects, camera bounds, units/items/destructables, groups). `log.lua` — call logging and map-function wrapping.
- `tdiff.py <orig.log> <new.log> [--json] [--tol]` — the diff.

## Log format
`KIND<TAB>ctx<TAB>t<TAB>text`. KIND: `C` native call, `W` warning, `E` coroutine error, `I` info, `A` alias.
- **ctx** is the innermost global function from `war3map.lua` on the call stack (`InitSounds`, `CreateAllItems`, `Trig_X_Actions`, ...). Table methods are not wrapped, so their calls go to the nearest global caller. Anonymous callbacks (timers, `TriggerExecute` closures) get `<spawner>~async`. Natives redefined by the map (hook libraries) are not used as contexts.
- **Arguments**: reals to 3 decimals, FourCC-looking integers as `'hfoo'`, handles as `<type@creatingctx#n>`. At the end of `main` and of the run, handles stored in `gg_` globals are renamed to the global (`<gg_snd_X>`). Pure math/string natives are not logged (`QUIET` in `sim.py`).
- **Warnings**: `NILH` nil passed for a handle parameter (boolexpr/code/string params excluded), `TYPE` wrong argument type, `UNDEF` read of an undefined global (including unassigned `gg_`/`udg_`), `AUTOSTUB` native or constant missing from `common.j`. Each warning is deduplicated per message, context and line.
- **Errors** carry the `war3map.lua` line chain (`[stack 11997<11395<33524]`).

## Diff
Calls are aligned per context with `difflib`. In a replaced block, calls to the same native are paired as **changed**, the rest are **missing** (only in orig) or **extra** (only in new). Changed calls whose float arguments differ by no more than `--tol` (or 1e-5 relative) are reported only as **float noise**: HiveWE writes full float32 values where the World Editor writes 1 decimal, and that difference spreads to every derived position. Warnings and errors are compared with line numbers removed. Only **new** warnings and errors are listed, plus errors that went away. The summary prints the top counts by (kind, ctx, native); `diff.json` has every call.

## Caps (resource rule: never let a job run away)
Lua heap is capped by `--mem` (MB, lupa `max_memory`). A per-coroutine debug hook enforces the CPU-time cap (`--wall`) and an instruction cap (`--imax`, millions). The scheduler stops at 300k ticks. The log keeps at most 6000 calls per context and 600k lines in total; the overflow is counted in `I` lines (`CTXCAP dropped N`). `simdiff.py` kills a simulation that runs longer than `1.5*wall+60` s. A capped run says `status=capped-*`.

## Determinism
Lua 5.3 seeds its string hash per process, so raw `next` order changes between runs. `pairs` and `next` are therefore replaced with versions that visit keys in sorted order (numbers, strings, booleans, then handles by creation order; table/function keys last, in first-seen order, which is not guaranteed to be stable). `math.random` is seeded and `os.clock`/`os.time`/`os.date` return simulated time. Verified 2026-11-09: all 49 stock Lua maps (stock_scripts 0239, 0240, 0407-0453) gave `IDENTICAL` when run against themselves.

## Limits
- This is not the game. Events never fire (no player input, unit deaths or region entry). Only `config`, `main`, `RunInitializationTriggers`, timers and `TriggerRegisterTimerEvent` run. Code behind events is not covered.
- Most natives are stubs: getters return 0, false, "" or a fresh handle. `Is*` auto-stubs return false. Group enumeration only sees units created by `CreateUnit*`. `Issue*Order` returns true. Constants missing from `common.j` become name-derived integers. Control flow that depends on real game state can differ from the game, but it differs the same way in both runs.
- `data/tools/common.j` is older than 2.0, so newer natives appear as `AUTOSTUB`.
- A clean diff means the two scripts make the same native calls in the simulated paths, nothing more.
