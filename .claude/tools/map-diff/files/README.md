# map-diff/files: field-level diff of war3map files

Part of the differential sweep: HiveWE loads a stock 3.0.1 map and saves a copy, and `fdiff.py`
compares every war3map file of the copy with the World Editor original, field by field.

The readers in `w3parsers.py` are **independent Python**. They were written from the public format
descriptions (WC3MapSpecification, War3Net) and the version notes in
`.claude/rules/hivewe-file-formats.md`. HiveWE's C++ was read only to learn which versions and
fields exist. They do not re-derive z, clamp doodad flags, split units from items or drop unknown
fields, so they see what HiveWE changes instead of agreeing with it.

## Usage

```
python -I fdiff.py <orig_dir> <new_dir> [--allow allow.json] [--json report.json]
                   [--triggerdata TriggerData.txt] [--max-diffs 300] [--quiet]
python -I selfcheck.py <stock_root> [--triggerdata TriggerData.txt] [--limit N] [--timeout 120] [--out x.json]
```

- The inputs are extracted map folders. Every file whose name starts with `war3map` is compared,
  recursively, so `_Locales/*.w3mod/war3map.wts` is included.
- Without `--json`, the JSON report goes to stdout after the text summary. `--quiet` prints only
  the summary.
- `--triggerdata` (or the `HIVEWE_TRIGGERDATA` env var) is needed for `war3map.wtg`. Without it
  the wtg file is `skipped`. The stock 3.0.1 copy lives at `<stock_root>/TriggerData.txt`.
- Exit code: 0 when no file is `differs`, `parse_failure`, `missing` or `extra`, otherwise 1.
- `selfcheck.py` compares each map folder with itself, one subprocess per map with a timeout. A
  correct parser gives `identical` everywhere. It also lists files that have trailing bytes.

## Per-file status

| status | meaning |
|---|---|
| identical | decoded trees are equal (bit-exact floats) |
| allowed | only allow-listed differences |
| differs | at least one difference that is not allow-listed |
| parse_failure | a reader failed on one side (`parse_errors` says where) |
| missing / extra | only in orig / only in new |
| unparsed_identical / unparsed_differs | no reader (e.g. `war3map.lua`, `.w3l`, `.w3grp`); bytes compared |
| skipped | wtg without `--triggerdata` |

Each diff has `path`, `orig` and `new`, and some have `record: added/removed`. Lists of records
are matched by key, not by position: units and doodads by creation number, imports by
case-folded path, object data by (base, new) id and modifications by (field, level, data pointer),
sounds by name, regions by creation number, players by number, wts strings by number, ini keys by
exact (case-sensitive) name, and wct texts by first line. A change in order is reported once, as
`<list>.__order__`. Keys that differ only in case in ini files are paired
(`case_only_rename_to/_from`). Large scalar lists, w3e corners, wpm and shd are reported as
counts with the first 10 differing indices.

## Allow-list (`allow.json`)

- `order_insensitive`: file globs whose `__order__` diffs are allowed (records and
  modifications in a different order).
- `rules`: `{id, file, path, when?, reason}`. `file` is a glob on the relative path or file name,
  `path` is a regex on the diff path, and `when` names a predicate in `fdiff.py`:
  - `unit-z`: unit z (HiveWE re-derives z from terrain on load).
  - `doodad-z-not-fixed` (`doodad_not_fixed_z`): doodad z when neither side has flag 0x04.
  - `units-subversion-9-to-11` (`units_subversion_9_to_11`): header subversion 9 to 11, plus the
    fields that appear only with subversion 11 (item_table_pointer -1, str/agi/int 0).
- `passthrough`: notes attached to identical files that HiveWE copies unchanged. `war3map.w3c` is
  never written (`GameCameras::save` is empty), so an identical result there proves nothing.

Allowed diffs stay in the report (`allowed_by_rule`, `allowed_sample`).

## Coverage

Parsed: w3i (v18, v25 to v33, v39 with the editor-6140 hud_skin gate), w3s (v1 to v3), doo (v7, v8,
v12, v13 incl. lights and special doodads), war3mapUnits.doo (v7/8/12/13, sub 9/11), imp, wts
(incl. `_Locales`), w3u/w3t/w3b/w3h and w3a/w3d/w3q plus all `war3mapSkin.*` (v1 to v3), w3r
(v5, v7), w3c (v0, v3; the 1.31 local rotation is chosen from the w3i game version), w3e (v11,
v12), wpm, shd, mmp, war3mapMisc/Skin/Extra.txt (ini), wtg (1.31+ format 0x80000004, sub 4
and 7) and wct (1.31+ format and the classic formats).

Not parsed: the pre-1.31 wtg format (no stock 3.0.1 map uses it), war3map.j/lua (use
`../../script-check/`), w3l, w3grp, war3mapPostProcessing.txt. These are compared as bytes.

The doodad and unit skin-id field is present when the w3i game version is 1.32 or later. Without
a w3i the reader tries both layouts and keeps the one that ends exactly at EOF.

## Known gaps and notes

- wtg parameter counts come from TriggerData.txt by position (E/C/A: fields after the first;
  calls: fields after the third; `nothing` excluded). A map that needs a different TriggerData
  gives a parse_failure that names the function.
- wtg header: after the seven deleted-id lists, 464 stock files have `0, 0, 2`. human05 and
  undead02 (reforged roc) have `1, 1, 0, 2`. Both are read as a counted int list
  (`unknown_list`), an unknown int, and the variable format version, and then parse exactly to EOF.
  What the list means is unknown (Needs verification).
- Floats compare bit-exact, so 0.0 and -0.0 differ, and x/y precision lost through the terrain
  offset shows up (expected in HiveWE output and not allow-listed).
- Object-data `end` tokens (the WE writes the object id, HiveWE writes 0) are reported as diffs.
