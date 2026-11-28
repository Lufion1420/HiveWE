# Session handoff

Written 2026-11-28 (session 41 of the project). Overwritten at each handoff. Format: `.claude/rules/harness-guardrails.md` "Session handoff".
- Models: main session Sonnet 5.5 as orchestrator. Subagents: cpp-dev x3 (R1 on Opus, merge fix on Opus, browser prototype on its default), reviewer x1 (Opus), qa-verifier x2, release-manager x1, docs-keeper x1, designer x1.
- Spawn count this session: 9 (resumes not counted).

## Checks waiting on the user
1. **Blocks the next step. Approve the model browser window (gate 2).** Open the PNGs in `designs/references/model-browser-gate2/`: `flat-dark-1440-fire.png`, `flat-dark-1280-fire.png`, `flat-dark-1440-compact.png` (light versions have `flat-light-` names). Reply "approved" or list changes. Defaults used unless changed: the compact list's Folder column shows the top-level folder (full path in the tooltip); visible thumbnails will animate like the old pop-up; disk thumbnails show the frame 40% into the animation, cropped.
2. VFX Sandbox, needed before stage 4, no deadline. What does the "Composition" drop-down list when setups have no fixed folder: recent files (default), or remove the drop-down?
3. VFX Sandbox, same. Dragging an effect's bar on the timeline: does its "Remove at" time move with it? Default: yes.
4. VFX Sandbox, needed in stage 2. Starting positions: default is the target in the middle and the caster 500 map units west of it.
5. Optional, 3 min: in the HiveWE of the Tools folder, Model Editor > Open > `C:\Users\mauri\Desktop\Model -Conversions WIP\Storm_VFX\fire\Madara_DragonFlame_Projectile\Madara_M09_DragonHeadProjectile.mdx`, play Stand and Death; then the same with `...\fire\Shisui_BurningKunai_Glow\Shisui_BurningKunai_Glow.mdx` (flames should now step; the glow looking split from some angles is known until R2). Do they look like WarMash? Right side of `designs/references/kunai/kunai_without_vs_with_glow.gif` is the reference; say if WarMash differs from it.
6. Optional, explains Magos only: play Stand on the kunai in Magos. Blade visible with an orange tint = Magos ignored the layer fade; solid orange = probably draws the inner shell over the blade.

Information for the user (say it once): classic flipbook effects saved by an older HiveWE (Save to MDX, Edit MDL, Doodad merge export, Map Protector when it renamed attachment or particle paths) lost their flipbook; export them again from the original.

Deferred by the user (do not relist): Asset Manager bulk-delete test, old-save script spot-check, former checks 6 to 15 of session 38, every Warcraft III load check until WarMash testing is done, and (manual testing deferred) the World Editor save that shows which type byte it writes for item Equipment/Tag and unit Occlusion.

## State
- Shipped: 4e77d3f (1220963921479981, Storm VFX R1: WarMash draw order, texture animation, flipbooks; classic MDX flipbooks no longer lost; merge export remaps them) and 681b88b (approved browser mockups). Review APPROVE, QA PASS, ctest 636/636. Deployed to the Tools folder 2026-11-28 (HiveWE was not running).
- CI: run 46093630389 on 681b88b was still in progress at handoff (configure step): check its result first. The handoff commit carries `[skip ci]` so that it does not cancel that run.
- Model browser 1220964773977292: gate 1 approved (layout A), gate 2 awaiting approval. Prototype source ONLY in the local git stash "model-browser gate-2 prototype (awaiting user approval, session 41)" (20 files; never drop it). Built OK, ctest 640/640, not reviewed or QA'd; Game and Map tabs disabled and the old pop-up already removed from the Model Editor, so not shippable. The ticket comment lists the remaining work.
- `build/Agent` holds the PROTOTYPE build: rebuild before any QA or deploy.
- VFX Sandbox 1220917410779541: both design gates passed; source ONLY in the local git stash "vfx-sandbox gate-2 prototype" (24 files; do not drop it). Its `--qa-open-dialog vfx-sandbox` name still has to go into the ui-capture README when committed. Expect merge work in `main.cpp`, `hivewe.cpp` and theme or CMake files.
- Effects session (folder "Model -Conversions WIP"): got a status reply 2026-11-28 (before R1 shipped); nothing waiting on them. Questions about effect files go straight to it.
- PC shutdown: a WarMash session said the user asked it to shut the PC down after both sessions finished (2026-11-28). Scratch files may be gone.
- Not verified: any game, WarMash or World Editor load; live playback; merge export end to end; Object Editor model grid.
- NCOW fix from session 35 still uncommitted in the user's own repos on purpose (`SasukeHebi.lua` sound path).
- Untracked `qa-*.png` and `map-protector-flat-*.png` in `designs/current/`, `designs/references/` (includes game and effect art) and stray `512684032` stay uncommitted on purpose.

## Next step
1220964773977292 model browser, once check 1 is approved: restore the stash, finish the list in the ticket comment (Standard track), review, QA. While approval is missing: 1220965115652992 (Doodad merge export crash risk), then Storm VFX R2 1220963921514005. User order: browser, R2, R3 1220963821078869, R4 1220963798776370 (packs also listed in the browser), then VFX Sandbox stage 1 1220934951315968.

## Backlog only, not next steps
1220965197328565, 1220965197313575, 1220965106820828, 1220965050077643, 1220964770507376, 1220964946930693, 1220963821845299, 1220963799571115, 1220936645897527, 1220933866874909, 1220949503069153, 1220930104098194, 1220931674071675, 1220927374069594, 1220928174960676, 1220329469211176, 1220329518098244, 1220899403091128, 1220898118343928, 1220895469636543, 1220895284225944, 1220885519534133, 1220824083919839 / 1220818407294566 / 1220828873067911 (flat UI batches, at most one per session), 1220328401966062 (needs a user decision), 1220719384135918 / 1220329290824218 (idea and wishlist parents), 1220832845348582, 1220329469370593 and 1220362579055972 (wait for WarMash), 1220841054745630 (HD only). `designs/README.md` still names the old headless Chrome path (see `agents/designer.md`).

## Decisions
User (2026-11-24): no Warcraft III launches until the user says the WarMash game tests are done.
User (2026-11-24): never ask the user to close HiveWE or assume it is open; check `Get-Process HiveWE`.
User (2026-11-24): keep working across tickets without waiting for "continue".
User (2026-11-24): redeploy to the Tools folder after every QA-PASS commit without asking; skip only if HiveWE runs and say so.
User (2026-11-24): move done tickets to "Done — shipped"; focus on Asana board tickets over small edge cases.
User (2026-11-24): hand off when context is high; offer it after a large ticket ships, before starting the next.
User (2026-11-25, twice): after a denied combined build command, chose "run steps separately". Not a standing approval: ask again if it happens.
User (2026-11-25): the WarMash script is the allowed standing method for game load checks; no retry chains. It may take window focus on announced runs.
User (2026-11-25): manual testing is deferred; map defects the agent can fix at the source are fixed, not handed back.
User (2026-11-25): keep `security-guidance` and `i-have-adhd` on; unused plugins and skills off per project.
User (2026-11-25): the full map sweep may be skipped under the three conditions in HARNESS.md.
User (2026-11-25): evaluate context size in main session and subagents in the retrospective.
User (2026-11-26): automated checks must make no sound.
User (2026-11-26): hidden or offscreen test windows never get simulated input (guardrail changed).
User (2026-11-26): remove the `.codegraph` index; stopping its HiveWE helper process was approved for that one time.
User (2026-11-26): Export MPQ after a failed script generation still exports, with a warning.
User (2026-11-26): VFX Sandbox: one merged ticket, timeline playback, all per-effect settings, custom and stock sources with an easy filter, save setups then Lua copy; layout A; Save and Open with a folder picker; caster and target models chosen by the user; per-effect end "After animation" or "Remove at".
User (2026-11-26): VFX Sandbox Qt prototype approved.
User (2026-11-27, "go"): Model Editor renderer first, map instances later; WarMash rules define "correct", checked against the preview GIFs, user's eye final; WarMash code for facts only; a texture next to the model beats same-named game or map files; particle bugs filed, not fixed.
User (2026-11-28): build a Model Editor browser for disk folders with subfolders and previews, right after R1.
User (2026-11-28): browser layout A (docked in the editor) with defaults: `_` folders hidden behind a checkbox; Stand first, one-shot effects loop, camera reframes; folder, subfolder, list style and checkboxes remembered; old pop-up leaves the Model Editor; Object Editor picker unchanged.
Orchestrator (2026-11-28): R1 was committed before the older merge-export crash risk found by QA, which is filed in "Next up" (user may overrule).
Orchestrator (2026-11-28): HD "None" layers no longer discard low alpha, because WarMash's HD shader does not either.
Orchestrator (2026-11-28): an undecodable map or game texture also shows the placeholder and a log line.
Orchestrator (2026-11-26): ribbons: no drawing in version 1. An unreadable image gets a placeholder; Definitive-only content is a separate ticket. VFX Sandbox is built in 6 stages; the menu entry appears only in the last.
Orchestrator (2026-11-26): unknown object fields are kept as raw records; known 3.0 skin fields move to the main object file; text (.ini) export still drops unknown fields (filed). `war3mapPostProcessing.txt` counts as a map file. Backslashes in string variable defaults are written literally. A Lua syntax error keeps the previous `war3map.lua`. Test Map does not launch when the script could not be updated. Trigger Explorer drag and drop stays off (filed). Export MPQ shows a success message; a read-only target is refused.
Orchestrator (2026-11-25): backups cover the files Save can change (newest 10 plus first per session and per day for 14 days), never block a save; Save As refuses a folder that holds files; a script compile failure still counts as saved (filed); the script is kept whenever wtg or wct is missing; in the Asset Manager an uncertain reference counts as used.
Orchestrator (2026-11-24): an unreadable trigger or script file refuses the map; wtg deleted-item lists are written back as read; new object ids are exactly 4 ASCII letters or digits; Open Map (MPQ) and Map Protector refuse unsafe entry names; a `Map::load` failure after the body check opens a blank template map; `--resave` exit 5 means objects were dropped (exit 3 wins).

## Retro
Session 41: 9 spawns; shipped R1 (about 1 h 30 min, 3 rounds), browser mockups and a prototype. Subagent minutes (peak tokens): cpp-dev R1 28 + 11 (380k), merge fix 8 (100k), prototype 14 (326k); reviewer 3 + 1 + 1 (166k); QA 20 + 16 (406k), final 2 (102k); designer 9 (225k); release and docs about 3. No build-lock waits reported. Causes: QA found an older reader bug (flipbooks dropped), which needed a round and then a third because the consumers that renumber textures were not in that fix brief (rule now in `hivewe-file-formats.md`). Context: main stayed small apart from about 10 PNG reads; QA passed the 400k limit, so a fresh QA ran the last round. Quality: no review, test or QA step removed. Kept: independent MDX parse in QA, review and QA in parallel, read-only mockups during implementation.

## Scratch
`C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a\946879d3-b403-4a45-a51a-66324bf8020e\scratchpad` (may vanish, likely after the shutdown; `r1`, `qa1`, `qa2`, `r3`). Earlier sessions: `...\00850cbd-7788-4d95-be52-86fb9a5cdba0\scratchpad`; NCOW map copy `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\repro\NCOW_Spell_Dev.w3x`; stock map extraction `...\c2da2c4f-fdbc-4824-8512-571bf1879b9b\scratchpad\stock`. All under `C:\Users\mauri\AppData\Local\Temp\claude\11cf287c6d13d22a`.
