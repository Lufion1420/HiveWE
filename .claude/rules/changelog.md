---
description: How and when to update CHANGELOG.md
---

# Changelog maintenance

**Always update [`CHANGELOG.md`](CHANGELOG.md) when completing a feature or bug fix**, before committing or at the end of a task. Do not wait for a release to update it.

Rules:
- All new work goes under the `## [Unreleased]` section at the top.
- Use `### New Features` for additions and `### Bug Fixes` for fixes. Use sub-headings (e.g. `#### Map Protector`) when grouping multiple entries under a feature area.
- Each entry should be one or two sentences: what changed and why it matters to the user. Avoid internal implementation details unless they affect behavior.
- When a release is tagged, rename `## [Unreleased]` to `## vX.YZ` and open a fresh `## [Unreleased]` section above it.
- Do not touch entries for already-released versions.
