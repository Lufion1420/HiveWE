# Session handoff

Written 2026-11-08. Overwritten at each handoff.

## State
- Branch `main`, pushed to `origin`. No HiveWE process left running by the session. `HKCU\Software\HiveWE` matched its original state at QA's last check.
- `[3.0] Refuse unknown map format versions` is committed. QA passed the automated tests (104/104) and the startup case on the real 3.0 map. The ticket stays open until the user does its manual checks, listed in the ticket: the file-dialog paths could not be driven.
- World Editor inspection is finished; the audit is corrected (`.claude/plans/world-editor-parity.md`): 143 rows, 26 complete, 49 partial, 61 missing, 6 NO MENU, 1 unknown.
- New Asana parent "Feature wishlist: what map makers want and what works badly" has 72 `[Wish]` subtasks. Research record: `.claude/plans/feature-wishlist.md`.
- New ticket "Map Protector: Remove listfile option probably keeps the listfile": not run yet.
- The user may have World Editor open with unsaved work. Do not touch it without new permission.

## Next step
Ask the user for the result of the manual checks on the refusal ticket. Then either close it, or send failures to `cpp-dev`.
Then `[3.0] Read and write w3i v39`, the next 3.0 parser, or a top `[Wish]` bug if the user prefers. Ranks 2-5 are small data-safety fixes.

## Decisions the user made
- 3.0 parsers: write our own against the verified layouts in `.claude/plans/wc3-3.0-compatibility.md`. Upstream is a reference only, because it has defects.
- MPQ refusal: check before unpacking, so a refused map writes nothing.
- Preflight also covers w3e, w3s and wpm.
- Agents may use Playwright for hiveworkshop.com: plain browser, no stealth, no login, follow robots.txt.
- NCOW uses no tech tree, upgrades, random groups, custom colours or waygates, so `[Parity 1]` is not urgent for it.
- Related tickets are grouped under parent tickets.
- Standing rules: see "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Open questions
- Reviewer's open item: `w3e->read_string(4)` sits outside the `try` in `src/map_protector/protection_pipeline.ixx` (`:263`, `:446`) and `asset_obfuscation.ixx:707`. A w3e under 4 bytes may leave `hierarchy.map_directory` on the temp directory. Not checked at runtime and not ticketed yet.
- A truncated file of an accepted version still crashes, because `Map::load` has no catch. Not ticketed yet.
- `load_mpq` ignores `unpack` and `extract_file` results. Not ticketed yet.
- Waiting on the user: "User checks in the real 3.0.1 World Editor" and "User hand-checks of the UI baselines".
