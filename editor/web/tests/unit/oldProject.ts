// A project in the format before structures existed (the economy-tactics template as it was): buildings are terrain
// with `capture`, `income`, `produces` and the tag `hq`, and the map has an `owners` list. Used by the Convert tests
// (unit and e2e) so they do not depend on today's templates.
import type { MapFile, ProjectDoc } from '../../src/model/types'

export const OLD_TERRAIN = [
  { id: 'plain', name: 'Plain', color: '#9ccc65', glyph: '.', props: { defense: 1 } },
  { id: 'road', name: 'Road', color: '#c9c3b0', glyph: 'r', props: { defense: 0 } },
  { id: 'forest', name: 'Forest', color: '#43803c', glyph: 'f', moveCost: { tread: 2 }, props: { defense: 2 } },
  { id: 'city', name: 'City', color: '#b7b7b7', glyph: 'c', props: { defense: 3 }, capture: 20, income: { funds: 1000 } },
  { id: 'factory', name: 'Factory', color: '#9a9a9a', glyph: 'F', props: { defense: 3 }, capture: 20, income: { funds: 1000 }, produces: ['infantry', 'tank', 'artillery'] },
  { id: 'hq', name: 'Headquarters', color: '#d4af37', glyph: 'H', props: { defense: 4 }, capture: 20, income: { funds: 1000 }, tags: ['hq'] },
]

export const OLD_MAP: MapFile = {
  name: 'Crossroads',
  grid: { type: 'square', width: 12, height: 8 },
  legend: { '.': 'plain', r: 'road', f: 'forest', c: 'city', F: 'factory', H: 'hq' },
  tiles: [
    '..f..cc..f..',
    '...rrrrrr...',
    '.H.r....r.H.',
    '..FrrrrrrF..',
    '.c.rf..fr.c.',
    '..r......r..',
    '.f.f.cc.f.f.',
    '..f......f..',
  ],
  sides: [
    { id: 'blue', name: 'Blue Army', color: '#3d85c6', controller: 'human', resources: { funds: 3000 } },
    { id: 'red', name: 'Red Army', color: '#cc0000', controller: 'ai', resources: { funds: 3000 } },
  ],
  owners: [
    { x: 1, y: 2, side: 'blue' }, { x: 2, y: 3, side: 'blue' }, { x: 1, y: 4, side: 'blue' },
    { x: 10, y: 2, side: 'red' }, { x: 9, y: 3, side: 'red' }, { x: 10, y: 4, side: 'red' },
    { x: 0, y: 0, side: 'blue' }, // on plain: never meant anything
  ],
  units: [
    { type: 'infantry', side: 'blue', x: 2, y: 2 },
    { type: 'tank', side: 'red', x: 8, y: 3 },
  ],
}

/** The old game.json, with a night style that recolours two of the buildings. */
export const OLD_GAME = {
  formatVersion: 1,
  name: 'Economy Tactics',
  stats: [{ id: 'hp', name: 'HP' }, { id: 'mov', name: 'Move' }],
  roles: { hp: 'hp', move: 'mov' },
  resources: [{ id: 'funds', name: 'Funds' }],
  moveTypes: [{ id: 'foot', name: 'Foot' }, { id: 'tread', name: 'Tread' }],
  rules: { turnOrder: 'sides', counterAttacks: true, defeat: ['noUnits', 'lostHq'], economy: { enabled: true }, production: { enabled: true }, capture: { enabled: true, amount: 'a.hp / 10' } },
  styles: [
    { id: 'classic', name: 'Classic' },
    { id: 'night', name: 'Night', background: '#05060f', terrainColors: { plain: '#2f4f3a', city: '#6a6a78', hq: '#8a7a30' } },
  ],
  maps: [{ id: 'crossroads', name: 'Crossroads', file: 'maps/crossroads.json' }],
}

export const OLD_UNITS = [
  { id: 'infantry', name: 'Infantry', moveType: 'foot' },
  { id: 'tank', name: 'Tank', moveType: 'tread' },
  { id: 'artillery', name: 'Artillery', moveType: 'tread' },
]

export function oldProject(): ProjectDoc {
  return JSON.parse(JSON.stringify({
    game: OLD_GAME,
    terrain: OLD_TERRAIN,
    structures: null,
    weapons: [],
    units: OLD_UNITS,
    maps: { crossroads: OLD_MAP },
    scripts: {},
    graphics: {
      sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 24, cellHeight: 24 }],
      terrain: {
        plain: { sheet: 'tiles', variants: [[0, 0], [1, 0]] },
        city: { sheet: 'tiles', cell: [3, 1] },
        hq: { sheet: 'tiles', frames: [[0, 2], [1, 2]], frameMs: 300 },
        road: { sheet: 'tiles', join: [0, 4], joinWith: ['city', 'factory', 'hq'] },
      },
      styles: { night: { terrain: { city: { sheet: 'tiles', cell: [4, 1] }, hq: null } } },
    },
  })) as ProjectDoc
}

/** The map and terrain files as they were on disk (the e2e test writes these into a game folder). */
export const OLD_FILES = {
  'data/terrain.json': { terrain: OLD_TERRAIN },
  'maps/crossroads.json': OLD_MAP,
}
