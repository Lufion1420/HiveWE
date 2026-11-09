"""Run one war3map.lua in the Lua 5.3 simulator and write a native-call log.

usage: sim.py <war3map.lua> <out.log> [--w3i war3map.w3i] [--lupa DIR] [--jdir data/tools]
              [--time 60] [--wall 90] [--mem 1024] [--imax 4000]
Log lines: KIND<TAB>ctx<TAB>t<TAB>text   (C call, W warning, E error, I info, A alias "label<TAB>gg_name")
"""
import argparse, os, re, struct, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
QUIET = ("I2S R2S R2SW S2I S2R I2R R2I SubString StringLength StringCase SquareRoot Sin Cos Tan Atan Atan2 Asin Acos "
         "Pow Deg2Rad Rad2Deg ModuloInteger ModuloReal IMinBJ IMaxBJ RMinBJ RMaxBJ IAbsBJ RAbsBJ GetHandleId "
         "GetRandomInt GetRandomReal TriggerEvaluate GetTriggeringTrigger GetExpiredTimer GetEnumPlayer GetEnumDestructable").split()


def read_w3i_camera(path):
    """Camera bounds (minx,miny,maxx,maxy) from war3map.w3i; only the header up to the bounds is parsed."""
    d = open(path, "rb").read()
    ver, = struct.unpack_from("<i", d, 0)
    o = 12 + (16 if ver >= 28 else 0)  # version, saves, editor version, [game version x4]
    for _ in range(4):  # name, author, description, recommended players
        o = d.index(b"\0", o) + 1
    f = struct.unpack_from("<8f", d, o)
    xs, ys = f[0::2], f[1::2]
    return [min(xs), min(ys), max(xs), max(ys)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("script"); ap.add_argument("out")
    ap.add_argument("--w3i"); ap.add_argument("--lupa", default=os.environ.get("LUPA_PATH"))
    ap.add_argument("--jdir", default=os.path.join(HERE, "..", "..", "..", "..", "data", "tools"))
    ap.add_argument("--time", type=float, default=60.0, help="simulated game seconds")
    ap.add_argument("--wall", type=float, default=90.0, help="CPU-seconds cap inside Lua")
    ap.add_argument("--mem", type=int, default=1024, help="Lua heap cap in MB")
    ap.add_argument("--imax", type=int, default=4000, help="instruction cap in millions")
    a = ap.parse_args()
    if a.lupa: sys.path.insert(0, a.lupa)
    import lupa.lua53 as lupa

    t0 = time.time()
    L = lupa.LuaRuntime(unpack_returned_tuples=True, max_memory=a.mem * 1024 * 1024)
    G = L.globals()
    cfg = L.table_from({"wall": a.wall, "imax": a.imax, "quiet": L.table_from(QUIET)})
    if a.w3i:
        cfg["cam"] = L.table_from(read_w3i_camera(a.w3i))
    G.__CFG = cfg
    rd = lambda n: open(os.path.join(HERE, n), encoding="utf-8").read()
    L.execute(rd("rt.lua"))
    J = a.jdir
    cj = open(os.path.join(J, "common.j"), encoding="latin1").read()
    bj = open(os.path.join(J, "blizzard.j"), encoding="latin1").read()
    sig = re.compile(r"^\s*(?:constant\s+)?(native|function)\s+(\w+)\s+takes\s+(.*?)\s+returns\s+(\w+)", re.M)
    for src in (cj, bj):
        for m in sig.finditer(src):
            ps = "" if m.group(3).strip() == "nothing" else ",".join(p.split()[0] for p in m.group(3).split(","))
            G.__mkfn(m.group(2), m.group(4), ps)
    for src in (cj, bj):
        for blk in re.findall(r"^globals(.*?)^endglobals", src, re.S | re.M):
            for line in blk.splitlines():
                line = line.split("//")[0].strip()
                m = re.match(r"(?:constant\s+)?(\w+)\s+(array\s+)?(\w+)\s*(?:=\s*(.*))?$", line)
                if m and m.group(1) != "native":
                    G.__mkglob(m.group(1), bool(m.group(2)), m.group(3), (m.group(4) or "").strip())
    L.execute(rd("impl.lua"))
    L.execute(rd("log.lua"))

    script = open(a.script, encoding="utf-8", errors="replace").read()
    lf = L.eval('function(s) local f,e=load(s,"=war3map.lua") return {f=f,e=e} end')
    r = lf(script)
    out = []
    status = "ok"
    if not r.f:
        out.append("E\tchunk\t0\tLOAD ERROR " + str(r.e).replace("\n", " "))
        status = "load-error"
    else:
        try:
            G.__run(r.f, "chunk", "chunk")
            nwrap = G.__wrapmap()
            G.__mainctx = "post-chunk"
            G.__run(G.config, "config", "config")
            G.__run(G.main, "main", "main")
            aliases = list(G.__aliases().values())
            G.__loop(a.time)
            aliases += list(G.__aliases().values())
            out += ["A\t" + s for s in aliases]
            out.append("I\tsim\t%.2f\twrapped %d map functions; ticks=%d capped=%s dropped=%d"
                       % (G.__now, nwrap, G.__ticks, G.__capped, G.__dropped))
            for k, v in G.__ctxdropped.items():
                out.append("I\t%s\t0\tCTXCAP dropped %d calls" % (k, v))
            if G.__capped: status = "capped-" + str(G.__capped)
        except lupa.LuaMemoryError:
            status = "memory-cap"
            out.append("E\tsim\t0\tSIMCAP Lua memory cap (%d MB)" % a.mem)
        except lupa.LuaError as e:
            status = "lua-error"
            out.append("E\tsim\t0\t" + str(e).replace("\n", " "))
    log = G.__log
    lines = [log[i] for i in range(1, len(log) + 1)] + out
    with open(a.out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(l.replace("\n", "\\n") for l in lines) + "\n")
    print("%s: %d log lines, status=%s, %.1fs" % (a.script, len(lines), status, time.time() - t0))


if __name__ == "__main__":
    main()
