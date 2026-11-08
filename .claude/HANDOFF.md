# Session handoff

Written 2026-11-09. Overwritten at each handoff.

## State
- Branch `main` holds finished work only. CI on 50f3ce0 passed (run 42521966521).
- **Unfinished work is on `wip/units-v12-v13`** (7bab9a8, pushed to `origin`, one commit on top of 50f3ce0). It has no review or QA pass yet and must not reach `main` as it is. Its message starts with "WIP:", so reword it when it lands.
- Nothing is running. `cpp-dev` was stopped on purpose seconds after it started the last fix; the tree showed no edits from that run.
- Closed: `[3.0] Refuse unknown map format versions`, `[3.0] Read and write w3i v39`, `Truncated map file of an accepted version still crashes the editor`, `[3.0] Read and write doodads v12/v13`. Bodies are validated before teardown by `src/base/map/map_body_check.ixx`, through the real parsers.
- Tickets: parent "Map load robustness: failures that crash or leave bad state" with subtasks "Map Protector: a war3map.w3e under 4 bytes leaves map_directory on the temp folder", "Open Map (MPQ) ignores failed extraction", "Tileset change leaves undo and clipboard with stale ground indices" (includes ground above the count on blight and cliff corners), "Map load can still throw after body validation passes" (now also humanx03 w3u), "Map with an unparsed wtg opens with no triggers, and a save would lose them (human05)", "Doodads::save writes short or empty ids with fewer than 4 bytes" (new). Under "World Editor parity: map output on save": "Subversion 9 units: hero stats and item table link live only in war3map.j and a HiveWE save may drop them" (new). Also "Forces: shared vision uses the wrong flag bit", "Map Description drops edits when the name, author or description is a literal", "Map save can report success after a failed or truncated file write".
- World Editor: the user closed it themselves on 2026-11-08. If it is open, do not touch it without new permission.

## In progress: `[3.0] Read and write units v12/v13` (Asana GID 1220328837271069)
Track risky, stage 2 of 5 (`cpp-dev`; then `reviewer`, `qa-verifier`, `release-manager`, commit). Reviewer rejection count: 1 of 3. Read the ticket's comments first: they hold the format note and the evidence.

Last seen on the branch, by `cpp-dev`, not rerun by QA: `ctest --preset Agent` 200 of 200 with `HIVEWE_STOCK_BODY_ROOT`, `HIVEWE_STOCK_W3I_ROOT` and `HIVEWE_BODY_CHECK_CASC` set. Stock writer round trip 0 differences on 191 files and 33813 records. Real load and save: every non-position byte, version, subversion and count unchanged; x and y change by at most 0.001953125; z by up to 1558.75; 76 files change order. Sweep: 466 checked, 275 accepted, 191 refused, 190 readable, 190 real doo, 190 real units, undead02 on wtg. 0 new maps open: all 191 also need w3r 7 and w3c 3.

Stock root: `C:/Users/mauri/AppData/Local/Temp/claude/11cf287c6d13d22a/624d2e1b-c296-4804-9b85-76d3a36906f9/scratchpad/stock` (session scratch, check it still exists). Stock tests pass silently with 0 assertions when the variable is unset, so QA must confirm the printed counts.

## Next step
1. `git switch wip/units-v12-v13`.
2. New `cpp-dev` (model opus), which has none of this context. Brief it to:
   - In `Map::save`, check all unit and item ids before anything is written. An id over 4 bytes shows a `QMessageBox` naming the id and returns false, in the style of `ensure_player_start_locations`. Nothing on disk changes. Confirm callers handle false (Save As, Test Map, Calculate Shadows and Save, Map Protector or export paths).
   - In `src/object_editor/object_editor.cpp`, count bytes, not QString characters, in `select` (it checks `id->text().size()`). Check the separate `copyRow(source_id, new_id)` path too, and other object types sharing the dialogs.
   - Keep the `write_id` throw as a backstop.
   - Test that the check fails on an over-long id with no files changed, a valid map passes, and multi-byte length counts bytes.
   - Read only, no fix: why z changes by up to 1558.75 in `(1)silverpinesprint.w3x` through real load. Is it stored height we re-derive, or a sweep terrain bug? Would v8 do the same?
3. `reviewer` (second pass on the whole diff against 50f3ce0), `qa-verifier`, `release-manager` for the changelog, `docs-keeper` for `hivewe-file-formats.md` lines saying units 12/13 are unsupported.
4. Commit to `main` with a real message, push, close the ticket with a comment, watch CI in the background. Delete the WIP branch on `origin` once it has landed.
5. Then w3r v7 and w3c v3. The human05 ticket (doo 12, units 12, w3r 7, w3c 3) and the humanx03 w3u throw become reachable only when the last of these lands.

## Decisions the user made
- Units: random type 2 count and empty ids are fixed for all versions, in the units ticket.
- Units: a sub 9 v12/v13 file saves as sub 11 only when a unit holds a non-default item table or str, agi or int. w3i flag 0x4000 is left alone. Whether the World Editor reads sub 11 with the flag unset is untested.
- Units: an id over 4 bytes is checked before saving, with a dialog; the Object Editor counts bytes; the throw stays.
- World Editor file: not waited for separately; the user supplied `Desktop/testAIHere.w3m` (read only). Its 361-byte units file is embedded as hex in the tests. User check: reopening it in the 3.0.1 World Editor resets the paladin's attributes and item table.
- w3i keeps the source version on save: v39 stays v39, older maps save as v33. Revisit once the other 3.0 writers land.
- Shared vision has its own ticket.
- Obfuscation is refused on v39.
- QA tries manual checks itself before asking the user (see `.claude/HARNESS.md` Gates).
- 3.0 parsers: write our own against the verified layouts in `.claude/plans/wc3-3.0-compatibility.md`. Upstream is a reference only, because it has defects.
- MPQ refusal: check before unpacking, so a refused map writes nothing.
- Preflight also covers w3e, w3s and wpm.
- A map whose wtg or wct fails to parse is refused.
- The w3i `remaining() < 4` leniency is kept.
- Out-of-range w3e cliff and ground texture indices are refused before teardown. A ground index equal to the count stays accepted, because HiveWE can save it through stale undo, and `get_terrain_pathing` bounds the read.
- A wpm smaller than `(terrain-1)*4` in either dimension is refused; zero-sized and larger stay accepted.
- Agents may use Playwright for hiveworkshop.com: plain browser, no stealth, no login, follow robots.txt.
- NCOW uses no tech tree, upgrades, random groups, custom colours or waygates, so `[Parity 1]` is not urgent for it.
- Related tickets are grouped under parent tickets.
- Doodad saves keep the file's flag byte, including state 3 and bit 0x08. v12/v13 doo files with leftover bytes are refused; v7 and v8 are not affected.
- Standing rules: see "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Orchestrator calls the user can overrule
- Unit x, y, z and record order are not kept byte-exact on save. This matches doodads and older versions. The user was told on 2026-11-08 and has not objected.
- Clearing a hero's stats in the same session can drop a promoted file back to sub 9; no data is lost.
- An id with bytes after its first NUL is refused on v12+, accepted on v8.

## Open questions
- Waiting on the user: three checks for the doodads v12/v13 ticket, listed on the ticket itself: copy/paste and undo on a crafted v13 map, placing a doodad with the brush and reopening, and loading HiveWE's v13 output in the real game and World Editor.
- Waiting on the user: "User checks in the real 3.0.1 World Editor" and "User hand-checks of the UI baselines".
- Waiting on the user: optional check that undo survives a refusal. In `build/Agent/Release/HiveWE.exe`, open a map with a placed unit, move it with the keypad, refuse a damaged map, then press Ctrl+Z; the move should undo. QA could not make a tracked edit without real mouse input. The fix is also not game-tested.
- Later, once a writer exists: open a crafted sub 11 copy with the flag unset in the World Editor, to settle promotion.
- A v39 w3i beside doo 8 and w3r 5 is untested in the game.
- v15/v18 fields `custom_light_tileset`, `game_data_set`, and v15 `map_version`, `loading_screen_number` are unset on stack `MapInfo`. Not ticketed.
