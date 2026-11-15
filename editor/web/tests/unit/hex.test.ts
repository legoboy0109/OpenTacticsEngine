// The hexagon geometry pinned against the contract text of docs/HEX.md (neighbour order, odd/even shift, L, boxes, picking).
import { describe, expect, it } from 'vitest'
import * as hex from '../../src/model/hex'
import { fillFrom } from '../../src/model/mapPaint'
import { makeGeo } from '../../src/model/mapGeo'
import { withinOf } from '../../src/model/gridSpec'
import type { MapFile, Terrain } from '../../src/model/types'

// The four lists exactly as the contract prints them.
const CONTRACT = {
  pointyEven: '(x, y-1) (x+1, y) (x, y+1) (x-1, y+1) (x-1, y) (x-1, y-1)',
  pointyOdd: '(x+1, y-1) (x+1, y) (x+1, y+1) (x, y+1) (x-1, y) (x, y-1)',
  flatEven: '(x, y-1) (x+1, y-1) (x+1, y) (x, y+1) (x-1, y) (x-1, y-1)',
  flatOdd: '(x, y-1) (x+1, y) (x+1, y+1) (x, y+1) (x-1, y+1) (x-1, y)',
}
/** Reads "(x+1, y-1)" at (x, y). */
function parse(text: string, x: number, y: number): Array<{ x: number; y: number }> {
  return [...text.matchAll(/\(x([+-]\d)?, y([+-]\d)?\)/g)].map((m) => ({ x: x + Number(m[1] ?? 0), y: y + Number(m[2] ?? 0) }))
}

describe('neighbours (docs/HEX.md "Coordinates")', () => {
  it('pointy: even and odd rows in the contract order', () => {
    expect(hex.neighbours('pointy', 4, 2)).toEqual(parse(CONTRACT.pointyEven, 4, 2))
    expect(hex.neighbours('pointy', 4, 3)).toEqual(parse(CONTRACT.pointyOdd, 4, 3))
    expect(hex.neighbours('pointy', 0, 0)).toEqual(parse(CONTRACT.pointyEven, 0, 0))
  })
  it('flat: even and odd columns in the contract order', () => {
    expect(hex.neighbours('flat', 2, 4)).toEqual(parse(CONTRACT.flatEven, 2, 4))
    expect(hex.neighbours('flat', 3, 4)).toEqual(parse(CONTRACT.flatOdd, 3, 4))
  })
  it('neighboursOn drops cells off the map and keeps the order (edges and corners)', () => {
    expect(hex.neighboursOn('pointy', 12, 9, 0, 0)).toEqual([{ x: 1, y: 0 }, { x: 0, y: 1 }])
    expect(hex.neighboursOn('pointy', 12, 9, 11, 1).map((c) => `${c.x},${c.y}`)).toEqual(['11,2', '10,1', '11,0'])
    expect(hex.neighboursOn('flat', 12, 9, 0, 0).map((c) => `${c.x},${c.y}`)).toEqual(['1,0', '0,1'])
    expect(hex.neighboursOn('flat', 12, 9, 11, 8).map((c) => `${c.x},${c.y}`)).toEqual(['11,7', '10,8'])
  })
  it('the neighbour relation is symmetric and direction i + 3 comes back', () => {
    for (const layout of ['pointy', 'flat'] as const) {
      for (let y = 0; y < 6; y++) for (let x = 0; x < 6; x++) {
        for (let d = 0; d < 6; d++) {
          const n = hex.neighbour(layout, x, y, d)
          expect(hex.neighbour(layout, n.x, n.y, (d + 3) % 6)).toEqual({ x, y })
        }
      }
    }
  })
})

describe('distance, within, ring', () => {
  it('distance equals the number of steps (breadth-first search) in both layouts', () => {
    for (const layout of ['pointy', 'flat'] as const) {
      const W = 9, H = 8
      for (const start of [{ x: 0, y: 0 }, { x: 4, y: 3 }, { x: 8, y: 7 }, { x: 3, y: 6 }]) {
        const steps = new Map<string, number>([[`${start.x},${start.y}`, 0]])
        const queue = [start]
        while (queue.length) {
          const c = queue.shift()!
          for (const n of hex.neighboursOn(layout, W, H, c.x, c.y)) {
            if (steps.has(`${n.x},${n.y}`)) continue
            steps.set(`${n.x},${n.y}`, steps.get(`${c.x},${c.y}`)! + 1)
            queue.push(n)
          }
        }
        for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) expect(hex.distance(layout, start, { x, y })).toBe(steps.get(`${x},${y}`))
      }
    }
  })
  it('within lists cells at distance <= r, ascending index, clipped to the map', () => {
    const cells = hex.within('pointy', 12, 9, { x: 5, y: 4 }, 2)
    expect(cells).toHaveLength(19)
    expect(cells.map((c) => c.y * 12 + c.x)).toEqual(cells.map((c) => c.y * 12 + c.x).sort((a, b) => a - b))
    expect(hex.within('flat', 12, 9, { x: 0, y: 0 }, 1).map((c) => `${c.x},${c.y}`)).toEqual(['0,0', '1,0', '0,1'])
    expect(withinOf({ type: 'hex', layout: 'flat' }, 5, 5, { x: 2, y: 2 }, 1)).toHaveLength(7)
    expect(withinOf({ type: 'square' }, 5, 5, { x: 2, y: 2 }, 1)).toHaveLength(5)
  })
  it('ring cells: the middle then its six neighbours; null when one is off the map', () => {
    const ring = hex.ringCells('pointy', 12, 9, { x: 5, y: 3 })!
    expect(ring[0]).toEqual({ x: 5, y: 3 })
    expect(ring.slice(1)).toEqual(hex.neighbours('pointy', 5, 3))
    expect(hex.ringCells('pointy', 12, 9, { x: 0, y: 3 })).toBeNull()
    expect(hex.ringCells('flat', 12, 9, { x: 5, y: 0 })).toBeNull()
    expect(hex.ringCells('flat', 12, 9, { x: 5, y: 4 })).not.toBeNull()
  })
  it('lines have no gaps: each step moves to a neighbour', () => {
    for (const layout of ['pointy', 'flat'] as const) {
      const cells = hex.line(layout, { x: 1, y: 1 }, { x: 8, y: 6 })
      expect(cells[0]).toEqual({ x: 1, y: 1 })
      expect(cells[cells.length - 1]).toEqual({ x: 8, y: 6 })
      for (let i = 1; i < cells.length; i++) expect(hex.distance(layout, cells[i - 1], cells[i])).toBe(1)
    }
  })
  it('translate moves a middle by the vector between two hexagons (the same shape under the pointer)', () => {
    const grab = hex.neighbour('pointy', 5, 4, 2)
    const target = { x: 8, y: 7 }
    const mid = hex.translate('pointy', { x: 5, y: 4 }, grab, target)
    expect(hex.neighbour('pointy', mid.x, mid.y, 2)).toEqual(target)
  })
})

describe('the picture (docs/HEX.md "Top-down picture")', () => {
  it('L is the multiple of 4 nearest to T x 1.1547', () => {
    expect([24, 16, 32].map(hex.hexL)).toEqual([28, 20, 36])
  })
  it('pointy boxes: left x*T (+T/2 on odd rows), top y*3L/4; map size', () => {
    expect(hex.boxPos('pointy', 24, 2, 0)).toEqual({ left: 48, top: 0 })
    expect(hex.boxPos('pointy', 24, 2, 1)).toEqual({ left: 60, top: 21 })
    expect(hex.boxSize('pointy', 24)).toEqual({ w: 24, h: 28 })
    expect(hex.mapPixelSize('pointy', 24, 12, 9)).toEqual({ w: 12 * 24 + 12, h: 8 * 21 + 28 })
  })
  it('flat boxes: left x*3L/4, top y*T (+T/2 on odd columns); map size', () => {
    expect(hex.boxPos('flat', 24, 2, 3)).toEqual({ left: 42, top: 72 })
    expect(hex.boxPos('flat', 24, 3, 3)).toEqual({ left: 63, top: 84 })
    expect(hex.boxSize('flat', 24)).toEqual({ w: 28, h: 24 })
    expect(hex.mapPixelSize('flat', 24, 12, 9)).toEqual({ w: 11 * 21 + 28, h: 9 * 24 + 12 })
  })
  it('the hexagon touches its box on all four sides; corner i to i+1 is the edge that faces neighbour i', () => {
    const p = hex.corners('pointy', 24)
    expect(p).toEqual([[12, 0], [24, 7], [24, 21], [12, 28], [0, 21], [0, 7]])
    const f = hex.corners('flat', 24)
    expect(f).toEqual([[7, 0], [21, 0], [28, 12], [21, 24], [7, 24], [0, 12]])
    // the midpoint of edge 1 (right side, pointy) is shared with the right neighbour's edge 4 (its left side)
    const a = hex.outline('pointy', 24, [{ x: 0, y: 0 }]).length
    expect(a).toBe(6)
    expect(hex.outline('pointy', 24, hex.ringCells('pointy', 12, 9, { x: 3, y: 3 })!)).toHaveLength(18)
  })
  it('picking returns the cell whose middle is nearest, in both layouts', () => {
    for (const layout of ['pointy', 'flat'] as const) {
      for (let y = 0; y < 5; y++) for (let x = 0; x < 6; x++) {
        const m = hex.middle(layout, 24, x, y)
        expect(hex.pick(layout, 24, 6, 5, m.x, m.y)).toEqual({ x, y })
        expect(hex.pick(layout, 24, 6, 5, m.x + 2, m.y - 2)).toEqual({ x, y })
      }
      expect(hex.pick(layout, 24, 6, 5, -1, 5)).toBeNull()
      const size = hex.mapPixelSize(layout, 24, 6, 5)
      expect(hex.pick(layout, 24, 6, 5, size.w + 1, 5)).toBeNull()
    }
  })
  it('the geometry object tiles: neighbouring boxes share corner points', () => {
    const geo = makeGeo({ type: 'hex', layout: 'pointy' }, 24, 6, 5)
    const a = geo.box(1, 1), r = geo.box(2, 1)
    // the right edge of a (corners 1-2) is the left edge of r (corners 5-4)
    expect([a.x + geo.corners[1][0], a.y + geo.corners[1][1]]).toEqual([r.x + geo.corners[5][0], r.y + geo.corners[5][1]])
    expect(geo.uw).toBeCloseTo((6 * 24 + 12) / 24)
  })
  it('the join mask has bit i set when neighbour i is the same (off the map: the edge setting)', () => {
    const same = (x: number, y: number) => y === 2 && x >= 2
    // (3, 2) on an even row: right (x+1, y) and left (x-1, y) are bits 1 and 4
    expect(hex.joinMask('pointy', 8, 6, { x: 3, y: 2 }, false, same)).toBe((1 << 1) | (1 << 4))
    // the corner (0, 0): upper right, lower left, left and upper left are off the map (bits 0, 3, 4, 5 take the edge setting)
    expect(hex.joinMask('pointy', 8, 6, { x: 0, y: 0 }, true, () => false)).toBe(1 | 8 | 16 | 32)
    expect(hex.joinMask('pointy', 8, 6, { x: 0, y: 0 }, false, () => false)).toBe(0)
  })
})

describe('fill goes through the six neighbours', () => {
  const terrain: Terrain[] = [{ id: 'a', name: 'A' }, { id: 'b', name: 'B' }]
  const map = (rows: string[]): MapFile => ({ name: 't', grid: { type: 'hex', width: rows[0].length, height: rows.length }, legend: { a: 'a', b: 'b' }, tiles: rows })
  it('reaches a neighbour that a square fill would not (pointy, odd row: upper right is (x+1, y-1))', () => {
    const m = map(['bba', 'bab', 'bbb'])
    // (1,1) is on an odd row: its upper right neighbour is (2,0)
    expect(fillFrom(m, terrain, 1, 1, 'b', { type: 'hex', layout: 'pointy' }).tiles).toEqual(['bbb', 'bbb', 'bbb'])
    expect(fillFrom(m, terrain, 1, 1, 'b', { type: 'square' }).tiles[0]).toBe('bba')
  })
  it('does not cross to the neighbour a hexagon does not have (even row: no (x+1, y-1))', () => {
    const m = map(['bab', 'bba', 'bbb'])
    // (1,0) is on an even row: its lower right is (1,1), not (2,1); (2,1) is not a neighbour of (1,0)
    const f = fillFrom(m, terrain, 1, 0, 'b', { type: 'hex', layout: 'pointy' })
    expect(f.tiles).toEqual(['bbb', 'bba', 'bbb'])
  })
  it('flat layout uses columns: odd columns sit lower, so (1,0) touches (0,0) and (0,1)', () => {
    const m = map(['bab', 'abb'])
    // (1,0) is an odd column: its lower left neighbour is (x-1, y+1) = (0,1)
    expect(fillFrom(m, terrain, 1, 0, 'b', { type: 'hex', layout: 'flat' }).tiles).toEqual(['bbb', 'bbb'])
    expect(fillFrom(m, terrain, 1, 0, 'b', { type: 'square' }).tiles).toEqual(['bbb', 'abb'])
  })
})
