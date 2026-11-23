# Generates the secondary-window mockup pages (<page>-flat.html) and the extra icons. Run: python build_windows.py
# Reuses ICONS/ic/head/chk from build.py without re-running its page output. Dark is the default theme; open the page with #light for Light.
import pathlib, html
here = pathlib.Path(__file__).parent
_src = (here / 'build.py').read_text(encoding='utf-8').split('def main_window')[0]
exec(_src)

EXTRA = {
 'generate': '<path d="M8 4c-2 0-3 1-3 3v2c0 1.5-1 2.5-2 3 1 .5 2 1.5 2 3v2c0 2 1 3 3 3"/><path d="M16 4c2 0 3 1 3 3v2c0 1.5 1 2.5 2 3-1 .5-2 1.5-2 3v2c0 2-1 3-3 3"/><path d="M9.5 12l2 2 3.5-4"/>',
 'folder-new': '<path d="M3 7a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/><path d="M12 11v5M9.5 13.5h5"/>',
 'trigger-gui': '<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4"/><path d="M12.5 10l-3 4h3l-1 3.5 3.5-4.5h-3z"/>',
 'trigger-script': '<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4"/><path d="M10.5 12l-2 2 2 2M13.5 12l2 2-2 2"/>',
 'comment': '<path d="M4 5h16v11H11l-4 4v-4H4z"/>',
 'variable': '<rect x="4" y="5" width="16" height="14" rx="1.5"/><path d="M9 9l6 6M15 9l-6 6"/>',
 'folder': '<path d="M3 7a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/>',
 'warn': '<path d="M12 4l9 16H3z"/><path d="M12 10v4M12 17h.01"/>',
 'error': '<circle cx="12" cy="12" r="9"/><path d="M9 9l6 6M15 9l-6 6"/>',
 'question': '<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.7.4-1 .9-1 1.7M12 17h.01"/>',
 'star': '<path d="M12 4l2.5 5 5.5.8-4 3.9.9 5.5-4.9-2.6-4.9 2.6.9-5.5-4-3.9 5.5-.8z"/>',
 'copy': '<rect x="8" y="8" width="12" height="12" rx="1"/><path d="M16 8V5a1 1 0 0 0-1-1H5a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h3"/>',
 'trash': '<path d="M5 7h14M10 7V4h4v3M7 7l1 13h8l1-13"/>',
 'plus': '<path d="M12 5v14M5 12h14"/>',
 'refresh': '<path d="M20 12a8 8 0 1 1-2.5-5.8"/><path d="M20 4v5h-5"/>',
}
ICONS.update(EXTRA)
for _n, _b in EXTRA.items():
    (here / 'icons' / f'{_n}.svg').write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">{_b}</svg>\n', encoding='utf-8')

def esc(s): return html.escape(str(s), quote=False)

# ---------------- widget helpers ----------------
def edit(t='', style='', cls='', ph=False):
    return f'<div class="edit {cls}" style="{style}"><span class="{"ph" if ph else ""}">{esc(t)}</span></div>'
def combo(t, style='', cls=''):
    return f'<div class="combo {cls}" style="{style}"><span>{esc(t)}</span><div class="dd"><div class="arrow"></div></div></div>'
def spin(v, style='', cls=''):
    return f'<div class="spin {cls}" style="{style}"><span>{esc(v)}</span><div class="ud"><div class="up"></div><div class="dn"></div></div></div>'
def btn(t, kind='', style='', icon=None):
    i = ic(icon, 's') if icon else ''
    return f'<div class="btn {kind}" style="{style}">{i}<span>{esc(t)}</span></div>'
def rad(t, on=False):
    return f'<div class="rad {"on" if on else ""}"><div class="dot"></div><span>{esc(t)}</span></div>'
def ck(t, on=False, dis=False):
    return f'<div class="chk {"on" if on else ""} {"dis" if dis else ""}"><div class="box">{CHECK if on else ""}</div><span>{esc(t)}</span></div>'
def frow(label, ctrl, lw=120, top=False, style=''):
    return f'<div class="frow" style="{style}"><div class="l {"top" if top else ""}" style="width:{lw}px">{esc(label)}</div>{ctrl}</div>'
def group(title, body, style='', gap=8):
    return f'<div class="group" style="{style};display:flex;flex-direction:column;gap:{gap}px"><div class="gt">{esc(title)}</div>{body}</div>'
def chip(t, on=False): return f'<div class="chip {"checked" if on else ""}">{esc(t)}</div>'
def badge(t, k=''): return f'<span class="badge {k}">{esc(t)}</span>'
def swatch(c): return f'<div class="ic16" style="background:{c}"></div>'
def cell(h, style=''): return f'<div style="{style}">{h}</div>'
def col(body, style='', cls=''): return f'<div class="col {cls}" style="{style}">{body}</div>'
def row(body, style='', cls=''): return f'<div class="row {cls}" style="{style}">{body}</div>'

def tabwrap(names, sel, body, style='', hover=None):
    t = ''.join(f'<div class="tab {"selected" if i==sel else ("hover" if i==hover else "")}">{esc(n)}</div>' for i, n in enumerate(names))
    return f'<div class="tabwrap" style="{style}"><div class="tabbar" style="flex:none">{t}</div><div class="tabpane">{body}</div></div>'

def table(headers, rows, widths, style='', sb=None, cls=''):
    def cw(i):
        w = widths[i]
        return f'width:{w}px;flex:none' if w else 'flex:1 1 0;min-width:0'
    h = ''
    if headers:
        h = '<div class="hdr">' + ''.join(f'<div style="{cw(i)}">{esc(x)}</div>' for i, x in enumerate(headers)) + '</div>'
    r = ''
    for rw in rows:
        cells, c = rw if isinstance(rw, tuple) else (rw, '')
        r += f'<div class="r {c}">' + ''.join(f'<div class="c" style="{cw(i)}">{x}</div>' for i, x in enumerate(cells)) + '</div>'
    s = ''
    if sb:
        s = f'<div class="sb"><div class="h" style="top:{sb[0]}px;height:{sb[1]}px"></div></div>'
    return f'<div class="view {cls} {"hassb" if sb else ""}" style="{style}">{h}{r}{s}</div>'

def trow(depth, state, icon, label, extra=''):
    return (f'<span style="width:{depth*16}px;flex:none"></span><div class="tw {state}"></div>{icon}<span>{esc(label)}</span>{extra}')

def ico16(c): return f'<div class="ic16" style="background:{c};margin-right:6px"></div>'
def fold(): return ic('folder', 't') .replace('class="i t"', 'class="i t" style="margin-right:6px;color:var(--color-text-muted)"')
def sico(n, color='var(--color-text-muted)'): return ic(n, 't').replace('class="i t"', f'class="i t" style="margin-right:6px;color:{color}"')

def win(title, w, h, body, icon='object-editor', maxbtn=True, style=''):
    mb = '<div class="wb"><svg viewBox="0 0 10 10"><rect x="0.5" y="0.5" width="9" height="9"/></svg></div>' if maxbtn else ''
    mn = '<div class="wb"><svg viewBox="0 0 10 10"><path d="M0 5h10"/></svg></div>' if maxbtn else ''
    return (f'<div class="win" style="width:{w}px;height:{h}px;{style}"><div class="wtitle">{ic(icon,"s")}<span>{esc(title)}</span><div class="grow"></div>'
            f'{mn}{mb}<div class="wb"><svg viewBox="0 0 10 10"><path d="M0 0l10 10M10 0L0 10"/></svg></div></div><div class="wbody">{body}</div></div>')

def dlg(title, w, h, body, buttons=('OK', 'Cancel'), icon='settings', default=0, pad=True, style=''):
    b = ''.join(btn(t, 'default' if i == default else '') for i, t in enumerate(buttons))
    inner = f'<div class="flex1 col {"pad" if pad else ""} gap8" style="min-height:0">{body}</div><div class="dlgbtns">{b}</div>'
    return win(title, w, h, inner, icon, maxbtn=False, style=style)

def cap(t): return f'<div class="cap">{esc(t)}</div>'
def labelled(caption, node): return cap(caption) + node

def dock_(tabs, body, style='', sel=0, bodycls='dock-body'):
    return dock(tabs, body, style, bodycls, sel)

PAGES = []
def page(name, title, content, w, h):
    doc = (head(f'HiveWE {title} - Flat') + '<link rel="stylesheet" href="windows.css"></head><body style="width:%dpx;height:%dpx">' % (w, h)
           + content + '</body></html>')
    (here / f'{name}-flat.html').write_text(doc, encoding='utf-8')
    PAGES.append((name, title, w, h))

def stackH(hs): return 32 + sum(18 + h for h in hs) + 14 * (len(hs) - 1)

# ============================================================ 1. Object Editor
def oe_tree():
    R = [
     (0, 'o', swatch_c('#7b5a3a'), 'Custom Units', ''),
     (1, 'n', swatch_c('#b8a272'), 'Elite Footman', 'h000 sel'),
     (1, 'n', swatch_c('#6e6e6e'), 'Veteran Rifleman', 'h001'),
     (1, 'n', swatch_c('#4f7a3a'), 'Orc Warlord', 'o000'),
     (0, 'o', fold(), 'Human', ''),
     (1, 'c', fold(), 'Heroes', ''),
     (1, 'o', fold(), 'Melee', ''),
     (2, 'n', swatch_c('#6e6e6e'), 'Footman', 'hfoo hover'),
     (2, 'n', swatch_c('#8a6a46'), 'Knight', 'hkni'),
     (2, 'n', swatch_c('#4f7a3a'), 'Rifleman', 'hrif'),
     (2, 'n', swatch_c('#b8a272'), 'Priest', 'hmpr'),
     (2, 'n', swatch_c('#7ebff1'), 'Sorceress', 'hsor'),
     (2, 'n', swatch_c('#6e6e6e'), 'Mortar Team', 'hmtm'),
     (2, 'n', swatch_c('#8a6a46'), 'Gryphon Rider', 'hgry'),
     (1, 'c', fold(), 'Campaign', ''),
     (0, 'c', fold(), 'Orc', ''),
     (0, 'c', fold(), 'Undead', ''),
     (0, 'c', fold(), 'Night Elf', ''),
     (0, 'c', fold(), 'Neutral Hostile', ''),
     (0, 'c', fold(), 'Neutral Passive', ''),
    ]
    rows = []
    for d, s, icn, lab, tag in R:
        parts = tag.split()
        rid = parts[0] if parts else ''
        cls = ' '.join(parts[1:])
        idh = f'<div class="grow"></div><span class="id">{rid}</span>' if rid else ''
        rows.append(([trow(d, s, icn, lab, idh)], cls))
    return table(None, rows, [None], 'flex:1 1 auto', sb=(2, 150))
def swatch_c(c): return f'<div class="ic16" style="background:{c};margin-right:6px"></div>'

def object_editor():
    tabs = ['Units', 'Items', 'Doodads', 'Destructibles', 'Abilities', 'Upgrades', 'Buffs']
    left_body = (
      row(f'<span class="title2 grow">Units</span><span class="muted small">1,396 objects</span>', 'gap:8px')
      + f'<div class="edit focus" style="width:auto"><span class="ph">Search Units</span></div>'
      + row(chip('All', True) + chip('Custom') + '<div class="grow"></div>' + btn('Reveal', 'sm') + btn('Reset', 'sm'), 'gap:4px')
      + oe_tree()
      + col(row(f'<span class="title2 grow">Elite Footman</span>{badge("Custom","cus")}', 'gap:8px')
            + f'<span class="muted small">Raw ID h000  |  Based on Footman (hfoo)</span>'
            + row(btn('Copy ID', 'sm'), 'gap:4px'), 'gap:6px;padding:8px;border:1px solid var(--color-border);border-radius:3px;background:var(--color-base)')
    )
    left = (f'<div class="col" style="width:560px;gap:4px;flex:none">' + dock_(tabs, left_body, 'flex:1 1 auto') + '</div>')

    fields = []
    def sec(t): fields.append(([f'<b>{t}</b>', ''], 'sub'))
    def f(k, v, mod=False, sel=False, hov=False): fields.append(([k, f'<span class="v">{esc(v)}</span>'], ('mod ' if mod else '') + ('sel ' if sel else '') + ('hover' if hov else '')))
    sec('Stats')
    f('Name', 'Elite Footman', True)
    f('Hit Points Maximum (Base)', '520', True, True)
    f('Mana Maximum', '0')
    f('Defense Base', '2')
    f('Defense Type', 'Heavy')
    f('Level', '2')
    f('Gold Cost', '165', True)
    f('Lumber Cost', '0')
    f('Food Used', '2')
    f('Build Time', '20')
    sec('Combat')
    f('Attack 1 - Damage Base', '14', True)
    f('Attack 1 - Number of Dice', '1')
    f('Attack 1 - Sides per Die', '2')
    f('Attack 1 - Cooldown Time', '1.35')
    f('Attack 1 - Range', '90', False, False, True)
    f('Abilities - Normal', 'Defend (Adef)')
    sec('Movement')
    f('Speed Base', '270')
    f('Movement Type', 'Foot')
    sec('Art')
    f('Model File', 'units\\human\\footman\\footman')
    f('Icon - Game Interface', 'ReplaceableTextures\\CommandButtons\\BTNFootman.blp')
    ftab = table(['Field', 'Value'], fields, [330, None], 'flex:1 1 auto', sb=(2, 250))
    summary = row(
        '<div class="ic16" style="width:48px;height:48px;background:#b8a272;flex:none"></div>'
        + col(row('<span class="title1">Elite Footman</span>' + badge('Modified', 'mod'), 'gap:8px')
              + row('<span class="muted" style="width:90px">Raw ID</span><span>h000</span>', 'height:18px')
              + row('<span class="muted" style="width:90px">Parent</span><span>Units</span>', 'height:18px')
              + row('<span class="muted" style="width:90px">Base Object</span><span>Footman (hfoo)</span>', 'height:18px'), 'gap:3px;flex:1 1 auto')
        + col(row(btn('Copy ID', '', '', 'copy') + btn('Open Parent') + btn('Modified Fields', 'checked') + btn('Unbookmark', '', '', 'star'), 'gap:4px'), 'align-items:flex-end'),
        'gap:12px;align-items:flex-start')
    bookmarks = row('<span class="sect" style="margin-right:4px">Bookmarks</span>' + btn('Elite Footman', 'sm') + btn('Paladin', 'sm') + btn('Peasant', 'sm') + btn('Archmage', 'sm'), 'gap:4px')
    filters = (
        row(f'<div class="edit grow"><span class="ph">Search fields</span></div>' + combo('All sections', 'width:140px') + chip('Modified', True) + chip('Core') + btn('Reset Filters', 'sm'), 'gap:4px')
        + row(''.join(chip(x, i == 0) for i, x in enumerate(['All', 'Stats', 'Combat', 'Movement', 'Art', 'Pathing', 'Sound', 'Text', 'Techtree', 'Editor'])), 'gap:4px')
        + '<div class="muted small">No active field filters</div>')
    foot = col(row(f'<span class="title2 grow">Hit Points Maximum (Base)</span>{badge("Modified","mod")}', 'gap:8px')
               + '<span class="muted small">Raw ID uhpm  |  Base value 420</span>' + row(btn('Copy Field ID', 'sm')),
               'gap:6px;padding:8px;border:1px solid var(--color-border);border-radius:3px;background:var(--color-base)')
    center = (f'<div class="dock" style="flex:1 1 0;min-width:0"><div class="dock-title"></div><div class="dock-body">{summary}<div class="sep-h"></div>{bookmarks}{filters}{ftab}{foot}</div></div>')
    body = f'<div class="dockhost">{left}{center}</div>'
    return win('Object Editor', 1568, 900, body, 'object-editor')

# ============================================================ 2. Trigger Editor
def trig_toolbar():
    sep = '<div class="tsep"></div>'
    return ('<div class="toolbar">' + tb('generate', 'hover') + sep + tb('folder-new') + tb('trigger-gui') + tb('trigger-script') + tb('comment') + tb('variable')
            + '<div class="grow"></div><span class="muted small">Generate Script  Ctrl+F10</span></div>')

def explorer():
    EV, CO, AC = 'var(--color-warn)', 'var(--color-ok)', 'var(--color-accent)'
    R = [
     (0, 'n', sico('trigger-script'), 'Map Header', ''),
     (0, 'o', fold(), 'Initialization', ''),
     (1, 'n', sico('trigger-gui'), 'Melee Initialization', ''),
     (1, 'n', sico('trigger-gui'), 'Setup Variables', ''),
     (0, 'o', fold(), 'Heroes', ''),
     (1, 'n', sico('trigger-gui'), 'Hero Revive', 'sel'),
     (1, 'n', sico('trigger-gui'), 'Hero Level Up', ''),
     (1, 'n', sico('trigger-script', 'var(--color-text-disabled)'), 'Spawn Creeps', 'dim'),
     (0, 'o', fold(), 'Variables', ''),
     (1, 'n', sico('variable'), 'CreepCount  (Integer)', 'hover'),
     (1, 'n', sico('variable'), 'PlayerHero  (Unit Array [12])', ''),
     (1, 'n', sico('variable'), 'ReviveAltar  (Region)', ''),
     (0, 'c', fold(), 'Boss Waves', ''),
     (0, 'n', sico('comment'), 'Notes: map credits', ''),
    ]
    rows = [([trow(d, s, i, lab)], c) for d, s, i, lab, c in R]
    return table(None, rows, [None], 'flex:1 1 auto', sb=(2, 180))

def eca(depth, label, icon, state='n', cls=''):
    return ([trow(depth, state, icon, label)], cls)

def gui_tree():
    EV = ico16('var(--color-warn)'); CO = ico16('var(--color-ok)'); AC = ico16('var(--color-accent)')
    sub = lambda: ''
    rows = [
     eca(0, 'Events', EV, 'o'),
     eca(1, 'Unit - A unit Dies', EV),
     eca(0, 'Conditions', CO, 'o'),
     eca(1, '((Triggering unit) is A Hero) Equal to True', CO),
     eca(0, 'Actions', AC, 'o'),
     eca(1, 'Set RevivingHero = (Triggering unit)', AC),
     eca(1, 'Wait 3.00 seconds', AC),
     eca(1, 'Hero - Instantly revive (Triggering unit) at (Center of ReviveAltar <gen>), Show revival graphics', AC, 'n', 'sel'),
     eca(1, 'Floating Text - Create floating text that reads Revived! above (Triggering unit) with Z offset 0.00, using font size 10.00, color (100.00%, 80.00%, 0.00%), and 0.00% transparency', AC),
     eca(1, 'If (All Conditions are True) then do (Then Actions) else do (Else Actions)', AC, 'o'),
     eca(2, 'If - Conditions', CO, 'o'),
     eca(3, '(Level of (Triggering unit)) Greater than 5', CO),
     eca(2, 'Then - Actions', AC, 'o'),
     eca(3, 'Hero - Set (Triggering unit) Hero-level to 5, Hide level-up graphics', AC),
     eca(2, 'Else - Actions', AC, 'o'),
     eca(3, 'Do nothing', AC, 'n', 'dim'),
    ]
    return table(None, rows, [None], 'flex:1 1 auto')

def ads_area(tabs, body, sel=0, style='', closable=True):
    t = ''.join(f'<div class="dtab {"selected" if i==sel else ""}">{esc(n)}{"<span class=x>&#10005;</span>" if closable and i==sel else ""}</div>' for i, n in enumerate(tabs))
    return (f'<div class="dock" style="{style}"><div class="dock-title"><div class="dt">{t}</div><div class="db"><div class="arrow"></div><span>&#10005;</span></div></div>'
            f'<div class="dock-body" style="padding:4px;gap:4px">{body}</div></div>')

def output_dock():
    lines = [
      ('cm', '12:04:31  Generating map script (Lua) ...'),
      ('', '12:04:31  3 triggers, 3 variables, 1 custom script'),
      ('er', '12:04:31  Error: trigger "Spawn Creeps", line 14: unexpected symbol near "=="'),
      ('cm', '12:04:31  Script generation failed: 1 error'),
    ]
    src = '\n'.join(f'<span class="{c}">{esc(t)}</span>' for c, t in lines)
    gut = '\n'.join(str(i + 1) for i in range(4))
    return ads_area(['Output'], f'<div class="code flex1"><div class="gut" style="display:none"></div><div class="src">{src}</div></div>', 0, 'height:150px;flex:none', closable=False)

def trigger_gui():
    explorer_dock = ads_area(['Trigger Explorer'], explorer(), 0, 'width:300px;flex:none', closable=False)
    comment = '<div class="text" style="height:52px;flex:none"><span>Revives a dead hero at the altar after 3 seconds. Used by all hero-type units.</span></div>'
    center = ads_area(['Hero Revive', 'Spawn Creeps', 'CreepCount'], comment + gui_tree(), 0, 'flex:1 1 0;min-height:0')
    right = col(center + output_dock(), 'flex:1 1 0;gap:4px;min-width:0;min-height:0')
    body = trig_toolbar() + f'<div class="dockhost">{explorer_dock}{right}</div>'
    return win('Trigger Editor', 1500, 780, body, 'trigger-editor')

def lua_lines():
    L = [
     ('<span class="cm">-- Spawn Creeps: Trigger runs every 30 seconds</span>',),
     ('<span class="kw">function</span> Trig_Spawn_Creeps_Actions()',),
     ('    <span class="kw">local</span> loc = GetRectCenter(gg_rct_CreepSpawn)',),
     ('    <span class="kw">for</span> i = 1, <span class="num">4</span> <span class="kw">do</span>',),
     ('        CreateUnitAtLoc(Player(<span class="num">12</span>), FourCC(<span class="str">"ngnb"</span>), loc, <span class="num">270.0</span>)',),
     ('        udg_CreepCount = udg_CreepCount + <span class="num">1</span>',),
     ('    <span class="kw">end</span>',),
     ('    RemoveLocation(loc)',),
     ('<span class="kw">end</span>',),
     ('',),
     ('<span class="kw">function</span> InitTrig_Spawn_Creeps()',),
     ('    gg_trg_Spawn_Creeps = CreateTrigger()',),
     ('    TriggerRegisterTimerEventPeriodic(gg_trg_Spawn_Creeps, <span class="num">30.00</span>)',),
     ('    <span class="kw">if</span> udg_CreepCount <span class="er">==</span> <span class="num">0</span> <span class="kw">then</span>',),
     ('        TriggerAddAction(gg_trg_Spawn_Creeps, Trig_Spawn_Creeps_Actions)',),
     ('    <span class="kw">end</span>',),
     ('<span class="kw">end</span>',),
    ]
    src = '\n'.join(l[0] for l in L)
    src = src.replace('udg_CreepCount', '<span class="hl">udg_CreepCount</span>')
    gut = '\n'.join(str(i + 1) for i in range(len(L)))
    return f'<div class="code flex1"><div class="gut">{gut}</div><div class="src">{src}</div></div>'

def trigger_script():
    find = (f'<div style="position:absolute;right:24px;top:6px;width:230px;padding:8px;background:var(--color-window);border:1px solid var(--color-border-strong);display:flex;flex-direction:column;gap:4px">'
            f'<div class="row gap4">{edit("udg_CreepCount", "flex:1 1 auto", "focus")}{btn("X", "ico")}</div>'
            f'<div class="row gap4">{btn("Aa", "ico checked")}{btn("ab", "ico")}{btn(".*", "ico")}<span class="muted small" style="margin-left:6px">2 matches</span></div></div>')
    comment = '<div class="text" style="height:30px;flex:none"><span class="ph">Optional comments here</span></div>'
    left = ads_area(['Map Header', 'Spawn Creeps'], comment + f'<div class="flex1 col" style="position:relative">{lua_lines()}{find}</div>', 1, 'flex:1 1 0;min-width:0')
    var = (col(frow('Variable Name:', edit('CreepCount', 'flex:1 1 auto'), 110) + frow('Variable Type:', edit('integer', 'flex:1 1 auto'), 110)
               + frow('', ck('Array', False), 110) + frow('Size:', spin('1', 'width:90px', 'dis'), 110) + frow('Initial Value:', edit('0', 'flex:1 1 auto'), 110), 'gap:8px;padding:8px'))
    right = ads_area(['CreepCount'], var, 0, 'width:360px;flex:none')
    body = f'<div class="dockhost">{left}{right}</div>'
    return win('Trigger Editor', 1500, 440, body, 'trigger-editor')

# ============================================================ 3. Model Editor (redesign)
GLBG = 'linear-gradient(to bottom,#4b4e54,#2a2c30)'
SIL = '#8b8f97'
def human_svg(w, h, grid=True):
    g = ''
    if grid:
        for i in range(-8, 9):
            g += f'<path d="M{w/2 + i*w/16*0.4:.0f} {h*0.62:.0f} L{w/2 + i*w/16*2.2:.0f} {h}" stroke="#6a6e75" stroke-width="1"/>'
        for y in [0.66, 0.72, 0.8, 0.9, 1.0]:
            g += f'<path d="M0 {h*y:.0f} H{w}" stroke="#6a6e75" stroke-width="1"/>'
    cx, cy = w / 2, h * 0.62
    s = h / 400
    fig = (f'<g fill="{SIL}"><circle cx="{cx}" cy="{cy-190*s}" r="{22*s}"/><path d="M{cx-34*s} {cy-160*s} h{68*s} l{14*s} {90*s} h{-24*s} l{4*s} {90*s} h{-26*s} l{-6*s} {-70*s} l{-6*s} {70*s} h{-26*s} l{4*s} {-90*s} h{-24*s}z"/>'
           f'<rect x="{cx+58*s}" y="{cy-170*s}" width="{6*s}" height="{150*s}"/><path d="M{cx-72*s} {cy-120*s} h{34*s} v{70*s} l{-17*s} {18*s} l{-17*s} {-18*s}z" fill="#a3a7ae"/></g>')
    return f'<svg viewBox="0 0 {w} {h}" width="{w}" height="{h}" style="position:absolute;left:0;top:0">{g}{fig}</svg>'

def seq_dock():
    S = [('Stand 1', '1,000'), ('Walk', '1,000'), ('Attack 1', '1,000'), ('Spell', '1,000'), ('Death', '2,167'), ('Decay Flesh', '2,000')]
    rows = [([n, f'<span style="margin-left:auto">{l}</span>'], 'sel' if i == 0 else ('hover' if i == 3 else '')) for i, (n, l) in enumerate(S)]
    return dock(['Sequences'], table(['Name', 'Length (ms)'], rows, [None, 90], 'flex:1 1 auto'), 'height:196px;flex:none', 'dock-body', 0)

def geo_dock():
    G = [('0', 'Footman_Body.blp', '812', True), ('1', 'Footman_Armor.blp', '404', True), ('2', 'Sword.blp', '160', True), ('3', 'Shield.blp', '140', True), ('4', 'Team Color', '58', False)]
    rows = [([n, m, f'<span style="margin-left:auto">{t}</span>', ck('', v)], 'sel' if n == '1' else '') for n, m, t, v in G]
    t = table(['#', 'Material', 'Tris', 'Show'], rows, [28, None, 52, 48], 'flex:1 1 auto')
    return dock(['Geosets', 'Materials'], t, 'height:174px;flex:none', 'dock-body', 0)

def info_dock():
    rows = [([k, v], '') for k, v in [('Vertices', '1,204'), ('Triangles', '1,516'), ('Bones', '38'), ('Geosets', '5'), ('Sequences', '12'), ('Bounds radius', '71.4')]]
    return dock(['Info'], table(['Property', 'Value'], rows, [110, None], 'flex:1 1 auto'), 'flex:1 1 auto', 'dock-body', 0)

def model_editor():
    tbar = ('<div class="toolbar">' + f'<div class="tbtn txt">{ic("open")}<span>Open</span></div>' + f'<div class="tbtn txt checked">{ic("model-editor")}<span>Browse Models</span></div>'
            + '<div class="tsep"></div>' + tb('wireframe') + tb('lighting', 'checked') + tb('pathing') + tb('debug') + tb('reset-camera')
            + '<div class="tsep"></div>' + tb('test-map', 'checked') + tb('refresh')
            + '<span class="muted" style="padding:0 6px 0 8px">Sequence</span>' + combo('Stand 1', 'width:150px') + '<span class="muted" style="padding:0 6px 0 8px">Speed</span>' + combo('1.0x', 'width:70px')
            + '<div class="grow"></div>' + f'<div class="edit" style="width:200px">{ic("search","s")}<span class="ph">Search actions</span></div></div>')
    tabs = ('<div class="ads-tabs"><div class="dtab selected">Footman.mdx<span class="x">&#10005;</span></div><div class="dtab">Archmage.mdx<span class="x">&#10005;</span></div>'
            '<div class="dtab">Peasant.mdx<span class="x">&#10005;</span></div></div>')
    vp = (f'<div class="viewport" style="flex:1 1 auto;border:1px solid var(--color-border);background:{GLBG}">{human_svg(965, 575)}'
          '<div class="hud" style="left:8px;top:8px">Footman.mdx | Stand 1 | OpenGL viewport (engine-rendered, not styled by QSS)</div>'
          '<div class="hud" style="right:8px;top:8px">Orbit: left drag | Pan: middle drag | Zoom: wheel</div></div>')
    slider = ('<div style="flex:1 1 auto;height:20px;position:relative"><div style="position:absolute;left:0;right:0;top:8px;height:4px;border-radius:2px;background:var(--color-border-strong)"></div>'
              '<div style="position:absolute;left:0;width:34%;top:8px;height:4px;border-radius:2px;background:var(--color-accent)"></div>'
              '<div style="position:absolute;left:34%;top:3px;width:14px;height:14px;margin-left:-7px;border-radius:7px;background:var(--color-accent-hover);border:1px solid var(--color-accent)"></div></div>')
    tl = row(btn('', 'ico', '', 'test-map') + btn('', 'ico', '', 'reset-camera') + slider + '<span class="muted" style="width:110px;text-align:right">340 / 1000 ms</span>' + ck('Loop', True),
             'gap:8px;height:32px;flex:none;padding:0 8px;background:var(--color-panel);border:1px solid var(--color-border)')
    center = col(vp + tl, 'flex:1 1 0;min-width:0;gap:4px')
    insp = col(seq_dock() + geo_dock() + info_dock(), 'width:300px;flex:none;gap:4px;min-height:0')
    status = ('<div class="statusbar"><span style="padding-right:8px">Ready</span><div class="grow"></div><div class="seg">Footman.mdx</div><div class="seg">Vertices 1,204</div>'
              '<div class="seg">Triangles 1,516</div><div class="seg">Bones 38</div><div class="seg">Camera: orbit</div></div>')
    body = f'<div class="col flex1">{tbar}{tabs}<div class="dockhost">{center}{insp}</div>{status}</div>'
    return win('Model Editor', 1280, 720, body, 'model-editor')

def model_tile(n, kind, sel=False):
    shapes = {
     'human': '<circle cx="48" cy="26" r="9"/><path d="M36 38h24l6 34h-10l-2 22h-10l-2-22h-10z"/>',
     'tower': '<path d="M34 90V30l-6-6V12h10v6h6v-6h6v6h6v-6h10v12l-6 6v60z"/>',
     'tree': '<path d="M48 8l26 38H62l18 28H16l18-28H22z"/><rect x="44" y="74" width="8" height="18"/>',
     'orb': '<circle cx="48" cy="52" r="26"/><path d="M48 14v10M48 80v10M10 52h10M76 52h10" stroke="#8b8f97" stroke-width="4"/>',
     'crate': '<path d="M18 34l30-14 30 14v40L48 88 18 74z"/>',
     'banner': '<path d="M30 10h4v82h-4zM34 14h38v34l-19-8-19 8z"/>',
     'rock': '<path d="M14 80l10-34 18-16 20 8 18 22 4 20z"/>',
     'beast': '<path d="M16 60c0-18 14-26 32-26s32 8 32 26v8H68v18H58V68H38v18H28V68H16z"/><circle cx="72" cy="38" r="9"/>',
    }
    bg = 'background:var(--color-selection);border:1px solid var(--color-checked-border)' if sel else 'border:1px solid transparent'
    return (f'<div class="col" style="width:124px;gap:3px;align-items:center;padding:4px;{bg}">'
            f'<div style="width:114px;height:92px;background:{GLBG};border:1px solid var(--color-border);position:relative"><svg viewBox="0 0 96 100" width="114" height="92" fill="{SIL}">{shapes[kind]}</svg></div>'
            f'<span class="small" style="max-width:116px;overflow:hidden;white-space:nowrap">{n}</span></div>')

def model_browser():
    cats = [('All models', '9,412', 0), ('Abilities', '1,238', 0), ('Buildings', '864', 0), ('Doodads', '2,310', 0), ('Environment', '412', 0), ('Objects', '96', 0), ('Shared Models', '188', 0), ('Units', '3,716', 1), ('Map (imported)', '14', 0)]
    rows = [([f'<span>{n}</span><span class="id" style="margin-left:auto">{c}</span>'], 'sel' if s else '') for n, c, s in cats]
    left = col('<span class="sect">CATEGORIES</span>' + table(None, rows, [None], 'flex:1 1 auto'), 'width:200px;flex:none;gap:4px')
    items = [('footman.mdx', 'human'), ('knight.mdx', 'human'), ('rifleman.mdx', 'human'), ('priest.mdx', 'human'), ('sorceress.mdx', 'human'), ('mortarteam.mdx', 'crate'), ('gryphonrider.mdx', 'beast'),
             ('peasant.mdx', 'human'), ('archmage.mdx', 'human'), ('paladin.mdx', 'human'), ('guardtower.mdx', 'tower'), ('ashenvale.mdx', 'tree'), ('stormbolt.mdx', 'orb'), ('banner.mdx', 'banner'),
             ('boulder.mdx', 'rock'), ('wolf.mdx', 'beast')]
    tiles = ''.join(model_tile(n, k, i == 0) for i, (n, k) in enumerate(items))
    grid = (f'<div class="view" style="flex:1 1 auto;flex-direction:row;flex-wrap:wrap;align-content:flex-start;gap:4px;padding:4px;position:relative">{tiles}'
            '<div class="sb"><div class="h" style="top:2px;height:150px"></div></div></div>')
    mid = col(row(f'<div class="edit focus grow">{ic("search","s")}<span class="ph">Search Models</span></div>', 'gap:8px') + grid + '<span class="muted small">3,716 models in Units, 16 shown</span>', 'flex:1 1 0;min-width:0;gap:6px')
    pv = f'<div class="viewport" style="height:230px;border:1px solid var(--color-border);background:{GLBG}">{human_svg(300, 230, True)}<div class="hud" style="left:6px;top:6px">Footman</div></div>'
    kv = table(None, [(['Path', 'units\\human\\footman\\footman.mdx'], ''), (['Vertices', '1,204'], ''), (['Triangles', '1,516'], ''), (['Sequences', '12'], ''), (['File size', '212 KB'], '')], [70, None], 'flex:none')
    right = col('<span class="sect">PREVIEW</span>' + pv + row('<span class="muted">Animation</span>' + combo('Stand', 'flex:1 1 auto'), 'gap:8px') + kv + row(btn('Open in Model Editor', '', '', 'model-editor')), 'width:300px;flex:none;gap:6px')
    body = row(left + mid + right, 'gap:12px;align-items:stretch;flex:1 1 auto;min-height:0')
    return dlg('Browse Models', 1200, 640, body, ('Open', 'Cancel'), 'model-editor')

# ============================================================ 4. Settings
def settings(tab):
    names = ['General', 'Testing', 'Script']
    if tab == 0:
        pop = ('<div class="menu" style="position:absolute;left:127px;top:30px;width:200px"><div class="m" style="padding-left:8px">Light</div><div class="m" style="padding-left:8px">Dark</div>'
               '<div class="m" style="padding-left:8px">Flat Dark</div><div class="m sel" style="padding-left:8px">Flat Light</div></div>')
        body = (frow('Theme', combo('Flat Light', 'width:200px', 'focus'), 120) + frow('Game Data', combo('Retail', 'width:200px'), 120)
                + ck('Enable HD Mode (requires restart)', True) + ck('Enable Teen Mode') + ck('Enable Comments', True) + '<div class="grow"></div>'
                + f'<div class="hint">Theme changes apply immediately. Press F5 in the app to reload a theme file.</div>' + pop)
    elif tab == 1:
        body = (frow('Arguments:', edit('-nowfpause', 'flex:1 1 auto'), 130) + frow('Difficulty', combo('Normal', 'width:200px'), 130)
                + frow('Window Mode', combo('Windowed', 'width:200px'), 130) + frow('Asset Mode', combo('Default', 'width:200px'), 130)
                + frow('Teen Mode', combo('Default', 'width:200px'), 130) + frow('Profile Name', edit('HiveWE', 'width:200px'), 130)
                + ck('Fixed random Seed') + ck('No Windows Focus Pause', True) + frow('Final arguments:', edit('-launch -windowmode windowed -nowfpause -loadfile "Map.w3x"', 'flex:1 1 auto', 'dis'), 130))
    else:
        body = (group('Generate map script', '<div class="hint" style="line-height:1.4;white-space:normal">Map script generation implies converting all GUI triggers to JASS and appending the text from all custom triggers.</div>' + ck('Run VJass preprocessor'))
                + group('Custom Tool', frow('Executable', edit('', 'flex:1 1 auto') + btn('...', 'ico'), 120) + frow('Arguments', edit('', 'flex:1 1 auto'), 120)
                        + frow('Working Directory', edit('', 'flex:1 1 auto') + btn('...', 'ico'), 120) + '<div class="hint">HiveWE exposes the following variables:</div>'))
    tw = tabwrap(names, tab, f'<div class="col gap8 flex1" style="position:relative">{body}</div>', 'flex:1 1 auto')
    return dlg('Settings', 730, 530, tw, ('OK', 'Cancel'), 'settings')

# ============================================================ 5. Unit Properties
def hero_header():
    return row('<div class="ic16" style="width:48px;height:48px;background:#b8a272;flex:none"></div>'
               + col('<span class="title1">Archmage</span><span class="muted small">Hamg  |  Hero</span>', 'gap:4px'), 'gap:12px;padding:0 0 4px')

def unit_props(tab):
    if tab == 0:
        ident = group('Identity', frow('Player', combo('Player 1 (Red)', 'width:260px'), 120) + frow('Facing (deg)', spin('270.00', 'width:120px'), 120))
        st = group('State', frow('Hit Points %', spin('100', 'width:90px') + '<span class="muted">of 550</span>', 110) + frow('Mana Points', spin('285', 'width:90px') + '<span class="muted">of 285</span>', 110)
                   + frow('Level', spin('1', 'width:90px'), 110), 'flex:1 1 0')
        at = group('Attributes', ck('Use Default Attributes', True) + frow('Strength', spin('17', 'width:90px', 'dis'), 110) + frow('Agility', spin('14', 'width:90px', 'dis'), 110)
                   + frow('Intelligence', spin('21', 'width:90px', 'dis'), 110), 'flex:1 1 0')
        body = ident + row(st + at, 'gap:12px;align-items:stretch')
        body = f'<div class="col gap12 flex1">{body}</div>'
    else:
        modes = group('Items Dropped', frow('Drop Mode', combo('Use Custom Item Sets', 'width:240px'), 120) + frow('Map Item Table', combo('None', 'width:240px', 'dis'), 120, )
                      + row(btn('Edit Map Item Tables'), '') + ck('Apply item drop settings to all placed units of this type'))
        sets = col('<span class="sect">Sets</span>' + table(None, [(['Set 1 (Total: 100%)'], 'sel'), (['Set 2 (Total: 35%)'], '')], [None], 'height:100px')
                   + row(btn('New Set', 'sm') + btn('Delete Set', 'sm'), 'gap:4px'), 'gap:4px;flex:1 1 0')
        its = col('<span class="sect">Items</span>' + table(['Chance', 'Item'], [(['60%', 'Potion of Healing'], 'sel'), (['40%', 'Scroll of Town Portal'], ''), (['Random', 'Level 2 Permanent'], '')], [70, None], 'height:100px')
                  + row(btn('New Item', 'sm') + btn('Edit Item', 'sm') + btn('Delete Item', 'sm'), 'gap:4px'), 'gap:4px;flex:1 1 0')
        body = f'<div class="col gap12 flex1">{modes}{row(sets + its, "gap:12px;align-items:stretch")}</div>'
    tw = tabwrap(['General', 'Items Dropped'], tab, body, 'flex:1 1 auto')
    return dlg('Unit Properties', 680, 520, hero_header() + tw, ('OK', 'Cancel'), 'units')

def drop_dialog():
    body = (frow('Chance', spin('60%', 'width:100px'), 70) + rad('Specific', True) + row(combo('Potion of Healing', 'flex:1 1 auto'), 'padding-left:20px')
            + rad('Random') + row(combo('Permanent', 'flex:1 1 auto', 'dis') + combo('Any Level', 'width:100px', 'dis'), 'padding-left:20px;gap:8px'))
    return dlg('Edit Drop Item', 420, 250, body, ('OK', 'Cancel'), 'item-tables')

# ============================================================ 6. Map Settings
def mapinfo(tab):
    names = ['Description', 'Options', 'Size and Camera Bounds', 'Loading Screen', 'Preferences']
    if tab == 0:
        body = (frow('Name:', edit('Hero Defense Reforged', 'flex:1 1 auto'), 130) + frow('Suggested Players:', edit('1-4 Players', 'flex:1 1 auto'), 130)
                + frow('Description', '', 130) + '<div class="text flex1" style="min-height:120px"><span>Defend the Lordaeron countryside from waves of undead. Revive fallen heroes at the altar.</span></div>'
                + frow('Author', edit('Arthas', 'flex:1 1 auto'), 130) + frow('Map Version:', '<span>5</span>', 130) + frow('Editor Version:', '<span>6114</span>', 130))
    elif tab == 1:
        body = (ck('Melee Map') + ck('Hide minimap in preview screens') + ck('Masked areas are partially visible', True) + ck('Show water waves on cliff shores', True)
                + ck('Show water waves on rolling shores', True) + ck('Use item classification system')
                + group('Use Terrain Fog', frow('Style', combo('Linear', 'width:160px'), 90) + frow('Z Start', edit('3000.0', 'width:120px'), 90) + frow('Z End', edit('5000.0', 'width:120px'), 90)
                        + frow('Density', edit('0.5', 'width:120px'), 90) + frow('Color', '<div class="cbtn"><i style="background:#7a8aa0"></i></div>', 90))
                + ck('Use Global Weather') + ck('Custom Sound Environment') + ck('Custom Light Environment')
                + frow('Game Data Set:', combo('Default (Map melee status)', 'width:240px'), 130) + ck('Use Water Tinting Color', True))
    elif tab == 2:
        def side(l): return row(f'<span class="muted" style="width:50px">{l}</span>' + spin('0', 'width:70px') + '<span class="muted small" style="width:70px;padding-left:6px">Camera</span>' + spin('0', 'width:70px'), 'gap:4px')
        bounds = group('Bounds', side('Left') + side('Right') + side('Top') + side('Bottom') + ck('Modify Map Bounds') + ck('Modify Camera Bounds', True) + btn('Reset Camera Bounds to Defaults'), 'width:330px')
        pv = ('<div class="viewport" style="width:300px;height:300px;border:1px solid var(--color-border)"><div class="patch" style="left:0;top:0;right:0;bottom:0;background:var(--color-viewport)"></div>'
              '<div class="patch" style="left:30px;top:30px;right:30px;bottom:30px;background:#4a6340"></div><div class="patch" style="left:18px;top:18px;right:18px;bottom:18px;border:1px dashed #fff"></div></div>')
        info = group('Map size', frow('Description:', '<span>Tiny</span>', 100) + frow('Playable:', '<span>52 x 52</span>', 100) + frow('Full:', '<span>64 x 64</span>', 100))
        body = row(bounds + col(pv + info, 'gap:8px'), 'gap:12px;align-items:flex-start')
    elif tab == 3:
        body = (group('Loading Screen Graphic', rad('Use Default Screen', True) + rad('Use Campaign Screen') + rad('Use Imported File') + row(edit('', 'flex:1 1 auto', 'dis') + btn('...', 'ico dis'), ''))
                + frow('Title', edit('Hero Defense Reforged', 'flex:1 1 auto'), 130) + frow('Subtitle', edit('Survive 30 waves', 'flex:1 1 auto'), 130)
                + frow('Text', '', 130) + '<div class="text flex1" style="min-height:100px"><span>Arthas leads the Alliance against the Scourge. Hold the line.</span></div>')
    else:
        body = '<span class="muted">?</span>'
    tw = tabwrap(names, tab, f'<div class="col gap8 flex1">{body}</div>', 'flex:1 1 auto')
    return dlg('Map Settings', 720, 600, tw, ('OK', 'Cancel'), 'map-options')

# ============================================================ 7. Scenario + Gameplay constants
def scenario_players():
    cols = ['#', 'Player Name', 'Color', 'Race', 'Controller', 'Fixed Start Location']
    PC = ['#ff0303', '#0042ff', '#1ce6b9', '#540081', '#fffc01', '#fe8a0e', '#20c000', '#e55bb0', '#959697', '#7ebff1', '#106246', '#4a2a04']
    CN = ['Red', 'Blue', 'Teal', 'Purple', 'Yellow', 'Orange', 'Green', 'Pink', 'Gray', 'Light Blue', 'Dark Green', 'Brown']
    rows = []
    for i in range(12):
        race = 'Human' if i < 4 else 'Orc' if i < 6 else 'Selectable'
        ctl = 'User' if i < 4 else 'Computer' if i < 8 else 'None'
        rows.append(([str(i + 1), f'Player {i+1}', f'<div class="ic16" style="background:{PC[i]};margin-right:6px"></div>{CN[i]}',
                      combo(race, 'height:20px;width:120px;flex:none'), combo(ctl, 'height:20px;width:110px;flex:none'), ck('', i < 3)], ('sel tall' if i == 1 else 'tall')))
    t = table(cols, rows, [40, 180, 140, 150, 140, None], 'flex:1 1 auto')
    body = t + row(btn('Reset Players to Defaults'), '')
    tw = tabwrap(['Players', 'Forces'], 0, f'<div class="col gap8 flex1">{body}</div>', 'flex:1 1 auto')
    return dlg('Scenario Properties', 980, 520, tw, ('OK', 'Cancel'), 'players-forces')

def scenario_forces():
    tree = table(None, [([trow(0, 'o', fold(), 'Force 1')], ''), ([trow(1, 'n', ico16('#ff0303'), 'Player 1')], ''), ([trow(1, 'n', ico16('#0042ff'), 'Player 2')], 'sel'),
                        ([trow(0, 'o', fold(), 'Force 2')], ''), ([trow(1, 'n', ico16('#1ce6b9'), 'Player 3')], ''), ([trow(1, 'n', ico16('#540081'), 'Player 4')], '')],
                 [None], 'flex:1 1 auto')
    left = col(tree + row(btn('Add Force', 'sm') + btn('Remove Force', 'sm') + btn('Rename Force', 'sm'), 'gap:4px'), 'width:280px;gap:8px;flex:none')
    hdr = ['Force', 'Allied', 'Allied Victory', 'Share Vision', 'Share Unit Control', 'Share Adv. Unit Control']
    rows = [(['Force 1', ck('', True), ck('', True), ck('', True), ck('', False), ck('')], 'sel'), (['Force 2', ck('', True), ck('', True), ck('', False), ck(''), ck('')], '')]
    right = table(hdr, rows, [80, 70, 100, 100, 130, None], 'flex:1 1 auto;align-self:flex-start;height:90px')
    body = row(ck('Use Custom Forces', True) + ck('Fixed Player Settings', True), 'gap:20px') + row(left + right, 'gap:12px;align-items:stretch;flex:1 1 auto')
    tw = tabwrap(['Players', 'Forces'], 1, f'<div class="col gap8 flex1">{body}</div>', 'flex:1 1 auto')
    return dlg('Scenario Properties', 980, 520, tw, ('OK', 'Cancel'), 'players-forces')

def gameplay():
    G = [('Combat', None), ('Attack Cooldown Gold Cost?', None)]
    rows = [(['<b>Hero</b>', ''], 'sub'), (['Maximum Hero Level', '10'], ''), (['Hero Experience Required (Level 2)', '200'], 'sel mod'), (['Hero Experience Gained from Kill (Base)', '25'], ''),
            (['Hero Revival Cost - Gold per Level', '0.0'], 'hover'), (['<b>Resources</b>', ''], 'sub'), (['Gold Mine Capacity (Default)', '12500'], ''), (['Tree Gold Return Rate', '0.5'], ''),
            (['Maximum Food Limit', '100'], ''), (['<b>Combat</b>', ''], 'sub'), (['Armor Damage Reduction Multiplier', '0.06'], ''), (['Heavy Armor Damage Modifier', '1.00'], ''),
            (['Defend Damage Reduction', '0.50'], ''), (['Attack Cooldown Gold Cost', '0'], 'dim')]
    body = edit('Search...', 'width:auto', 'focus', ph=True) + table(None, rows, [360, None], 'flex:1 1 auto', sb=(2, 180))
    return win('Gameplay Constants', 760, 560, f'<div class="col pad gap8 flex1">{body}</div>', 'gameplay-constants', maxbtn=True)

# ============================================================ 8. New map, file menu, small dialogs
def new_map():
    left = col('<span class="title2">Map Setup</span><span class="muted small">Core identity and world scale for the new map.</span>'
               + frow('Map Name', edit('Hero Defense Reforged', 'flex:1 1 auto'), 80) + frow('Tileset', combo('Lordaeron Summer', 'flex:1 1 auto'), 80)
               + row('<span style="width:80px">Width</span>' + combo('128', 'width:90px') + '<span style="width:60px;text-align:right;padding-right:8px">Height</span>' + combo('128', 'width:90px'), 'gap:0;height:24px'),
               'gap:12px;padding:16px;flex:3 1 0;border:1px solid var(--color-border);background:var(--color-panel)')
    tiles = ''.join(f'<div class="ic16" style="width:48px;height:36px;background:{c}"></div>' for c in ['#4f7a3a', '#6b6a3c', '#8a6a46', '#b8a272', '#6e6e6e', '#3a5c2c'])
    right = col('<span class="title2">Live Summary</span><span class="muted small">A quick read on the map footprint before you create it.</span>'
                + col('<span class="sect">MAP SCALE</span><span class="title1">Medium</span><span>Playable 116 x 116 (full 128 x 128)</span>'
                      '<span class="sect" style="margin-top:6px">TILESET PREVIEW</span>' + f'<div class="row" style="gap:8px;flex-wrap:wrap">{tiles}</div>',
                      'gap:6px;padding:12px;background:var(--color-base);border:1px solid var(--color-border)'),
                'gap:12px;padding:16px;flex:2 1 0;border:1px solid var(--color-border);background:var(--color-panel)')
    body = '<span class="title1">Start a new world</span><span class="muted">Set the map scale and starting tileset for the new map.</span>' + row(left + right, 'gap:12px;align-items:stretch;flex:1 1 auto')
    return dlg('Create New Map', 760, 520, body, ('Cancel', 'Create Map'), 'new', default=1)

def file_menu():
    items = [('New Map', '', 'new'), ('Open Map (Folder)', 'Ctrl+O', 'open'), ('Open Map (MPQ)', '', 'open-mpq'), ('Recent Maps', '', 'recent', 'sel'), None,
             ('Save Map', 'Ctrl+S', 'save'), ('Save Map As', 'Ctrl+Shift+S', 'save-as'), ('Export MPQ', '', 'export-mpq'), ('Test Map', '', 'test-map'), None, ('Settings', '', 'settings'), ('Exit', '', 'exit')]
    m = ''
    for it in items:
        if it is None: m += '<div class="ms"></div>'; continue
        cls = it[3] if len(it) > 3 else ''
        sc = '<span class="arr"></span>' if it[0] == 'Recent Maps' else it[1]
        m += f'<div class="m {cls}"><span>{it[0]}</span><span class="sc">{sc}</span><span class="ic">{ic(it[2], "s")}</span></div>'
    sub = ''.join(f'<div class="m"><span>{n}</span><span class="sc">{k}</span></div>' for n, k in
                  [('D:\\Maps\\HeroDefense.w3x', ''), ('D:\\Maps\\Footmen Frenzy.w3x', ''), ('C:\\Users\\Me\\Documents\\Warcraft III\\Maps\\Test.w3x', '')])
    bar = '<div class="menubar" style="width:520px">' + ''.join(f'<div class="mi {"open" if x=="File" else ""}">{x}</div>' for x in ['File', 'Edit', 'View', 'Terrain', 'Map', 'Tools']) + '</div>'
    return (f'<div class="col" style="width:520px;height:330px;background:var(--color-window);border:1px solid var(--color-border-strong);position:relative">{bar}'
            f'<div class="menu" style="position:absolute;left:4px;top:23px;width:200px">{m}</div>'
            f'<div class="menu" style="position:absolute;left:200px;top:98px;width:300px">{sub}</div></div>')

def copy_dialog():
    body = (frow('Source', '<span>Footman (hfoo)</span>', 70) + frow('New Name', edit('Elite Footman', 'flex:1 1 auto', 'focus'), 70)
            + frow('ID', edit('h000', 'width:100px') + '<span class="badge ok">Free ID</span>', 70))
    return dlg('Copy Unit', 440, 210, body, ('OK', 'Cancel'), 'object-editor')

def remove_dialog():
    body = f'<div class="msg" style="padding:4px 0"><div class="big" style="color:var(--color-warn)">{ic("warn")}</div><div class="txt"><span class="title2">Remove this custom unit?</span><span class="muted">Elite Footman (h000)</span><span class="muted small">Placed copies on the map will be removed too.</span></div></div>'
    return dlg('Remove Unit', 440, 190, body, ('Remove', 'Cancel'), 'object-editor', pad=True)

def ctx_menu():
    items = [('Add Unit', 'plus'), ('Duplicate Unit', 'copy'), ('Copy Unit', 'copy'), ('Paste Unit', None), ('Remove Unit', 'trash')]
    m = ''.join(f'<div class="m {"sel" if i==1 else ("dis" if i==3 else "")}"><span>{t}</span><span class="sc"></span><span class="ic">{ic(ico,"s") if ico else ""}</span></div>' for i, (t, ico) in enumerate(items))
    return f'<div class="menu" style="width:200px">{m}</div>'

def errbox():
    body = f'<div class="msg" style="padding:4px 0"><div class="big" style="color:var(--color-error)">{ic("error")}</div><div class="txt"><span class="title2">Opening model failed</span><span>Failed to read units\\human\\footman\\footman.mdx</span></div></div>'
    return dlg('Opening model failed', 420, 150, body, ('OK',), 'model-editor')

# ============================================================ 9. Tools dialogs
def asset_manager():
    rows = []
    D = [('war3mapImported\\Footman.mdx', 'Model', 3), ('war3mapImported\\Footman.blp', 'Texture', 3), ('war3mapImported\\OldBanner.blp', 'Texture', 0), ('war3mapImported\\Fanfare.mp3', 'Sound', 1),
         ('war3mapImported\\Rain.mdx', 'Model', 0), ('Units\\Human\\Footman\\Footman_Portrait.mdx', 'Model', 1), ('war3map.j', 'Script', 1), ('war3mapImported\\Tower.blp', 'Texture', 0)]
    for i, (f, t, u) in enumerate(D):
        rows.append(([f'<span style="{"color:var(--color-text-muted)" if u==0 else ""}">{f}</span>', t, f'<span style="margin-left:auto">{u}</span>'], 'sel' if i == 2 else ''))
    t = table(['File', 'Type', 'Usages'], rows, [None, 90, 70], 'flex:1 1 auto', sb=(2, 160))
    body = (row('<span class="grow">214 files, 31 unused (4.2 MB)</span>' + btn('Rescan', '', '', 'refresh'), 'gap:8px')
            + row(combo('Unused only', 'width:140px') + '<span class="muted small">Imported files with no references in the map</span>', 'gap:8px') + t
            + row('<span class="grow muted">1 file selected</span>' + btn('Select all unused') + btn('Delete', '', '', 'trash'), 'gap:8px'))
    return win('Asset Manager', 640, 560, f'<div class="col pad gap8 flex1">{body}</div>', 'asset-manager')

def protector():
    src = group('Source', rad('Currently loaded map', True) + row(rad('Map file or folder:') + edit('', 'flex:1 1 auto', 'dis') + btn('Browse File...', 'disabled') + btn('Browse Folder...', 'disabled'), 'gap:8px'))
    out = row('<span style="width:80px">Output file:</span>' + edit('D:\\Maps\\HeroDefense_protected.w3x', 'flex:1 1 auto') + btn('Browse...'), 'gap:8px')
    g1 = group('MPQ Archive Hardening', ck('Remove listfile', True) + ck('Remove attributes file', True) + ck('Encrypt MPQ files', True) + ck('Inject junk files') + frow('Junk file count:', spin('50', 'width:90px', 'dis'), 110))
    g2 = group('Trigger / Script Hardening', ck('Remove GUI trigger data', True) + ck('Strip trigger strings', True))
    g3 = group('Asset Path Obfuscation', ck('Rename imported files to random names', True))
    g4 = group('Metadata Sanitization', ck('Clear map author') + ck('Clear map description') + ck('Clear loading screen text') + ck('Normalize map name'))
    body = src + out + row(col(g1 + g3, 'gap:8px;flex:1 1 0') + col(g2 + g4, 'gap:8px;flex:1 1 0'), 'gap:12px;align-items:flex-start') + (
        row('<span class="grow">Ready</span>' + btn('Export Protected Map', 'default'), 'gap:8px'))
    return win('Map Protector', 760, 470, f'<div class="col pad gap8 flex1">{body}</div>', 'map-protector')

def import_objects():
    names = ['Validation', 'New Objects', 'Conflicts']
    hdr = ['Type', 'Object', 'Message']
    rows = [(['Unit', 'h000 Elite Footman', f'{badge("OK","ok")}'], ''), (['Unit', 'h001 Veteran Rifleman', badge('Warning', 'mod') + ' Missing model war3mapImported\\Rifle.mdx'], 'sel'),
            (['Ability', 'A000 Shockwave+', f'{badge("OK","ok")}'], ''), (['Item', 'I000 Mana Elixir', badge('Error', 'err') + ' Unknown base item "ixyz"'], ''), (['Upgrade', 'R000 Iron Forged Swords', f'{badge("OK","ok")}'], '')]
    t = table(hdr, rows, [90, 240, None], 'flex:1 1 auto')
    body = '<span>Review validation results and choose which conflicting objects to overwrite.</span>' + tabwrap(names, 0, f'<div class="col flex1">{t}</div>', 'flex:1 1 auto') + ck('Proceed despite warnings', True)
    return dlg('Import Object Data', 1100, 460, body, ('Import', 'Cancel'), 'import-objects')

# ============================================================ 10. Misc dialogs
def item_tables():
    tables = group('Tables', table(None, [(['Creep Drops'], 'sel'), (['Boss Drops'], ''), (['Shop Stock'], '')], [None], 'height:100px') + row(btn('Add Table', 'sm') + btn('Delete Table', 'sm'), 'gap:4px'), 'width:230px;flex:none')
    sets = group('Sets', table(None, [(['Set 1 (Total: 100%)'], 'sel'), (['Set 2 (Total: 40%)'], '')], [None], 'height:70px') + row(btn('New Set', 'sm') + btn('Delete Set', 'sm'), 'gap:4px'))
    ent = group('Entries', table(['Chance', 'Item'], [(['70%', 'Potion of Healing (phea)'], 'sel'), (['30%', 'Scroll of Protection (sprn)'], '')], [70, None], 'height:80px') + row(btn('New Item', 'sm') + btn('Edit Item', 'sm') + btn('Delete Item', 'sm'), 'gap:4px'))
    assign = group('Assign To Unit Type', frow('Unit Type', combo('Gnoll (ngnl)', 'width:240px'), 110) + frow('Map Item Table', combo('Creep Drops', 'width:240px'), 110)
                   + ck('Remove existing custom drop sets on affected units') + row(btn('Apply To Placed Units')))
    body = row(tables + col(frow('Table Name', edit('Creep Drops', 'flex:1 1 auto'), 90) + sets + ent + assign, 'gap:8px;flex:1 1 0'), 'gap:12px;align-items:flex-start')
    return dlg('Item Tables', 940, 590, body, ('OK', 'Cancel'), 'item-tables')

def config():
    rows = [(['Save Map', 'Ctrl+S'], ''), (['Open Map (Folder)', 'Ctrl+O'], ''), (['Undo', 'Ctrl+Z'], 'sel'), (['Redo', 'Ctrl+Y'], ''), (['Toggle Units', 'Ctrl+U'], ''), (['Toggle Doodads', 'Ctrl+D'], ''),
            (['Toggle Wireframe', 'Ctrl+T'], ''), (['Reset Camera', 'Ctrl+Shift+C'], ''), (['Debug overlay', 'F3'], ''), (['Pan camera', 'Middle mouse button'], 'dim'), (['Rotate camera', 'Ctrl + Middle mouse button'], 'dim')]
    body = '<span>Current shortcut and input reference.</span>' + table(['Description', 'Shortcut'], rows, [None, 240], 'flex:1 1 auto', sb=(2, 200))
    return dlg('Config', 700, 400, body, ('Close',), 'config', default=0)

def tile_setter():
    t1 = table(None, [(['Lgrs'], 'sel'), (['Ldrt'], ''), (['Lrok'], '')], [None], 'height:78px')
    t2 = table(None, [(['Lsan'], ''), (['Lgrd'], '')], [None], 'height:56px')
    body = ('<span class="title2">Selected Tiles</span>' + t1 + row('<span class="grow muted">Tile: Grass</span>' + btn('Shift Left', 'sm') + btn('Shift Right', 'sm') + btn('Remove', 'sm'), 'gap:4px')
            + '<span class="title2" style="margin-top:6px">Additional Tiles</span>' + combo('Barrens', 'flex:none') + t2 + row('<span class="grow muted">Tile: Sand</span>' + btn('Add', 'sm'), 'gap:4px'))
    return dlg('Change Tileset', 360, 420, body, ('OK', 'Cancel'), 'change-tileset')

def tile_pather():
    pth = group('Pathing', row(btn('Unwalkable', 'checked') + btn('Unflyable') + btn('Unbuildable', 'checked'), 'gap:4px'))
    op = group('Operation Type', rad('Replace') + rad('Add', True) + rad('Remove'))
    ap = group('Application', ck('Apply Retroactively'))
    body = '<span class="title2">Pathing Tile</span><span class="muted">Tile: Dirt</span>' + pth + op + ap
    return dlg('Tile Pathing', 360, 420, body, ('OK', 'Cancel'), 'change-tile-pathing')

# ============================================================ assemble pages
def L(c, node): return cap(c) + node
def V(*parts): return col(''.join(parts), 'gap:14px;align-items:flex-start')
def H(*parts): return '<div class="pagerow">' + ''.join(parts) + '</div>'

page('object-editor', 'Object Editor', L('Object Editor / Units tab, custom object and field selected', object_editor()), 1600, stackH([900]))
page('trigger-editor', 'Trigger Editor',
     L('Trigger Editor / GUI trigger tab, Output dock with an error', trigger_gui()) + L('Trigger Editor / Script (Lua) trigger with Find box, variable tab beside', trigger_script()),
     1532, stackH([780, 440]))
page('model-editor', 'Model Editor',
     L('Model Editor / Model open, Browse Models button in the tab bar', model_editor()) + L('Model Editor / Browse Models dialog', model_browser()),
     1312, stackH([720, 640]))
page('settings', 'Settings',
     H(V(L('Settings / General tab, Theme combo open', settings(0))), V(L('Settings / Testing tab', settings(1)))) + L('Settings / Script tab', settings(2)),
     1560, 32 + 2 * (18 + 530) + 14)
page('unit-properties', 'Unit Properties',
     H(V(L('Unit Properties / General tab (hero)', unit_props(0))), V(L('Unit Properties / Items Dropped tab', unit_props(1)))) + L('Unit Properties / Edit Drop Item dialog', drop_dialog()),
     1420, 32 + (18 + 520) + 14 + (18 + 250))
page('map-settings', 'Map Settings',
     H(V(L('Map Settings / Description tab', mapinfo(0))), V(L('Map Settings / Options tab', mapinfo(1))))
     + H(V(L('Map Settings / Size and Camera Bounds tab', mapinfo(2))), V(L('Map Settings / Loading Screen tab', mapinfo(3)))),
     1520, 32 + 2 * (18 + 600) + 14)
page('scenario-gameplay', 'Scenario Properties and Gameplay Constants',
     H(V(L('Scenario Properties / Players tab, row 2 selected', scenario_players()), L('Scenario Properties / Forces tab', scenario_forces())),
       V(L('Gameplay Constants / Property table, one row selected', gameplay()))),
     1788, 32 + 2 * (18 + 520) + 14)
page('new-map-file', 'New Map, File menu and small dialogs',
     H(V(L('Create New Map / Defaults edited', new_map()),
         H(V(L('Object Editor / Tree context menu', ctx_menu())), V(L('Model Editor / Error box', errbox())))),
       V(L('File menu / Recent Maps submenu open', file_menu()), L('Object Editor / Copy Unit dialog', copy_dialog()), L('Object Editor / Remove confirmation', remove_dialog()))),
     1344, 32 + 18 + 330 + 14 + 18 + 210 + 14 + 18 + 190 + 10)
page('tools-dialogs', 'Asset Manager, Map Protector, Import Object Data',
     H(V(L('Asset Manager / Unused filter, one file selected', asset_manager()), L('Map Protector / Defaults', protector())),
       V(L('Import Object Data / Warnings and an error', import_objects()))),
     1920, 32 + 2 * 18 + 560 + 470 + 14)
page('misc-dialogs', 'Item Tables, Config, Tile dialogs',
     H(V(L('Item Tables / Table and set selected', item_tables())),
       V(L('Config / Shortcut reference', config()), H(V(L('Change Tileset / Dialog', tile_setter())), V(L('Tile Pathing / Dialog', tile_pather()))))),
     1760, 32 + 18 + 400 + 14 + 18 + 420 + 10)

import json
(here / 'pages.json').write_text(json.dumps([[n, t, w, h] for n, t, w, h in PAGES]), encoding='utf-8')
print(len(PAGES), 'pages')
