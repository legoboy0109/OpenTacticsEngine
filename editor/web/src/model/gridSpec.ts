// Which grid a game has, and the small set of grid questions the rest of the editor asks (neighbours, distance, within).
// Squares and hexagons answer through the same functions, so tools, fog and the checks do not branch on the grid.
// A game has ONE grid (game.json `grid`); its maps carry the same type (docs/HEX.md).
import * as hex from './hex'
import type { HexLayout } from './hex'
import type { Cell } from './mapGrid'
import type { Game } from './types'

export interface GridSpec { type: 'square' | 'hex'; layout?: HexLayout }
export const SQUARE: GridSpec = { type: 'square' }

/** The grid game.json asks for. Anything unreadable counts as squares (the helper reports the real problem). */
export function gridOfGame(game: Pick<Game, 'grid'> | null | undefined): GridSpec {
  const g = game?.grid
  if (g?.type === 'hex') return { type: 'hex', layout: g.layout === 'flat' ? 'flat' : 'pointy' }
  return SQUARE
}

export const isHex = (g: GridSpec): boolean => g.type === 'hex'
export const layoutOf = (g: GridSpec): HexLayout => g.layout ?? 'pointy'

/** Plain words for the Settings screen and the new-game cards. */
export function gridLabel(g: GridSpec): string {
  if (g.type !== 'hex') return 'Squares'
  return layoutOf(g) === 'flat' ? 'Hexagons, flat-top' : 'Hexagons, pointy-top'
}

/** The neighbours of a cell on a width x height map, in the engine's direction order (squares: up, right, down, left). */
export function neighboursOf(g: GridSpec, w: number, h: number, c: Cell): Cell[] {
  if (g.type === 'hex') return hex.neighboursOn(layoutOf(g), w, h, c.x, c.y)
  return [[0, -1], [1, 0], [0, 1], [-1, 0]].map(([dx, dy]) => ({ x: c.x + dx, y: c.y + dy })).filter((n) => n.x >= 0 && n.y >= 0 && n.x < w && n.y < h)
}

/** Steps between two cells on an empty map. */
export const distanceOf = (g: GridSpec, a: Cell, b: Cell): number => (g.type === 'hex' ? hex.distance(layoutOf(g), a, b) : Math.abs(a.x - b.x) + Math.abs(a.y - b.y))

/** Every cell within `radius` steps of `c` (c included), clipped to the map. */
export function withinOf(g: GridSpec, w: number, h: number, c: Cell, radius: number): Cell[] {
  if (g.type === 'hex') return hex.within(layoutOf(g), w, h, c, radius)
  const out: Cell[] = []
  for (let y = Math.max(0, c.y - radius); y <= Math.min(h - 1, c.y + radius); y++) {
    const span = radius - Math.abs(y - c.y)
    for (let x = Math.max(0, c.x - span); x <= Math.min(w - 1, c.x + span); x++) out.push({ x, y })
  }
  return out
}

/** The cells on a straight line between two cells (a fast drag leaves no gaps). */
export function lineOf(g: GridSpec, a: Cell, b: Cell): Cell[] {
  if (g.type === 'hex') return hex.line(layoutOf(g), a, b)
  const out: Cell[] = []
  const n = Math.max(Math.abs(b.x - a.x), Math.abs(b.y - a.y))
  for (let i = 0; i <= n; i++) {
    const t = n === 0 ? 0 : i / n
    out.push({ x: Math.round(a.x + (b.x - a.x) * t), y: Math.round(a.y + (b.y - a.y) * t) })
  }
  return out
}
