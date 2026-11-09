"""Diff two simulator logs (World Editor original vs HiveWE-saved).

usage: tdiff.py <orig.log> <new.log> [--json out.json] [--top 25]
Calls are aligned per context (innermost map function). Reports missing (only in orig), extra (only in new) and
changed calls (same native, different arguments), plus warnings/errors that appear only in the new log.
"""
import argparse, collections, difflib, json, re, sys

LINE = re.compile(r"\bline \d+|war3map\.lua:\d+|\[stack [\d<]+\]")
NAME = re.compile(r"^(\w+)\(")


def load(path):
    calls = collections.OrderedDict(); warns = collections.OrderedDict(); errs = collections.OrderedDict()
    alias = {}; info = []
    raw = open(path, encoding="utf-8", errors="replace").read().splitlines()
    for l in raw:
        p = l.split("\t", 3)
        if p[0] == "A" and len(p) >= 3: alias[p[1]] = p[2]
    sub = re.compile(r"<([^<>]+)>")
    ren = lambda s: sub.sub(lambda m: "<" + alias.get(m.group(1), m.group(1)) + ">", s)
    for l in raw:
        p = l.split("\t", 3)
        if len(p) < 4: continue
        k, ctx, t, text = p
        if k == "C": calls.setdefault(ctx, []).append(ren(text))
        elif k == "W":
            key = ctx + " | " + LINE.sub("line N", ren(text))
            warns.setdefault(key, text)
        elif k == "E":
            key = ctx + " | " + LINE.sub("L", text)
            errs.setdefault(key, text)
        elif k == "I": info.append(ctx + " " + text)
    return calls, warns, errs, info


def args_of(c):
    i = c.find("("); return c[i + 1:-1]


NUM = re.compile(r"-?\d+\.\d+")


def near(x, y, tol):
    """same call except float args within tol (absolute) or 1e-5 relative"""
    if NUM.sub("#", x) != NUM.sub("#", y): return False
    for p, q in zip(NUM.findall(x), NUM.findall(y)):
        p, q = float(p), float(q)
        if abs(p - q) > max(tol, 1e-5 * max(abs(p), abs(q))): return False
    return True


def diff_ctx(a, b):
    miss, extra, chg = [], [], []
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal": continue
        A, B = a[i1:i2], b[j1:j2]
        # pair same-native calls in order -> changed
        used = set()
        for x in A:
            nx = NAME.match(x).group(1) if NAME.match(x) else x
            hit = None
            for j, y in enumerate(B):
                if j in used: continue
                if (NAME.match(y).group(1) if NAME.match(y) else y) == nx: hit = j; break
            if hit is None: miss.append(x)
            else: used.add(hit); chg.append((x, B[hit]))
        extra += [y for j, y in enumerate(B) if j not in used]
    return miss, extra, chg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("orig"); ap.add_argument("new")
    ap.add_argument("--json"); ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--tol", type=float, default=0.1, help="float args closer than this count as float noise")
    a = ap.parse_args()
    oc, ow, oe, oi = load(a.orig)
    nc, nw, ne, ni = load(a.new)
    res = {"contexts": {}, "only_in_orig_ctx": [], "only_in_new_ctx": [],
           "new_warnings": [], "new_errors": [], "gone_errors": [], "info": {"orig": oi, "new": ni}}
    agg = collections.Counter()
    for ctx in list(oc) + [c for c in nc if c not in oc]:
        A, B = oc.get(ctx, []), nc.get(ctx, [])
        if not A: res["only_in_new_ctx"].append([ctx, len(B)])
        if not B: res["only_in_orig_ctx"].append([ctx, len(A)])
        if A == B: continue
        m, e, c = diff_ctx(A, B)
        noise = [p for p in c if near(p[0], p[1], a.tol)]
        c = [p for p in c if not near(p[0], p[1], a.tol)]
        if noise:
            res.setdefault("float_noise", {})[ctx] = {"count": len(noise), "example": list(noise[0])}
        if not (m or e or c): continue
        res["contexts"][ctx] = {"orig_calls": len(A), "new_calls": len(B), "missing": m, "extra": e,
                                "changed": [{"orig": x, "new": y} for x, y in c]}
        for x in m: agg[("missing", ctx, NAME.match(x).group(1) if NAME.match(x) else x)] += 1
        for y in e: agg[("extra", ctx, NAME.match(y).group(1) if NAME.match(y) else y)] += 1
        for x, y in c: agg[("changed", ctx, NAME.match(x).group(1))] += 1
    res["new_warnings"] = [k for k in nw if k not in ow]
    res["new_errors"] = [ne[k] for k in ne if k not in oe]
    res["gone_errors"] = [oe[k] for k in oe if k not in ne]
    res["summary"] = [{"kind": k, "ctx": c, "native": n, "count": v} for (k, c, n), v in agg.most_common()]
    if a.json:
        json.dump(res, open(a.json, "w", encoding="utf-8"), indent=1)
    # short text summary
    tot = collections.Counter()
    for (k, _, _), v in agg.items(): tot[k] += v
    print("contexts differing: %d | missing %d, extra %d, changed %d | new warnings %d, new errors %d, gone errors %d"
          % (len(res["contexts"]), tot["missing"], tot["extra"], tot["changed"], len(res["new_warnings"]),
             len(res["new_errors"]), len(res["gone_errors"])))
    fn = res.get("float_noise", {})
    if fn:
        k = max(fn, key=lambda c: fn[c]["count"])
        print("  float noise (<= tol %.3g, not counted): %d calls in %d contexts, e.g. %s: %s -> %s"
              % (a.tol, sum(v["count"] for v in fn.values()), len(fn), k, fn[k]["example"][0][:80], args_of(fn[k]["example"][1])[:60]))
    if res["only_in_orig_ctx"]: print("  ctx only in orig:", ", ".join("%s(%d)" % tuple(x) for x in res["only_in_orig_ctx"][:15]))
    if res["only_in_new_ctx"]: print("  ctx only in new:", ", ".join("%s(%d)" % tuple(x) for x in res["only_in_new_ctx"][:15]))
    for s in res["summary"][:a.top]:
        ex = ""
        cd = res["contexts"][s["ctx"]]
        if s["kind"] == "changed":
            p = next(p for p in cd["changed"] if p["orig"].startswith(s["native"] + "("))
            ex = "  e.g. %s -> %s" % (args_of(p["orig"])[:70], args_of(p["new"])[:70])
        print("  %-7s %4d x %-34s in %s%s" % (s["kind"], s["count"], s["native"], s["ctx"], ex))
    pri = sorted(res["new_warnings"], key=lambda w: ("NILH" not in w, "UNDEF" not in w))
    for w in pri[:a.top]: print("  NEW WARN", w[:200])
    for e in res["new_errors"][:10]: print("  NEW ERROR", e[:200])
    for e in res["gone_errors"][:5]: print("  GONE ERROR", e[:200])
    clean = not (res["contexts"] or res["new_warnings"] or res["new_errors"] or res["gone_errors"]
                 or res["only_in_orig_ctx"] or res["only_in_new_ctx"])
    print("RESULT:", "IDENTICAL" if clean else "DIFFERENT")
    return 0 if clean else 1


if __name__ == "__main__":
    sys.exit(main())
