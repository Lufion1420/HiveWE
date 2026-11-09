"""Field-by-field diff of two extracted Warcraft III map folders.

    python fdiff.py <orig_dir> <new_dir> [--allow allow.json] [--json out.json]
                    [--triggerdata TriggerData.txt] [--max-diffs N] [--quiet]

Every war3map* file in either folder (recursive, so _Locales/*.w3mod/war3map.wts counts) is
parsed with the independent readers in w3parsers.py and the decoded trees are compared.
Per-file status: identical, allowed (only allow-listed differences), differs, parse_failure,
missing (only in orig), extra (only in new), unparsed_identical / unparsed_differs (no parser,
bytes compared). Exit code 0 when nothing is differs/parse_failure/missing/extra, else 1.
"""
import argparse
import fnmatch
import json
import math
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w3parsers as W  # noqa: E402

ABSENT = "<absent>"


# --------------------------------------------------------------------------------------- diff engine


def _same_scalar(a, b):
    if isinstance(a, float) and isinstance(b, float):
        # Bit-exact, so 0.0 vs -0.0 counts; NaN equals NaN.
        return struct.pack("<f", a) == struct.pack("<f", b) or (math.isnan(a) and math.isnan(b))
    return type(a) is type(b) and a == b


def _show(v):
    if isinstance(v, float):
        return repr(v) if math.isfinite(v) else str(v)
    if isinstance(v, str) and len(v) > 160:
        return v[:160] + f"...<{len(v)} chars>"
    if isinstance(v, W.Recs):
        return {"records": len(v.items)}
    if isinstance(v, W.Blob):
        return {"bytes": len(v.data)}
    if isinstance(v, dict):
        return {k: _show(x) for k, x in v.items()}
    if isinstance(v, list):
        if len(v) > 12:
            return [_show(x) for x in v[:12]] + [f"...<{len(v)} items>"]
        return [_show(x) for x in v]
    if isinstance(v, tuple):
        return [_show(x) for x in v]
    return v


def _keystr(k):
    # NUL bytes in ids (an empty new_id is four NULs) are dropped from the displayed path.
    if isinstance(k, tuple):
        return ",".join(str(x).replace("\x00", "") for x in k)
    return str(k).replace("\x00", "")


class Differ:
    def __init__(self, max_diffs):
        self.diffs = []
        self.total = 0
        self.max = max_diffs

    def add(self, path, a, b, ctx_a, ctx_b, extra=None):
        self.total += 1
        d = {"path": path, "orig": _show(a), "new": _show(b), "_ctx": (ctx_a, ctx_b, a, b)}
        if extra:
            d.update(extra)
        self.diffs.append(d)

    def walk(self, a, b, path="", ctx_a=None, ctx_b=None):
        if isinstance(a, W.Recs) and isinstance(b, W.Recs):
            return self._recs(a, b, path)
        if isinstance(a, W.Blob) and isinstance(b, W.Blob):
            return self._blob(a, b, path, ctx_a, ctx_b)
        if isinstance(a, dict) and isinstance(b, dict):
            for k in list(a.keys()) + [k for k in b if k not in a]:
                p = f"{path}.{k}" if path else k
                if k not in a:
                    self.add(p, ABSENT, b[k], a, b)
                elif k not in b:
                    self.add(p, a[k], ABSENT, a, b)
                else:
                    self.walk(a[k], b[k], p, a, b)
            return
        if isinstance(a, list) and isinstance(b, list):
            if all(not isinstance(x, (dict, list, W.Recs)) for x in a + b) and (len(a) > 16 or len(b) > 16):
                bad = [i for i in range(min(len(a), len(b))) if not _same_scalar(a[i], b[i])]
                if bad or len(a) != len(b):
                    self.add(path, f"<{len(a)} items>", f"<{len(b)} items>", ctx_a, ctx_b,
                             {"differing_indices": len(bad), "first": [
                                 {"i": i, "orig": _show(a[i]), "new": _show(b[i])} for i in bad[:10]]})
                return
            for i in range(max(len(a), len(b))):
                p = f"{path}[{i}]"
                if i >= len(a):
                    self.add(p, ABSENT, b[i], ctx_a, ctx_b)
                elif i >= len(b):
                    self.add(p, a[i], ABSENT, ctx_a, ctx_b)
                else:
                    self.walk(a[i], b[i], p, ctx_a, ctx_b)
            return
        if isinstance(a, (dict, list, W.Recs, W.Blob)) or isinstance(b, (dict, list, W.Recs, W.Blob)):
            self.add(path, a, b, ctx_a, ctx_b)
            return
        if not _same_scalar(a, b):
            extra = None
            if isinstance(a, str) and isinstance(b, str) and (len(a) > 160 or len(b) > 160):
                n = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
                extra = {"first_difference_at": n, "orig_near": a[max(0, n - 40):n + 80],
                         "new_near": b[max(0, n - 40):n + 80]}
            self.add(path, a, b, ctx_a, ctx_b, extra)

    @staticmethod
    def _index(recs):
        seen = {}
        out = []
        for k, r in recs.items:
            n = seen.get(k, 0)
            seen[k] = n + 1
            out.append(((k, n), r))
        return out

    def _recs(self, a, b, path):
        ia, ib = self._index(a), self._index(b)
        da, db = dict(ia), dict(ib)
        for k, r in ia:
            p = f"{path}[{_keystr(k[0])}]" + (f"#{k[1]}" if k[1] else "")
            if k not in db:
                self.add(p, r, ABSENT, r, None, {"record": "removed"})
            else:
                self.walk(r, db[k], p, r, db[k])
        for k, r in ib:
            if k not in da:
                p = f"{path}[{_keystr(k[0])}]" + (f"#{k[1]}" if k[1] else "")
                self.add(p, ABSENT, r, None, r, {"record": "added"})
        common_a = [k for k, _ in ia if k in db]
        common_b = [k for k, _ in ib if k in da]
        if common_a != common_b:
            moved = sum(1 for x, y in zip(common_a, common_b) if x != y)
            self.add(f"{path}.__order__", f"<{len(common_a)} common records>", f"<{moved} positions differ>",
                     None, None, {"first_orig": [_keystr(k[0]) for k in common_a[:8]],
                                  "first_new": [_keystr(k[0]) for k in common_b[:8]]})

    def _blob(self, a, b, path, ctx_a, ctx_b):
        if a.data == b.data:
            return
        s = a.stride
        n = min(len(a.data), len(b.data)) // s
        bad = [i for i in range(n) if a.data[i * s:(i + 1) * s] != b.data[i * s:(i + 1) * s]]
        self.add(path, f"<{len(a.data)} bytes>", f"<{len(b.data)} bytes>", ctx_a, ctx_b,
                 {"record_size": s, "differing_records": len(bad),
                  "first": [{"i": i, "orig": a.data[i * s:(i + 1) * s].hex(), "new": b.data[i * s:(i + 1) * s].hex()}
                            for i in bad[:10]]})


# --------------------------------------------------------------------------------------- allow-list

PREDICATES = {}


def predicate(name):
    def deco(fn):
        PREDICATES[name] = fn
        return fn
    return deco


@predicate("doodad_not_fixed_z")
def _p_doodad(diff, trees):
    ca, cb = diff["_ctx"][0], diff["_ctx"][1]
    return all(c is not None and not (c.get("flags", 0) & 0x04) for c in (ca, cb))


@predicate("units_subversion_9_to_11")
def _p_units(diff, trees):
    ta, tb = trees
    try:
        if not (ta["header"]["subversion"] == 9 and tb["header"]["subversion"] == 11):
            return False
    except (KeyError, TypeError):
        return False
    if diff["path"] == "header.subversion":
        return True
    # Fields that only exist from subversion 11 on: absent in the original, default in the new file.
    a, b = diff["_ctx"][2], diff["_ctx"][3]
    field = diff["path"].rsplit(".", 1)[-1]
    defaults = {"item_table_pointer": -1, "strength": 0, "agility": 0, "intelligence": 0}
    return a == ABSENT and field in defaults and b == defaults[field]


def load_allow(path):
    if not path:
        return {"rules": [], "order_insensitive": [], "passthrough": []}
    with open(path, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    for r in cfg.get("rules", []):
        r["_re"] = re.compile(r["path"])
        if r.get("when") and r["when"] not in PREDICATES:
            raise SystemExit(f"allow.json: unknown predicate {r['when']!r}")
    cfg.setdefault("order_insensitive", [])
    cfg.setdefault("passthrough", [])
    return cfg


def _match_file(pattern, rel):
    rel = rel.replace("\\", "/")
    return fnmatch.fnmatch(rel.lower(), pattern.lower()) or fnmatch.fnmatch(rel.split("/")[-1].lower(), pattern.lower())


def apply_allow(rel, diffs, cfg, trees):
    order_ok = any(_match_file(p, rel) for p in cfg["order_insensitive"])
    for d in diffs:
        if order_ok and d["path"].endswith(".__order__"):
            d["allowed"] = "order_insensitive"
            continue
        for r in cfg["rules"]:
            if not _match_file(r["file"], rel) or not r["_re"].search(d["path"]):
                continue
            if r.get("when") and not PREDICATES[r["when"]](d, trees):
                continue
            d["allowed"] = r["id"]
            break


def ini_case_notes(diffs):
    """Pair a removed key with an added key that differs only in case."""
    removed = {d["path"]: d for d in diffs if d.get("record") == "removed"}
    added = {d["path"]: d for d in diffs if d.get("record") == "added"}
    notes = []
    for pa in removed:
        for pb in added:
            if pa.lower() == pb.lower() and pa != pb:
                notes.append({"orig_key": pa, "new_key": pb})
                removed[pa]["case_only_rename_to"] = pb
                added[pb]["case_only_rename_from"] = pa
    return notes


# --------------------------------------------------------------------------------------- driver


def list_files(root):
    out = {}
    for dp, _, fs in os.walk(root):
        for f in fs:
            if f.lower().startswith("war3map"):
                rel = os.path.relpath(os.path.join(dp, f), root).replace("\\", "/")
                out[rel.lower()] = rel
    return out


def read(path):
    with open(path, "rb") as f:
        return f.read()


def parse_ctx(root, files, td):
    ctx = {"triggerdata": td, "w3i": None}
    if "war3map.w3i" in files:
        try:
            ctx["w3i"] = W.parse_w3i(read(os.path.join(root, files["war3map.w3i"])))
        except W.ParseError:
            pass
    return ctx


def compare_dirs(da, dbb, cfg, td=None, max_diffs=300):
    fa, fb = list_files(da), list_files(dbb)
    ctx_a, ctx_b = parse_ctx(da, fa, td), parse_ctx(dbb, fb, td)
    results = {}
    for key in sorted(set(fa) | set(fb)):
        rel = fa.get(key) or fb.get(key)
        res = {"file": rel}
        results[rel] = res
        if key not in fb:
            res["status"] = "missing"
            continue
        if key not in fa:
            res["status"] = "extra"
            continue
        ba, bb = read(os.path.join(da, fa[key])), read(os.path.join(dbb, fb[key]))
        res["sizes"] = [len(ba), len(bb)]
        res["bytes_identical"] = ba == bb
        parser = W.parser_for(rel)
        if parser is None:
            res["status"] = "unparsed_identical" if ba == bb else "unparsed_differs"
            continue
        if parser is W.parse_wtg and td is None:
            res["status"] = "skipped"
            res["reason"] = "no TriggerData.txt (pass --triggerdata)"
            continue
        trees = []
        for side, data, ctx in (("orig", ba, ctx_a), ("new", bb, ctx_b)):
            try:
                trees.append(parser(data, ctx))
            except (W.ParseError, struct.error, IndexError, ValueError) as e:
                res.setdefault("parse_errors", {})[side] = str(e)
        if "parse_errors" in res:
            res["status"] = "parse_failure"
            continue
        for side, t in zip(("orig", "new"), trees):
            if isinstance(t, dict) and "_trailing" in t:
                res.setdefault("trailing_bytes", {})[side] = t["_trailing"]
        dif = Differ(max_diffs)
        dif.walk(trees[0], trees[1])
        diffs = dif.diffs
        if parser is W.parse_ini:
            notes = ini_case_notes(diffs)
            if notes:
                res["case_only_key_changes"] = len(notes)
        apply_allow(rel, diffs, cfg, trees)
        for d in diffs:
            d.pop("_ctx", None)
        hard = [d for d in diffs if "allowed" not in d]
        soft = [d for d in diffs if "allowed" in d]
        pt = next((p for p in cfg["passthrough"] if _match_file(p["file"], rel)), None)
        if not diffs:
            res["status"] = "identical"
            if pt:
                res["note"] = pt["note"]
        elif not hard:
            res["status"] = "allowed"
        else:
            res["status"] = "differs"
        res["differences"] = len(hard)
        res["allowed_differences"] = len(soft)
        if soft:
            by = {}
            for d in soft:
                by[d["allowed"]] = by.get(d["allowed"], 0) + 1
            res["allowed_by_rule"] = by
        res["diffs"] = hard[:max_diffs]
        if len(hard) > max_diffs:
            res["diffs_truncated"] = len(hard) - max_diffs
        res["allowed_sample"] = soft[:10]
    return results


def summary_lines(results):
    lines = []
    counts = {}
    for rel, r in results.items():
        st = r["status"]
        counts[st] = counts.get(st, 0) + 1
        if st in ("identical", "unparsed_identical"):
            continue
        tail = ""
        if st in ("differs", "allowed"):
            tail = f" {r['differences']} diffs, {r['allowed_differences']} allowed"
            if r["diffs"]:
                tops = []
                for d in r["diffs"][:3]:
                    tops.append(d["path"])
                tail += " e.g. " + "; ".join(tops)
        elif st == "parse_failure":
            tail = " " + "; ".join(f"{k}: {v}" for k, v in r["parse_errors"].items())
        elif st == "skipped":
            tail = " " + r["reason"]
        lines.append(f"  {st:<18} {rel}{tail}")
    head = "  ".join(f"{k}={v}" for k, v in sorted(counts.items()))
    return [head] + lines


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("orig")
    ap.add_argument("new")
    ap.add_argument("--allow")
    ap.add_argument("--json", help="write the full JSON report here (default: stdout after the summary)")
    ap.add_argument("--triggerdata", default=os.environ.get("HIVEWE_TRIGGERDATA"))
    ap.add_argument("--max-diffs", type=int, default=300)
    ap.add_argument("--quiet", action="store_true", help="summary only, no JSON on stdout")
    args = ap.parse_args(argv)
    cfg = load_allow(args.allow)
    td = W.TriggerData(args.triggerdata) if args.triggerdata else None
    results = compare_dirs(args.orig, args.new, cfg, td, args.max_diffs)
    for ln in summary_lines(results):
        print(ln)
    report = {"orig": args.orig, "new": args.new, "files": results}
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1, ensure_ascii=False, default=str)
    elif not args.quiet:
        print(json.dumps(report, indent=1, ensure_ascii=False, default=str))
    bad = {"differs", "parse_failure", "missing", "extra"}
    return 1 if any(r["status"] in bad for r in results.values()) else 0


if __name__ == "__main__":
    sys.exit(main())
