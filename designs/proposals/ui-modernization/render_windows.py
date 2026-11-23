# Renders every page in pages.json to renders/<page>-flat-dark.png and -light.png with headless Chromium.
# Run: python render_windows.py [page ...]
import json, subprocess, os, glob, sys, pathlib
here = pathlib.Path(__file__).parent.resolve()
cands = sorted(glob.glob(os.path.expandvars(r'%LOCALAPPDATA%\ms-playwright\chromium-*\chrome-win*\chrome.exe')))
ch = cands[-1]
only = set(sys.argv[1:])
for n, t, w, h in json.loads((here / 'pages.json').read_text()):
    if only and n not in only:
        continue
    for th in ('dark', 'light'):
        out = here / 'renders' / f'{n}-flat-{th}.png'
        url = 'file:///' + (here / f'{n}-flat.html').as_posix() + ('#light' if th == 'light' else '')
        r = subprocess.run([ch, '--headless', '--disable-gpu', '--hide-scrollbars', f'--window-size={w},{h}', '--virtual-time-budget=8000',
                            f'--screenshot={out}', url], capture_output=True, text=True, timeout=120)
        print(n, th, r.returncode)
