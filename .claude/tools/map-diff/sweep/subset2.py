import os, struct, sys, json
S = sys.argv[1]
out = []
for i in sorted(d for d in os.listdir(S + "/stock") if d.isdigit()):
    w3i = S + "/stock/" + i + "/war3map.w3i"
    ver = struct.unpack_from("<i", open(w3i, "rb").read(4), 0)[0] if os.path.exists(w3i) else -1
    sd = S + "/stock_scripts/" + i
    kind = "lua" if os.path.exists(sd + "/war3map.lua") else ("j" if os.path.exists(sd + "/war3map.j") else "none")
    if ver == 39 or kind == "lua": out.append((i, ver, kind))
print(len(out), "lua", sum(o[2] == "lua" for o in out), "v39", sum(o[1] == 39 for o in out), "none", sum(o[2] == "none" for o in out))
json.dump(out, open(sys.argv[2], "w"))
