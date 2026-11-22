# Generates the mockup HTML pages and icons/*.svg. Run: python build.py
import re, pathlib
here = pathlib.Path(__file__).parent

ICONS = {
 'new': '<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4"/><path d="M12 11v6M9 14h6"/>',
 'open': '<path d="M3 7a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/>',
 'open-mpq': '<path d="M3 5h18v4H3z"/><path d="M5 9v10h14V9"/><path d="M10 13h4"/>',
 'recent': '<circle cx="12" cy="12" r="8"/><path d="M12 7v5l3 2"/>',
 'save': '<path d="M5 4h11l3 3v13H5z"/><path d="M8 4v5h7V4"/><path d="M8 20v-6h8v6"/>',
 'save-as': '<path d="M5 4h9l3 3v4"/><path d="M5 4v16h8"/><path d="M8 4v5h6"/><path d="M17 15v6M14 18h6"/>',
 'export-mpq': '<path d="M3 5h18v4H3z"/><path d="M5 9v10h14V9"/><path d="M12 17v-5M9.5 14.5L12 12l2.5 2.5"/>',
 'test-map': '<path d="M7 4l13 8-13 8z"/>',
 'settings': '<circle cx="12" cy="12" r="3"/><path d="M12 3v3M12 18v3M3 12h3M18 12h3M5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1M18.4 5.6l-2.1 2.1M7.7 16.3l-2.1 2.1"/>',
 'exit': '<path d="M10 4H5v16h5"/><path d="M10 12h10M16 8l4 4-4 4"/>',
 'undo': '<path d="M9 14L4 9l5-5"/><path d="M4 9h10a6 6 0 0 1 0 12h-4"/>',
 'redo': '<path d="M15 14l5-5-5-5"/><path d="M20 9H10a6 6 0 0 0 0 12h4"/>',
 'trigger-editor': '<path d="M13 3L5 13h6l-1 8 8-10h-6z"/>',
 'object-editor': '<path d="M12 3l8 4v10l-8 4-8-4V7z"/><path d="M4 7l8 4 8-4M12 11v10"/>',
 'model-editor': '<path d="M12 3l8 5-8 5-8-5z"/><path d="M4 13l8 5 8-5"/>',
 'asset-manager': '<ellipse cx="12" cy="6" rx="7" ry="3"/><path d="M5 6v12c0 1.7 3 3 7 3s7-1.3 7-3V6"/><path d="M5 12c0 1.7 3 3 7 3s7-1.3 7-3"/>',
 'tooltip-editor': '<path d="M4 5h16v11H11l-4 4v-4H4z"/><path d="M8 9h8M8 12h5"/>',
 'map-protector': '<path d="M12 3l8 3v6c0 5-4 8-8 9-4-1-8-4-8-9V6z"/><path d="M9 12l2 2 4-4"/>',
 'config': '<rect x="3" y="6" width="18" height="12" rx="1.5"/><path d="M7 10h.01M11 10h.01M15 10h.01M7 14h10"/>',
 'export-objects': '<path d="M12 15V4M8 8l4-4 4 4"/><path d="M5 13v7h14v-7"/>',
 'import-objects': '<path d="M12 4v11M8 11l4 4 4-4"/><path d="M5 13v7h14v-7"/>',
 'terrain': '<path d="M3 19l6-11 4 6 3-4 5 9z"/>',
 'doodads': '<path d="M12 3l6 9h-3l4 6H5l4-6H6z"/><path d="M12 18v3"/>',
 'units': '<circle cx="12" cy="8" r="3.5"/><path d="M5 20c0-4 3-6 7-6s7 2 7 6"/>',
 'pathing': '<circle cx="6" cy="18" r="2"/><circle cx="18" cy="6" r="2"/><path d="M8 18h6a3 3 0 0 0 0-6h-4a3 3 0 0 1 0-6h6"/>',
 'regions': '<path d="M4 4h16v16H4z" stroke-dasharray="3 3"/>',
 'lighting': '<circle cx="12" cy="12" r="4"/><path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6L7 7M17 17l1.4 1.4M18.4 5.6L17 7M7 17l-1.4 1.4"/>',
 'water': '<path d="M12 3c4 5 6 8 6 11a6 6 0 0 1-12 0c0-3 2-6 6-11z"/>',
 'click-helpers': '<path d="M6 3l11 8-5 1-2 5z"/>',
 'wireframe': '<path d="M12 3l9 17H3z"/><path d="M12 3v17M7.5 11.5h9"/>',
 'debug': '<ellipse cx="12" cy="13" rx="4" ry="6"/><path d="M12 7V4M8 9L5 7M16 9l3-2M8 13H4M16 13h4M8 17l-3 2M16 17l3 2"/>',
 'minimap': '<path d="M3 6l6-2 6 2 6-2v14l-6 2-6-2-6 2z"/><path d="M9 4v14M15 6v14"/>',
 'brush': '<path d="M4 20l1-4L16 5l3 3L8 19z"/><path d="M14 7l3 3"/>',
 'reset-camera': '<path d="M20 12a8 8 0 1 1-2.5-5.8"/><path d="M20 4v5h-5"/>',
 'description': '<path d="M6 3h9l4 4v14H6z"/><path d="M9 11h7M9 14h7M9 17h5"/>',
 'loading-screen': '<rect x="3" y="5" width="18" height="14" rx="1"/><path d="M3 16l5-5 4 4 3-3 6 6"/><circle cx="16" cy="9" r="1.5"/>',
 'map-options': '<path d="M4 7h10M18 7h2M4 12h4M12 12h8M4 17h12M20 17h0"/><circle cx="16" cy="7" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="18" cy="17" r="2"/>',
 'gameplay-constants': '<path d="M9 3h6M10 3v6l-5 9a2 2 0 0 0 2 3h10a2 2 0 0 0 2-3l-5-9V3"/>',
 'item-tables': '<rect x="3" y="5" width="18" height="14" rx="1"/><path d="M3 10h18M9 5v14"/>',
 'players-forces': '<circle cx="9" cy="8" r="3"/><path d="M3 19c0-3 2.5-5 6-5s6 2 6 5"/><circle cx="17" cy="9" r="2.5"/><path d="M17 14c2.5 0 4 1.5 4 4"/>',
 'import-heightmap': '<path d="M3 20l5-8 3 4 3-3 7 7z"/><path d="M12 3v6M9.5 6.5L12 9l2.5-2.5"/>',
 'change-tileset': '<rect x="3" y="3" width="8" height="8"/><rect x="13" y="3" width="8" height="8"/><rect x="3" y="13" width="8" height="8"/><path d="M13 17h8M17 13l4 4-4 4"/>',
 'change-tile-pathing': '<path d="M4 4h16v16H4z"/><path d="M4 12h16M12 4v16"/><path d="M14.5 14.5l3 3M17.5 14.5l-3 3"/>',
 'switch-warcraft': '<path d="M3 7a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/><path d="M9 13h6M13 11l2 2-2 2"/>',
 'search': '<circle cx="11" cy="11" r="6"/><path d="M16 16l5 5"/>',
}
for n, body in ICONS.items():
    (here / 'icons' / f'{n}.svg').write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">{body}</svg>\n', encoding='utf-8')

def ic(n, cls=''):
    return f'<svg class="i {cls}" viewBox="0 0 24 24">{ICONS[n]}</svg>'

CHECK = '<svg viewBox="0 0 10 10"><path d="M2 5.5l2 2 4-5"/></svg>'

def head(title, direction='flat', theme='dark', tex=False):
    return f'''<!doctype html><html lang="en" data-dir="{direction}" data-theme="{theme}"{' data-tex="1"' if tex else ''}><head><meta charset="utf-8"><title>{title}</title>
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600&display=swap" rel="stylesheet">
<link rel="stylesheet" href="tokens.css"><link rel="stylesheet" href="widgets.css">{'<link rel="stylesheet" href="textured.css">' if tex else ''}
<script>if(location.hash==="#light")document.documentElement.dataset.theme="light";</script>'''

def tb(n, state=''):
    return f'<div class="tbtn {state}">{ic(n)}</div>'
def drop(n, state=''):
    return f'<div class="tbtn drop {state}">{ic(n)}<div class="arrow"></div></div>'

def toolbar():
    sep = '<div class="tsep"></div>'
    return ('<div class="toolbar">' + tb('new') + drop('open') + tb('save', 'hover') + tb('test-map') + sep
      + tb('undo') + tb('redo', 'disabled') + sep
      + tb('units', 'checked') + tb('doodads', 'checked') + tb('pathing') + tb('lighting', 'checked') + tb('water', 'checked')
      + tb('wireframe') + tb('minimap') + tb('reset-camera') + sep
      + tb('trigger-editor') + tb('object-editor') + tb('model-editor') + tb('asset-manager')
      + '<div class="grow"></div>'
      + f'<div class="edit" style="width:260px">{ic("search","s")}<span class="ph">Search actions  Shift Shift</span></div></div>')

def menubar():
    return '<div class="menubar">' + ''.join(f'<div class="mi">{m}</div>' for m in ['File','Edit','View','Terrain','Map','Tools']) + '</div>'

def dock(tabs, body, style='', bodycls='dock-body', sel=0):
    t = ''.join(f'<div class="dtab {"selected" if i==sel else ""}">{x}</div>' for i, x in enumerate(tabs))
    return (f'<div class="dock" style="{style}"><div class="dock-title"><div class="dt">{t}</div>'
            f'<div class="db"><div class="arrow"></div><span>&#10005;</span></div></div><div class="{bodycls}">{body}</div></div>')

def chk(label, on=False):
    return f'<div class="chk {"on" if on else ""}"><div class="box">{CHECK if on else ""}</div><span>{label}</span></div>'

def tilelist():
    tiles = [('Ldrt','Dirt','#7b5a3a'),('Ldro','Rough Dirt','#8a6a46'),('Ldrg','Dirt Grass','#6b6a3c'),('Lrok','Rock','#6e6e6e'),
             ('Lgrs','Grass','#4f7a3a'),('Lgrd','Dark Grass','#3a5c2c'),('Lsan','Sand','#b8a272')]
    rows = ''
    for i, (tid, name, col) in enumerate(tiles):
        cls = 'sel' if i == 4 else ('hover' if i == 2 else '')
        rows += f'<div class="r {cls}"><div><span class="sw" style="background:{col}"></span>{name}</div><div class="grow"></div><div class="id">{tid}</div></div>'
    return f'<div class="view grow"><div class="hdr"><div>Tile</div><div class="grow"></div><div>ID</div></div>{rows}</div>'

def propview():
    def r(k, v, cls=''):
        return f'<div class="r {cls}"><div style="width:120px">{k}</div><div class="grow">{v}</div></div>'
    rows = (r('Unit','', 'sub') + r('Type','hfoo (Footman)') + r('Owner','Player 1 (Red)') + r('Facing','270.0') + r('Hit points','420 / 420', 'sel')
            + r('Position','', 'sub') + r('X','1234.0') + r('Y','-512.0') + r('Z','128.0') + r('Custom','', 'sub')
            + r('Hero level','1') + r('Waygate','Inactive','dis'))
    return f'<div class="view grow"><div class="hdr"><div style="width:120px">Property</div><div>Value</div></div>{rows}</div>'

def minimap():
    return ('<div class="viewport" style="height:180px;border:1px solid var(--color-border)">'
      '<div class="patch" style="left:0;top:0;width:100%;height:100%;background:#2c4a2f"></div>'
      '<div class="patch" style="left:20px;top:30px;width:70px;height:50px;background:#3f6b3a"></div>'
      '<div class="patch" style="left:110px;top:90px;width:60px;height:70px;background:#7b5a3a"></div>'
      '<div class="patch" style="left:30px;top:100px;width:50px;height:40px;background:#2a5a8a"></div>'
      '<div class="patch" style="left:60px;top:40px;width:60px;height:40px;border:1px solid #fff"></div></div>')

def main_window(direction):
    left = (f'<div class="col sidebar" style="width:300px;gap:4px">'
      '<div class="row" style="gap:4px">' + ''.join(
         f'<div class="btn mode {"checked" if n=="terrain" else ""}">{ic(n,"s")}<span>{l}</span></div>'
         for n, l in [('terrain','Terrain'),('doodads','Doodads'),('units','Units')]) + '</div>'
      '<div class="row" style="gap:4px">' + ''.join(
         f'<div class="btn mode" style="flex:0 0 97.33px">{ic(n,"s")}<span>{l}</span></div>' for n, l in [('pathing','Pathing'),('regions','Regions')]) + '<div style="flex:1 1 0"></div></div>'
      + dock(['Minimap'], minimap(), 'height:206px')
      + dock(['Terrain Palette'],
          '<div class="tabbar" style="margin:-8px -8px 0"><div class="tab selected">Texture</div><div class="tab">Cliff</div><div class="tab">Deformation</div><div class="tab hover">Cell</div></div>'
          '<div class="row" style="gap:8px"><span class="muted">Tileset</span><div class="combo grow"><span>Lordaeron Summer</span><div class="dd"><div class="arrow"></div></div></div></div>'
          '<div class="edit focus"><span class="ph">Filter tiles</span></div>'
          + tilelist() +
          '<div class="group"><div class="gt">Brush Size</div><div class="row" style="gap:4px">'
          + ''.join(f'<div class="btn {"checked" if s==3 else ""}" style="flex:1;padding:0">{s}</div>' for s in (1,3,5,7,9))
          + '</div></div>' + chk('Random variations', True) + chk('Show tile ids'),
          'flex:1 1 auto') + '</div>')
    center = ('<div class="viewport grow" style="border:1px solid var(--color-border)">'
      '<div class="patch" style="left:0;top:0;right:0;bottom:0;background:var(--color-viewport)"></div>'
      '<div class="patch" style="left:120px;top:90px;width:520px;height:330px;background:#3d5236"></div>'
      '<div class="patch" style="left:300px;top:200px;width:420px;height:280px;background:#4a6340"></div>'
      '<div class="patch" style="left:640px;top:380px;width:360px;height:240px;background:#6b5638"></div>'
      '<div class="patch" style="left:60px;top:500px;width:300px;height:210px;background:#2b4766"></div>'
      '<div class="patch" style="left:520px;top:300px;width:96px;height:96px;border:2px solid #ffffff"></div>'
      '<div class="hud" style="left:8px;top:8px">Viewport (OpenGL, engine-rendered, not styled by QSS)</div>'
      '<div class="hud" style="left:8px;bottom:8px">X 1234.0   Y -512.0   Z 128.0</div></div>')
    right = ('<div class="col" style="width:300px;gap:4px">'
      + dock(['Properties', 'Regions'], propview(), 'flex:1 1 auto')
      + dock(['Preview'], '<div class="viewport" style="height:200px;border:1px solid var(--color-border)"><div class="hud" style="left:6px;top:6px">Footman (hfoo)</div></div>', 'height:240px')
      + '</div>')
    status = ('<div class="statusbar"><span style="padding-right:8px">Ready</span><div class="grow"></div>'
      '<div class="seg">Cursor 1234.0, -512.0</div><div class="seg">Tileset Lordaeron Summer</div><div class="seg">Map 128 x 128</div>'
      '<div class="seg">Terrain brush 3x3</div><div class="seg" style="color:var(--color-warn)">Unsaved changes</div></div>')
    body = f'<div class="row grow" style="align-items:stretch;gap:4px;padding:4px;background:var(--color-window);min-height:0">{left}{center}{right}</div>'
    return f'<div class="col" style="width:1600px;height:1000px">{menubar()}{toolbar()}{body}{status}</div>'

for direction, fname, t in [('flat','main-window-flat-dark.html','A Flat Dark IDE'),('warcraft','main-window-warcraft.html','B Warcraft gold and stone')]:
    (here / fname).write_text(head(f'HiveWE main window - {t}', direction) + '</head><body>' + main_window(direction) + '</body></html>', encoding='utf-8')

(here / 'main-window-warcraft-textured.html').write_text(head('HiveWE main window - B2 Warcraft Textured', 'warcraft', tex=True) + '</head><body>' + main_window('warcraft') + '</body></html>', encoding='utf-8')

# ---------------- tokens.html ----------------
css = (here / 'tokens.css').read_text(encoding='utf-8')
def block(sel_re):
    m = re.search(sel_re + r'\s*\{(.*?)\}', css, re.S)
    return dict(re.findall(r'(--[\w-]+):\s*([^;]+);', m.group(1)))
shared = block(r':root')
def panel(direction, theme, label):
    vals = block(r'\[data-dir="%s"\]\[data-theme="%s"\]' % (direction, theme))
    shape = block(r'\[data-dir="%s"\]' % direction)
    sw = ''.join(f'<div style="display:flex;align-items:center;gap:8px;height:22px"><div style="width:44px;height:18px;background:{v};border:1px solid var(--color-border-strong)"></div>'
                 f'<span style="width:190px">{k}</span><span class="muted">{v}</span></div>' for k, v in vals.items() if v.startswith('#'))
    sh = ''.join(f'<div style="height:18px"><span style="display:inline-block;width:190px">{k}</span><span class="muted">{v}</span></div>' for k, v in list(shape.items()) + list(shared.items()))
    samples = ('<div class="toolbar" style="border:1px solid var(--color-border)">' + tb('save') + tb('undo','hover') + tb('units','checked') + tb('redo','disabled') + '<div class="tsep"></div>'
      + f'<div class="edit grow">{ic("search","s")}<span class="ph">Search actions</span></div></div>'
      '<div class="row" style="gap:8px"><div class="btn">Cancel</div><div class="btn hover">Apply</div><div class="btn default">OK</div><div class="btn disabled">Disabled</div>'
      '<div class="combo" style="width:150px"><span>Lordaeron Summer</span><div class="dd"><div class="arrow"></div></div></div></div>'
      '<div class="tabbar"><div class="tab selected">Texture</div><div class="tab">Cliff</div><div class="tab hover">Cell</div></div>'
      '<div class="row" style="gap:12px;align-items:flex-start">' + dock(['Terrain Palette'], tilelist(), 'width:260px;height:200px', 'dock-body')
      + '<div class="menu" style="width:250px">'
        f'<div class="m"><span>Open Map (Folder)</span><span class="sc">Ctrl+O</span><span class="ic">{ic("open","s")}</span></div>'
        f'<div class="m sel"><span>Save Map</span><span class="sc">Ctrl+S</span><span class="ic">{ic("save","s")}</span></div>'
        f'<div class="m dis"><span>Redo</span><span class="sc">Ctrl+Y</span><span class="ic">{ic("redo","s")}</span></div><div class="ms"></div>'
        f'<div class="m"><span>Exit</span><span class="sc"></span><span class="ic">{ic("exit","s")}</span></div></div></div>')
    return (f'<div data-dir="{direction}" data-theme="{theme}" style="background:var(--color-window);color:var(--color-text);padding:16px;border:1px solid #888;width:780px">'
            f'<div style="font-size:16px;font-weight:600;margin-bottom:10px">{label}</div>'
            f'<div style="display:flex;gap:16px"><div style="width:330px">{sw}</div><div class="col" style="gap:10px;width:380px">{samples}</div></div>'
            f'<div style="margin-top:12px;font-size:11px;column-count:2">{sh}</div></div>')
tk = (head('HiveWE tokens') + '</head><body style="background:#777;width:1600px;padding:8px"><div style="display:grid;grid-template-columns:780px 780px;gap:12px">'
      + panel('flat','dark','A Flat Dark IDE - Dark') + panel('flat','light','A Flat Dark IDE - Light')
      + panel('warcraft','dark','B Warcraft gold and stone - Dark') + panel('warcraft','light','B Warcraft gold and stone - Light')
      + '</div></body></html>')
(here / 'tokens.html').write_text(tk, encoding='utf-8')

# ---------------- toolbar-command-map.html ----------------
TB = 'Toolbar'
rows = [
 ('History','Undo','Ctrl+Z',f'{TB} (History), Edit menu','undo'),
 ('History','Redo','Ctrl+Y',f'{TB} (History), Edit menu','redo'),
 ('Home / Editors','Trigger Editor','',f'{TB} (Editors), Tools menu','trigger-editor'),
 ('Home / Editors','Object Editor','O',f'{TB} (Editors), Tools menu','object-editor'),
 ('Home / Editors','Model Editor','',f'{TB} (Editors), Tools menu','model-editor'),
 ('Home / Editors','Asset Manager','',f'{TB} (Editors), Tools menu','asset-manager'),
 ('Home / Editors','Tooltip Editor','','Tools menu only','tooltip-editor'),
 ('Home / Config','Export Objects','','Tools menu > Object Data','export-objects'),
 ('Home / Config','Import Objects','','Tools menu > Object Data','import-objects'),
 ('Home / Config','Map Protector','','Tools menu only','map-protector'),
 ('Home / Config','Config','','Tools menu only','config'),
 ('Terrain / Palette','Terrain palette','T','Sidebar mode button, View menu > Palettes','terrain'),
 ('Terrain / Terrain Tools','Import Heightmap','','Terrain menu','import-heightmap'),
 ('Terrain / Terrain Tools','Change Tileset','','Terrain menu','change-tileset'),
 ('Terrain / Terrain Tools','Change Tile Pathing','','Terrain menu','change-tile-pathing'),
 ('Doodads / Palette','Doodads palette','D','Sidebar mode button, View menu > Palettes','doodads'),
 ('Doodads / Viewport','Doodads (visible)','Ctrl+D','Merged with View / Visible (below)','doodads'),
 ('Doodads / Viewport','Click Helpers (visible)','Ctrl+I','View menu only','click-helpers'),
 ('Doodads / Viewport','Object Editor (mirror)','O','Merged with Editors (above)','object-editor'),
 ('Units / Palette','Units palette','U','Sidebar mode button, View menu > Palettes','units'),
 ('Units / Viewport','Units (visible)','Ctrl+U','Merged with View / Visible (below)','units'),
 ('Units / Viewport','Minimap (mirror)','','Merged with View / Visible (below)','minimap'),
 ('Units / Data','Item Tables','','Map menu','item-tables'),
 ('Pathing / Palette','Pathing palette','P','Sidebar mode button, View menu > Palettes','pathing'),
 ('Pathing / Overlay','Pathing (visible)','Ctrl+P','Merged with View / Visible (below)','pathing'),
 ('Pathing / Overlay','Brush (visible)','','View menu only','brush'),
 ('Pathing / Overlay','Wireframe (mirror)','Ctrl+T','Merged with View / Visible (below)','wireframe'),
 ('Regions / Palette','Regions palette','R','Sidebar mode button, View menu > Palettes','regions'),
 ('Regions / Map Context','Description (mirror)','','Merged with Map (below)','description'),
 ('Regions / Map Context','Options (mirror)','','Merged with Map (below)','map-options'),
 ('View / Visible','Units','Ctrl+U',f'{TB} (Visibility), View menu','units'),
 ('View / Visible','Doodads','Ctrl+D',f'{TB} (Visibility), View menu','doodads'),
 ('View / Visible','Pathing','Ctrl+P',f'{TB} (Visibility), View menu','pathing'),
 ('View / Visible','Brush','','View menu only','brush'),
 ('View / Visible','Lighting','Ctrl+L',f'{TB} (Visibility), View menu','lighting'),
 ('View / Visible','Water','Ctrl+W',f'{TB} (Visibility), View menu','water'),
 ('View / Visible','Click Helpers','Ctrl+I','View menu only','click-helpers'),
 ('View / Visible','Wireframe','Ctrl+T',f'{TB} (Visibility), View menu','wireframe'),
 ('View / Visible','Debug','F3','View menu only','debug'),
 ('View / Visible','Minimap','',f'{TB} (Visibility), View menu','minimap'),
 ('View / Camera','Reset Camera','Ctrl+Shift+C',f'{TB} (Camera), View menu','reset-camera'),
 ('Map / Map','Description','','Map menu','description'),
 ('Map / Map','Loading Screen','','Map menu','loading-screen'),
 ('Map / Map','Options','','Map menu','map-options'),
 ('Map / Map','Gameplay Constants','','Map menu','gameplay-constants'),
 ('Map / Scenario','Players / Forces','','Map menu','players-forces'),
 ('Map / Game','Change Game Folder','','Tools menu','switch-warcraft'),
 ('File menu','New Map','',f'{TB} (File), File menu','new'),
 ('File menu','Open Map (Folder)','Ctrl+O',f'{TB} (File, split button), File menu','open'),
 ('File menu','Open Map (MPQ)','','File menu, also in the Open split button','open-mpq'),
 ('File menu','Recent maps (3 slots)','','File menu > Recent Maps submenu','recent'),
 ('File menu','Save Map','Ctrl+S',f'{TB} (File), File menu','save'),
 ('File menu','Save Map As','Ctrl+Shift+S','File menu only','save-as'),
 ('File menu','Export MPQ','','File menu only','export-mpq'),
 ('File menu','Test Map','',f'{TB} (File), File menu','test-map'),
 ('File menu','Settings','','File menu only','settings'),
 ('File menu','Exit','','File menu only','exit'),
 ('Ribbon corner','Open / Save / Test Map quick buttons','Ctrl+O / Ctrl+S','Merged with File buttons on the toolbar','save'),
 ('Ribbon corner','Search actions','Double Shift','Toolbar, right-aligned search field','search'),
]
tr = ''.join(f'<tr><td>{g}</td><td>{c}</td><td>{s or "<span class=muted>none</span>"}</td><td>{l}</td><td><span class="row" style="gap:8px">{ic(i)}<span class="muted small">icons/{i}.svg</span></span></td></tr>' for g, c, s, l, i in rows)
cm = (head('HiveWE ribbon to toolbar command map') + '''<style>
body{width:1600px;padding:16px} table{border-collapse:collapse;width:100%} th,td{text-align:left;padding:0 8px;height:30px;border-bottom:1px solid var(--color-border)}
th{background:var(--color-titlebar-top);color:var(--color-text-muted);font-weight:600;position:static}
tr:nth-child(even) td{background:var(--color-alt-base)} h1{font-size:16px;font-weight:600;margin-bottom:6px} p{margin-bottom:10px;color:var(--color-text-muted);line-height:1.4}
</style></head><body><h1>Ribbon command map (src/main_window/main_ribbon.cpp) to compact toolbar and menus</h1>
<p>Menu bar: File, Edit, View, Terrain, Map, Tools. Toolbar groups left to right: File, History, Visibility, Camera, Editors, search field. Palette modes (Terrain, Doodads, Units, Pathing, Regions) live on the existing sidebar mode buttons.
Commands that appear in several ribbon tabs (mirror_button) collapse to one place. Shortcuts are the ones wired in hivewe.cpp. Note: HiveWE.ui also gives the Test Map action Ctrl+T, but hivewe.cpp binds Ctrl+T to Wireframe; whether the .ui action is live is unverified, so Test Map is shown without a shortcut.</p>
<table><tr><th style="width:210px">Ribbon group</th><th style="width:300px">Command</th><th style="width:240px">Current shortcut</th><th>New location</th><th style="width:240px">Icon</th></tr>'''
      + tr + '</table></body></html>')
(here / 'toolbar-command-map.html').write_text(cm, encoding='utf-8')
print(len(ICONS), 'icons', len(rows), 'rows')