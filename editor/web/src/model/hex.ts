// Hexagon geometry for the editor: the contract of docs/HEX.md in one small module. Everything about hexagon maps
// (neighbours, distance, rings, where a hexagon sits in the picture, which one is under the pointer) goes through here,
// so the canvas, the tools and the checks agree. Pure, no React.
//
// Cells are (x, y) = (column, row) on a rectangle of width x height cells ("offset" coordinates, docs/HEX.md):
//  - pointy: odd rows sit half a hexagon to the right; rows run straight across.
//  - flat: odd columns sit half a hexagon down; columns run straight down.
import type { Cell } from './mapGrid'

export type HexLayout = 'pointy' | 'flat'

/** The six neighbours in the contract's fixed order, as offsets by parity (even/odd row for pointy, even/odd column for flat).
 *  pointy: upper right, right, lower right, lower left, left, upper left. flat: up, upper right, lower right, down, lower left, upper left. */
const POINTY: Record<'even' | 'odd', Array<[number, number]>> = {
  even: [[0, -1], [1, 0], [0, 1], [-1, 1], [-1, 0], [-1, -1]],
  odd: [[1, -1], [1, 0], [1, 1], [0, 1], [-1, 0], [0, -1]],
}
const FLAT: Record<'even' | 'odd', Array<[number, number]>> = {
  even: [[0, -1], [1, -1], [1, 0], [0, 1], [-1, 0], [-1, -1]],
  odd: [[0, -1], [1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0]],
}

const par = (n: number): 'even' | 'odd' => (Math.abs(n) % 2 === 1 ? 'odd' : 'even')

/** The neighbour of (x, y) in direction `dir` (0..5); it may lie outside the map. */
export function neighbour(layout: HexLayout, x: number, y: number, dir: number): Cell {
  const [dx, dy] = layout === 'pointy' ? POINTY[par(y)][dir] : FLAT[par(x)][dir]
  return { x: x + dx, y: y + dy }
}

/** All six neighbours in direction order, including cells off the map. */
export const neighbours = (layout: HexLayout, x: number, y: number): Cell[] => [0, 1, 2, 3, 4, 5].map((d) => neighbour(layout, x, y, d))

const inside = (w: number, h: number, c: Cell): boolean => c.x >= 0 && c.y >= 0 && c.x < w && c.y < h

/** The neighbours that lie on a width x height map, still in direction order. */
export const neighboursOn = (layout: HexLayout, w: number, h: number, x: number, y: number): Cell[] => neighbours(layout, x, y).filter((c) => inside(w, h, c))

// ---- cube coordinates (for distance and straight lines) ----
interface Cube { q: number; r: number }
function toCube(layout: HexLayout, x: number, y: number): Cube {
  return layout === 'pointy' ? { q: x - (y - (y & 1)) / 2, r: y } : { q: x, r: y - (x - (x & 1)) / 2 }
}
function fromCube(layout: HexLayout, q: number, r: number): Cell {
  return layout === 'pointy' ? { x: q + (r - (r & 1)) / 2, y: r } : { x: q, y: r + (q - (q & 1)) / 2 }
}

/** Number of steps between two cells on an empty map. */
export function distance(layout: HexLayout, a: Cell, b: Cell): number {
  const p = toCube(layout, a.x, a.y), q = toCube(layout, b.x, b.y)
  const dq = p.q - q.q, dr = p.r - q.r
  return Math.max(Math.abs(dq), Math.abs(dr), Math.abs(dq + dr))
}

/** Every cell of the map within `radius` steps of `c` (c included), ascending index (row by row), like the engine's `within`. */
export function within(layout: HexLayout, w: number, h: number, c: Cell, radius: number): Cell[] {
  const out: Cell[] = []
  // The radius is a limit on steps, so a box of radius rows around the cell contains every candidate.
  const y0 = Math.max(0, c.y - radius - 1), y1 = Math.min(h - 1, c.y + radius + 1)
  const x0 = Math.max(0, c.x - radius - 1), x1 = Math.min(w - 1, c.x + radius + 1)
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) if (distance(layout, c, { x, y }) <= radius) out.push({ x, y })
  return out
}

/** The middle cell and its six neighbours (`ringCells` of the engine); null when any of them lies off the map. */
export function ringCells(layout: HexLayout, w: number, h: number, middle: Cell): Cell[] | null {
  const out = [middle, ...neighbours(layout, middle.x, middle.y)]
  return out.every((c) => inside(w, h, c)) ? out : null
}

/** The cells a straight line from `a` to `b` passes (both included): a fast drag leaves no gaps. */
export function line(layout: HexLayout, a: Cell, b: Cell): Cell[] {
  const n = distance(layout, a, b)
  if (n === 0) return [{ x: a.x, y: a.y }]
  const p = toCube(layout, a.x, a.y), q = toCube(layout, b.x, b.y)
  const out: Cell[] = []
  for (let i = 0; i <= n; i++) {
    // A tiny nudge keeps ties from landing exactly on an edge between two cells.
    const t = i / n
    const fq = p.q + (q.q - p.q) * t + 1e-6, fr = p.r + (q.r - p.r) * t + 2e-6
    out.push(fromCube(layout, ...roundCube(fq, fr)))
  }
  return out
}
function roundCube(fq: number, fr: number): [number, number] {
  const fs = -fq - fr
  let q = Math.round(fq), r = Math.round(fr)
  const s = Math.round(fs)
  const dq = Math.abs(q - fq), dr = Math.abs(r - fr), ds = Math.abs(s - fs)
  if (dq > dr && dq > ds) q = -r - s
  else if (dr > ds) r = -q - s
  return [q, r]
}

/** Moves `middle` by the same hexagon vector that takes `from` to `to` (dragging a structure by the hexagon it was grabbed at).
 *  Returns the new middle, which may be off the map. */
export function translate(layout: HexLayout, middle: Cell, from: Cell, to: Cell): Cell {
  const m = toCube(layout, middle.x, middle.y), f = toCube(layout, from.x, from.y), t = toCube(layout, to.x, to.y)
  return fromCube(layout, m.q + t.q - f.q, m.r + t.r - f.r)
}

// ---- the picture (docs/HEX.md "Top-down picture") ----
/** L: the multiple of 4 nearest to T * 1.1547 (24 -> 28, 16 -> 20, 32 -> 36). */
export const hexL = (tileSize: number): number => 4 * Math.round((tileSize * 1.1547) / 4)

/** A hexagon's box in pixels of the tile size: pointy T wide and L tall, flat L wide and T tall. */
export const boxSize = (layout: HexLayout, tileSize: number): { w: number; h: number } =>
  layout === 'pointy' ? { w: tileSize, h: hexL(tileSize) } : { w: hexL(tileSize), h: tileSize }

/** Where cell (x, y)'s box starts, in pixels of the tile size. */
export function boxPos(layout: HexLayout, tileSize: number, x: number, y: number): { left: number; top: number } {
  const L = hexL(tileSize)
  return layout === 'pointy'
    ? { left: x * tileSize + (Math.abs(y) % 2 === 1 ? tileSize / 2 : 0), top: (y * 3 * L) / 4 }
    : { left: (x * 3 * L) / 4, top: y * tileSize + (Math.abs(x) % 2 === 1 ? tileSize / 2 : 0) }
}

/** The whole map in pixels of the tile size. */
export function mapPixelSize(layout: HexLayout, tileSize: number, w: number, h: number): { w: number; h: number } {
  const L = hexL(tileSize)
  return layout === 'pointy'
    ? { w: w * tileSize + tileSize / 2, h: ((h - 1) * 3 * L) / 4 + L }
    : { w: ((w - 1) * 3 * L) / 4 + L, h: h * tileSize + tileSize / 2 }
}

/** The middle of a hexagon in pixels of the tile size. */
export function middle(layout: HexLayout, tileSize: number, x: number, y: number): { x: number; y: number } {
  const { left, top } = boxPos(layout, tileSize, x, y)
  const b = boxSize(layout, tileSize)
  return { x: left + b.w / 2, y: top + b.h / 2 }
}

/** The six corners of a hexagon, relative to its box's top-left, in order: edge `i` (corner i to corner i+1) faces neighbour `i`.
 *  pointy: top, upper right, lower right, bottom, lower left, upper left. flat: top left, top right, right, bottom right, bottom left, left. */
export function corners(layout: HexLayout, tileSize: number): Array<[number, number]> {
  const { w, h } = boxSize(layout, tileSize)
  return layout === 'pointy'
    ? [[w / 2, 0], [w, h / 4], [w, (3 * h) / 4], [w / 2, h], [0, (3 * h) / 4], [0, h / 4]]
    : [[w / 4, 0], [(3 * w) / 4, 0], [w, h / 2], [(3 * w) / 4, h], [w / 4, h], [0, h / 2]]
}

/** The cell whose middle is nearest to the point (px, py), in pixels of the tile size; null outside the map's picture. */
export function pick(layout: HexLayout, tileSize: number, w: number, h: number, px: number, py: number): Cell | null {
  const size = mapPixelSize(layout, tileSize, w, h)
  if (px < 0 || py < 0 || px >= size.w || py >= size.h) return null
  const L = hexL(tileSize)
  // A rough guess of the row/column, then the nearest middle among the cells around it.
  const gx = layout === 'pointy' ? Math.floor(px / tileSize) : Math.floor(px / ((3 * L) / 4))
  const gy = layout === 'pointy' ? Math.floor(py / ((3 * L) / 4)) : Math.floor(py / tileSize)
  let best: Cell | null = null, bestD = Infinity
  for (let y = gy - 2; y <= gy + 2; y++) {
    for (let x = gx - 2; x <= gx + 2; x++) {
      if (!inside(w, h, { x, y })) continue
      const m = middle(layout, tileSize, x, y)
      const d = (m.x - px) ** 2 + (m.y - py) ** 2
      if (d < bestD) { bestD = d; best = { x, y } }
    }
  }
  return best
}

/** The six-bit join number of a cell: bit i set when neighbour i counts as the same (off the map: `edge`). */
export function joinMask(layout: HexLayout, w: number, h: number, c: Cell, edge: boolean, same: (x: number, y: number) => boolean): number {
  let mask = 0
  neighbours(layout, c.x, c.y).forEach((n, i) => { if (inside(w, h, n) ? same(n.x, n.y) : edge) mask |= 1 << i })
  return mask
}

/** The outline edges of a set of cells: the edges that have no neighbour inside the set, as pairs of points in pixels of the
 *  tile size (the frame around a structure, a selection or a story area). */
export function outline(layout: HexLayout, tileSize: number, cells: Cell[]): Array<[[number, number], [number, number]]> {
  const set = new Set(cells.map((c) => `${c.x},${c.y}`))
  const cs = corners(layout, tileSize)
  const out: Array<[[number, number], [number, number]]> = []
  for (const c of cells) {
    const { left, top } = boxPos(layout, tileSize, c.x, c.y)
    neighbours(layout, c.x, c.y).forEach((n, i) => {
      if (set.has(`${n.x},${n.y}`)) return
      const a = cs[i], b = cs[(i + 1) % 6]
      out.push([[left + a[0], top + a[1]], [left + b[0], top + b[1]]])
    })
  }
  return out
}
