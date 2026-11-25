# HiveWE agent harness

Created 2026-11-08, adapted from the Lendrill harness; reworked 2026-11-20 on the WarMash harness (which descends from this one) with the HiveWE safety rules kept. The main Claude Code session is the orchestrator: it plans, delegates, owns the gates and git, and talks to the user. Subagents do the work. Subagents cannot spawn subagents, so the orchestrator runs each stage. Rules for everyone: [`rules/harness-guardrails.md`](rules/harness-guardrails.md).

**Handoff:** `.claude/HANDOFF.md` holds the state for the next session; rules in the guardrails ("Session handoff"). **Tools:** `.claude/tools/ui-capture/` has the UIA and capture scripts the designer uses (see its README). `.claude/tools/map-diff/` compares HiveWE output with World Editor output (`files/fdiff.py` independent Python parsers plus `allow.json`, `scripts/sdiff.py` per-function Lua/JASS diff, `sim/simdiff.py` Lua native-call trace diff); `files/`, `scripts/`, `sim/` and `sweep/` each have a README; the sweep tooling lives in `sweep/` (there is no `map-diff/README.md`); `--resave` usage is in `scripts/README.md` and `src/main.cpp`. `.claude/tools/deploy-local/` deploys the Agent build to the folder the user launches HiveWE from (`D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`); the orchestrator checks `Get-Process HiveWE` itself and runs it after every shipped user-visible fix, skipping with one line when HiveWE is running; never ask the user to close HiveWE or assume it is open (user, 2026-11-20 and 2026-11-24). When the user reports a bug, check the date of that exe first (the 2026-11-20 crash came from a stale June build). The differential sweep (HiveWE resave vs World Editor original) is the default way to find save bugs, instead of manual in-game side-by-side.

**Tickets:** [Asana project "HiveWE"](https://app.asana.com/1/1213523605265041/project/1220328345553589) (GID 1220328345553589). The orchestrator owns the board. Asana MCP: `search_tasks` needs Premium, so use `get_tasks`. Tickets become subtasks with `update_tasks` (set parent, plus `remove_projects`). Subtasks come back in shuffled order, so names must not rely on list position. When a ticket is finished, add a comment (what shipped, how it was verified, what was not), mark it complete, and move it to "Done — shipped". `add_projects` takes objects `[{project_id, section_id}]`; `create_tasks` with `projects` leaves memberships empty, so call `update_tasks` with `add_projects` afterwards.

## Roster

| Agent | Job | Edits | Model |
|---|---|---|---|
| `cpp-dev` | Implements the change and tests, compiles before returning | Source, tests | Sonnet; Opus on the risky track |
| `reviewer` | Adversarial diff review, APPROVE or REJECT; never builds | Nothing | Opus; Sonnet for a re-review of a small fix-round diff |
| `qa-verifier` | Rebuilds, runs tests, writes manual UI checks; PASS, FAIL or COULD-NOT-RUN; owns `build/Agent` | Nothing | Sonnet |
| `release-manager` | Changelog, version bumps, `release.yml`, publishing releases | Changelog, version strings, release workflow | Sonnet |
| `designer` | HTML/CSS mockups plus rendered PNGs for approval, and the as-built screenshot baseline | `designs/` only | Sonnet |
| `wc3-format-specialist` | Sourced note on formats and compatibility before implementation | Nothing | Opus |
| `build-engineer` | CMake, vcpkg, overlay ports, CI, ICE and linker failures | Build files | Sonnet |
| `docs-keeper` | `.claude/rules/`, `.claude/skills/`, `.claude/plans/`, `.claude/agents/`, `.claude/HARNESS.md`, `CLAUDE.md` | Docs only | Sonnet |

Model rule (user, 2026-11-19, adopted from WarMash 2026-11-20): main session Opus 5.5; subagents Sonnet by default, Opus for `reviewer`, `wc3-format-specialist` and `cpp-dev` on the risky track. Exception (user, 2026-11-24): a `reviewer` re-review of a fix round with a small diff may run on Sonnet; the first review of a Standard or Risky task stays Opus. Never Fable, and no fallback to Fable or usage credits.

Left out of the roster: `api-chronicler` (`release-manager` owns `CHANGELOG.md`; HiveWE has no script-API docs) and a docs-site agent (HiveWE has no docs site). Add either when first needed.

Built-in `Explore` and `Plan` handle scoping when ownership is unknown; launch two `Explore` agents in parallel when both the UI and format layers are unknown. Use Codegraph (`codegraph_explore`) before wide searches; it needs `projectPath` set to the repo path, and has failed project detection for agents (one saw "not initialized"), so fall back to Grep. `hivewe.cpp` line numbers drift quickly: cite symbol names in tickets and briefs. Run the `security-review` skill only when the user asks (MPQ export, external `QProcess`, file I/O on user paths). Add `security-auditor` (untrusted map parsing, `QProcess`, Map Protector) and `perf-auditor` (Tracy, render loop, load times) when first needed.

## Pipeline

| Track | Use for | Stages |
|---|---|---|
| Small | Tooltip, label, one-line fix | `cpp-dev`, `qa-verifier`, `release-manager` |
| Standard | Most features and bugs | `cpp-dev`, then `reviewer` and `qa-verifier` in parallel, `release-manager` |
| Risky | Save or format code, threading, map reload, Map Protector | `wc3-format-specialist`, then Standard with `cpp-dev` on Opus |

Parallel review and QA: as soon as `cpp-dev` returns, launch `reviewer` and `qa-verifier` together. Both are read-only on the diff; QA owns `build/Agent`, the reviewer never builds.

QA tiers: the orchestrator names the tier in each QA brief.
- **Smoke:** `ctest --preset Agent` plus one `data/test map` save. For small changes and every fix round.
- **Build mode (name it in every QA brief):** `reuse` or `rebuild`. `reuse` is allowed only when `cpp-dev`'s report states the exact build and `ctest` result on the current tree and the staleness check in `agents/qa-verifier.md` prints no file; QA then skips build and `ctest` and runs only the targeted offscreen checks and hooks. Anything else is `rebuild`. The lock file, one-build-at-a-time and offscreen rules apply unchanged (user, 2026-11-24).
- **Targeted:** map-diff on a map subset for the changed writer.
- **Full:** the 237-map map-diff sweep, once per task on the final round, and always when a writer or the script generator changed.

Pipelining: read-only prep for task N+1 (`wc3-format-specialist`, `Explore`, `Plan`) may run during review and QA of task N. Never two builds at once.

Add-ons on any track:
- **New window or changed layout:** run `designer` first. Stop at `AWAITING_APPROVAL` until the user approves the rendered PNGs (gate 1). The first `cpp-dev` step is then a real Qt prototype screenshotted with `ui-capture`, which the user approves (gate 2) before the rest of the UI work. After shipping, run `designer` again to refresh the screenshots in `designs/current/`.
- **Durable fact or convention learned:** run `docs-keeper`.
- **Toolchain failure:** run `build-engineer`.
- **Agent definition files (`.claude/agents/*.md` must stay LF).** A CRLF rewrite made an agent unavailable for the rest of a WarMash session (the registry does not reload mid-session). `.gitattributes` pins `eol=lf`, but the working copy can still be CRLF: check `git ls-files --eol` after any edit and convert with `sed -i 's/\r$//'`. `.claude/rules/*.md` can be CRLF in the working copy too (`harness-guardrails.md` was on 2026-11-22): check them the same way.

Then the orchestrator commits and pushes, only after QA PASS (and reviewer APPROVE on Standard and Risky). Internal-only changes get no changelog entry, so `release-manager` is skipped for them. After each QA-PASS commit the orchestrator also redeploys the Tools folder (standing order, see guardrails "Git and releases").

## Choosing work
- The handoff "Next step" always names an Asana ticket from "Next up". Read "Next up" at session start and check that the named ticket is still open before starting.
- A reviewer or QA finding becomes work in the same task only if it crashes, corrupts a save, blocks the current ticket, or is visible in the user's maps (NCOW, Lua). Anything else is filed as a ticket under "Harness and quality follow-ups" and is not added to the next step. Do not chase edge cases.
- At most one leftover batch per session.
- "No eligible ticket" must be checked against the whole "Next up" list, including bug tickets such as the MDX ones, not only the tickets the handoff names.
- Sweep-driven parity work counts as leftover work (user concern, 2026-11-22): the subtasks of "World Editor parity: map output on save" (1220328838360686) and any new sweep kinds. At most one parity ticket or batch per session. The handoff "Next step" list alternates: the first item is a non-parity "Next up" ticket (a feature, a user-reported bug or a decision ticket), and a parity item may follow. Background: in sessions up to 2026-11-20 the Next step always named the next save-parity item from the sweep, while the tickets created on 2026-11-08 (UI modernization 1220328401966062, trigger variable types, save-reports-success) sat untouched. WarMash had the same drift in sessions 56-61.
- Current order is the user's decision of 2026-11-22; it is in the Decisions list of `HANDOFF.md`.
- Before every handoff the orchestrator runs the session retrospective: `.claude/rules/harness-guardrails.md` "Session retrospective".
- At session start, sweep "Next up" and "Harness and quality follow-ups" for completed tasks and move them to "Done — shipped" (section 1220328136214002). Completing a ticket is not enough (user, 2026-11-24): also add it to that section with `update_tasks` `add_projects` (project 1220328345553589); subtasks have no membership and are only completed.
- JASS-only parity waits unless a map needs it (Lua first).
- The orchestrator picks the next ticket itself without asking (orchestrator decision; the user may overrule).

## Gates
- `reviewer` REJECT goes back to `cpp-dev`, then review and QA rerun together; the QA result of the rejected round is discarded. After 3 rejections, stop and ask the user. If review rejects while QA holds `build/Agent`, wait for QA before resuming `cpp-dev`.
- `qa-verifier` FAIL goes back to `cpp-dev`, then review and QA again. COULD-NOT-RUN is not a pass.
- **Trivial fix round** (user-approved rule, 2026-11-24): a fix round whose diff touches only tests, comments, docs or `CHANGELOG.md`, with no change to any non-test source line that compiles into the app (code, CMake, `.ui`, shaders, data files), after `cpp-dev` already ran `ctest` green on that tree. It needs no new `qa-verifier` run and no new full review: the orchestrator may commit once `reviewer` confirms the diff is trivial (a short check on Sonnet), or without review when only docs or changelog changed. Any change to non-test source behaviour, including a "tiny" one, still needs review and QA as above. Edits that change test expectations to make a failing test pass are not trivial.
- Fix rounds resume the same `cpp-dev`, `reviewer` and `qa-verifier` with SendMessage (keeps context). Use a fresh agent once the old one is above about 400k tokens (`cpp-dev`: about 350k).
- A QA FAIL can be a wrong expectation in the brief: check the expectation against the code or specialist note before a fix round. When a brief contains a switch default, check it against the specialist note line by line.
- `cpp-dev` time cap (user, 2026-11-24): an open-ended investigation (bisecting a leak, chasing an unexplained bug) stops after about 20 minutes of work or two failed hypotheses and returns `NEEDS_DECISION:` with the ruled-out list and a recommended fallback.
- Unverified WE/WC3 behaviour goes in one switchable function plus a pinning test.
- "Premise did not reproduce" from `cpp-dev` is accepted only with tests that pin the current behaviour.
- For a "still broken" report, check with `git merge-base --is-ancestor <fix> <user's build commit>` whether the user's build had the fix.
- Usage limit or outage: a 429 or 403 means resume the same agent after the reset or re-login. When the user says the limit is near, start no new subagents.
- QA tries every manual check itself first, with the ui-capture method, including finding test data and making edits (user request, 2026-11-08). QA tries UIA first (ValuePattern SetValue, IsEnabled). What the no-SendInput rule blocks (brush, copy/paste/undo, in-session edits) goes back to `cpp-dev` as an in-process test or hook, not to the user (2026-11-09). Loading in the real game is checked by launching Warcraft III 3.0.1 or its World Editor on sandbox map copies (never save, never real maps; the orchestrator warns the user in chat first). Before handing anything over, the orchestrator sorts checks into automatable, blocked by a rule, and truly user-only; only the last group goes to the user as a short list with exact inputs named. The change is described as unverified until they confirm.
- `qa-verifier` `NEEDS_HOOK` goes to `cpp-dev` to add an in-process test for that check, then QA reruns. It is not a pass.
- Brief rules: game-load briefs name `.claude/tools/ui-capture/README.md` and the WarMash method up front (a brief without them cost a COULD-NOT-RUN on 2026-11-24); exception-class (path/encoding) fix briefs say "grep all callers/the whole dialog chain"; string/field storage briefs say "grep every place the field is emitted into script/save text" (a missed Lua-script writer cost a REJECT round on 2026-11-24); cpp-dev briefs ask for a RED run of the new test on the old code when cheap; briefs for GUI behaviour changes ask cpp-dev for first-round in-process coverage of every dialog path, including clicking real modal dialogs and the accepted path (a missed one cost a NEEDS_HOOK round on 2026-11-24), and name window-rectangle sampling for offscreen runs.
- Stop and ask on `NEEDS_DECISION` or `NEEDS_GAME_TEST`.
- Keep the orchestrator's context small: subagents report under 25 lines and the orchestrator does not read build logs.

## Limits
- The orchestrator-only rule lives in `CLAUDE.md` as an instruction; nothing mechanically enforces it. The orchestrator still uses read-only lookups, codegraph and git.
- `.claude/settings.json` mechanically denies Read and Grep on `.pen` (archived Pencil files), configuring or building any auto-running preset (`Release`, `ReleaseTracing`, `Debug`, `RelWithDebInfo`: `HIVEWE_AUTO_RUN_AFTER_BUILD` defaults to ON, only `Agent` sets it OFF), pushing to `upstream`, force, mirror, `+refspec` and `:`-delete pushes, deleting `main` on origin, changing repo visibility, `gh auth switch`, and killing `HiveWE`, Warcraft III or `code` by image name. `--force-with-lease` is on "ask". Deny patterns match the whole command line, so a commit message containing the literal text of such a push in the same command is blocked too: commit and push in separate commands.
- Lendrill's push-guard hook was not ported: it was disabled there for causing too many prompts.
- Only one build and one designer can run at a time, so development runs sequentially.
- No automated UI testing exists. A headless `data/test map` load/save round-trip test would be the biggest verification upgrade.

## Sources

What each agent's process drew on. `wc3-format-specialist` and `designer` are essentially ours: nothing suitable exists for WC3 formats or Qt-faithful mockups. Collections such as wshobson/agents and VoltAgent/awesome-claude-code-subagents were rejected as generic personas for the wrong toolchain.

- `reviewer`: Qt's [qt-cpp-review rules](https://github.com/TheQtCompanyRnD/agent-skills) (vendored, BSD-3, in `references/qt-review-rules.md`), [chrxh/alien reviewer](https://github.com/chrxh/alien/blob/HEAD/.claude/agents/reviewer.md).
- `cpp-dev`: obra/superpowers [implementer prompt](https://github.com/obra/superpowers/blob/HEAD/skills/subagent-driven-development/implementer-prompt.md) and test-driven-development.
- `qa-verifier`: superpowers [verification-before-completion](https://github.com/obra/superpowers/blob/HEAD/skills/verification-before-completion/SKILL.md), [Jinja2Cpp verifier](https://github.com/jinja2cpp/Jinja2Cpp/blob/HEAD/.claude/agents/verifier.md) (ideas only, MPL).
- `release-manager`: public release-manager agents (Donchitos Claude-Code-Game-Studios, mampfes, thedevelobear) for the failure path.
- `docs-keeper`: Anthropic [claude-md-improver update guidelines](https://github.com/anthropics/claude-plugins-official/blob/HEAD/plugins/claude-md-management/skills/claude-md-improver/references/update-guidelines.md), superpowers writing-skills.
- `build-engineer`: [Yuzu build-ci](https://github.com/DevNullLtd/Yuzu) and Jinja2Cpp ci-triage failure classes (ideas only; AGPL and MPL).
- `wc3-format-specialist`: reference list in the agent file (WC3MapSpecification, War3Net, mdx-m3-viewer, StormLib, WC3MapTranslator). wenbinio/wc3 has no license and was not used.
- `designer`: our own process (HTML/CSS mockups limited to QSS-expressible CSS, headless Chromium render); Pencil was used until 2026-11-22.
