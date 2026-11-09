# script-check

Checks for a generated `war3map.lua` / `.w3x`. Copied from the 2026-11-09 script-generation work. Run from your scratchpad, not the repo.

Setup: `pip install --target <scratch>\lupa lupa` (no other dependency). Pass `<scratch>\lupa` as the lupa path below. `lupa`'s `load` returns multiple values, so the scripts wrap it in a Lua function.

- `lchk.py <new.lua> <orig.lua> <lupa_path>`: loads both scripts with Lua 5.3 (syntax), then diffs the `gg_trg_` ids, `InitTrig_` calls and definitions against a World Editor original. Also flags quoted operators, `null`, trailing commas and undeclared `gg_trg_` globals.
- `pack.py <StormLib.dll> <src_dir> <out.w3x>`: packs a folder into an MPQ the game accepts (flags 0x80000200 compress, replace) and prints the w3i, w3r, w3c, doo and units versions.
- `sim2.py <lupa_path> <script.lua> <tag> <this_dir> [data/tools dir]`: runs the script in a Lua 5.3 simulator. `pre.lua` and `impl.lua` are the runtime (handles, timers, coroutine scheduler, type-checked natives), `ev.lua` has events and destructable stubs. Natives and globals are stubbed from `data/tools/common.j` and `blizzard.j` (default: found relative to the script). It warns on bad argument types, undefined globals and auto-stubbed natives, and logs errors per coroutine.
- The tail of `sim2.py` is specific to Forgotten Hollow's Orb of Start (traced function names, firing item pickup `I004`). Edit those lines for another map.
- The simulator is not the game: a clean run proves no Lua error in the traced path, nothing more.
