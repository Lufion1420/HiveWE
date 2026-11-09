"""Parser sanity sweep: compare every map folder under ROOT with itself.

    python selfcheck.py ROOT [--triggerdata TriggerData.txt] [--limit N] [--timeout S]

Each folder runs in its own process with a wall-clock timeout. A self-compare must be
identical for every file with no parse failure; files with trailing bytes are listed too,
since a correct reader should consume each file exactly.
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("--triggerdata")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--out", help="write per-map results as JSON")
    a = ap.parse_args()
    dirs = sorted(d for d in os.listdir(a.root) if os.path.isdir(os.path.join(a.root, d)))
    if a.limit:
        dirs = dirs[:a.limit]
    tally, problems, trailing = {}, [], []
    tmp = os.path.join(os.environ.get("TEMP", "."), "fdiff_selfcheck.json")
    for d in dirs:
        p = os.path.join(a.root, d)
        cmd = [sys.executable, "-I", os.path.join(HERE, "fdiff.py"), p, p, "--json", tmp, "--quiet"]
        if a.triggerdata:
            cmd += ["--triggerdata", a.triggerdata]
        try:
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=a.timeout, check=False)
        except subprocess.TimeoutExpired:
            problems.append((d, "*", "timeout"))
            continue
        try:
            with open(tmp, encoding="utf-8") as f:
                files = json.load(f)["files"]
        except (OSError, ValueError) as e:
            problems.append((d, "*", f"no report: {e}"))
            continue
        os.remove(tmp)
        for rel, r in files.items():
            name = rel.split("/")[-1].lower()
            tally.setdefault(name, {}).setdefault(r["status"], 0)
            tally[name][r["status"]] += 1
            if r["status"] not in ("identical", "unparsed_identical"):
                problems.append((d, rel, r["status"] + " " + json.dumps(r.get("parse_errors", r.get("reason", "")))))
            if r.get("trailing_bytes"):
                t = r["trailing_bytes"]["orig"]
                trailing.append((d, rel, t["length"], t["all_zero"]))
    print(f"maps: {len(dirs)}")
    for name in sorted(tally):
        print(f"  {name:<22} " + " ".join(f"{k}={v}" for k, v in sorted(tally[name].items())))
    print(f"problems: {len(problems)}")
    for p in problems[:40]:
        print("  ", *p)
    print(f"files with trailing bytes: {len(trailing)}")
    by = {}
    for d, rel, n, z in trailing:
        k = (rel.split('/')[-1].lower(), z)
        by[k] = by.get(k, 0) + 1
    for k, v in sorted(by.items()):
        print(f"  {k[0]} all_zero={k[1]}: {v}")
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            json.dump({"tally": tally, "problems": problems, "trailing": trailing}, f, indent=1)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
