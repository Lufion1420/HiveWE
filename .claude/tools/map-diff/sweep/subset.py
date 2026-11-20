import os, struct, sys, json
P = sys.argv[1]
ids = sorted(os.listdir(P + "/stock"))
out = []
for i in ids:
    w3i = P + "/stock/" + i + "/war3map.w3i"
    ver = struct.unpack_from("<i", open(w3i, "rb").read(4), 0)[0] if os.path.exists(w3i) else -1
    sd = P + "/stock_scripts/" + i
    lua = os.path.exists(sd + "/war3map.lua")
    if ver == 39 or lua:
        out.append((i, ver, "lua" if lua else "j"))
print(len(out), sum(1 for o in out if o[2] == "lua"), sum(1 for o in out if o[1] == 39))
json.dump(out, open(sys.argv[2], "w"))
