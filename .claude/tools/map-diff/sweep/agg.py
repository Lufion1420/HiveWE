import json, os, collections, sys
S = sys.argv[1]; W = S + '/w4'
rows = [json.load(open(W + '/work/res/' + f)) for f in sorted(os.listdir(W + '/work/res'))]
print('maps', len(rows), 'lua', sum(r['lang'] == 'lua' for r in rows))
print('status', dict(collections.Counter(r['status'] for r in rows)))
print('refused/non-ok non-compile:', [(r['id'], r['status'], r['err'][:80]) for r in rows if r['status'] not in ('ok', 'script_compile_error')])
print('no gen script:', [r['id'] for r in rows if not r['gen']])
print('peak max MB', max(r['peak_mb'] or 0 for r in rows))
for lang in ('lua', 'j'):
    rs = [r for r in rows if r['lang'] == lang and 'color' in r]
    print(lang, 'maps analysed', len(rs))
    for k in ('color', 'amount'):
        print('  ', k, 'orig total', sum(r[k]['orig'] for r in rs), 'new total', sum(r[k]['new'] for r in rs), 'maps lost>0', [(r['id'], r[k]['lost']) for r in rs if r[k]['lost']], 'maps gained>0', [(r['id'], r[k]['gained']) for r in rs if r[k]['gained']])
        print('   any-count mismatch', [(r['id'], r[k]['orig_any'], r[k]['new_any']) for r in rs if r[k]['orig_any'] != r[k]['new_any']][:10])
for r in rows:
    if r['lang'] == 'j' and r.get('color', {}).get('lost'):
        print('JASS color lost', r['id'], r['color']['lost'], r['color']['lost_items'][:20], 'gained', r['color']['gained'])
sim = [r for r in rows if 'sim' in r]
print('sim maps', len(sim), 'rc', dict(collections.Counter(r['sim_rc'] for r in sim)), 'sim_err', [r['id'] for r in rows if 'sim_err' in r])
print('new warnings maps', [(r['id'], r['sim']['new_warnings'][:2]) for r in sim if r['sim']['new_warnings']][:10])
print('new errors maps', [(r['id'], r['sim']['new_errors'][:2]) for r in sim if r['sim']['new_errors']][:10])
net = collections.Counter(); netm = collections.Counter()
for r in sim:
    for k, v in r['sim']['net'].items():
        net[k] += v; netm[k] += 1
print('sim net natives (maps, sum):'); 
for k, v in sorted(netm.items(), key=lambda x: -x[1]): print('  ', k, v, net[k])
# fdiff status histogram per file
fd = collections.defaultdict(collections.Counter)
for r in rows:
    for f, (st, n) in r.get('fd', {}).items():
        fd[f.split('/')[-1] if '_Locales' not in f else '_Locales/' + f.split('/')[-1]][st] += 1
print('fdiff:')
for f, c in sorted(fd.items()): print('  ', f, dict(c))
# sdiff lost natives histogram, JASS and lua
for lang in ('lua', 'j'):
    lm = collections.Counter(); gm = collections.Counter()
    for r in rows:
        if r['lang'] == lang and 'sd' in r:
            for k in r['sd']['lost']: lm[k] += 1
            for k in r['sd']['gain']: gm[k] += 1
    print(lang, 'sdiff lost-line heads (maps):', lm.most_common(45))
    print(lang, 'sdiff gained-line heads (maps):', gm.most_common(30))