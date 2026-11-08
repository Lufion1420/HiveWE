# Session handoff

Written 2026-11-08. Overwritten at each handoff.

## State
- Branch `main`, pushed at 3a5c6ed plus the docs commit that carries this file. CI on 3a5c6ed: the orchestrator is watching it. fbf09a6 passed.
- Closed: `[3.0] Refuse unknown map format versions`, `[3.0] Read and write w3i v39`.
- New tickets: parent "Map load robustness: failures that crash or leave bad state" with subtasks "Map Protector: a war3map.w3e under 4 bytes leaves map_directory on the temp folder", "Truncated map file of an accepted version still crashes the editor", "Open Map (MPQ) ignores failed extraction". Also "Forces: shared vision uses the wrong flag bit" and "Map Description drops edits when the name, author or description is a literal".
- World Editor: the user closed it themselves on 2026-11-08. If it is open, do not touch it without new permission.

## Next step
1. Check CI on 3a5c6ed.
2. The user chooses:
   - `Truncated map file of an accepted version still crashes the editor`, then the 3.0 parsers for doodads, units, regions and cameras. The ticket must land first, because each accepted version turns damaged bodies into a `Map::load` crash. Asana's free plan has no dependencies, so this ordering is only in the ticket comment.
   - Or a top `[Wish]` bug. Ranks 2-5 are small data-safety fixes.

## Decisions the user made
- w3i keeps the source version on save: v39 stays v39, older maps save as v33. Revisit once the other 3.0 writers land.
- Shared vision has its own ticket.
- Obfuscation is refused on v39.
- QA tries manual checks itself before asking the user (see `.claude/HARNESS.md` Gates).
- 3.0 parsers: write our own against the verified layouts in `.claude/plans/wc3-3.0-compatibility.md`. Upstream is a reference only, because it has defects.
- MPQ refusal: check before unpacking, so a refused map writes nothing.
- Preflight also covers w3e, w3s and wpm.
- Agents may use Playwright for hiveworkshop.com: plain browser, no stealth, no login, follow robots.txt.
- NCOW uses no tech tree, upgrades, random groups, custom colours or waygates, so `[Parity 1]` is not urgent for it.
- Related tickets are grouped under parent tickets.
- Standing rules: see "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Open questions
- Waiting on the user: "User checks in the real 3.0.1 World Editor" and "User hand-checks of the UI baselines".
- A v39 w3i beside doo 8 and w3r 5 is untested in the game.
- v15/v18 fields `custom_light_tileset`, `game_data_set`, and v15 `map_version`, `loading_screen_number` are unset on stack `MapInfo`. Not ticketed.
