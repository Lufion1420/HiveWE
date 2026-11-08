# HiveWE feature wishlist (research record)

Date: 2026-11-08. Game 3.0.1, repo HEAD d4149c6. Research only: nothing built, launched or edited in the fork's source.
Merged from three research passes (Hive Workshop browser pass, Blizzard forums and GitHub pass, third-party tools and HiveWE code pass).
Each item below is one filed ticket (all named `[Wish] ...`). Rank 1 is highest: value to map makers, evidence and doability, with data safety first.

## Method and limits

- Hive Workshop: 53 page loads in a headed browser, not logged in. Thread dates are post dates. Long threads were only sampled (for example 4 of about 11 HiveWE thread pages). Poster counts are a floor, not a survey.
- Blizzard forums: opened through a summarizing fetch tool, so quotes are paraphrase-level. Some were seen only as search-listing titles (labelled listing-only). Hive links in that pass are search snippets only.
- Tools and code: tool claims come from their own docs, repo metadata or snippets; no tool was run. HiveWE was not run. Code claims were read in a second pass on 2026-11-08 (see Verification).
- Counts in each item say how many distinct threads or sources were seen, with "about" when the passes overlapped. SINGLE-SOURCE means one person or one thread. "Snippet" means search-result text only.
- Doability is a rough guess with no design behind it. Quoted forum text was treated as data.
- No Reddit, XGM, Discord or YouTube evidence was usable. archive.org was not reachable. Upstream HiveWE issues come from upstream builds, not this fork.
- Line numbers in `src/main_window/hivewe.cpp` moved by about 40 lines against the tools pass because of uncommitted edits in the tree. Lines below are current as read on 2026-11-08.
- The fix for unknown format versions is uncommitted in the tree, so it is not filed as a gap.

## Verification (second pass, code read on 2026-11-08)

| Claim | Result | What was read |
|---|---|---|
| Saves overwrite in place, no safe write or autosave | VERIFIED | `src/base/hierarchy.ixx:169` std::ofstream on the real file; no temp or rename; grep for autosave in `src/` finds nothing |
| Unsaved Object Editor, trigger or Map Info edits lost on quit | VERIFIED | `src/main_window/hivewe.cpp:1753` checks only `filesystem_path.empty()` or `world_undo.has_pending_changes()`; `clear_all_undo` only at `hivewe.cpp:1040` and `src/base/map/resize.cpp:24` |
| Map saved after script compile error | VERIFIED | `src/base/map/map.ixx:441-449`, `:457 return true`; `hivewe.cpp:792`; `hivewe.cpp:882` play_test continues. An error dialog is shown first |
| Failed JassHelper treated as success | VERIFIED | `src/base/triggers/map_script.cpp:873-888`: no exit code check, stale `data/tools/war3map.j` copied when output has neither error string (stale outcome inferred, not run) |
| `.git` and non-map files in imports and exports | VERIFIED | `src/base/imports.ixx:61-83` (name blacklist only), `hivewe.cpp:869-871`, `src/base/map/unused_files.cpp:90` |
| Nondeterministic saves | NOT VERIFIED | Unordered containers exist (`slk.ixx:41-49`, `modification_tables.ixx:373`) but output order was not tested |
| Asset Manager false unused flags | VERIFIED | `unused_files.cpp:66-67` (upgrade commented out), `:60-65`, `:185-192` |
| One-line crash in Model Editor | VERIFIED | `src/model_editor/model_editor.cpp:103-106` calls `result.value()` after the error box |
| Non-Latin damage in the script editor | NOT VERIFIED | No setUtf8 or codepage in `src/trigger_editor`; default not checked; nothing run |
| TRIGSTR references inlined | VERIFIED for the Object Editor path | `src/models/single_model.cpp:678-679`, `:750-751`. Map Protector strip option is off by default (`map_protector.cpp:100`). The upstream maintainer statement on issue 126 was not re-read |
| Test Map `-loadfile` with a folder map | Code VERIFIED, failure NOT VERIFIED | `hivewe.cpp:889` passes the map folder; the game's behaviour was not tested |
| Hive report: water impassable to boats | NOT VERIFIED | `src/base/terrain.ixx:1220-1232` sets unfloatable when no visible water; no test against the base WE |
| Other bug claims read | VERIFIED | Export MPQ `hivewe.cpp:857, :873, :877`; sound duration `map_script.cpp:436-437`; script editor `jass_editor.cpp:169, :373`; terrain brush `terrain_brush.cpp:542-556`; undo `world_undo_manager.ixx:31-32, :63-74`; Game Data Set TODO `map.ixx:106-108`; water shader `terrain.ixx:503-507`; CornEmitter `mdl_writer.cpp:920-924`; Test Map `hivewe.cpp:886, :893, :895` |
| Not checked | NOT VERIFIED | Selection kept through undo (`hivewe.cpp:114, :131`), doodad mesh release (`doodads.ixx:45-46`), model optimizer, Map Protector crash comment status |

## Items by area


### Save safety and data loss

#### [Wish] Add safe writes and automatic versioned backups
- Rank 1. Doability: Medium.
- Map makers lose work to failed saves, crashes and corrupted maps (data going to 0 KB, imports lost) and keep numbered copies by hand; JNGP once did this automatically. Evidence (about 9 threads: 4 opened Hive threads with 8+ posters, 3 opened Blizzard threads, rest search snippets): https://www.hiveworkshop.com/threads/huge-bug-in-the-world-editor.326538/ ; https://www.hiveworkshop.com/threads/it-never-cease-to-amaze-me.370916/ ; https://us.forums.blizzard.com/en/warcraft3/t/custom-map-data-keeps-breaking/39367 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-map-save-corruption/24244. HiveWE today: no. VERIFIED src/base/hierarchy.ixx:169 opens the real file with std::ofstream (truncates), no temp file and rename, write result not checked; grep of src finds no autosave or backup code. Doability: Medium. Write to a temp location, swap, keep the last N saves; no format work.

#### [Wish] Prompt before quitting with unsaved Object Editor, trigger or Map Info edits
- Rank 2. Doability: Easy.
- Bug: only world edits count as unsaved, so edits in the Object Editor, trigger editor, Map Info or Scenario windows can be lost on quit with no prompt, and the prompt appears after a clean save. Maps makers need quit and New Map to ask whenever anything differs from disk. Evidence: code reading only (tools-and-code research, no user report). HiveWE today: VERIFIED src/main_window/hivewe.cpp:1753 `has_unsaved_changes = filesystem_path.empty() || world_undo.has_pending_changes()`; clear_all_undo is called only at hivewe.cpp:1040 (new map) and src/base/map/resize.cpp:24, never on save. Doability: Easy to Medium. A dirty flag must reach several editors.

#### [Wish] Stop reporting Map saved and launching Test after a script compile error
- Rank 3. Doability: Easy.
- Bug: when script generation fails the map is still reported as saved and Test Map launches the stale script. Map makers should see a failed save, and Test should not start. Evidence: code reading only (tools-and-code research). HiveWE today: VERIFIED src/base/map/map.ixx:441-449 shows a 'vJass output' dialog on error but line 457 still returns true; hivewe.cpp:792 then shows 'Map saved' and play_test (hivewe.cpp:882) continues to launch. The user does see the error dialog. Doability: Easy. Return a status from the script step and let save_current_map stop.

#### [Wish] Treat a failed JassHelper run as a failure
- Rank 4. Doability: Easy.
- Bug: a missing or slow JassHelper run can leave the map with a script from a previous map. Map makers need an error instead of a silently wrong war3map.j. Evidence: code reading only (tools-and-code research). HiveWE today: VERIFIED src/base/triggers/map_script.cpp:873-888: no exit code check, stderr ignored, waitForFinished uses the default 30 s, and if the output contains neither error string the shared data/tools/war3map.j is copied into the map. The QProcess is never deleted. The stale-file outcome is inferred, not run. Doability: Easy. Check start and exit status, use a per-run output path.

#### [Wish] Keep non-map files out of imports and exported archives
- Rank 7. Doability: Easy.
- Bug: a .git folder or notes in the map folder get listed in war3map.imp and packed into the exported .w3x. This hurts the folder-map-in-git workflow serious teams use. Evidence: code reading only; the workflow itself is described by https://github.com/tdauth/wowr (tdauth/wowr, repo docs). HiveWE today: VERIFIED src/base/imports.ixx:61-83 walks every regular file and skips only names in the fixed blacklist (imports.ixx:15, :64, :76); src/main_window/hivewe.cpp:869-871 (Export MPQ) adds every regular file; src/base/map/unused_files.cpp:90 lists them too. Doability: Easy. Add an exclude list for dot folders and chosen patterns.

#### [Wish] Report Export MPQ results and keep the old file until the new one succeeds
- Rank 10. Doability: Easy.
- Makers need to know if the exported .w3x is complete. Berr (Hive, 2026-06-06) asked for an export to w3x so they need not return to the base WE; Export MPQ already exists in HiveWE, so the remaining gap is feedback. Evidence (SINGLE-SOURCE for the request, plus code): https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11 ; https://www.hiveworkshop.com/threads/got-world-editor-working-mostly.370410/. HiveWE today: partial. VERIFIED src/main_window/hivewe.cpp:857 deletes the old file before the new archive is made; :873 prints failed files to the console only; :877 ignores the compaction result. Doability: Easy. Write to a temp name, show a result dialog, then replace.

#### [Wish] Warn when a save or open changes data from an older game version
- Rank 11. Doability: Medium.
- Makers fear saving old maps in a new editor: per-level ability data zeroed past level 4, gold costs warped, maps saved before the game data version field treated as Forsaken Kingdom data, and an older editor reporting missing or invalid data on a newer map. They want a warning and a before and after report. Evidence (about 8 sources: 3 opened Hive threads, 4 opened Blizzard threads): https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/ ; https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-6 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-30-%E2%80%93-critical-bugs-in-custom-maps/39756 ; https://us.forums.blizzard.com/en/warcraft3/t/patch-300-crashes-custom-maps-that-use-slk-data/38840. HiveWE today: partial. Always saves the newest formats (upstream decision, not reversible). VERIFIED src/base/map/map.ixx:106-108: a TODO says stock data is always loaded from /Units regardless of Game Data Set. The unknown-format-version fix is uncommitted in the tree and is not a gap here. Doability: Medium. Comparing loaded and to-be-saved data per file is feasible; clear wording is the work.

#### [Wish] Add save round-trip tests for all map files
- Rank 29. Doability: Medium.
- A regression should not be able to silently damage saved maps. Evidence: tools-and-code research only; no user thread. HiveWE today: partial. tests/ has MDL, object data and format version checks; research found no save round trip for w3e, doo, w3r, w3c, w3i, wtg or wts; tests/map_format_check_test.cpp is uncommitted. Not re-checked. Doability: Medium.

#### [Wish] Warn before a map reaches its archive size limit
- Rank 30. Doability: Easy.
- Saves and test launches crash around 3.5 GB in the base WE with no explanation; the 4 GB explanation is a guess. Folder saves still work. Evidence (3 posters on 2 opened Hive threads; SINGLE-SOURCE for the figure): https://www.hiveworkshop.com/threads/world-editor-crash-on-non-folder-save-or-map-test-launch.372846/ ; https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/. HiveWE today: not checked. Doability: Easy for a warning; raising the limit is Unlikely.


### Stability and bad imports

#### [Wish] Do not crash when a model fails to open in the Model Editor
- Rank 5. Doability: Easy.
- Bug: opening a model that fails to parse shows an error and then crashes the editor, losing unsaved map work; 3.0 v1800 models make this reachable. Evidence: code reading only (tools-and-code research). HiveWE today: VERIFIED src/model_editor/model_editor.cpp:103-106 shows the error box and then calls result.value() on the failed result anyway; the browse path at :129-132 returns correctly. Doability: Easy, one added return.

#### [Wish] Survive a bad imported model or texture instead of failing the map load
- Rank 6. Doability: Medium.
- A model or texture that works in game can stop the base World Editor from ever opening the map again; makers want the editor to open the map anyway or refuse the file at import time. Berr reports HiveWE already opens maps the WE cannot. Evidence (at least 17 posters on opened Hive threads, 1 opened Blizzard thread, 6 snippets): https://www.hiveworkshop.com/threads/it-never-cease-to-amaze-me.370916/ ; https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-4 ; https://www.hiveworkshop.com/threads/how-to-prevent-world-editor-crashes-with-imported-assets.348051/ ; https://us.forums.blizzard.com/en/warcraft3/t/broken-maps/39981. HiveWE today: not checked how it handles one failing model at load. Doability: Medium. Parse in isolation and show a placeholder on failure; some risk around GL loading.


### Test Map and iteration

#### [Wish] Make Test Map launch errors visible and fix argument handling
- Rank 14. Doability: Easy.
- Test Map fails silently, sits at the home screen, or drops units placed in HiveWE (two reports). Folder maps are reported to be kicked to the main menu after the October 2026 patch. Makers want a clear message when the launch fails and quoted arguments to work. Evidence (about 10 sources: 6 opened Hive pages, 1 opened Blizzard thread https://us.forums.blizzard.com/en/warcraft3/t/new-patch-wont-load-folder-maps/39588): https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-5 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11. HiveWE today: Bug: VERIFIED src/main_window/hivewe.cpp:886 fs::canonical on the game exe (throws if missing, inferred); :893 splits the test arguments on every space; :895 starts the process with no failure report. :889 passes the map folder to -loadfile, so the folder-map report applies; the game's behaviour was not tested (NOT VERIFIED). Doability: Medium. Check status, try a packed temp copy if folder loading fails.

#### [Wish] Let an external build step own the script, and export typed map globals
- Rank 22. Doability: Easy.
- Wurst, TypeScript and Ceres users want to press Test in HiveWE and run their own build without HiveWE regenerating their script, and want typed gg_ names for regions, cameras and placed units. Evidence (about 7 sources, mostly tool docs and snippets): http://wurstlang.org/start ; https://github.com/cipherxof/wc3-ts-template ; https://github.com/ceres-wc3/ceres ; https://github.com/stijnherfst/HiveWE/issues/62. HiveWE today: no. VERIFIED src/base/map/map.ixx:441 regenerates the script on every save. A 'Custom Tool' field in menus/settings_editor.ui is said to be unwired (parity audit, not checked). Doability: Easy for a flag and command. Medium if output is captured.

#### [Wish] Add Test Map presets: windowed, difficulty, extra arguments and fixed seed
- Rank 35. Doability: Easy.
- Named setups and an option for a fixed random seed, so crashes can be reproduced. Makers also want to test as another player and fill slots with AI. Evidence (about 5 sources: 2 opened Blizzard threads, 2 Hive posts): https://us.forums.blizzard.com/en/warcraft3/t/fixedseed-1-does-not-apply-to-wgc-wetestgameconfig/39833 ; https://us.forums.blizzard.com/en/warcraft3/t/patch-300-suggestions/37928 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10. HiveWE today: partial. hivewe.cpp:892-893 (read) takes one free-text testArgs string. The game may ignore -fixedseed when launched this way (reported). Doability: Easy for presets; Hard for player and AI slots, which depend on game launch options not checked.

#### [Wish] Hot-reload code in a running Test Map
- Rank 41. Doability: Medium.
- Change a script and see it in the running game without restarting; this saves minutes on maps with long loads. Evidence (tool docs only, none from map makers): https://github.com/lep/jhcr ; http://wurstlang.org/news/jhcr-updates.html. HiveWE today: no. Test Map always saves and relaunches (hivewe.cpp:881-896, read). Doability: Medium. JHCR is a separate alpha compiler; Lua needs another route; license not checked.

#### [Wish] Launch several game clients for desync testing
- Rank 42. Doability: Easy.
- Desyncs only show in multiplayer; teams test by opening several clients by hand. Evidence (SINGLE-SOURCE snippet plus project docs): https://www.hiveworkshop.com/threads/multiple-warcraft-3-running-on-a-single-pc.226287/ ; https://github.com/tdauth/wowr. Hive's advanced test settings request (Barade, Lizreu) is related. HiveWE today: no. Whether 3.0.1 allows multiple clients is not confirmed. Doability: Easy for launching; Unlikely for automatic lobby join.

#### [Wish] Start Test Map from the current view
- Rank 64. Doability: Medium.
- Start near the camera with a chosen hero instead of playing through. Evidence: no map-maker request; a forum question about controlling enemy units in tests only. HiveWE today: no. Doability: Medium. Needs temporary script injection in a test copy so the real map stays clean.


### Scripting and code workflow

#### [Wish] Add a real Lua script mode
- Rank 23. Doability: Medium.
- Lua is what new script work uses, but HiveWE edits it as vJass and copies it unchecked. Makers want correct highlighting and a syntax check at save. Evidence (6 sources: 3 opened Blizzard threads, 3 snippets): https://us.forums.blizzard.com/en/warcraft3/t/provide-zoom-copy-four-character-code-function-for-we/39071 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683 ; https://github.com/cipherxof/wc3-ts-template. HiveWE today: partial. Research found no Lua lexer in src and a 'VJass' language setting at jass_editor.cpp:129-131 and an unchecked copy at map_script.cpp:889-891 (research, not re-read). Doability: Medium. QScintilla has a Lua lexer; a syntax check needs a parser.

#### [Wish] Add go-to-definition, find references and search across all triggers
- Rank 24. Doability: Medium.
- Makers navigate large scripts in VS Code because the editor lacks this. They also want one search across trigger names and actions, and function docs and autocomplete for map functions. Evidence (about 8 sources: 3 opened Hive threads, 1 opened Blizzard thread, tool pages): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10 ; https://www.hiveworkshop.com/threads/world-editor-is-a-mess.319963/ ; https://us.forums.blizzard.com/en/warcraft3/t/provide-zoom-copy-four-character-code-function-for-we/39071 ; https://github.com/WarRaft/JASS-Tree-sitter-Rust. HiveWE today: partial. Highlighting and API completion exist; research reports completion reads only common.j, blizzard.j and cheats.j (jass_editor.cpp:266-344) and search covers open tabs only (trigger_editor.cpp:122-131); not re-read. Doability: Medium for Jass with the existing tokenizer; vJass harder.

#### [Wish] Make script compile errors clickable
- Rank 25. Doability: Medium.
- Click an error and land on the trigger and line, instead of reading raw JassHelper text against a generated script. Evidence: tools-and-code research; no user thread (every IDE tool does it). HiveWE today: partial. map_script.cpp:882-886 (read) returns the raw error text; the trigger_editor.cpp:94-96 read-only display was not re-read. Doability: Medium. Lines refer to the merged script, so a line map back to triggers is the work.

#### [Wish] Expose JassHelper options per map and fix its flags
- Rank 26. Doability: Easy.
- Debug builds, vJass toggles, optimizer and an import folder per map currently mean hand-editing a shared config; one report says the WE passes wrong arguments so JassHelper always optimises. Evidence (SINGLE-SOURCE each): https://us.forums.blizzard.com/en/warcraft3/t/world-editor-trigger-jasshelper-always-optimized/36613 ; https://github.com/tdauth/wowr. HiveWE today: partial. VERIFIED map_script.cpp:875-878 passes a fixed --scriptonly argument list. data/tools/jasshelper.conf was not read by me. Doability: Easy. Generate arguments and a config.

#### [Wish] Add pjass checks and leak warnings for scripts
- Rank 27. Doability: Easy.
- Warnings for handle leaks, shadowed globals, uninitialised globals and oversized strings while editing; 3.0 added GUI leak natives. Evidence: tool docs only (https://github.com/lep/pjass ; https://github.com/WarRaft/JASS-Tree-sitter-Rust) and one Blizzard listing. HiveWE today: no. pjass runs only through JassHelper with no extra flags (research, not re-read). Doability: Easy for pjass flags, Medium for leak analysis.

#### [Wish] Write correct sound durations to the generated script
- Rank 44. Doability: Medium.
- Bug: sounds get a duration equal to the fade-in rate, so SetSoundDuration is wrong for scripted audio. Evidence: code reading only. HiveWE today: VERIFIED src/base/triggers/map_script.cpp:437 `SetSoundDuration(sound_name, i.fade_in_rate)` with a TODO at :436 asking how to determine the duration; the in-game effect was not tested. Doability: Medium. Needs a reader for the audio length.

#### [Wish] Improve the script editor: theme colours, zoom and speed on large scripts
- Rank 61. Doability: Easy.
- Readable colours in the light theme, zoom and tab options, and typing that stays responsive in large custom text. Evidence (about 4 sources: 2 opened Blizzard threads, snippets): https://us.forums.blizzard.com/en/warcraft3/t/provide-zoom-copy-four-character-code-function-for-we/39071 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683. HiveWE today: partial. VERIFIED src/trigger_editor/jass_editor.cpp:169 copies the whole document each styling pass and :373 calls text() inside the search loop; not profiled. Fixed colours at :96-109 not re-read. Doability: Easy.

#### [Wish] Check non-Latin text in the script editor
- Rank 62. Doability: Easy.
- Needs verification. Russian, Chinese or other non-Latin comments and strings should stay intact through editing and saving. Evidence: code reading only. HiveWE today: NOT VERIFIED. A grep of src/trigger_editor finds no setUtf8 or codepage setting; QScintilla's default was not checked and nothing was run, so no damage is confirmed. Doability: Easy if real. A test with a copy of a map settles it.


### Version control and collaboration

#### [Wish] Save maps as a text view git can diff, with a command-line entry
- Rank 20. Doability: Medium.
- Readable text for objects and triggers while working, binary for release. Teams bridge with W3Diff, w3x2lni, WC3MapTranslator or Patchwork. Headless save, compile and export would also let CI build releases. Evidence (about 14 sources: 4 opened Hive threads, 2 opened Blizzard threads, tool pages): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-2 ; https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/ ; https://www.hiveworkshop.com/threads/patchwork-mapconverter.350385/ ; https://us.forums.blizzard.com/en/warcraft3/t/suggestion-adding-project-management-to-the-war3-map-editor/350. HiveWE today: partial. Folder maps save, files stay binary; an object data text export exists (changelog v0.21, not checked). Map::load and the GL pool are bound together, so a CLI needs separation. Doability: Medium to Hard. Objects and triggers are tractable; terrain text is large and unreadable.

#### [Wish] Keep triggers and scripts as files in the map folder
- Rank 21. Doability: Medium.
- Edit scripts in any editor with live pickup, get readable diffs and no merge conflict on one binary. Better Triggers, Patchwork and wc3-ts-template do this. AtheosTG asks for subdirectories of triggers and custom scripts. Evidence (about 6 sources: 1 opened Blizzard thread, tool docs, snippets): https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683 ; https://thelazzoro.github.io/BetterTriggersGuide ; https://www.hiveworkshop.com/threads/patchwork-mapconverter.350385/ ; https://github.com/cipherxof/wc3-ts-template. HiveWE today: no. src/base/triggers/triggers.ixx:905 and :939 write wtg and wct only (read in research, not by me). Doability: Medium if the files are an extra synced view and wtg and wct stay the output.

#### [Wish] Make saves deterministic so unchanged files stay byte-identical
- Rank 28. Doability: Medium.
- NOT VERIFIED. Diffs and git history should show only real changes. Evidence: one snippet on Wurst deterministic builds; no map-maker thread. HiveWE today: NOT VERIFIED. imports.ixx:72 lists files in filesystem order (read); src/file_formats/slk.ixx:41-49 and modification_tables.ixx:373 store data in hive::unordered_map (read), but whether written output order changes between saves was not tested. Doability: Easy to Medium. Sort on write; check compatibility.

#### [Wish] Define objects from code or data files
- Rank 65. Doability: Medium.
- Create and change objects by script, as Grimex did in JNGP. Evidence (2 sources: 1 GitHub issue with a 'maybe in the future' reply, 1 snippet): https://github.com/stijnherfst/HiveWE/issues/38. HiveWE today: no. Object data import exists. Doability: Medium.

#### [Wish] Open several maps and copy between them
- Rank 68. Doability: Hard.
- Several map windows, with objects and triggers copied across. Footman16 warns that multiple maps are a known corruption cause in the base WE. Evidence (SINGLE-SOURCE on Hive, 1 opened Blizzard post): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/ ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396. HiveWE today: no. A global map object (map_global.ixx) makes it hard in one process; two processes not tested. Doability: Hard in one window; Easy if two instances work.


### Performance and big maps

#### [Wish] Speed up opening and saving big maps
- Rank 15. Doability: Medium.
- Maps with many GUI triggers, objects and destructibles take minutes to open or save; one report ties nearly all of it to GUI triggers, and editing a name can stall for over 10 seconds in the base WE. Evidence (at least 11 posters on opened Hive threads, 4 opened Blizzard threads): https://www.hiveworkshop.com/threads/minimizing-world-editor-loading-times.365479/ ; https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/ ; https://us.forums.blizzard.com/en/warcraft3/t/bug-renaming-custom-object-creates-excessive-lag/39180 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-lag-and-slower-terrain/39221. HiveWE today: not measured. The maintainer says 'way snappier' (https://github.com/stijnherfst/HiveWE/issues/111); eejin reports 32 s down to 4 s on one test map. Constraint for the GUI trigger editing ticket: stay fast at about 1,900 triggers. Doability: Medium. Benchmark a trigger-heavy map first.

#### [Wish] Make unit and variable pickers fast with thousands of objects
- Rank 16. Doability: Easy.
- Choosing a unit or variable in the trigger editor can take seconds on big maps (hundreds of variables, many gg_unit names). The Variable Editor is asked to get a search bar. Evidence (about 5 sources: 2 opened Hive threads, 2 opened Blizzard threads, 1 snippet): https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/ ; https://www.hiveworkshop.com/threads/reducing-world-editor-lag-trigger-editor-gui.325033/ ; https://us.forums.blizzard.com/en/warcraft3/t/massive-lag-when-opening-variable-editor-set-skin-bug/36749. HiveWE today: community research says grep finds no filter widget in variable_editor.cpp/.h; not checked by me. Doability: Easy to Medium. Lazy, searchable pickers; adds to the GUI trigger editing and variable types tickets.

#### [Wish] Check whether big doodad counts lock maps out of the editor
- Rank 36. Doability: Easy.
- One map's developer cannot use the Reforged editor because of a reported 30k doodad cap, while their map has about 52k doodads that run fine in game. Evidence (SINGLE-SOURCE developer, 1 opened and 1 listing-only Blizzard thread): https://us.forums.blizzard.com/en/warcraft3/t/doodad-limit-increase/36350. HiveWE today: possibly no cap; research grep finds no 30000 or 52092 constant. Not tested at that size. Doability: Easy to Medium. Verify with a large map copy. The game's own limit stays.

#### [Wish] Speed up terraining on doodad-heavy maps
- Rank 37. Doability: Easy.
- Height and cliff dragging slows with many doodads; 3.0 also slowed SD framerate unless draw distance is cut. Makers want lower editor load while terraining. Evidence (about 4 sources: 2 opened Hive posts, 2 opened Blizzard threads): https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/ ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-lag-and-slower-terrain/39221. HiveWE today: VERIFIED src/brush/terrain_brush.cpp:542-556 scans all doodads per update and does a linear std::find_if over pre_change_doodads with a lambda capturing a Doodad by value; quadratic with area is inferred, not profiled. Doability: Easy. Use a set keyed by creation number; add a draw-distance setting if absent.


### Object Editor

#### [Wish] Let people organise objects in custom nested folders
- Rank 17. Doability: Medium.
- The Object Editor is restricted to the fixed race list, so people use the race field for tidiness, which changes gameplay. They want custom, nested, drag-and-drop folders. Evidence (at least 6 posters on 5 opened Hive pages, 3 opened Blizzard threads): https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/ ; https://www.hiveworkshop.com/threads/spotlight-hivewe-0-10-now-available.372626/ ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestion/34452 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683. HiveWE today: no user folders (a Custom folder exists; bookmarks and history exist). BogdanW3 says the map format has no place for folders. Doability: Medium. Needs an editor-only sidecar file that does not disturb the map files.

#### [Wish] Merge object data without overwriting and renumber colliding IDs
- Rank 18. Doability: Easy.
- Importing object data today is all-or-one and colliding IDs silently replace existing objects. Makers want to pick objects, compare, and renumber collisions with references remapped. Evidence (at least 6 posters on opened Hive threads; 1 opened Blizzard thread): https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/ ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10 ; https://www.hiveworkshop.com/threads/alternative-to-jngp-object-merger-for-reforged.372785/ ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396. HiveWE today: partial. Changelog lists a conflict preview and per-object overwrite; renumbering and reference remapping not checked. Doability: Easy. Renumbering and remapping references is the part to check.

#### [Wish] Lock object fields against game patches
- Rank 31. Doability: Medium.
- Unedited fields inherit game data, so a patch can silently change a custom ability. Makers want to freeze a field or object, and to export all fields. Evidence (3 posters on 3 opened Hive pages): https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/ ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-5 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-2. HiveWE today: not checked. Doability: Medium. Writing current values as explicit modifications would do it. Related to the Reset Field and Raw Data ticket.

#### [Wish] Explain Object Editor fields with tooltips and show raw IDs and order strings
- Rank 32. Doability: Easy.
- Tooltips on what each field does, hardcoded spell behaviour on hover, IDs always visible (Footman (hfoo)), and copy of order strings with quotes next to the rawcode. Evidence (4 posters on opened Hive pages, plus 1 opened Blizzard thread): https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/ ; https://us.forums.blizzard.com/en/warcraft3/t/provide-zoom-copy-four-character-code-function-for-we/39071. HiveWE today: partial. Ability Insights exists; a copy-ID button exists (research cites object_editor.cpp:1551, not re-read). Doability: Easy. Writing the text is the main cost.

#### [Wish] Show hidden fields, favourites and instant filtering in the Object Editor
- Rank 51. Doability: Easy.
- Fields hidden by 'Is Building' still function (a non-building unit can have a pathing texture); a new rival editor shows what people respond to: instant field filtering, collapsible categories, favourite fields. Evidence (3 posters on 1 opened Hive thread): https://www.hiveworkshop.com/threads/building-a-world-editor-replacement-objects-triggers-and-terrain-previews-and-feedback.375822/. HiveWE today: not checked. Doability: Easy. A competing open-source replacement is a sign terrain parity is HiveWE's lead.

#### [Wish] Edit several objects at once
- Rank 52. Doability: Medium.
- Multi-select and apply one value to many objects, instead of copy-pasting stats to three dozen heroes. Evidence (2 posters, opened Hive pages): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-5 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/. HiveWE today: not checked. Doability: Medium.

#### [Wish] Finish Object Editor pickers and list every stock object
- Rank 53. Doability: Easy.
- Buff fields show raw IDs, and base-WE lists have missing objects (two venom buffs, a Keg of Ale, new units in the palette). Evidence (SINGLE-SOURCE for the buff picker, 4 listing-only Blizzard threads): https://github.com/stijnherfst/HiveWE/issues/122. HiveWE today: partial. The maintainer says not all Object Editor sub-menus are finished; ability_list_editor.ixx exists; the buff picker was not checked. Doability: Easy.


### Terrain, doodads and placement

#### [Wish] Keep hand-painted pathing when a map passes through the base World Editor
- Rank 19. Doability: Medium.
- The base WE rewrites war3map.wpm on save and discards pathing painted in HiveWE, so mixed workflows need manual file copying. Evidence (5 posters on 2 opened Hive pages): https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-4. HiveWE today: not checked. Doability: Medium. The base WE cannot be changed; HiveWE could keep the painted layer separately and reapply it on open or save.

#### [Wish] Support the 64-tile tileset limit and show the tile count
- Rank 43. Doability: Medium.
- The tileset limit moved from 16 to 64 in 2.0.3 PTR notes; one report said it did not fully work, and makers have long wanted more. Evidence (about 6 sources: 2 opened Hive pages, 1 opened and 3 listing-only Blizzard threads): https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/ ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-9 ; https://us.forums.blizzard.com/en/warcraft3/t/instead-of-tile-limit-increase-easier-faster-better-feature/37018 ; https://us.forums.blizzard.com/en/warcraft3/t/20323051-tile-limit-increase-broken/37008. HiveWE today: not checked; research grep found no tile limit constant. Doability: Medium.

#### [Wish] Check boat pathing for water edited in HiveWE
- Rank 46. Doability: Medium.
- Needs verification. One report says water created or modified in HiveWE is unpathable for boats in game until the map is re-saved in the base WE. Evidence (SINGLE-SOURCE report, 1 corroborating post on the paradigm difference): https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11 ; https://www.hiveworkshop.com/threads/it-never-cease-to-amaze-me.370916/. HiveWE today: NOT VERIFIED. src/base/terrain.ixx:1220-1232 sets unfloatable for any cell without visible water (water_pathing flag, corner_water, and height above ground); the changelog says water pathing for units was fixed, boats not checked. Doability: Medium to Hard. Needs a test map against the base WE output.

#### [Wish] Improve pathing preview: texture preview, collision radius and air pathing
- Rank 47. Doability: Medium.
- Preview a pathing texture before placing it, show collision radius, view air pathing. Pathing painting itself is widely praised. Evidence (4 posters on 3 opened Hive pages): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-5 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-2 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11. HiveWE today: not checked. Doability: Easy to Medium.

#### [Wish] Group doodads so they move and select as one
- Rank 54. Doability: Medium.
- Named groups that move and rotate together, plus a way to exclude huge glows that block selecting other doodads. Evidence (3 posters on 2 opened Hive threads, dated 2009 and 2018): https://www.hiveworkshop.com/threads/terrain-editor-features.133010/page-2 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-2 ; https://www.hiveworkshop.com/threads/terrain-editor-features.133010/. HiveWE today: not checked. Doability: Medium. Groups need a sidecar file, as with object folders.

#### [Wish] Add terrain shaping brushes: slopes, hardness, tile noise and fill
- Rank 55. Doability: Medium.
- Slope and gradient tools, brush hardness, quick terrain generation, random tile mix and bucket fill. Evidence (about 4 posters on opened Hive threads; 1 snippet list): https://www.hiveworkshop.com/threads/terrain-editor-features.133010/ ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-4 ; https://www.hiveworkshop.com/threads/editor-feature-request-bug-fix-list-for-patch-during-after-blizzcon.373753/. HiveWE today: partial. Heightmap import exists (0.4); other brushes not checked. Doability: Medium. Adds to the selection and terrain tools ticket.

#### [Wish] Delete water directly and stop tileset changes rerolling blight
- Rank 56. Doability: Easy.
- Removing scenic water is hard because water is tied to cliff levels, and changing the tileset or painting blight rerolls dirt tiles everywhere. Evidence (about 4 sources: 1 opened Blizzard thread, 3 snippets): https://us.forums.blizzard.com/en/warcraft3/t/modifying-tileset-in-editor-rerolls-blight-dirt-placement/38155 ; https://www.hiveworkshop.com/threads/editor-feature-request-bug-fix-list-for-patch-during-after-blizzcon.373753/ ; https://www.hiveworkshop.com/threads/quality-of-life-changes-editor.278312/. HiveWE today: not checked. Doability: Easy for the tools; relaxing cliff height limits is Hard because the game must render what the editor allows.

#### [Wish] Add a stamp library for terrain, doodads and units
- Rank 63. Doability: Medium.
- Save a built cluster (camp, gate, forest edge) and reuse it across maps. Evidence: tools-and-code research only (https://www.hiveworkshop.com/threads/how-to-create-melee-maps.187652/); no user request. HiveWE today: partial. Per-brush copy and paste exist; session only (inferred). Doability: Medium.

#### [Wish] Add a live symmetry brush
- Rank 70. Doability: Medium.
- Paint one half of a melee map and the other side updates, including rotational symmetry. Evidence: tools-and-code research only; no finished tool found (https://www.hiveworkshop.com/threads/how-to-create-melee-maps.187652/). HiveWE today: no. Mirror paste is tracked separately. Doability: Medium. Doodads and cliffs at corners make it harder than tiles.


### Models, textures and previews

#### [Wish] Explain why a model is white, invisible or missing textures
- Rank 33. Doability: Medium.
- A very common beginner problem: the texture path in the model does not match the import path. People open MDL text to find it. A list of expected textures and which ones resolve would answer it. Evidence (8 sources, mostly search snippets; none opened): https://www.hiveworkshop.com/threads/warcraft-iii-previewer.357420/ ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683. HiveWE today: not checked at this level. Doability: Easy to Medium. HiveWE already parses MDX. Overlaps the Map Doctor ticket.

#### [Wish] Convert and preview BLP, TGA and PNG textures and create BTN and DISBTN icons
- Rank 34. Doability: Medium.
- Drop in an image and get BTN and DISBTN with correct paths and border, view BLP, DDS and TGA in place, and convert in bulk. Today this needs Button Manager or similar. Evidence (3 opened sources plus tool pages and snippets): https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/ ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396 ; https://www.hiveworkshop.com/threads/button-manager-v1-8-2.116280/latest. Spellbound's request is SINGLE-SOURCE on the icon manager. HiveWE today: no. Research found no BLP writer in src; textures load only for rendering. Map Protector knows the BTN and DISBTN pairing (asset_obfuscation.ixx:405-447, not re-read). Doability: Medium. Needs a BLP encoder.

#### [Wish] Add a model-editing pass: save, convert, retarget textures
- Rank 48. Doability: Medium.
- Small edits (MDL and MDX conversion, fixing texture paths, renaming sequences) now mean opening Retera's Java app. Popcorn (CornEmitter) data is also dropped when writing MDL. Evidence: tools-and-code research only; tools are Retera's Model Studio and Magos. HiveWE today: partial. Viewer only; VERIFIED src/file_formats/mdx/mdl_writer.cpp:920-924 has a TODO to decode CornEmitter fields. A full model editor is Unlikely; link out instead. Doability: Medium, only after v1800 work (tracked 3.0 format ticket).

#### [Wish] Show which graphics modes a model and a map really support
- Rank 49. Doability: Medium.
- Makers import every file three times to cover SD, HD and DE, want models from every mode regardless of asset mode, a per-object model version choice, and report pre-3.0 objects scaled wrongly in DE. Evidence (about 6 sources: 2 opened Hive threads, 3 opened Blizzard threads): https://www.hiveworkshop.com/threads/can-you-use-1-import-for-both-hd-and-de.375627/ ; https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-6 ; https://us.forums.blizzard.com/en/warcraft3/t/a-small-request-from-a-long-time-map-maker/38619 ; https://us.forums.blizzard.com/en/warcraft3/t/pre-30-custom-map-issues-with-the-30-world-editor/39317. HiveWE today: not checked. Doability: Medium for an editor preview switch; mixing modes in game is Unlikely. Fallback rules are unconfirmed.

#### [Wish] Browse and import Hive resources from inside the editor
- Rank 69. Doability: Medium.
- Pick a model or icon from the Hive without manual download and path setup. Evidence (4 posters on 2 opened Hive threads): https://www.hiveworkshop.com/threads/spotlight-hivewe-0-10-now-available.372626/ ; https://www.hiveworkshop.com/threads/introducing-hivewe.303183/. HiveWE today: no. Doability: Medium; needs Hive's agreement. Hive robots.txt disallows the resource download paths, so it must not be scraped.

#### [Wish] Add an HD water shader in the viewport
- Rank 71. Doability: Hard.
- Water looks like the game in HD mode. Evidence: code comment only. HiveWE today: no. VERIFIED src/base/terrain.ixx:503-507 forces SD water textures 'till I implement a water shader'. Depends on the 3.0 format ticket. Doability: Hard.


### Import and asset management

#### [Wish] Stop Asset Manager flagging used files as unused
- Rank 9. Doability: Easy.
- Bug: bulk-deleting 'unused' files can remove files the map really uses, such as upgrade icons or files referenced only by FDF, TOC or war3mapSkin.txt. Evidence: code reading only (tools-and-code research). HiveWE today: VERIFIED src/base/map/unused_files.cpp:66-67 (upgrade lookup commented out), :60-65 (fixed field lists; the ability list has targetart, effectart, specialart, art, researchart only), :185-192 (script text is the only extra source). Which stock fields are missing from those lists was not checked. The recycle bin softens the damage. Doability: Easy to Medium. Map Protector already reads FDF, TOC and skin files.

#### [Wish] Auto-create objects for imported models and overwrite same-name imports
- Rank 50. Doability: Easy.
- Importing a model should offer an object for it, and importing the same name should overwrite instead of adding a second entry. Also wanted: multi-select delete and list position kept after rename. Evidence (4 posters on opened Hive pages, 3 opened Blizzard threads): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10 ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-5 ; https://www.hiveworkshop.com/threads/workaround-model-file-crashes-world-editor.367159/ ; https://us.forums.blizzard.com/en/warcraft3/t/multiple-selection-for-asset-manager-world-editor/37093. HiveWE today: partial. Asset Manager has multi-select, an Unused only filter and Select all unused. Auto-creating objects is the new part. Doability: Easy. Verdun's reaction suggests folder mode also needs explaining.


### UI, frames and usability

#### [Wish] Add base-WE hotkeys, layout options and path entry for people switching over
- Rank 38. Doability: Medium.
- Long-time users are put off by small differences: button positions, hotkeys, a traditional trigger editor, and the Open dialog (paste a path, remember locations). Evidence (4 posters on 3 opened Hive pages, plus 2 more on the Open dialog): https://www.hiveworkshop.com/threads/any-way-to-fix-world-editor-ui-bugging-out-after-enough-extended-use.375796/ ; https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10 ; https://www.hiveworkshop.com/threads/building-a-world-editor-replacement-objects-triggers-and-terrain-previews-and-feedback.375822/. HiveWE today: not checked. Doability: Easy for hotkeys and path entry, Medium for layout. The World Editor audit in recent commits is the input.

#### [Wish] Polish undo: memory cap, resize warning and selection kept after undo
- Rank 45. Doability: Easy.
- Long terrain sessions should not grow without limit (the WE has an Undo Limit setting), resize should say it clears undo, and undoing a nudge should keep the selection. Evidence: code reading only. HiveWE today: partial. VERIFIED src/base/world_undo_manager.ixx:31-32 and :63-74 have no cap; resize.cpp:24 clears undo with a comment that says it copies vanilla WE behaviour. The selection-after-undo claim (hivewe.cpp:114, :131) was not checked. Doability: Easy for the cap and warning, Medium for selection.

#### [Wish] Design UI frames visually and read FDF
- Rank 66. Doability: Hard.
- A visual FDF designer instead of external tools and pasted code. Evidence (about 8 sources: 1 opened Hive thread, 1 opened Blizzard post, snippets): https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10 ; https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/ ; https://www.hiveworkshop.com/threads/warcraft-3-reforged-ui-designer-ruid.334868. HiveWE today: no. FDF appears only in Map Protector. Upstream README lists it as a potential contribution. Doability: Hard. A validator is Medium; a faithful preview needs engine layout.

#### [Wish] Add UI scaling and zoom in every editor
- Rank 67. Doability: Medium.
- Small dense text; zoom in every editor including triggers. Evidence (2 opened Blizzard posts): https://us.forums.blizzard.com/en/warcraft3/t/provide-zoom-copy-four-character-code-function-for-we/39071 ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180. HiveWE today: not checked. Qt scaling may help. Doability: Easy to Medium.


### Localization and strings

#### [Wish] Keep TRIGSTR references when editing object text
- Rank 12. Doability: Medium.
- Bug: editing a translated object name or tooltip replaces the TRIGSTR reference with plain text, which breaks translated maps. Evidence: code comments; upstream maintainer comment on https://github.com/stijnherfst/HiveWE/issues/126 (2026-03, via search, not re-read) says HiveWE 'isn't TRIGSTR preserving' and inlines strings. HiveWE today: VERIFIED src/models/single_model.cpp:678-679 and :750-751 comments state the TRIGSTR reference is overwritten. The Map Protector 'Strip trigger strings' option is off by default (src/map_protector/map_protector.cpp:100); other save paths were not checked. Doability: Medium. Edit the string table entry, not the reference.

#### [Wish] Add a translation workspace with stable string keys
- Rank 39. Doability: Medium.
- Side-by-side master and locale strings, a list of untranslated or outdated entries, string keys referenced in any field, translated war3mapMisc.txt and working string import. Today makers use external tools like wc3trans. Evidence (about 8 sources: 3 opened Hive threads, 2 opened Blizzard threads, tool pages): https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/ ; https://us.forums.blizzard.com/en/warcraft3/t/editor-import-strings-function-not-working-properly/28246 ; https://us.forums.blizzard.com/en/warcraft3/t/bugs-critical-bugs-and-suggetions/39522 ; https://github.com/tdauth/wc3trans. HiveWE today: no. hierarchy.ixx:21 fixes locale to enus and wts round-trips only (research, not re-read). Depends on the TRIGSTR ticket. Doability: Medium. Adds to the Export Strings ticket.

#### [Wish] Translate the editor interface and fall back to English for missing strings
- Rank 40. Doability: Medium.
- The base WE has missing strings on non-English clients, and HiveWE users ask for a language file; a community zhCN patch exists, and HiveWE needs the English install. Evidence (about 9 sources: 4 opened Hive pages, 1 opened Blizzard thread, 3 listings, 1 GitHub issue): https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-5 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10 ; https://us.forums.blizzard.com/en/warcraft3/t/dear-developers-can-you-look-back-at-some-minor-issues/39059 ; https://github.com/stijnherfst/HiveWE/issues/150. HiveWE today: not checked. Doability: Medium. Qt supports translation files; extracting strings is the work. Fix the English-only install message first.


### Release and map checks

#### [Wish] Add a Map Doctor report for known breakage
- Rank 8. Doability: Medium.
- Makers find culprits by deleting objects one at a time. They want one report that names the file: missing imports, dangling paths, zero-face geosets, paths without extension or with a second dot, SLK-shipped data, stray comment markers in FDF, broken BTN/DISBTN pairs. Evidence (about 12 sources: 5 opened Hive threads, 4 opened Blizzard threads, a third-party checker site): https://www.hiveworkshop.com/threads/editor-crashing-on-you-i-found-the-fix-for-mine-and-hope-it-helps-others.373580/ ; https://us.forums.blizzard.com/en/warcraft3/t/models-with-empty-geosets-crash/38837 ; https://us.forums.blizzard.com/en/warcraft3/t/breaking-changes-in-existing-assets/38684 ; https://wc3mapdoctor.com/guides/map-crashes-on-loading-screen/. HiveWE today: partial. Map Protector already scans references for dangling paths (src/map_protector/asset_obfuscation.ixx:1102-1180, not read by me); an uncommitted src/base/map/map_format_check.ixx is in the tree and was not read, so check overlap. Doability: Medium. Each rule is simple, but the list follows game patches.

#### [Wish] Scan scripts for deprecated natives
- Rank 57. Doability: Easy.
- List non-Blz natives that 3.0 notes say will be removed, and natives removed since the 2025 baseline, before a patch breaks the map. Evidence: tools-and-code research only (plan .claude/plans/wc3-3.0-compatibility.md and Wurst grill patch http://wurstlang.org/start). HiveWE today: no. Doability: Easy. A text scan against the live common.j.

#### [Wish] Add a Build Release command: optimize, protect and export
- Rank 58. Doability: Medium.
- One button for the final file, replacing a chain of tools run by hand (old optimizers of unverified status). Includes stripping unused assets for a playable-only export. Evidence (about 8 sources: 3 opened Hive posts, 3 opened Blizzard posts, snippets): https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/ ; https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180 ; https://www.hiveworkshop.com/threads/optimizing-filesize-through-imports-and-map-optimizers.167574 ; https://github.com/stijnherfst/HiveWE/issues/43. HiveWE today: partial. Map Protector and Export MPQ (hivewe.cpp:844) are separate. The maintainer called stripping 'planned at some point'. Doability: Medium. Script minify and comment stripping are new.

#### [Wish] Show a map size breakdown with suggestions
- Rank 59. Doability: Easy.
- Where the megabytes go by type and largest file, plus unused-file shortcuts. Evidence: tutorials only (https://www.hiveworkshop.com/threads/optimizing-filesize-through-imports-and-map-optimizers.167574). HiveWE today: partial. Asset Manager has stats and sizes. Doability: Easy.

#### [Wish] Close out Map Protector's temporary crash hooks
- Rank 60. Doability: Medium.
- A comment tracks an unresolved in-game crash with obfuscated maps. Evidence: code comment only. HiveWE today: NOT VERIFIED. src/map_protector/asset_obfuscation.ixx:311-321 dated 2026-08-07; later commits may have fixed it. Doability: Medium. Retest, then remove or fix.


### Getting started and setup

#### [Wish] Explain why HiveWE will not start or accept the game folder
- Rank 13. Doability: Easy.
- The folder picker reopens forever or the program closes with no message. Reported causes: wrong game version, non-English install, missing GL_ARB_bindless_texture, a new patch, a blank white screen. Makers want a message that names the cause and a log. Evidence (at least 11 posters on opened Hive pages, 2 opened GitHub issues, titles of 2 more): https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-9 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10 ; https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11 ; https://github.com/stijnherfst/HiveWE/issues/149. HiveWE today: not checked in this fork; the GitHub reports are from upstream builds. Doability: Easy. Check version, locale and GL features at startup and report them. Many older reports are version mismatches.

#### [Wish] Support running HiveWE on Linux
- Rank 72. Doability: Medium.
- A user already ported HiveWE to Linux; base WE has no Mac version. Evidence (4 GitHub issues, 1 with a port; Blizzard support page): https://github.com/stijnherfst/HiveWE/issues/146 ; https://us.support.blizzard.com/en/article/127828. HiveWE today: Windows-first per build policy; PR status not checked. Doability: Medium for Linux; Hard for Mac (OpenGL 4.5).

## Evidence for existing tickets

Not filed as new tickets. The ticket names are those already on the board.

- **3.0 format support:** Hive reports of custom abilities resetting or disappearing after save, 3.0-derived fields resetting (https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-3, https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-5; Limitteditemeater, The Nightmare Book, loktar). Zero-face geosets crash on load (https://us.forums.blizzard.com/en/warcraft3/t/models-with-empty-geosets-crash/38837, later addressed on PTR). Model paths without an extension or with a second dot stopped working (https://us.forums.blizzard.com/en/warcraft3/t/breaking-changes-in-existing-assets/38684); 3.0.1 reportedly fixed dotted names (https://wc3mapdoctor.com/guides/map-crashes-on-loading-screen/). 3.0 added Copy As Script, convert triggers to Lua, a string ID tool, a paintable minimap, a light editor, post processing, a large undo option, copied objects keeping height and GUI leak natives (https://liquipedia.net/warcraft/Patch_3.0.0).
- **GUI trigger editing:** copying triggers failed in HiveWE (esdo); corrupted trigger comments stop maps opening in the base WE (https://www.hiveworkshop.com/threads/got-world-editor-working-mostly.370410/); pasting triggers before variables blanks them and reopening clears the clipboard (4 Hive snippets, 1 Blizzard post https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396); deleting a variable in use can freeze the editor or delete whole actions (https://us.forums.blizzard.com/en/warcraft3/t/patch-300-suggestions/37928, items 113 and 472); sub-folders, multi-select, local variables and functions, downcasting and missing native strings (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396, https://us.forums.blizzard.com/en/warcraft3/t/dear-developers-can-you-look-back-at-some-minor-issues/39059, GitHub issue 117 (no events and actions can be added)); user GUI function packs and plugins (UMSWE, YDWE, Better Triggers; https://thelazzoro.github.io/BetterTriggersGuide); converting a map between Jass and Lua (War3Net https://github.com/Drake53/War3Net); GUI access to frame natives (https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10); GUI leak natives; InsaneMonster's call to document every GUI element and an API browser (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683). Use lazy, searchable pickers from the start.
- **Variable types:** no new evidence beyond picker speed (see the picker ticket).
- **Script gameplay data:** zeroed per-level fields on SLK maps (https://us.forums.blizzard.com/en/warcraft3/t/patch-300-crashes-custom-maps-that-use-slk-data/38840); gold costs warped (https://us.forums.blizzard.com/en/warcraft3/t/pre-30-custom-map-issues-with-the-30-world-editor/39317). See the version-change ticket.
- **Minimap and shadow regeneration, Game Interface, Techtree, Ability, Upgrade, Ally and Random Group dialogs:** no new evidence found.
- **Selection and terrain tools:** doodad height lost on paste, Replace Doodad keeps old scale, no numeric position and rotation (7 Hive posters since 2009: https://www.hiveworkshop.com/threads/terrain-editor-features.133010/, https://www.hiveworkshop.com/threads/terrain-editor-features.133010/page-2, https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-2); paste terrain relative to existing height and copy across layers (OutsiderXE https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396, Blizzcon list https://www.hiveworkshop.com/threads/editor-feature-request-bug-fix-list-for-patch-during-after-blizzcon.373753/); import terrain from another map with its dependencies, with the 16/64 tile cap and ID remapping making it Medium to Hard (https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-2, https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-9, https://www.hiveworkshop.com/threads/world-editor-is-a-mess.319963/); copy and paste across palettes places the wrong type or duplicates palette windows (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-duplicating-pallet-window/36514); mirror paste.
- **Camera palette:** in-game camera previews (Squidle, 2009); cinematic cameras drifting between patches (raypack, Retera, https://www.hiveworkshop.com/threads/it-never-cease-to-amaze-me.370916/). A keyframe cinematic timeline has no direct demand found.
- **View toggles:** region tools (hide, diamond shapes, combine or subtract), SINGLE-SOURCE (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396). Region shapes are probably limited by the format (from memory, not checked).
- **Placed object properties and items:** double-clicking a placed random neutral unit crashes the base WE (Deliverance, https://www.hiveworkshop.com/threads/warcraft-iii-3-0-bugs-issues.374131/page-4).
- **Map Info windows:** changing the description in 3.0.1 clears load screen player names (AtivEnergy, one poster, disputed by Dr Super Good); the `info.lua` toggle sits there.
- **Export Strings, Script, Minimap:** reference war3map.wts and StringList keys in any string field, translate war3mapMisc.txt, stop upgrade effect fields leaking into WTS (https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://www.hiveworkshop.com/threads/patchwork-mapconverter.350385/); a read-only view of the generated script, SINGLE-SOURCE (AtheosTG https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683); Berr's request for a plain w3x export (https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-11); export already exists, see the Export MPQ ticket.
- **Dialogue, Sound Set, Object Group editors:** the Sound Set editor has the strongest demand here: Kyrbi0, Spellbound, Nichilus, loktar, apsyll, Asuna Tichondrius on Hive; Mautrix, AtheosTG, andreasasp2 (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683) and Flinn (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180) on Blizzard, from 2019 to 2025. Makers want to create a set rather than overwrite one, and Nichilus wants custom sets listed as User defined. Whether the game loads new set names from a map is unchecked.
- **Object Editor Reset Field, Auto Fill, Raw Data:** upstream issue 125 is still open; see also the lock fields ticket.
- **AI and Campaign Editors:** Test Map ignores campaign assets (Chaosium, 2 Hive posts: https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/); shared campaign triggers, opening other missions after a failed save, custom objects not detected in campaigns which allows duplicate IDs (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-bug-feedback/37104, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestion/34452); custom-race AI is hard and AI should read custom data without export.
- **Asset Manager import and export:** model and texture help (https://us.forums.blizzard.com/en/warcraft3/t/multiple-selection-for-asset-manager-world-editor/37093, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-bug-feedback/37104: exports lose extensions, slider resets); the game file browser with an "override in this map" action (replaces CascView and Ladik's MPQ Editor for the common case; Medium, CascLib present); sound extraction reported broken for default FLAC sounds, disputed (https://us.forums.blizzard.com/en/warcraft3/t/world-editor-bug-feedback/37104, one thread); explaining SD/HD/DE import fallback.
- **Object Manager and cross reference:** opening any tab beyond Map Summary crashes the base WE (Eaglendia, https://www.hiveworkshop.com/threads/minimizing-world-editor-loading-times.365479/); search across triggers and unused-variable finding.
- **Sound Editor:** see FLAC note under Asset Manager.
- **Effect Placer idea:** a third-party Intuitive Effect Designer exists (https://www.hiveworkshop.com/threads/intuitive-effect-designer-v1-1-0.328372), which suggests demand; check its features first. Demand is inferred, SINGLE-SOURCE.

## Not possible from an editor

No tickets. An editor cannot change what the game does.

| Wish | Source |
|---|---|
| Inventory above 6, more than 5 hero abilities, more command buttons, more than 4 item abilities, squad selection above 12 | https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180, https://us.forums.blizzard.com/en/warcraft3/t/suggestions-for-the-world-editor/30794 |
| More than 24 players, speed cap above 522 | https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180 |
| Maps above 480x480, seamless map transitions, map file above 500 MB, saving above about 3.5 GB | https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180, https://www.hiveworkshop.com/threads/world-editor-crash-on-non-folder-save-or-map-test-launch.372846/ |
| More than 2 cliff types rendered, higher terrain elevation, overhanging cliffs and caves, SC2-style texture blending | https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://www.hiveworkshop.com/threads/hivewe-world-editor-0-10.303110/page-10, https://www.hiveworkshop.com/threads/terrain-editor-features.133010/page-2 |
| New natives (buff API, attack pipeline, ability events, mouse, keys, missiles), template strings, longer JASS lines | https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-upgrade/36180, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-feedback/9683 |
| Custom currencies, spell resource costs, charges on any ability, custom races beyond lobby edits | https://www.hiveworkshop.com/threads/world-editor-limitations-removal.359998/, https://www.hiveworkshop.com/threads/editor-3-0-feature-wishlist.374310/, https://us.forums.blizzard.com/en/warcraft3/t/suggestions-for-the-world-editor/30794, https://us.forums.blizzard.com/en/warcraft3/t/world-editor-suggestions-by-avid-campaign-maker/1396 |
| OGG audio, shared asset folder across maps, in-game crash stack traces with variable values | https://www.hiveworkshop.com/threads/feedback-request-features-for-jass-and-the-world-editor.308099/page-10, https://www.hiveworkshop.com/threads/world-editor-crash-on-non-folder-save-or-map-test-launch.372846/ |
| Pathing blockers that do not block vision, XYZ destructible scale, elevation radius on all doodads | https://www.hiveworkshop.com/threads/editor-feature-request-bug-fix-list-for-patch-during-after-blizzcon.373753/ (snippet) |
| Offline Test Map without login and internet (client enforced since 3.0) | https://us.forums.blizzard.com/en/warcraft3/t/having-to-login-to-test-maps-is-an-insult-to-map-makers/39625, 5 more Blizzard threads and one Hive thread |
| Saving down to old formats | Upstream HiveWE decision, issues 78 and 115 (not re-read) |
| Real map encryption (the game must read the map) | https://us.forums.blizzard.com/en/warcraft3/t/suggestion-we-need-stronger-native-map-protection-and-encr/38213 |
| Fixing editor UI decay over long sessions in the base WE | https://www.hiveworkshop.com/threads/any-way-to-fix-world-editor-ui-bugging-out-after-enough-extended-use.375796/; only relevant to HiveWE if seen there |

## Not filed (low evidence or no demand)

- Planning image overlay (SINGLE-SOURCE, OutsiderXE), release mouse input outside the window (SINGLE-SOURCE, TimeKiller), explain folder maps to new users (SINGLE-SOURCE, Verdun), game debug output panel in Test Map (tool-only evidence, Hard), compare two map versions (no demand found, Hard), SLK-style object data optimization (low confidence it is useful on 3.0, Hard), doodad mesh release and model optimizer (code TODOs only, not checked), embedding Warsmash, WFE-style hooks, Map Adapter downgrades, a full mesh and animation model editor (Retera's tool is active; link out), live co-editing of maps, a custom faction editor (unclear what it would edit; engine limits custom races).
- Quiet filterable load-error log (popcornfx error floods): base WE only per the evidence; HiveWE status not checked.
- Already answered in the fork per the research: Tooltip colouring, global search, pathing palette, dark panels, custom object IDs when creating an object (`object_editor.cpp:1179-1198`), Map Protector as the answer to protection requests, object conflict preview. Not tested.
- Upstream issues fixed and to be checked in the fork: 120 (doodad height decimals), 121 (missing ability fields), 123 (move preplaced units), 124 (ability suffixes). 135 and 136 (HD team colour and normals) are rendering work, not a user request.
- NCOW (the user's own map: Lua, no tech tree, upgrades or random groups) did not drive ranking.
