"""one.py <id> <lang> : resave + analyse one map. Writes work/res/<id>.json. One map per process."""
import json, os, re, shutil, subprocess, sys, time, collections
S = os.environ['S']; R = os.environ['R']; W = S + '/w4'; T = R + '/.claude/tools/map-diff'
mid, lang = sys.argv[1], sys.argv[2]
exe = R + '/build/Agent/Release/HiveWE.exe'
res = {'id': mid, 'lang': lang}
os.makedirs(W + '/work/res', exist_ok=True); os.makedirs(W + '/work/gen', exist_ok=True)
tmp_input = os.path.join(os.environ['LOCALAPPDATA'], 'Temp', 'input.lua')
t0 = time.time()
if os.path.exists(tmp_input):
    os.remove(tmp_input)
p = subprocess.run([sys.executable, '-I', T + '/sweep/resave.py', exe, S + '/stock', S + '/stock_scripts', W + '/work', mid], capture_output=True, text=True, timeout=400)
line = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ''
parts = line.split('\t')
res['status'] = parts[1] if len(parts) > 1 else 'noresult'
res['wall'] = parts[2] if len(parts) > 2 else ''
res['peak_mb'] = float(parts[3].split()[1]) if len(parts) > 3 and parts[3].startswith('peak') else None
res['err'] = parts[4][:200] if len(parts) > 4 else ''
out = W + '/work/out/' + mid
orig_script = S + '/stock_scripts/' + mid + '/' + ('war3map.lua' if lang == 'lua' else 'war3map.j')
if lang == 'lua':
    gen = out + '/war3map.lua'
else:
    gen = W + '/work/gen/' + mid + '.j'
    if os.path.exists(tmp_input) and os.path.getmtime(tmp_input) >= t0:
        shutil.copy(tmp_input, gen)
    else:
        gen = None
res['gen'] = bool(gen and os.path.exists(gen))

def calls(path):
    txt = open(path, 'rb').read().decode('latin1')
    t = re.sub(r'\s+', '', txt)
    col = collections.Counter(re.findall(r'SetUnitColor\(\w+,ConvertPlayerColor\((\d+)\)\)', t))
    amt = collections.Counter(re.findall(r'SetResourceAmount\(\w+,(\d+)\)', t))
    return col, amt, len(re.findall(r'SetUnitColor\(', t)), len(re.findall(r'SetResourceAmount\(', t))
if res['gen']:
    oc, oa, ocn, oan = calls(orig_script); nc, na, ncn, nan_ = calls(gen)
    res['color'] = {'orig': sum(oc.values()), 'new': sum(nc.values()), 'orig_any': ocn, 'new_any': ncn,
                    'lost': sum((oc - nc).values()), 'gained': sum((nc - oc).values()),
                    'lost_items': sorted((oc - nc).elements())[:40], 'gained_items': sorted((nc - oc).elements())[:40]}
    res['amount'] = {'orig': sum(oa.values()), 'new': sum(na.values()), 'orig_any': oan, 'new_any': nan_,
                     'lost': sum((oa - na).values()), 'gained': sum((na - oa).values()),
                     'lost_items': sorted((oa - na).elements())[:40], 'gained_items': sorted((na - oa).elements())[:40]}
    sj = W + '/work/sd_' + mid + '.json'
    subprocess.run([sys.executable, '-I', T + '/scripts/sdiff.py', orig_script, gen, '--json', sj, '--max-diff', '100000', '--top', '1'], capture_output=True, text=True, timeout=300)
    try:
        d = json.load(open(sj))
        lost = collections.Counter(); gain = collections.Counter()
        for f in d['functions']['changed']:
            for l in f['diff']:
                if l[:1] in '-+' and l[:3] not in ('---', '+++'):
                    m = re.match(r'[-+]\s*(?:call\s+|set\s+)?(?:\w+\s*=\s*)?([A-Za-z_]\w*)', l)
                    (lost if l[0] == '-' else gain)[m.group(1) if m else '?'] += 1
        res['sd'] = {'missing_fn': len(d['functions']['missing']), 'extra_fn': len(d['functions']['extra']), 'changed_fn': len(d['functions']['changed']),
                     'gl_missing': len(d['globals']['missing']), 'gl_extra': len(d['globals']['extra']), 'gl_changed': len(d['globals']['changed']),
                     'lost': dict(lost), 'gain': dict(gain)}
        os.remove(sj)
    except Exception as e:
        res['sd_err'] = str(e)[:100]
if os.path.isdir(out):
    fj = W + '/work/fd_' + mid + '.json'
    subprocess.run([sys.executable, '-I', T + '/files/fdiff.py', S + '/stock/' + mid, out, '--allow', T + '/files/allow.json', '--triggerdata', S + '/stock/TriggerData.txt', '--max-diffs', '20000', '--json', fj, '--quiet'], capture_output=True, text=True, timeout=300)
    try:
        d = json.load(open(fj))
        res['fd_top'] = list(d.keys())[:8]
        files = d.get('files', d)
        st = {}
        if isinstance(files, dict):
            for k, v in files.items():
                if isinstance(v, dict):
                    dl = v.get('diffs')
                    st[k] = [v.get('status'), len(dl) if isinstance(dl, list) else None]
        elif isinstance(files, list):
            for v in files:
                if isinstance(v, dict):
                    dl = v.get('diffs')
                    st[v.get('file') or v.get('path')] = [v.get('status'), len(dl) if isinstance(dl, list) else None]
        res['fd'] = st
        os.remove(fj)
    except Exception as e:
        res['fd_err'] = str(e)[:100]
if lang == 'lua' and res['gen']:
    sim = W + '/work/sim_' + mid; os.makedirs(sim, exist_ok=True)
    sp = subprocess.run([sys.executable, '-I', T + '/sim/simdiff.py', orig_script, gen, '--w3i', S + '/stock/' + mid + '/war3map.w3i', '--w3i-new', out + '/war3map.w3i', '--lupa', W + '/lupa', '--out', sim], capture_output=True, text=True, timeout=400)
    res['sim_rc'] = sp.returncode
    try:
        d = json.load(open(sim + '/diff.json'))
        net = collections.Counter()
        for ctx, c in d['contexts'].items():
            for x in c.get('missing', []): net[x.split('(')[0]] += 1
            for x in c.get('extra', []): net[x.split('(')[0]] -= 1
        res['sim'] = {'net': {k: v for k, v in net.items() if v}, 'new_warnings': d['new_warnings'][:10], 'new_errors': d['new_errors'][:10], 'gone_errors': len(d['gone_errors'])}
    except Exception as e:
        res['sim_err'] = str(e)[:100] + ' ' + sp.stdout[-200:]
    shutil.rmtree(sim, ignore_errors=True)
json.dump(res, open(W + '/work/res/' + mid + '.json', 'w'))
shutil.rmtree(W + '/work/sandbox/' + mid, ignore_errors=True)
if lang != 'lua': shutil.rmtree(out, ignore_errors=True)
print(mid, res['status'], res.get('peak_mb'), res.get('color', {}).get('lost'), res.get('amount', {}).get('lost'), flush=True)