"""Headless HiveWE resave of stock map folders, one map per process.

python -I resave.py <hivewe_exe> <stock_root> <scripts_root> <work_root> <id> [<id> ...] [--timeout S] [--max-mb MB]

For each id: copies <stock_root>/<id> to <work_root>/sandbox/<id>, adds the World Editor script from <scripts_root>/<id>
(so the save overwrites it), then runs `HiveWE.exe --resave <sandbox> <work_root>/out/<id>` with the exe folder as the
working directory (it needs data/). Exit 3 (saved, but the generated script did not compile, so
the old war3map.j is kept) is recorded as script_compile_error. The stock folders are never written. A process over the timeout or the memory cap is
killed (only the process this script started). Results go to stdout and <work_root>/results.tsv.
"""

import csv
import os
import shutil
import subprocess
import sys
import time


def working_set_mb(pid):
    out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    for row in csv.reader(out.splitlines()):
        if len(row) >= 5 and row[1] == str(pid):
            return int(row[4].replace("\xa0", "").replace(",", "").replace(".", "").split()[0]) / 1024
    return 0.0


def kill_tree(proc):
    """Kills the process this script started and its children (clijasshelper started by the save)."""
    subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)], capture_output=True)
    if proc.poll() is None:
        proc.kill()
    proc.wait()


def is_inside(path, root):
    path, root = os.path.normcase(os.path.realpath(path)), os.path.normcase(os.path.realpath(root))
    return os.path.commonpath([path, root]) == root


def valid_map_id(map_id):
    return bool(map_id) and map_id not in (".", "..") and not os.path.isabs(map_id) and not any(c in map_id for c in "/\\:")


def resave(exe, stock_root, scripts_root, work_root, map_id, timeout, max_mb):
    sandbox = os.path.join(work_root, "sandbox", map_id)
    out = os.path.join(work_root, "out", map_id)
    logs = os.path.join(work_root, "logs")
    os.makedirs(logs, exist_ok=True)
    for path in (sandbox, out):
        if os.path.exists(path):
            shutil.rmtree(path)
    shutil.copytree(os.path.join(stock_root, map_id), sandbox)
    script_dir = os.path.join(scripts_root, map_id)
    if os.path.isdir(script_dir):
        for name in os.listdir(script_dir):
            shutil.copy2(os.path.join(script_dir, name), sandbox)

    start = time.monotonic()
    peak = 0.0
    status = None
    with open(os.path.join(logs, f"{map_id}.out.txt"), "w") as so, open(os.path.join(logs, f"{map_id}.err.txt"), "w") as se:
        proc = subprocess.Popen([exe, "--resave", sandbox, out], cwd=os.path.dirname(exe), stdout=so, stderr=se)
        while proc.poll() is None:
            peak = max(peak, working_set_mb(proc.pid))
            if time.monotonic() - start > timeout:
                status = "timeout"
            elif peak > max_mb:
                status = "memory"
            if status:
                kill_tree(proc)
                break
            time.sleep(0.5)
    wall = time.monotonic() - start
    if status is None:
        status = {0: "ok", 3: "script_compile_error", 4: "no_casc_or_gl"}.get(proc.returncode, f"exit {proc.returncode}")
    with open(os.path.join(logs, f"{map_id}.err.txt")) as f:
        err = " | ".join(line.strip() for line in f if line.strip())[:300]
    return status, wall, peak, err


def main():
    args = sys.argv[1:]
    timeout, max_mb = 300.0, 2048.0
    if "--timeout" in args:
        i = args.index("--timeout")
        timeout = float(args[i + 1])
        del args[i:i + 2]
    if "--max-mb" in args:
        i = args.index("--max-mb")
        max_mb = float(args[i + 1])
        del args[i:i + 2]
    if len(args) < 5:
        print(__doc__)
        return 2
    exe, stock_root, scripts_root, work_root = (os.path.abspath(a) for a in args[:4])
    for root in (stock_root, scripts_root):
        if is_inside(work_root, root):
            print(f"Refused: work_root {work_root} is inside {root}, which must stay read-only")
            return 2
    bad_ids = [map_id for map_id in args[4:] if not valid_map_id(map_id)]
    if bad_ids:
        print(f"Refused: map ids must be bare folder names: {bad_ids}")
        return 2
    os.makedirs(work_root, exist_ok=True)
    failures = 0
    with open(os.path.join(work_root, "results.tsv"), "a", newline="") as f:
        writer = csv.writer(f, delimiter="\t")
        for map_id in args[4:]:
            status, wall, peak, err = resave(exe, stock_root, scripts_root, work_root, map_id, timeout, max_mb)
            failures += status != "ok"
            writer.writerow([map_id, status, f"{wall:.1f}", f"{peak:.0f}", err])
            f.flush()
            print(f"{map_id}\t{status}\t{wall:.1f}s\tpeak {peak:.0f} MB\t{err}", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
