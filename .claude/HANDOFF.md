# Session handoff

Written 2026-11-08. Overwritten at each handoff.

## State
- Branch `main`. The orchestrator is committing "Truncated map file of an accepted version still crashes the editor" right after this and is watching CI on that commit. Run 42505028807 on 7d3fdf6 passed, so `[3.0] Read and write w3i v39` stays closed.
- Closed: `[3.0] Refuse unknown map format versions`, `[3.0] Read and write w3i v39`, `Truncated map file of an accepted version still crashes the editor`. Bodies are validated before teardown by `src/base/map/map_body_check.ixx`, through the real parsers.
- Tickets: parent "Map load robustness: failures that crash or leave bad state" with subtasks "Map Protector: a war3map.w3e under 4 bytes leaves map_directory on the temp folder", "Open Map (MPQ) ignores failed extraction", "Tileset change leaves undo and clipboard with stale ground indices" (includes ground above the count on blight and cliff corners), "Map load can still throw after body validation passes", "Map with an unparsed wtg opens with no triggers, and a save would lose them (human05)". Also "Forces: shared vision uses the wrong flag bit" and "Map Description drops edits when the name, author or description is a literal".
- World Editor: the user closed it themselves on 2026-11-08. If it is open, do not touch it without new permission.

## Next step
The user chooses:
- The 3.0 parsers for doodads, units, regions and cameras, now unblocked. Each must add its version to `map_body_check` and extend the readable-parts sweep. Check the human05 ticket when doo 12 lands.
- Another subtask under Map load robustness: "Map Protector: a war3map.w3e under 4 bytes leaves map_directory on the temp folder" or "Open Map (MPQ) ignores failed extraction".
- Or a top `[Wish]` bug. Ranks 2-5 are small data-safety fixes.

## Decisions the user made
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
- Standing rules: see "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Open questions
- Waiting on the user: "User checks in the real 3.0.1 World Editor" and "User hand-checks of the UI baselines".
- Waiting on the user: optional check that undo survives a refusal. In `build/Agent/Release/HiveWE.exe`, open a map with a placed unit, move it with the keypad, refuse a damaged map, then press Ctrl+Z; the move should undo. QA could not make a tracked edit without real mouse input. The fix is also not game-tested.
- A v39 w3i beside doo 8 and w3r 5 is untested in the game.
- v15/v18 fields `custom_light_tileset`, `game_data_set`, and v15 `map_version`, `loading_screen_number` are unset on stack `MapInfo`. Not ticketed.
