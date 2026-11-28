/* Shared markup for the Model Browser pages. Folder, file and sequence names come from the real Storm_VFX test folder
   (read only). Effect pictures are drawn here as placeholders; no game art and no user files are copied. */
if (location.hash === '#light') document.documentElement.dataset.theme = 'light';

const ic = (n, c = '') => ICONS[n].replace('<svg ', `<svg class="i ${c}" `);
const arrow = '<div class="arrow"></div>';
const combo = (text, w, c = '') => `<div class="combo ${c}" style="${w ? `width:${w}px` : 'flex:1 1 0'}"><span>${text}</span><div class="dd">${arrow}</div></div>`;
const check = (label, on, c = '') => `<div class="check ${on ? 'on' : ''} ${c}"><i></i>${label}</div>`;
const btn = (label, c = '', icon = '') => `<div class="btn ${c}">${icon ? ic(icon, 'xs') : ''}${label}</div>`;
const tb = (icon, label = '', c = '') => `<div class="tbtn ${label ? 'text' : ''} ${c}">${icon ? ic(icon) : ''}${label}</div>`;
const tsep = '<div class="tsep"></div>';

/* ---- placeholder effect pictures (stand in for the OpenGL render / thumbnail) ---- */
let gid = 0;
function art(kind, seed, big = false) {
  let s = seed * 9301 + 49297;
  const r = () => (s = (s * 9301 + 49297) % 233280) / 233280;
  const pal = { fire: ['#fff3b0', '#ff9a1f', '#c4280a'], smoke: ['#e8e2d8', '#9b948a', '#4a4540'], water: ['#e6fbff', '#46b4ee', '#14508c'],
    dark: ['#f2d4ff', '#8a3fc4', '#1a0c24'], blue: ['#e8f4ff', '#4c86ff', '#1b2c90'] }[kind] || ['#fff3b0', '#ff9a1f', '#c4280a'];
  const id = 'g' + (gid++);
  let out = `<svg viewBox="0 0 100 100" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg"><defs>` +
    pal.map((c, i) => `<radialGradient id="${id}${i}"><stop offset="0" stop-color="${c}" stop-opacity="0.95"/><stop offset="1" stop-color="${c}" stop-opacity="0"/></radialGradient>`).join('') + '</defs>';
  const blob = (x, y, rx, ry, c) => `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${rx.toFixed(1)}" ry="${ry.toFixed(1)}" fill="url(#${id}${c})"/>`;
  const cx = 44 + r() * 12, cy = 46 + r() * 12, shape = seed % 3;
  for (let i = 0; i < (big ? 16 : 9); i++) {
    const a = r() * 6.283, d = r() * (shape === 1 ? 24 : 15);
    const x = cx + Math.cos(a) * d, y = cy + Math.sin(a) * d * (shape === 2 ? 1.5 : 0.7) - (shape === 2 ? 8 : 0);
    const size = 6 + r() * (big ? 12 : 15);
    out += blob(x, y, size, size * (0.8 + r() * 0.5), 2 - (i % 2));
  }
  if (shape === 1) out += `<ellipse cx="${cx}" cy="${cy + 14}" rx="32" ry="10" fill="none" stroke="${pal[1]}" stroke-width="1.6" stroke-opacity="0.7"/>`;
  out += blob(cx, cy, 11 + r() * 5, 11 + r() * 5, 0);
  for (let i = 0; i < (big ? 60 : 12); i++) {
    const a = r() * 6.283, d = 12 + r() * 32;
    out += `<circle cx="${(cx + Math.cos(a) * d).toFixed(1)}" cy="${(cy + Math.sin(a) * d * 0.8).toFixed(1)}" r="${(0.25 + r() * (big ? 0.55 : 1.1)).toFixed(2)}" fill="${pal[i % 2]}" fill-opacity="${(0.45 + r() * 0.5).toFixed(2)}"/>`;
  }
  return out + '</svg>';
}

/* ---- real files: [folder, file, picture kind, state] ---- */
const f = (folder, file, kind = 'fire') => ({ folder, file: file + '.mdx', kind });
const FIRE = [
  f('B043_FireballRising_DarkSmoke', 'B043_FireballRising_DarkSmoke', 'smoke'), f('B044_FireballRising_GreySmoke', 'B044_FireballRising_GreySmoke', 'smoke'),
  f('B045_Fireball_DarkSmoke', 'B045_Fireball_DarkSmoke', 'smoke'), f('B046_Fireball_GreySmoke', 'B046_Fireball_GreySmoke', 'smoke'),
  f('B078_Fireball_Smoke', 'B078_Fireball_Smoke'), f('B079_Fireball_BreakSmoke', 'B079_Fireball_BreakSmoke', 'smoke'),
  f('B080_Fireball_RingSmoke', 'B080_Fireball_RingSmoke'), f('B081_FireBurst_Yellow', 'B081_FireBurst_Yellow'),
  f('B082_FlameBurst_Ragged', 'B082_FlameBurst_Ragged'), f('FI068_FlameCrown', 'Fire_FI068_FlameCrown'),
  f('FI072_FireBursts', 'Fire_FI072_FireBursts'), f('FI073_FireSwipe', 'Fire_FI073_FireSwipe'),
  f('FI076_FlyingFireball', 'Fire_FI076_FlyingFireball'), f('FI077_FireHoopAttack', 'Fire_FI077_FireHoopAttack'),
  f('FI103_FlyingComet', 'Fire_FI103_FlyingComet'), f('FI105_Fireball', 'Fire_FI105_Fireball'),
  f('FI111_FireBallBurst', 'Fire_FI111_FireBallBurst'), f('FI112_FlyingFire', 'Fire_FI112_FlyingFire'),
  f('FI113_LavaRocks', 'Fire_FI113_LavaRocks'), f('FI118_FireBurstRing', 'Fire_FI118_FireBurstRing'),
  f('M006_Blue_Flame_Pillar', 'M006_Blue_Flame_Pillar', 'blue'), f('Madara_DragonFlame2_Projectile', 'Madara_M18_FireProjectile_Light'),
];
const GAME = [['archnathid', 'units\\creeps\\archnathid'], ['archnathidgreen', 'units\\creeps\\archnathidgreen'], ['archnathidpurple', 'units\\creeps\\archnathidpurple'],
  ['assassin', 'units\\creeps\\assassin'], ['azuredragon', 'units\\creeps\\azuredragon']];

/* ---- window chrome ---- */
function titlebar(title) {
  return `<div class="titlebar"><div class="ico"></div><div class="grow elide">${title}</div><div class="wb">&#8211;</div><div class="wb">&#9633;</div><div class="wb">&#10005;</div></div>`;
}

/* Model Editor toolbar (ModelEditor::build_toolbar). o: dir 'a'|'b', off = no model, seq, browser */
function toolbar(o = {}) {
  const d = o.off ? 'disabled' : '';
  const second = o.dir === 'b' ? tb('model-editor', 'Library', o.library ? 'checked' : '') : tb('model-editor', 'Model Browser', o.browser === false ? '' : 'checked');
  return `<div class="toolbar">${tb('open', 'Open')}${second}${tsep}${tb('view-wireframe', '', d)}${tb('frame-model', '', d)}${tsep}
    ${tb('description', 'Edit MDL', d)}${tb('save', 'Save to MDX', d)}${tsep}<div class="tlabel">Sequence</div>${combo(o.off ? '' : (o.seq || 'Stand'), 160, d)}</div>`;
}

function timeline(o = {}) {
  const off = o.off, pct = off ? 0 : (o.pct ?? 40);
  return `<div class="timeline ${off ? 'disabled' : ''}"><div class="tbtn small-btn ${off ? 'disabled' : ''}">${ic(off ? 'play' : 'pause', 's')}</div>
    <div class="tbtn small-btn ${off ? 'disabled' : ''}">${ic('restart', 's')}</div>
    <div class="slider ${off ? 'disabled' : ''}"><div class="g"></div><div class="s" style="width:${pct}%"></div><div class="h" style="left:${pct}%"></div></div>
    <div class="read">${off ? '0 / 0 ms' : (o.read || '212 / 533 ms')}</div>${check('Loop', true, off ? 'disabled' : '')}${combo('1.0x', 66, off ? 'disabled' : '')}</div>`;
}

function statusbar(left, segs) {
  return `<div class="statusbar"><span class="grow elide">${left}</span>${segs.map(s => `<span class="sg ${s.startsWith('!') ? 'warn' : ''}">${s.replace(/^!/, '')}</span>`).join('')}</div>`;
}
const SB_FI105 = ['Fire_FI105_Fireball', 'Vertices 13,332', 'Triangles 10,672', 'Bones 72', 'Camera: orbit'];

/* ---- QTableView / QTreeView rows ---- */
const table = (cols, rows, sel = -1) => `<div class="view grow"><div class="hdr">${cols.map(c => `<div class="td ${c.w ? '' : 'flex'} ${c.num ? 'num' : ''}" style="${c.w ? `width:${c.w}px` : ''}">${c.t}</div>`).join('')}</div>
  ${rows.map((row, i) => `<div class="tr ${i % 2 ? 'alt' : ''} ${i === sel ? 'sel' : ''}">${row.map((v, k) => `<div class="td ${cols[k].w ? '' : 'flex'} ${cols[k].num ? 'num' : ''}" style="${cols[k].w ? `width:${cols[k].w}px` : ''}">${v}</div>`).join('')}</div>`).join('')}</div>`;

/* Right inspector docks (ModelInspector). Values read from Fire_FI105_Fireball.mdx; geoset materials are sample. */
function inspector(o = {}) {
  const off = o.off;
  const dock = (title, body, style) => `<div class="dock" style="${style}"><div class="dtitle">${title}<span class="dbtns">&#10064; &#10005;</span></div>${body}</div>`;
  const check14 = '<span class="check on" style="display:inline-flex"><i></i></span>';
  const seq = table([{ t: 'Name' }, { t: 'Length (ms)', w: 84, num: 1 }], off ? [] : [['Stand', '533'], ['Death', '967']], off ? -1 : 0);
  const geo = table([{ t: '#', w: 28 }, { t: 'Material' }, { t: 'Tris', w: 46, num: 1 }, { t: 'Show', w: 44 }], off ? [] :
    [[0, 'FI105_9kkjeff1_fire00_00_1.blp', '6'], [1, 'FI105_9kkjeff1_fire00_00_2.blp', '4'], [2, 'FI105_9kkjeff1_fire00_01_1.blp', '6'], [3, 'FI105_fire01_1.blp', '16'],
     [4, 'FI105_9kkjeff1_fire02_1.blp', '5,760'], [5, 'FI105_part14_1.blp', '3,840'], [6, 'FI105_9kkjeff1_part00_1.blp', '6'], [7, 'FI105_9kkjeff1_part00_2.blp', '6'],
     [8, 'FI105_9kkjeff1_fire11_1.blp', '4'], [9, 'FI105_fire03_1.blp', '224']].map(r => [...r, check14]));
  const info = table([{ t: 'Property', w: 130 }, { t: 'Value' }], off ? [] :
    [['Name', 'Fire_FI105_Fireball'], ['Format version', '800'], ['Vertices (drawn)', '13,332'], ['Triangles (drawn)', '10,672'], ['Bones', '72'], ['Geosets', '72'],
     ['Sequences', '2'], ['Materials', '72'], ['Textures', '18'], ['Bounds radius', '694.8']]);
  return `<div class="col" style="width:${o.w || 300}px;flex:none">
    ${dock('Sequences', `<div class="col grow">${seq}</div>`, 'height:26%;flex:none')}
    ${dock('Geosets and Materials', `<div class="tabbar"><div class="on">Geosets</div><div>Materials</div></div><div class="col grow">${geo}</div>`, 'height:40%;flex:none')}
    ${dock('Info', `<div class="col grow">${info}</div>`, 'flex:1 1 0')}</div>`;
}

/* ---- the model browser dock (direction A) ---- */
const sources = (on) => `<div class="tabbar">${['Folder', 'Game', 'Map'].map(n => `<div class="${n === on ? 'on' : ''}">${n}</div>`).join('')}</div>`;
const folderRow = (path, c = '') => `<div class="row" style="gap:3px;flex:none">${combo(path, 0, c)}<div class="tbtn small-btn">${ic('folder', 's')}</div><div class="tbtn small-btn ${c ? 'disabled' : ''}">${ic('refresh', 's')}</div></div>`;
const ROOT = 'C:\\...\\Model -Conversions WIP\\Storm_VFX';
const ROOT_SHORT = '...\\WIP\\Storm_VFX';
const FOLDERS = [['auras', 4], ['explosions', 7], ['fire', 47], ['other', 5], ['rings_waves', 8], ['rocks', 4], ['smoke_dust', 10], ['WarMash_M247', 1],
  ['WarMash_M259', 1], ['WarMash_P043_Ribbon', 2], ['WarMash_RingTest', 1], ['water', 25], ['wind', 14], ['wood', 3]];

function treeRow(name, count, o = {}) {
  const br = o.leaf ? '' : `<div class="arrow ${o.open ? '' : 'right'}"></div>`;
  return `<div class="tr ${o.sel ? 'sel' : ''}" style="padding-left:${(o.depth || 0) * 16}px"><div class="branch">${br}</div>${o.noicon ? '' : ic('folder', 'xs')}<div class="td flex">${name}</div><div class="td num muted" style="margin-right:12px">${count}</div></div>`;
}
function folderTree(sel, h) {
  return `<div class="view" style="height:${h}px">${treeRow('Storm_VFX', '132', { open: 1, sel: sel === 'Storm_VFX' })}${FOLDERS.map(([n, c]) =>
    treeRow(n, c, { depth: 1, sel: sel === n })).join('')}<div class="vscroll"><i style="top:3px;height:${Math.round(h * h / 332)}px"></i></div></div>`;
}
const GAMECATS = [['All models', '2,903'], ['Models', '2,473'], ['VFX/SFX', '430'], ['Abilities', '428'], ['Buildings', '163'], ['Doodads', '1,694'], ['Environment', '48'],
  ['Objects', '133'], ['Shared Models', '20'], ['Units', '417']];
function gameTree(sel, h) {
  return `<div class="view" style="height:${h}px">${GAMECATS.map(([n, c]) => treeRow(n, c, { leaf: 1, noicon: 1, sel: sel === n })).join('')}<div class="vscroll"><i style="top:3px;height:${Math.round(h * h / 224)}px"></i></div></div>`;
}

const searchBox = (text, ph = 'Search Models', c = '') => `<div class="edit ${text ? 'focus' : ''} ${c}" style="flex:1 1 0">${ic('search', 'xs')}${text ? `<span class="grow">${text}</span><span class="muted">&#10005;</span>` : `<span class="ph">${ph}</span>`}</div>`;
const modeButtons = (mode) => `<div class="tbtn small-btn ${mode === 'thumbs' ? 'checked' : ''}">${ic('view-thumbnails', 's')}</div><div class="tbtn small-btn ${mode === 'compact' ? 'checked' : ''}">${ic('view-compact', 's')}</div>`;
const searchRow = (text, mode = 'thumbs', c = '') => `<div class="row" style="gap:3px;flex:none">${searchBox(text, 'Search Models', c)}${modeButtons(mode)}</div>`;
const optionRow = (sub = true, hidden = false, c = '') => `<div class="row" style="gap:14px;flex:none;height:18px">${check('Include subfolders', sub, c)}${check('Show _ folders', hidden, c)}</div>`;

function thumbRow(m, i, state = '', root = 'fire\\') {
  const thumb = state.includes('bad') ? '<div class="thumb bad">!</div>' : state.includes('wait') ? '<div class="thumb"></div>' : `<div class="thumb">${art(m.kind, i + 3)}</div>`;
  return `<div class="item ${state}">${thumb}<div class="two"><div class="elide">${m.file}</div><div class="elide small muted">${root}${m.folder}</div></div></div>`;
}

/* browser dock: o = {w, source, path, tree, top (tree/list splitter), search, mode, options, list, footer, scroll} */
function browser(o) {
  return `<div class="dock" style="width:${o.w || 320}px;flex:none;${o.h ? `height:${o.h}px` : ''}"><div class="dtitle">Model Browser<span class="dbtns">&#10064; &#10005;</span></div><div class="dbody">
    ${sources(o.source || 'Folder')}${o.path === null ? '' : folderRow(o.path || ROOT_SHORT, o.pathState || '')}${o.tree || ''}${o.tree ? '<div class="splith" style="margin:-3px 0"></div>' : ''}
    ${o.searchRow ?? searchRow(o.search, o.mode)}${o.options ?? optionRow()}${o.list}
    <div class="small muted elide" style="flex:none">${o.footer || ''}</div></div></div>`;
}

const listView = (rows, sbTop, sbH) => `<div class="view grow">${rows}<div class="vscroll"><i style="top:${sbTop}%;height:${sbH}%"></i></div></div>`;
const msgView = (html) => `<div class="view grow"><div class="listmsg">${html}</div></div>`;

function modelTabs(tabs, extra = '') {
  return `<div class="ads">${tabs.map(t => `<div class="atab ${t.on ? 'on' : ''} ${t.preview ? 'preview' : ''}"><span>${t.name}</span>${t.nox ? '' : '<span class="x">&#10005;</span>'}</div>`).join('')}${extra}</div>`;
}
const keepTab = `<div class="tbtn text">${ic('keep-tab', 's')}Keep Tab</div>`;
const glEffect = (kind, seed) => `<div class="gl">${art(kind, seed, true)}</div>`;
const glMessage = (html) => `<div class="gl"><div class="glmsg">${html}</div></div>`;
