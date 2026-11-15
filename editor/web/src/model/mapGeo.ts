// Where cells are on the picture of a map, in "units" of one tile (T pixels of the game's tile size). The map canvas, the
// previews and the tools ask this one object, so squares and hexagons share all of the tool code:
//   squares: cell (x, y) is the unit square at (x, y);
//   hexagons: cell (x, y) has the box of docs/HEX.md "Top-down picture" (model/hex.ts), measured in tiles.
import * as hex from './hex'
import { isHex, layoutOf, type GridSpec } from './gridSpec'
import type { Cell } from './mapGrid'

export interface Box { x: number; y: number; w: number; h: number }
export type Edge = [[number, number], [number, number]]

export interface Geo {
  grid: GridSpec
  hex: boolean
  /** Cells across and down. */
  w: number
  h: number
  /** The whole map's picture, in tiles. */
  uw: number
  uh: number
  /** The cell's box (a hexagon touches it on all four sides). */
  box(x: number, y: number): Box
  /** The cell at a point of the picture (null outside the map); hexagons: the nearest middle. */
  at(ux: number, uy: number): Cell | null
  /** The cells whose box may show in the rectangle of the picture (a little more than needed is fine). */
  range(ux0: number, uy0: number, ux1: number, uy1: number): { x0: number; x1: number; y0: number; y1: number }
  /** The six corners of a hexagon, relative to the cell's box, in tiles (edge i faces neighbour i). Squares: the four corners. */
  corners: Array<[number, number]>
  /** The edges around a set of cells (frame, selection, areas), as pairs of points of the picture in tiles. */
  outline(cells: Cell[]): Edge[]
}

/** `tile` = the game's tile size in pixels (view.tileSize, default 24). */
export function makeGeo(grid: GridSpec, tile: number, w: number, h: number): Geo {
  if (!isHex(grid)) return squareGeo(grid, w, h)
  const layout = layoutOf(grid)
  const T = tile
  const size = hex.mapPixelSize(layout, T, w, h)
  const bs = hex.boxSize(layout, T)
  const cs = hex.corners(layout, T).map(([a, b]) => [a / T, b / T] as [number, number])
  const L = hex.hexL(T)
  // Cell spacing in tiles.
  const sx = layout === 'pointy' ? 1 : (3 * L) / 4 / T, sy = layout === 'pointy' ? (3 * L) / 4 / T : 1
  return {
    grid, hex: true, w, h, uw: size.w / T, uh: size.h / T,
    box: (x, y) => { const p = hex.boxPos(layout, T, x, y); return { x: p.left / T, y: p.top / T, w: bs.w / T, h: bs.h / T } },
    at: (ux, uy) => hex.pick(layout, T, w, h, ux * T, uy * T),
    // One cell of slack on every side covers the half-cell shifts and the overlap of the boxes.
    range: (ux0, uy0, ux1, uy1) => ({
      x0: Math.max(0, Math.floor(ux0 / sx) - 2), x1: Math.min(w - 1, Math.floor(ux1 / sx) + 1),
      y0: Math.max(0, Math.floor(uy0 / sy) - 2), y1: Math.min(h - 1, Math.floor(uy1 / sy) + 1),
    }),
    corners: cs,
    outline: (cells) => hex.outline(layout, T, cells).map(([a, b]) => [[a[0] / T, a[1] / T], [b[0] / T, b[1] / T]] as Edge),
  }
}

function squareGeo(grid: GridSpec, w: number, h: number): Geo {
  return {
    grid, hex: false, w, h, uw: w, uh: h,
    box: (x, y) => ({ x, y, w: 1, h: 1 }),
    at(ux, uy) {
      const x = Math.floor(ux), y = Math.floor(uy)
      return x >= 0 && y >= 0 && x < w && y < h ? { x, y } : null
    },
    range: (ux0, uy0, ux1, uy1) => ({ x0: Math.max(0, Math.floor(ux0)), x1: Math.min(w - 1, Math.floor(ux1)), y0: Math.max(0, Math.floor(uy0)), y1: Math.min(h - 1, Math.floor(uy1)) }),
    corners: [[0, 0], [1, 0], [1, 1], [0, 1]],
    outline(cells) {
      const set = new Set(cells.map((c) => `${c.x},${c.y}`))
      const out: Edge[] = []
      for (const c of cells) {
        if (!set.has(`${c.x},${c.y - 1}`)) out.push([[c.x, c.y], [c.x + 1, c.y]])
        if (!set.has(`${c.x + 1},${c.y}`)) out.push([[c.x + 1, c.y], [c.x + 1, c.y + 1]])
        if (!set.has(`${c.x},${c.y + 1}`)) out.push([[c.x, c.y + 1], [c.x + 1, c.y + 1]])
        if (!set.has(`${c.x - 1},${c.y}`)) out.push([[c.x, c.y], [c.x, c.y + 1]])
      }
      return out
    },
  }
}
