# Session handoff

Written 2026-11-08. Overwritten at each handoff.

## State
- Branch `main`, pushed to `origin`, tree clean. No HiveWE process left running by the session. `HKCU\Software\HiveWE` matched its original state at the last check.
- Finished: "Compare UI baselines to the running app", "Research newest Warcraft III patch and HiveWE compatibility", "Audit feature parity with the base World Editor". All are in Done on the Asana board.
- Nothing is in progress.

## Main findings
- **3.0 maps crash HiveWE.** Newest patch is 3.0.1 build 24371. HiveWE tracks formats through 2.0.3a. Stock map (1)ForgottenHollow (w3i v39) kills the process about 2 seconds after launch (0xc0000409); an unmodified test map loads fine. Cause not isolated. Details: `.claude/plans/wc3-3.0-compatibility.md`.
- **Parity:** 141 features: 26 complete, 48 partial, 66 missing, 1 unknown, by code reading. Details: `.claude/plans/world-editor-parity.md`.
- **UI baselines** in `designs/current/` now match captures where reachable. Unreached frames are marked UNCERTAIN in their frame names.

## Next step
Start `[3.0] Refuse unknown map format versions instead of parsing on`, a subtask of "Support Warcraft III 3.0 maps, models and data". Begin by pinning the crash cause.

Sample map: not in the repo. Extract `war3.w3mod:maps/forsakenkingdom/scenario/(1)forgottenhollow.w3x` from `C:/Program Files (x86)/Warcraft III` with the repo's CascLib, then unpack it with StormLib. Use the path form `dir/:w3`. To test, replace `data/test map/` in a sandbox copy of `data/`, as `.claude/skills/hivewe-build-and-debug/SKILL.md` describes.

## Decisions the user made
- NCOW uses no tech tree, upgrades, random groups, custom colours or waygates, so `[Parity 1]` is not urgent for it.
- Related tickets are grouped under parent tickets.
- Standing rules: see "Session handoff" and "Self-updating guidance" in `.claude/rules/harness-guardrails.md`.

## Open questions
- Port upstream's 3.0 parser fixes or write them ourselves? The research lists upstream defects (`.claude/plans/wc3-3.0-compatibility.md`, Upstream HiveWE), so do not copy blindly. Recommended: write them against the verified layouts, using upstream as a reference.
- User checks are waiting in two tickets: "User checks in the real 3.0.1 World Editor" and "User hand-checks of the UI baselines".
