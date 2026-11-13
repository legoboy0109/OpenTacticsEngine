import { describe, expect, it } from 'vitest'
import { cellHash, joinMaskAt, lookCells, lookShape, pickCell } from '../../src/model/pickCell'
import type { Look } from '../../src/model/types'

// Values printed by the C++ cellHash in engine/core/data/graphics.cpp (copied verbatim into a tiny program).
const CPP: Array<[number, number, number]> = [
  [0, 0, 1421339475], [1, 0, 3000735075], [0, 1, 3786603423], [1, 1, 2518099951], [2, 3, 3525833578],
  [5, 5, 2348739325], [7, 2, 991586464], [10, 10, 448161490], [63, 63, 259339321], [100, 200, 924981042],
  [255, 0, 2443226318], [0, 255, 2322833608], [1000, 1000, 2669954889], [12345, 54321, 785446783],
  [255, 255, 3215707438], [31, 17, 2547228076],
]

describe('cellHash (port of the engine)', () => {
  it('matches the C++ function exactly', () => {
    for (const [x, y, h] of CPP) expect(cellHash(x, y), `(${x}, ${y})`).toBe(h)
  })
  it('is always an unsigned 32-bit number', () => {
    for (let x = 0; x < 40; x++) for (let y = 0; y < 40; y++) {
      const h = cellHash(x, y)
      expect(Number.isInteger(h) && h >= 0 && h <= 0xffffffff).toBe(true)
    }
  })
})

describe('pickCell', () => {
  const variants: Look = { sheet: 's', variants: [[0, 0], [1, 0], [2, 0]] }
  it('picks a variant by position: hash modulo the count', () => {
    for (const [x, y, h] of CPP) expect(pickCell(variants, x, y, 0, 0)).toEqual([h % 3, 0])
  })
  it('a still look ignores everything; a one-cell list is a still', () => {
    expect(pickCell({ sheet: 's', cell: [3, 4] }, 9, 9, 15, 999)).toEqual([3, 4])
    expect(lookShape({ sheet: 's', variants: [[1, 1]] })).toBe('still')
    expect(lookShape({ sheet: 's', frames: [[1, 1]] })).toBe('still')
    expect(pickCell({ sheet: 's', frames: [[2, 2]] }, 0, 0, 0, 5000)).toEqual([2, 2])
  })
  it('animations loop by time; frameMs defaults to 400', () => {
    const a: Look = { sheet: 's', frames: [[0, 1], [1, 1], [2, 1]], frameMs: 500 }
    expect([0, 499, 500, 1000, 1499, 1500].map((t) => pickCell(a, 0, 0, 0, t)[0])).toEqual([0, 0, 1, 2, 2, 0])
    const d: Look = { sheet: 's', frames: [[0, 0], [1, 0]] }
    expect([399, 400, 800].map((t) => pickCell(d, 0, 0, 0, t)[0])).toEqual([0, 1, 0])
  })
  it('join: piece n sits at column n % 4, row n / 4 of the block', () => {
    const j: Look = { sheet: 's', join: [4, 8] }
    for (let n = 0; n < 16; n++) expect(pickCell(j, 0, 0, n, 0)).toEqual([4 + (n % 4), 8 + Math.floor(n / 4)])
    expect(pickCell(j, 0, 0, 0xff, 0)).toEqual([4 + 3, 8 + 3]) // only the low 4 bits count
  })
  it('lookCells lists the cells of any kind', () => {
    expect(lookCells({ sheet: 's', cell: [1, 2] })).toEqual([[1, 2]])
    expect(lookCells({ sheet: 's', join: [0, 4] })).toEqual([[0, 4]])
    expect(lookCells({ sheet: 's' })).toEqual([])
  })
})

describe('join mask', () => {
  // A road along the middle row of a 3 x 3 map; the other squares are something else.
  const road = (x: number, y: number) => y === 1 && x >= 0 && x < 3
  it('N=1 E=2 S=4 W=8, from the neighbours that count as the same', () => {
    expect(joinMaskAt(1, 1, 3, 3, false, road)).toBe(2 | 8) // E+W = 10: a road running left-right
    expect(joinMaskAt(0, 1, 3, 3, false, road)).toBe(2) // E only: a dead end, edge does not count
    expect(joinMaskAt(2, 1, 3, 3, false, road)).toBe(8)
    expect(joinMaskAt(1, 0, 3, 3, false, () => false)).toBe(0) // alone
  })
  it('the map edge counts as a neighbour with joinEdge', () => {
    expect(joinMaskAt(0, 1, 3, 3, true, road)).toBe(2 | 8) // west is the edge
    expect(joinMaskAt(0, 0, 3, 3, true, () => false)).toBe(1 | 8) // top-left corner: N and W are the edge
    expect(joinMaskAt(2, 2, 3, 3, true, () => false)).toBe(2 | 4)
  })
  it('all four neighbours make piece 15 (a crossing)', () => {
    expect(joinMaskAt(1, 1, 3, 3, false, () => true)).toBe(15)
  })
})
