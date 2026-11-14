// The drawing order of structures and units (engine/player/battle/battle_draw.cpp): by the row of the bottom edge,
// structures first on a tie, a unit on a structure sorts with that structure's bottom edge.
import { describe, expect, it } from 'vitest'
import { drawOrder, structureRows } from '../../src/screens/maps/editor/drawStructures'
import type { Scene } from '../../src/screens/maps/editor/drawMap'
import type { MapFile, Structure } from '../../src/model/types'

const kinds: Structure[] = [
  { id: 'castle', name: 'Castle', width: 2, height: 2 },
  { id: 'city', name: 'City' },
  { id: 'gate', name: 'Gate', width: 2, height: 1 },
]
const scene = (m: Partial<MapFile>): Scene => ({
  map: { name: 't', grid: { width: 10, height: 10 }, legend: {}, tiles: [], ...m } as MapFile,
  terrain: new Map(), types: new Map(), structures: new Map(kinds.map((k) => [k.id, k])), flagged: new Set(),
})
const label = (s: Scene) => drawOrder(s).map((i) => `${i.kind === 0 ? 's' : 'u'}${i.index}`).join(' ')

describe('drawing order', () => {
  it('sorts by bottom edge; structures before units on a tie', () => {
    const s = scene({ structures: [{ type: 'city', x: 5, y: 3 }, { type: 'city', x: 1, y: 1 }], units: [{ type: 'u', side: 'a', x: 8, y: 3 }, { type: 'u', side: 'a', x: 8, y: 0 }] })
    // keys: s0 = 4, s1 = 2, u0 = 4, u1 = 1
    expect(label(s)).toBe('u1 s1 s0 u0')
  })
  it('a unit on the upper row of a 2 x 2 structure is drawn over it (sorts with its bottom edge)', () => {
    const s = scene({ structures: [{ type: 'castle', x: 2, y: 2 }], units: [{ type: 'u', side: 'a', x: 2, y: 2 }, { type: 'u', side: 'a', x: 3, y: 3 }] })
    expect(label(s)).toBe('s0 u0 u1')
  })
  it('a unit beside a tall structure on the same row is drawn after one in front but a tall structure covers a unit behind', () => {
    const s = scene({ structures: [{ type: 'castle', x: 2, y: 2 }], units: [{ type: 'u', side: 'a', x: 2, y: 1 }, { type: 'u', side: 'a', x: 6, y: 5 }] })
    expect(label(s)).toBe('u0 s0 u1') // u0 stands behind (key 2), the castle's bottom edge is 4, u1 in front (key 6)
  })
  it('units keep file order on a tie; structures keep list order', () => {
    const s = scene({ structures: [{ type: 'city', x: 0, y: 0 }, { type: 'city', x: 5, y: 0 }], units: [{ type: 'u', side: 'a', x: 3, y: 0 }, { type: 'u', side: 'a', x: 4, y: 0 }] })
    expect(label(s)).toBe('s0 s1 u0 u1')
  })
})

describe('rows a structure occupies', () => {
  it('without a picture exactly its footprint', () => {
    expect(structureRows(scene({}), { type: 'castle', x: 2, y: 3 })).toEqual([3, 4])
    expect(structureRows(scene({}), { type: 'gate', x: 0, y: 7 })).toEqual([7, 7])
  })
})
