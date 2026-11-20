import json, os, subprocess, sys, time
S = os.environ['S']; W = S + '/w4'
sub = json.load(open(W + '/subset.json'))
t0 = time.time()
for mid, ver, lang in sub:
    if os.path.exists(W + '/work/res/' + mid + '.json'):
        continue
    try:
        p = subprocess.run([sys.executable, '-I', W + '/one.py', mid, lang], capture_output=True, text=True, timeout=900)
        print(p.stdout.strip() or ('FAIL ' + mid + ' ' + p.stderr[-200:]), flush=True)
    except subprocess.TimeoutExpired:
        print('TIMEOUT', mid, flush=True)
print('ALLDONE', time.time() - t0, flush=True)