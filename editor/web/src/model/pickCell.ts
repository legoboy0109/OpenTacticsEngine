// Port of engine/core/data/graphics.cpp (cellHash, pickCell) so the editor's canvas chooses the SAME piece
// as the player for every square. All arithmetic is 32-bit unsigned: Math.imul + `>>> 0`.
// tests/unit/pickCell.test.ts pins values taken from the C++ function.
import type { CellRef, Look } from './types'

export const JOIN_N = 1, JOIN_E = 2, JOIN_S = 4, JOIN_W = 8

export function cellHash(x: number, y: number): number {
  let h = (Math.imul(x, 0x9e3779b1) ^ ((Math.imul(y, 0x85ebca77) + 0x165667b1) | 0)) >>> 0
  h = (h ^ (h >>> 15)) >>> 0
  h = Math.imul(h, 0x2c1b3c6d) >>> 0
  h = (h ^ (h >>> 12)) >>> 0
  h = Math.imul(h, 0x297a2d39) >>> 0
  h = (h ^ (h >>> 15)) >>> 0
  return h
}

export const DEFAULT_FRAME_MS = 400

/** The blocks of a joining look: `join` is either one cell [c, r] or a list of cells [[c, r], ...] (an animated join). */
export function joinBlocks(join: CellRef | CellRef[]): CellRef[] {
  return Array.isArray(join[0]) ? (join as CellRef[]) : [join as CellRef]
}

/** The cells a look draws from (for any kind). For a join: its 4 x 4 blocks. */
export function lookCells(look: Look): CellRef[] {
  if (look.cell) return [look.cell]
  if (look.variants?.length) return look.variants
  if (look.frames?.length) return look.frames
  if (look.join) return joinBlocks(look.join)
  return []
}

export type LookShape = 'still' | 'variants' | 'animated' | 'join'
/** What the engine makes of a look: one cell in a list is a plain still. */
export function lookShape(look: Look): LookShape {
  if (look.join) return 'join'
  if (look.variants) return look.variants.length > 1 ? 'variants' : 'still'
  if (look.frames) return look.frames.length > 1 ? 'animated' : 'still'
  return 'still'
}

/** The piece to draw for square (x, y). `joinMask` = N1+E2+S4+W8 of neighbours that count as the same terrain. */
export function pickCell(look: Look, x: number, y: number, joinMask: number, timeMs: number): CellRef {
  const cells = lookCells(look)
  if (!cells.length) return [0, 0]
  const first = cells[0]
  switch (lookShape(look)) {
    case 'variants': return cells[cellHash(x, y) % cells.length]
    case 'animated': {
      const ms = look.frameMs !== undefined && look.frameMs > 0 ? look.frameMs : DEFAULT_FRAME_MS
      return cells[Math.floor(timeMs / ms) % cells.length]
    }
    case 'join': {
      // Several blocks = an animated join: time picks the block, the neighbours pick the piece inside it.
      let block = first
      if (cells.length > 1) {
        const ms = look.frameMs !== undefined && look.frameMs > 0 ? look.frameMs : DEFAULT_FRAME_MS
        block = cells[Math.floor(timeMs / ms) % cells.length]
      }
      const n = joinMask & 15
      return [block[0] + (n % 4), block[1] + Math.floor(n / 4)]
    }
    default: return first
  }
}

/** Neighbour bits for a terrain look at (x, y): `same` says whether the square at (nx, ny) counts as the same terrain. */
export function joinMaskAt(x: number, y: number, w: number, h: number, edgeCounts: boolean, same: (nx: number, ny: number) => boolean): number {
  const test = (nx: number, ny: number) => (nx < 0 || ny < 0 || nx >= w || ny >= h ? edgeCounts : same(nx, ny))
  return (test(x, y - 1) ? JOIN_N : 0) | (test(x + 1, y) ? JOIN_E : 0) | (test(x, y + 1) ? JOIN_S : 0) | (test(x - 1, y) ? JOIN_W : 0)
}
