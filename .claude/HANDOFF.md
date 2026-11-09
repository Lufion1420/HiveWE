# Session handoff

Written 2026-11-09 (evening). Overwritten at each handoff.

## State
- `main` holds all work; tree clean after the handoff commit. Today's commits:
  - 5dfae77 automation rules.
  - 0f288e2 w3r v7 + w3c v3 (3.0 maps open: 465/466 stock accepted, undead02 refused on wtg).
  - cdaf4d7 docs (CI passed, run 42670833789).
  - 3a21f4c start-location indices by position (CI passed, run 42676375283).
  - 5feea88 unquoted trigger operators, Lua `~=` and nil.
  - d611820 Lua: no gg_trg_/InitTrig_ for custom-text triggers, identifier sanitizing, no trailing comma for parameterless events.
  - d7126ea Lua globals get per-type defaults.
  - 93c859a gg_dest_ numbers below 1000 padded.
  - 1f4262f script-check tools and rules.
  - CI for commits after 3a21f4c was not checked.
- User game test of the HiveWE-saved Forgotten Hollow (93c859a build): map loads, fades in, units and abilities work, Orb of Start teleports and effects appear. Differences from stock the user saw:
  - no hero voice line near the first altar;
  - music in the post-teleport area sounds buggy;
  - combat sound effects missing;
  - extra pickup rings near the first altar that vanish on pickup.
  - Tickets: "HiveWE-saved map loses voice lines, plays music badly and drops combat sounds" and "HiveWE-saved map shows pickup rings near the first altar that stock doesn't have" (both under "World Editor parity: map output on save").
- Closed today: [3.0] w3r v7, [3.0] w3c v3, start-location indices, quoted operators, Lua trigger names/custom text, Lua global defaults, gg_dest_ padding.
- Open tickets added today:
  - "Automate the pending user checks with in-process test hooks".
  - "Automate game and World Editor load checks on map copies".
  - "Startup map load failure aborts silently instead of showing an error".
  - "[3.0] Stock 3.0 map logs model and texture load errors".
  - "HiveWE-saved 3.0 map refused at Create in game": cause found and fixed across today's commits; close it after the sound/rings work, or now with a note.
  - "Script generator diverges from World Editor output". It collects: day/night models, SetTeams(1) vs (2), missing SetHDWaterParamsEx/SetPlayerRaceSkin, Cursed_Flute extra trigger, unit numbers >= 10000 truncated by a 4-char scan, gg_item_ not scanned, Run/Enable Trigger on Lua custom-text triggers, padded trigger names not trimmed at reference, map_script.cpp ~119 initial values bypass normalize_preset_value.
  - "Automated save-and-compare sweep of HiveWE's generated script over all 191 stock 3.0 maps".
- Earlier open tickets (map load robustness, other parity items) are unchanged.
- Test maps for the user: `C:\Users\mauri\Documents\Warcraft III\Maps\HiveWE test\` (1 original, 2 hivewe). Delete that folder when no longer needed.
- Session scratchpad maps (mapA_*.w3x, fh_orig) are under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\e3a7362c-2d41-4624-a3e0-97467df743ab\scratchpad\w` and may disappear. Tools are now in `.claude/tools/script-check`.
- Stock root for stock tests (check it still exists): `C:/Users/mauri/AppData/Local/Temp/claude/11cf287c6d13d22a/624d2e1b-c296-4804-9b85-76d3a36906f9/scratchpad/stock`. Set `HIVEWE_STOCK_BODY_ROOT` and `HIVEWE_STOCK_W3I_ROOT` to it, and `HIVEWE_BODY_CHECK_CASC` to `C:/Program Files (x86)/Warcraft III`. Stock tests pass silently with 0 assertions when unset.
- World Editor: if it is open, do not touch it without new permission.
- Nothing running.

## Next step
1. Sound ticket first (user-visible, found in game): `wc3-format-specialist` diffs stock vs HiveWE-saved Forgotten Hollow sound sections (InitSounds, gg_snd_, music, w3s), then `cpp-dev`.
2. Rings ticket.
3. Then the 191-map sweep ticket, so script divergences are found automatically instead of one map at a time.
- In-game checks: put maps in `Documents\Warcraft III\Maps\<named subfolder>`; the user starts them via Battle.net. Agents can't click in the game.

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
- 2026-11-09: checks blocked by the no-SendInput rule get in-process test hooks instead of going to the user. Agents may launch the World Editor (and the game via Battle.net) on sandbox copies after telling the user. The orchestrator sorts checks into automatable / rule-blocked / user-only before handing any over.
- Never launch `Warcraft III.exe` directly (login screen the user can't use with Google sign-in); use the Battle.net launcher.
- Script fixes target general HiveWE save correctness; Forgotten Hollow is the reference map, not the goal.
- Standing rules: "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Orchestrator calls the user can overrule
- Unit x, y, z and record order are not kept byte-exact on save. `Units::load` re-derives z from terrain on all versions.
- Clearing a hero's stats in the same session can drop a promoted file back to sub 9. No data is lost.
- An id with bytes after its first NUL is refused on v12+ and accepted on v8.
- Pasted units copy `group_id` and the trailer. What `group_id` means is unknown.
- Camera 3.0 script calls (CAMERA_FIELD_DEPTH_OF_FIELD_*, ZABSOLUTE, BlzCameraSetupSetCameraType) are deferred to "[3.0] Emit 3.0 calls in the script generator".
- Startup map load failure: planned fix is a QMessageBox with the error instead of abort.

## Open questions (waiting on the user)
- None blocking. Optional: the earlier units v12/v13 and doodads checks are now covered by the "in-process test hooks" ticket, not the user.
- In-game confirmation of the sound and rings fixes once built (the agent puts maps in `Documents\Warcraft III\Maps\HiveWE test\`).
