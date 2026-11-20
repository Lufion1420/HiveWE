import json, os, re, sys, collections
S = sys.argv[1]; T = sys.argv[2]
sys.path.insert(0, T + '/files')
import w3parsers as W
sub = json.load(open(S + '/w4/subset_all.json'))
rows = []
for mid, ver, lang in sub:
    b = open(S + '/stock/' + mid + '/war3map.w3i', 'rb').read()
    try:
        w = W.parse_w3i(b)
    except Exception as e:
        rows.append({'id': mid, 'err': str(e)[:60]}); continue
    pl = w['players']
    plist = [r for _, r in pl.items]
    human = sum(1 for p in plist if p['type'] == 1)
    comp = sum(1 for p in plist if p['type'] == 2)
    sc = open(S + '/stock_scripts/' + mid + '/' + ('war3map.lua' if lang == 'lua' else 'war3map.j'), 'rb').read().decode('latin1')
    st = re.search(r'SetTeams\(\s*(\d+)\s*\)', sc); sg = re.search(r'SetGamePlacement\(\s*(\w+)\s*\)', sc); sp = re.search(r'SetPlayers\(\s*(\d+)\s*\)', sc)
    fl = w['flags']
    rows.append({'id': mid, 'lang': lang, 'ver': ver, 'flags': fl, 'f20': bool(fl & 0x20), 'f40': bool(fl & 0x40), 'players': len(plist), 'human': human, 'comp': comp,
                 'forces': len(w['forces']), 'teams': int(st.group(1)) if st else None, 'place': sg.group(1).replace('MAP_PLACEMENT_', '') if sg else None, 'setplayers': int(sp.group(1)) if sp else None,
                 'force_flags': [f['flags'] for f in w['forces']][:6]})
json.dump(rows, open(S + '/w4/teams_rows_all.json', 'w'))
ok = [r for r in rows if 'err' not in r]
print('rows', len(rows), 'errors', len(rows) - len(ok), 'no SetTeams', sum(r['teams'] is None for r in ok), 'no placement', sum(r['place'] is None for r in ok))
def grp(name, rs):
    c = collections.Counter((r['place']) for r in rs)
    print(name, len(rs), 'placement', dict(c))
    c2 = collections.Counter(('teams==players' if r['teams'] == r['setplayers'] else 'teams==forces' if r['teams'] == r['forces'] else 'teams other') for r in rs)
    print('   teams vs', dict(c2))
for f20 in (True, False):
    for f40 in (True, False):
        for h in ('h1', 'h2+'):
            rs = [r for r in ok if r['f20'] == f20 and r['f40'] == f40 and (r['human'] > 1) == (h == 'h2+')]
            if rs: grp('f20=%s f40=%s %s' % (f20, f40, h), rs)
print()
for r in ok:
    if r['f20'] and r['human'] > 1:
        print(r['id'], r['lang'], 'v%d' % r['ver'], hex(r['flags']), 'players', r['players'], 'human', r['human'], 'comp', r['comp'], 'forces', r['forces'], 'ff', r['force_flags'], 'SetPlayers', r['setplayers'], 'SetTeams', r['teams'], r['place'])