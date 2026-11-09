# Session handoff

Written 2026-11-09 (afternoon). Overwritten at each handoff.

## State
- `main` holds all finished work. `[3.0] Read and write units v12/v13` landed in a14cdea, and the session closed its ticket. CI on a14cdea was still running at the time of writing (run 42658845053); check it first. The previous run, on 17a7231, passed.
- `wip/units-v12-v13` is deleted locally and on `origin`. No other WIP branches.
- Nothing is running.
- Closed tickets: `[3.0] Refuse unknown map format versions`, `[3.0] Read and write w3i v39`, `Truncated map file of an accepted version still crashes the editor`, `[3.0] Read and write doodads v12/v13`, `[3.0] Read and write units v12/v13`.
- New tickets, 2026-11-09, both under "World Editor parity: map output on save":
  - "Object tables write custom ids over 4 bytes with no length check". Covers `save_modification_table`, and the new-object tooltip that still says "4 characters".
  - "Save As leaves the map pointed at the new folder when the start-location dialog is refused". Also covers `fs::equivalent` throwing in `Map::save`.
- Other open tickets:
  - Under "Map load robustness: failures that crash or leave bad state":
    - Map Protector w3e under 4 bytes
    - Open Map (MPQ) ignores failed extraction
    - Tileset change leaves undo and clipboard stale
    - Map load can still throw after body validation passes (includes the humanx03 w3u)
    - human05 unparsed wtg
    - `Doodads::save` short ids
  - Under "World Editor parity: map output on save": "Subversion 9 units: hero stats and item table link live only in war3map.j".
  - Standalone: Forces shared vision bit, Map Description literal edits, Map save reports success after a failed write.
- World Editor: if it is open, do not touch it without new permission.

## Next step
1. Check CI run 42658845053 passed.
2. Start `[3.0] w3r v7` and `[3.0] w3c v3` (find them with `get_tasks` on project 1220328345553589, under parent "Support Warcraft III 3.0 maps, models and data"). Risky track: `wc3-format-specialist` first, then `cpp-dev` on Opus. Layouts are verified in `.claude/plans/wc3-3.0-compatibility.md` section 3. When the last of these lands, 1.32+ maps become openable. The human05 ticket and the humanx03 w3u throw become reachable then.
3. Stock root for the stock tests (session scratch, check it still exists): `C:/Users/mauri/AppData/Local/Temp/claude/11cf287c6d13d22a/624d2e1b-c296-4804-9b85-76d3a36906f9/scratchpad/stock`. Set `HIVEWE_STOCK_BODY_ROOT` and `HIVEWE_STOCK_W3I_ROOT` to it. Set `HIVEWE_BODY_CHECK_CASC` to `C:/Program Files (x86)/Warcraft III`. Stock tests pass silently with 0 assertions when these are unset.

## Decisions the user made
- Units: random type 2 count and empty ids are fixed for all versions.
- Units: a sub 9 v12/v13 file saves as sub 11 only when a unit holds a non-default item table or str, agi or int. w3i flag 0x4000 is left alone. Whether the World Editor reads sub 11 with the flag unset is untested.
- Units: an id over 4 bytes is checked before saving, with a dialog. The Object Editor counts bytes. The `write_id` throw stays.
- w3i keeps the source version on save: v39 stays v39, older maps save as v33.
- Shared vision has its own ticket. Obfuscation is refused on v39.
- QA tries manual checks itself before asking the user.
- 3.0 parsers: write our own against the verified layouts. Upstream is a reference only.
- MPQ refusal: check before unpacking. Preflight also covers w3e, w3s and wpm. A map whose wtg or wct fails to parse is refused. The w3i `remaining() < 4` leniency is kept.
- Out-of-range w3e cliff and ground indices are refused before teardown. A ground index equal to the count stays accepted. A wpm smaller than `(terrain-1)*4` is refused.
- Agents may use Playwright for hiveworkshop.com: plain browser, no stealth, no login, follow robots.txt.
- NCOW uses no tech tree, upgrades, random groups, custom colours or waygates.
- Related tickets are grouped under parent tickets.
- Doodad saves keep the file's flag byte. v12/v13 doo files with leftover bytes are refused.
- Standing rules: "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Orchestrator calls the user can overrule
- Unit x, y, z and record order are not kept byte-exact on save. `Units::load` re-derives z from terrain on all versions.
- Clearing a hero's stats in the same session can drop a promoted file back to sub 9. No data is lost.
- An id with bytes after its first NUL is refused on v12+ and accepted on v8.
- Pasted units copy `group_id` and the trailer. What `group_id` means is unknown.

## Open questions (waiting on the user)
- Units v12/v13, in `build/Agent/Release/HiveWE.exe` on a copy of the test map:
  1. Object Editor > Units > New: type `hfoö`. OK should stay disabled.
  2. With `hfoö` typed, double-click a base unit. No object should be created.
  3. A placed unit with a 5-byte id should get the "Id Too Long" dialog on Save, Save As (no folder picker), Test Map and Export MPQ, with no files changed.
- Doodads v12/v13: three checks listed on that ticket.
- "User checks in the real 3.0.1 World Editor" and "User hand-checks of the UI baselines".
- Optional: undo survives a refused map load (steps in the earlier ticket).
- Later: open a crafted sub 11 copy with the flag unset in the World Editor.
- Does the game read the stored unit z? Untested.
- A v39 w3i beside doo 8 and w3r 5 is untested in the game.
