import re, sys
S = sys.argv[1]
b = open(S + '/rel_in/war3map.lua', 'rb').read().decode('latin1')
g = open(S + '/rel_out/war3map.lua', 'rb').read().decode('latin1')
def calls(txt, start):
    i = txt.index(start); j = txt.find('function ', i + len(start))
    seg = txt[i:j if j > 0 else None]
    return re.findall(r'[A-Za-z_]\w*\([^()]*\)', re.sub(r'\s+', ' ', seg))
for fn in ('function config()', 'function main()', 'function InitCustomPlayerSlots()', 'function InitCustomTeams()', 'function InitAllyPriorities()', 'function CreateAllUnits()'):
    try:
        cb = calls(b, fn)
    except ValueError:
        print(fn, 'missing in bundle'); continue
    cg = calls(g, fn)
    import difflib
    d = [l for l in difflib.unified_diff(cb, cg, lineterm='', n=0) if l[:1] in '+-' and l[:3] not in ('+++', '---')]
    print(fn, 'bundle calls', len(cb), 'resaved', len(cg), 'diff entries', len(d))
    for l in d[:12]: print('   ', l[:150])