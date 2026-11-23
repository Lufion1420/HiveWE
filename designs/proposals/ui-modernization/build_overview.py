# Writes overview.html (contact sheet of every page, Dark left, Light right) and renders renders/overview.png. Run after render_windows.py.
import json, pathlib, subprocess, glob, os
here = pathlib.Path(__file__).parent.resolve()
pages = json.loads((here / 'pages.json').read_text())
TW = 440  # thumbnail width
cells = ''
for n, t, w, h in pages:
    if n == 'trigger-editor':
        continue  # skipped: GUI trigger system is being reworked
    th = round(h * TW / w)
    cells += (f'<div class="cell"><div class="t">{t}</div><div class="pair">'
              f'<div><img src="renders/{n}-flat-dark.png" style="width:{TW}px;height:{th}px"><div class="l">Dark</div></div>'
              f'<div><img src="renders/{n}-flat-light.png" style="width:{TW}px;height:{th}px"><div class="l">Light</div></div></div></div>')
doc = ('<!doctype html><meta charset="utf-8"><title>HiveWE Flat windows overview</title><style>'
       'body{margin:0;padding:16px;background:#555;font-family:"Segoe UI",Arial,sans-serif;width:2848px}'
       'h1{font-size:18px;color:#fff;margin:0 0 12px}.grid{display:flex;flex-wrap:wrap;gap:16px;align-items:flex-start}'
       '.cell{background:#333;padding:8px}.t{color:#fff;font-size:14px;font-weight:600;margin-bottom:6px}.pair{display:flex;gap:8px}'
       '.l{color:#bbb;font-size:11px;text-align:center;margin-top:2px}img{display:block;border:1px solid #777}</style>'
       '<h1>HiveWE Flat: all windows (left Dark, right Light). Main window approved earlier.</h1><div class="grid">' + cells + '</div>')
(here / 'overview.html').write_text(doc, encoding='utf-8')
ch = sorted(glob.glob(os.path.expandvars(r'%LOCALAPPDATA%\ms-playwright\chromium-*\chrome-win*\chrome.exe')))[-1]
H = int(os.environ.get('OVH', '2000'))
subprocess.run([ch, '--headless', '--disable-gpu', '--hide-scrollbars', f'--window-size=2880,{H}', '--virtual-time-budget=5000',
                f'--screenshot={here / "renders" / "overview.png"}', 'file:///' + (here / 'overview.html').as_posix()], check=True)
