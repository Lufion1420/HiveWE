"""Differential sweep entry point: simulate the World Editor and the HiveWE war3map.lua, then diff the call logs.

usage: simdiff.py <orig.lua> <new.lua> [--w3i orig/war3map.w3i] [--w3i-new new/war3map.w3i] [--lupa DIR]
                  [--out DIR] [--time 60] [--wall 90] [--mem 1024] [--tol 0.1] [--top 25]
Exit code: 0 identical, 1 different, 2 a simulation failed to run.
"""
import argparse, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def run_sim(script, out, w3i, a):
    cmd = [sys.executable, os.path.join(HERE, "sim.py"), script, out, "--time", str(a.time), "--wall", str(a.wall),
           "--mem", str(a.mem)]
    if w3i: cmd += ["--w3i", w3i]
    if a.lupa: cmd += ["--lupa", a.lupa]
    try:  # hard cap on the whole process; one simulation at a time
        p = subprocess.run(cmd, timeout=a.wall * 1.5 + 60, capture_output=True, text=True)
    except subprocess.TimeoutExpired:
        print("SIM TIMEOUT (killed):", script); return False
    sys.stdout.write(p.stdout); sys.stdout.flush()
    if p.returncode != 0:
        sys.stdout.write(p.stderr[-2000:]); return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("orig"); ap.add_argument("new")
    ap.add_argument("--w3i"); ap.add_argument("--w3i-new")
    ap.add_argument("--lupa", default=os.environ.get("LUPA_PATH"))
    ap.add_argument("--out", default=os.path.join(os.getcwd(), "simdiff_out"))
    ap.add_argument("--time", type=float, default=60.0)
    ap.add_argument("--wall", type=float, default=90.0)
    ap.add_argument("--mem", type=int, default=1024)
    ap.add_argument("--tol", type=float, default=0.1)
    ap.add_argument("--top", type=int, default=25)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    sys.stdout.reconfigure(line_buffering=True)
    lo, ln = os.path.join(a.out, "orig.log"), os.path.join(a.out, "new.log")
    if not run_sim(a.orig, lo, a.w3i, a): return 2
    if not run_sim(a.new, ln, a.w3i_new or a.w3i, a): return 2
    p = subprocess.run([sys.executable, os.path.join(HERE, "tdiff.py"), lo, ln, "--json", os.path.join(a.out, "diff.json"),
                        "--tol", str(a.tol), "--top", str(a.top)], timeout=600)
    print("logs + diff.json in", a.out)
    return p.returncode


if __name__ == "__main__":
    sys.exit(main())
