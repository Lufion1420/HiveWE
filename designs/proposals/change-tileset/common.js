// Mockup helper only (not shipped). Reads the theme from the URL hash and builds repeated tile widgets.
document.documentElement.dataset.theme = (location.hash.slice(1) || 'dark');

// id, name, texture class, file; sample data from the Ashenvale/Lordaeron tilesets (names illustrative).
const T = {
  Adrt: ['Dirt', 'adrt', 'TerrainArt\\Ashenvale\\Ashen_Dirt.blp'],
  Agrs: ['Grass', 'agrs', 'TerrainArt\\Ashenvale\\Ashen_Grass.blp'],
  Arck: ['Rock', 'arck', 'TerrainArt\\Ashenvale\\Ashen_Rock.blp'],
  Agrd: ['Grassy Dirt', 'agrd', 'TerrainArt\\Ashenvale\\Ashen_GrassyDirt.blp'],
  Avin: ['Vines', 'avin', 'TerrainArt\\Ashenvale\\Ashen_Vines.blp'],
  Adrg: ['Dirt Grass', 'adrg', 'TerrainArt\\Ashenvale\\Ashen_DirtGrass.blp'],
  Alvd: ['Leaves', 'alvd', 'TerrainArt\\Ashenvale\\Ashen_Leaves.blp'],
  Acbl: ['Cobble Path', 'acbl', 'TerrainArt\\Ashenvale\\Ashen_Cobble.blp'],
  Ldrt: ['Dirt', 'ldrt', 'TerrainArt\\LordaeronSummer\\Lords_Dirt.blp'],
  Ldro: ['Rough Dirt', 'ldro', 'TerrainArt\\LordaeronSummer\\Lords_DirtRough.blp'],
  Lrok: ['Rock', 'lrok', 'TerrainArt\\LordaeronSummer\\Lords_Rock.blp'],
  Lgrs: ['Grass', 'lgrs', 'TerrainArt\\LordaeronSummer\\Lords_Grass.blp'],
  Lgrd: ['Grassy Dirt', 'lgrd', 'TerrainArt\\LordaeronSummer\\Lords_GrassDark.blp'],
  Cx01: ['Marble Tiles', 'cx01', 'ReplaceableTextures\\CustomTiles\\marble_tile.blp'],
  Cx02: ['Frozen Moss', 'cx02', 'ReplaceableTextures\\CustomTiles\\frozen_moss.blp'],
  Cx03: ['Sand Stone', 'cx03', 'ReplaceableTextures\\CustomTiles\\sand_stone.blp'],
};
const tile = (id, o = {}) => {
  const [n, c] = T[id];
  return `<div class="tile ${o.checked ? 'checked' : ''} ${o.disabled ? 'disabled' : ''} ${o.removed ? 'removed' : ''}"><div class="img tx tx-${c}">` +
    `<span class="id">${id}</span>${o.badge ? `<span class="badge ${o.badge[0]}">${o.badge[1]}</span>` : ''}</div></div>`;
};
const thumb = (id, cls = '') => `<div class="thumb ${cls} tx tx-${T[id][1]}"></div>`;
const swatch = id => `<div class="swatch">${('<div class="tx tx-' + T[id][1] + '"></div>').repeat(9)}</div>`;
const chk = (on, label) => `<span class="chk ${on ? 'on' : ''}"><i></i>${label}</span>`;
const titlebar = t => `<div class="titlebar"><div class="ico"></div><div class="t">${t}</div><div class="wb">&#8211;</div><div class="wb">&#9744;</div><div class="wb">&#10005;</div></div>`;
