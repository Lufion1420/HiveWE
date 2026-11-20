import json, os, subprocess, sys
S, W = sys.argv[1], sys.argv[2]
m = sorted(json.load(open(W + '/maps_all.json')))
idx = {}
for i, c in enumerate(m):
    mid = '%04d' % i; idx[mid] = c
    for d in (S + '/stock/' + mid, S + '/stock_scripts/' + mid): os.makedirs(d, exist_ok=True)
    try:
        p = subprocess.run([sys.executable, '-I', W + '/ext1.py', W + '/dll', c, S + '/stock/' + mid, S + '/stock_scripts/' + mid], capture_output=True, text=True, timeout=120)
        print(mid, p.returncode, p.stdout.strip()[:60], p.stderr.strip()[-80:], flush=True)
    except subprocess.TimeoutExpired:
        print(mid, 'TIMEOUT', flush=True)
json.dump(idx, open(S + '/stock/index.json', 'w'))
