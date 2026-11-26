/* Shared markup for the VFX Sandbox pages. Effect names are stock Warcraft III paths; "war3mapImported\..." rows are
   invented sample imports. Delays, durations and counts are sample values (430 stock effects is the real VFX/SFX count). */
if (location.hash === '#light') document.documentElement.dataset.theme = 'light';

const ic = (n, c = '') => ICONS[n].replace('<svg ', `<svg class="i ${c}" `);
const arrow = '<div class="arrow"></div>';
const combo = (text, w, c = '') => `<div class="combo ${c}" style="${w ? `width:${w}px;flex:none` : 'flex:1'}"><span>${text}</span><div class="dd">${arrow}</div></div>`;
const spin = (text, w, c = '') => `<div class="spin ${c}" style="${w ? `width:${w}px;flex:none` : 'flex:1 1 0'}"><span>${text}</span><div class="ud"><div class="arrow up"></div>${arrow}</div></div>`;
const check = (label, on, c = '') => `<div class="check ${on ? 'on' : ''} ${c}"><i></i>${label}</div>`;
const btn = (label, c = '', icon = '') => `<div class="btn ${c}">${icon ? ic(icon, 'xs') : ''}${label}</div>`;
const slider = pct => `<div class="slider"><div class="g"></div><div class="s" style="width:${pct}%"></div><div class="h" style="left:${pct}%"></div></div>`;
const tb = (icon, label = '', c = '') => `<div class="tbtn ${label ? 'text' : ''} ${c}">${ic(icon)}${label}</div>`;
const tsep = '<div class="tsep"></div>';

const STOCK = 'Abilities\\Spells\\';
const FX = {
  thunder: { n: 'ThunderClapCaster.mdx', f: STOCK + 'Human\\ThunderClap', t: 'thunderclap' },
  ring:    { n: 'FlameStrikeTarget.mdx', f: STOCK + 'Human\\FlameStrike', t: 'fsring' },
  flame:   { n: 'FlameStrike1.mdx', f: STOCK + 'Human\\FlameStrike', t: 'flame' },
  nova:    { n: 'FrostNovaTarget.mdx', f: STOCK + 'Undead\\FrostNova', t: 'frostnova' },
  holy:    { n: 'HolyBoltSpecialArt.mdx', f: STOCK + 'Human\\HolyBolt', t: 'holybolt' },
  bliz:    { n: 'BlizzardTarget.mdx', f: STOCK + 'Human\\Blizzard', t: 'ice' },
  tele:    { n: 'MassTeleportCaster.mdx', f: STOCK + 'Human\\MassTeleport', t: 'orb' },
  coil:    { n: 'DeathCoilSpecialArt.mdx', f: STOCK + 'Undead\\DeathCoil', t: 'heal' },
  star:    { n: 'StarfallTarget.mdx', f: STOCK + 'NightElf\\Starfall', t: 'orb' },
  cfire:   { n: 'FireNova.mdx', f: 'war3mapImported', t: 'firenova', c: 1 },
  carc:    { n: 'ArcaneBurst.mdx', f: 'war3mapImported', t: 'arcane', c: 1 },
  cring:   { n: 'FrostRing.mdx', f: 'war3mapImported', t: 'fsring', c: 1 },
  cheal:   { n: 'HealingBloom.mdx', f: 'war3mapImported', t: 'heal', c: 1 },
  crune:   { n: 'RuneRing_Gold.mdx', f: 'war3mapImported\\Spells', t: 'holybolt', c: 1 },
  cburst:  { n: 'Ring_Burst.mdx', f: 'war3mapImported\\Spells', t: 'arcane', c: 1 },
  cold:    { n: 'OldGlyph.mdx', f: 'war3mapImported', t: null, c: 1 },
};
const thumb = (k) => FX[k].t ? `<div class="thumb"><img src="viewport/th-${FX[k].t}.png"></div>` : '<div class="thumb bad">!</div>';

function titlebar(title) {
  return `<div class="titlebar"><div class="ico"></div><div class="grow elide">${title}</div><div class="wb">&#8211;</div><div class="wb">&#9633;</div><div class="wb">&#10005;</div></div>`;
}

function toolbar(o = {}) {
  const empty = o.empty;
  const ref = (label, on) => `<div class="tbtn menu ${on ? 'checked' : ''}">${ic(label.startsWith('Caster') ? 'units' : 'target', 's')}<span>${label}</span><div class="mb">${arrow}</div></div>`;
  return `<div class="toolbar">${tb('new')}${tb('open')}${tb('save', '', empty ? 'disabled' : '')}${tb('save-as', '', empty ? 'disabled' : '')}${tsep}
    <div class="tlabel">Composition</div>${combo(empty ? 'Untitled' : 'Frost Judgement*', 190)}${tsep}
    ${tb('copy-lua', 'Copy as Lua', empty ? 'disabled' : '')}${tsep}
    <div class="tlabel">Scale reference</div>${ref('Caster: Archmage', true)}${ref('Target: Footman', true)}${tsep}
    ${tb('grid', '', 'checked')}${tb('reset-camera')}<div class="grow"></div>
    <div class="tlabel">Ground</div>${combo('Lordaeron Summer', 148)}</div>`;
}

function segments(sel, counts) {
  return `<div class="seg">${['All', 'Custom', 'Stock'].map((n, i) => `<div class="${n === sel ? 'on' : ''}">${n} <span class="n">${counts[i]}</span></div>`).join('')}</div>`;
}

function search(text) {
  return `<div class="edit ${text ? 'focus' : ''}" style="flex:none">${ic('search', 'xs')}${text ? `<span class="grow">${text}</span><span class="muted">&#10005;</span>` : '<span class="ph">Search effects</span>'}</div>`;
}

/* o: source, counts, query, rows [[key, state]], status, message, scroll */
function library(o) {
  const rows = (o.rows || []).map(([k, st]) => `<div class="item ${st || ''}">${thumb(k)}<div class="two"><div class="elide">${FX[k].n}</div>
      <div class="elide small muted">${FX[k].f}</div></div>${FX[k].c ? '<div class="tag">Custom</div>' : ''}</div>`).join('');
  return `<div class="dock" style="width:272px;flex:none"><div class="dtitle">Effects<span class="dbtns">&#10064; &#10005;</span></div><div class="dbody">
    ${segments(o.source || 'All', o.counts || ['468', '38', '430'])}${search(o.query)}
    ${check('Include unit and doodad models', false)}
    <div class="list">${rows}${o.message ? `<div class="listmsg">${o.message}</div>` : ''}${o.scroll === false ? '' : '<div class="vscroll"><i style="top:3px;height:38px"></i></div>'}</div>
    <div class="small muted">${o.status || '468 effects, 468 shown'}</div>
    <div class="small muted" style="line-height:1.45">Drag onto the ground or a timeline row.<br>Double-click adds at the target point.</div></div></div>`;
}

const ALL_ROWS = [['cfire'], ['carc'], ['cring'], ['cheal'], ['crune'], ['cburst'], ['thunder'], ['ring'], ['flame'], ['nova'], ['holy'], ['bliz'], ['tele'], ['coil'], ['star']];

/* o: mode = 'sel' | 'none' | 'missing' */
function inspector(o = {}) {
  const mode = o.mode || 'sel';
  const none = mode === 'none', bad = mode === 'missing';
  const k = bad ? 'cold' : 'nova';
  const d = none ? 'disabled' : '';
  const v = (x) => none ? '' : x;
  const head = none
    ? '<div class="col" style="height:46px;justify-content:center;gap:4px;flex:none"><div class="strong">No effect selected</div><div class="small muted">Click one in the view or on the timeline.</div></div>'
    : `<div class="row" style="gap:8px;height:46px;flex:none">${thumb(k)}<div class="col grow" style="gap:3px"><div class="strong elide">${FX[k].n}</div>
       <div class="small muted elide">${bad ? 'Custom (map import)' : 'Stock (game files)'}</div><div class="small muted elide">${FX[k].f}</div></div></div>`;
  const warn = bad ? `<div class="row err" style="gap:6px;align-items:flex-start;flex:none">${ic('warning', 's')}<div style="line-height:1.4">File not found in the map's imports. The effect is skipped until you replace it.</div></div>
      <div class="row" style="flex:none">${btn('Replace Model...', 'default')}</div>` : '';
  return `<div class="dock" style="width:300px;flex:none"><div class="dtitle">Effect<span class="dbtns">&#10064; &#10005;</span></div><div class="dbody">
    ${head}${warn}
    <div class="group ${d}"><div class="gt">Timing</div>
      <div class="form ${d}"><div class="lb">Start delay</div>${spin(v(bad ? '1.40 s' : '0.80 s'), 0)}</div>
      <div class="form ${bad ? 'disabled' : d}"><div class="lb">Animation</div>${combo(v(bad ? 'Birth' : 'Birth (1.80 s)'), 0)}</div>
      <div class="form ${d}"><div class="lb">Speed</div>${spin(v('1.00x'), 0)}</div>
      <div class="form ${d}"><div class="lb">Ends</div>${combo(v('After animation'), 0)}</div>
      <div class="form disabled"><div class="lb muted" style="color:var(--color-text-disabled)">Remove at</div>${spin(v('2.60 s'), 0)}</div>
    </div>
    <div class="group ${d}"><div class="gt">Transform</div>
      <div class="form ${d}"><div class="lb">Offset X</div>${spin(v('0.0'), 0)}<div class="lb2">Y</div>${spin(v('0.0'), 0)}</div>
      <div class="form ${d}"><div class="lb">Height</div>${spin(v('32.0'), 0)}<div class="lb2">Scale</div>${spin(v('0.80'), 0, mode === 'sel' ? 'focus' : '')}</div>
      <div class="form ${d}"><div class="lb">Facing</div>${slider(none ? 0 : 0)}${spin(v('0&#176;'), 72)}</div>
      <div class="form ${d}"><div class="lb">Pitch</div>${spin(v('0&#176;'), 0)}<div class="lb2">Roll</div>${spin(v('0&#176;'), 0)}</div>
    </div>
    <div class="group ${d}"><div class="gt">Appearance</div>
      <div class="form ${d}"><div class="lb">Tint</div><div class="swatch" style="background:${none ? 'var(--color-window)' : '#c8f0ff'}"></div><span class="grow muted">${v('200, 240, 255')}</span>${btn('Reset', d)}</div>
      <div class="form ${d}"><div class="lb">Opacity</div>${slider(none ? 0 : 100)}${spin(v('100 %'), 72)}</div>
    </div>
    <div class="row" style="gap:6px;flex:none;margin-top:2px">${btn('Duplicate', d, 'duplicate')}${btn('Remove', d, 'remove')}</div>
    <div class="small muted" style="line-height:1.45;flex:none">Offsets are map units from the target point (X east, Y north). Height is above the ground.</div>
    </div></div>`;
}

/* rows: [key, start, length, tail, state] ; state: sel | off | bad */
const SPELL = [
  ['thunder', 0.00, 1.33, 0.00],
  ['ring',    0.30, 2.70, 0.50],
  ['nova',    0.80, 1.80, 0.60, 'sel'],
  ['holy',    1.10, 1.00, 0.40],
  ['cring',   1.40, 2.00, 0.00, 'off'],
  ['bliz',    2.00, 3.30, 0.00],
];
const SPAN = 6.0;
const pc = t => (t / SPAN * 100).toFixed(3) + '%';

function transport(o = {}) {
  const d = o.empty ? 'disabled' : '';
  return `<div class="transport"><div class="tbtn ${d}">${ic(o.empty || o.paused ? 'play' : 'pause', 's')}</div><div class="tbtn ${d}">${ic('restart', 's')}</div>
    <div style="width:104px;text-align:right" class="${o.empty ? '' : ''} muted nw">${o.empty ? '0.00 / 0.00 s' : `${o.time || '1.20'} / 5.30 s`}</div>
    ${check('Loop', !o.empty, d)}${combo('1.0x', 66, d)}<div class="grow"></div>
    <div class="muted">Snap</div>${combo('0.05 s', 76, d)}<div class="muted" style="margin-left:8px">Zoom</div><div class="${d}" style="width:110px;display:flex">${slider(35)}</div></div>`;
}

function timeline(o = {}) {
  const rows = o.rows || SPELL;
  const play = o.play ?? 1.2;
  let head = '';
  for (let i = 0; i <= 12; i++) {
    const t = i / 2;
    head += `<div class="tick ${i % 2 ? 'minor' : ''}" style="left:${pc(t)}"></div>`;
    if (i % 2 === 0 && t < SPAN) head += `<div class="tlab" style="left:${pc(t)}">${t} s</div>`;
  }
  const grid = Array.from({ length: 13 }, (_, i) => `<div class="tick" style="left:${pc(i / 2)}"></div>`).join('');
  const body = o.empty
    ? '<div class="tlempty">No effects yet. Each effect you add gets its own row; drag its bar to change the start delay.</div>'
    : rows.map(([k, s, len, tail, st], i) => {
        const label = st === 'bad' ? 'Missing file' : `${s.toFixed(2)} s &#8211; ${(s + len).toFixed(2)} s`;
        return `<div class="tlrow ${i % 2 ? 'alt' : ''} ${o.selBad && st === 'bad' ? 'sel' : ''} ${st === 'sel' ? 'sel' : ''}">
          <div class="tlname ${st === 'off' ? 'muted' : ''} ${st === 'bad' ? 'err' : ''}">${ic(st === 'bad' ? 'warning' : st === 'off' ? 'eye-off' : 'eye', 'xs')}<span class="elide grow">${FX[k].n}</span><span class="d">${s.toFixed(2)}</span></div>
          <div class="tlarea">${grid}<div class="bar ${st || ''}" style="left:${pc(s)};width:${pc(len)}">${label}</div>
          ${tail ? `<div class="tail" style="left:${pc(s + len)};width:${pc(tail)}"></div>` : ''}<div class="playhead" style="left:${pc(play)}"></div></div></div>`;
      }).join('');
  return `<div class="dock" style="height:262px;flex:none"><div class="dtitle">Timeline<span class="dbtns">&#10064; &#10005;</span></div>${transport(o)}
    <div class="tl"><div class="tlrow tlhead"><div class="tlname"><span class="grow">Effect</span><span class="d">Start</span></div>
      <div class="tlarea">${head}${o.empty ? '' : `<div class="playhead" style="left:${pc(play)};top:12px"></div><div class="playgrip" style="left:${pc(play)};top:11px"></div>`}</div></div>${body}</div></div>`;
}

function statusbar(left, segs) {
  return `<div class="statusbar"><span class="grow elide">${left}</span>${segs.map(s => `<span class="sg ${s.startsWith('!') ? 'warn' : ''}">${s.replace(/^!/, '')}</span>`).join('')}</div>`;
}

function viewA(o = {}) {
  const labels = `<div class="vlabel" style="left:23.7%;top:75%"><span>Caster</span></div><div class="vlabel" style="left:58%;top:70%"><span>Target</span></div>`;
  return `<div class="view grow"><img class="scene" src="viewport/${o.scene || (o.empty ? 'scene-a-empty' : 'scene-a')}.jpg">${labels}
    ${o.empty ? '<div class="vhint">Drag an effect from the Effects list onto the ground,<br>or double-click it to add it at the target point.</div>' : `<div class="vtime">${o.time || '1.20'} s</div>`}${o.extra || ''}</div>`;
}

/* Direction A window: library | view over timeline | effect settings */
function windowA(o = {}) {
  document.body.innerHTML = `<div class="window">${titlebar(o.title || 'Frost Judgement* - VFX Sandbox')}${toolbar(o)}
    <div class="row grow" style="align-items:stretch">${library(o.library || { rows: ALL_ROWS.map(r => r[0] === 'nova' ? ['nova', 'sel'] : r) })}<div class="splitv"></div>
      <div class="col grow">${viewA(o.view || {})}<div class="splith"></div>${timeline(o.timeline || {})}</div><div class="splitv"></div>
      ${inspector(o.inspector || {})}</div>
    ${statusbar(o.statusLeft || 'Drag an effect: move.  Shift + drag: height.  Right drag: orbit.  Wheel: zoom.', o.status || ['6 effects', 'Selected X 0.0  Y 0.0  Z 32.0', 'Cursor 192.0, -64.0', 'Unsaved changes'])}</div>`;
}
