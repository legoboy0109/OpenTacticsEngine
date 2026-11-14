import { describe, expect, it } from 'vitest'
import { mapWarnings } from '../../src/model/mapChecks'
import { ctxOf, editMap, type MapCtx } from '../../src/model/mapCtx'
import { paintCells } from '../../src/model/mapPaint'
import { resizeLoss, resizeMap } from '../../src/model/mapResize'
import { removeSide } from '../../src/model/mapSides'
import {
  changeStructureKind, fitProblem, indexByOrigin, misfits, moveStructureBy, placeStructure, removeStructureAt, setStructureOwner,
  structureAt, structureIndexAt,
} from '../../src/model/mapStructures'
import { placeUnit } from '../../src/model/mapUnits'
import { setSize, addStructure } from '../../src/model/structures'
import type { ProjectDoc, Structure } from '../../src/model/types'
import { loadDemo } from './fixtures'

// demo-economy "twin_rivers" (15 x 10): blue hq 2 x 2 at (0,4), factory (2,3), city (1,6); blue infantry on (2,4) and (2,5), tank (3,4).
const eco = () => loadDemo('demo-economy')
const ctx = (d: ProjectDoc): MapCtx => ctxOf(d)!
const WALL: Structure = { id: 'wall', name: 'Wall', moveCost: { foot: 0 } }
const withWall = (): ProjectDoc => { const d = eco(); return { ...d, structures: [...d.structures!, WALL] } }

describe('placing', () => {
  it('puts the top-left corner on the square; the owner is optional; terrain is untouched', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const r = placeStructure(m, ctx(d), { type: 'city', x: 5, y: 2 })
    expect(r.error).toBeNull()
    expect(r.map.structures!.at(-1)).toEqual({ type: 'city', x: 5, y: 2 })
    expect(r.map.tiles).toEqual(m.tiles)
    const owned = placeStructure(m, ctx(d), { type: 'city', x: 5, y: 2, side: 'red' })
    expect(owned.map.structures!.at(-1)).toEqual({ type: 'city', x: 5, y: 2, side: 'red' })
    expect(placeStructure(m, ctx(d), { type: 'city', x: 5, y: 2, side: '' }).map.structures!.at(-1)).toEqual({ type: 'city', x: 5, y: 2 })
  })
  it('refuses a footprint that sticks out of the map, naming the size, and leaves the map as it was', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const r = placeStructure(m, ctx(d), { type: 'hq', x: 14, y: 9 })
    expect(r.error).toMatch(/2 × 2/)
    expect(r.map).toBe(m)
    expect(placeStructure(m, ctx(d), { type: 'hq', x: 13, y: 8 }).error).toBeNull() // the last square that fits
    expect(placeStructure(m, ctx(d), { type: 'hq', x: 14, y: 8 }).error).not.toBeNull()
    expect(placeStructure(m, ctx(d), { type: 'city', x: -1, y: 0 }).error).toMatch(/outside/)
    expect(placeStructure(m, ctx(d), { type: 'city', x: 0, y: 10 }).error).toMatch(/outside/)
  })
  it('refuses overlap, also with a square that is not the other one\'s corner', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(placeStructure(m, ctx(d), { type: 'city', x: 1, y: 5 }).error).toMatch(/overlap the Headquarters/) // (1,5) is the hq's lower right square
    expect(placeStructure(m, ctx(d), { type: 'hq', x: 6, y: 4 }).error).toMatch(/overlap the City/) // covers the city at (7,5)
    expect(placeStructure(m, ctx(d), { type: 'hq', x: 6, y: 2 }).error).toBeNull()
  })
  it('refuses an unknown kind or owner', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(placeStructure(m, ctx(d), { type: 'ghost', x: 5, y: 2 }).error).toBeTruthy()
    expect(placeStructure(m, ctx(d), { type: 'city', x: 5, y: 2, side: 'ghost' }).error).toBeTruthy()
  })
  it('refuses a structure that would strand a unit, and a unit on a square the structure makes impassable', () => {
    const d = withWall(); const m = d.maps.twin_rivers!
    const r = placeStructure(m, ctx(d), { type: 'wall', x: 2, y: 4 }) // infantry stands there
    expect(r.error).toMatch(/Infantry.*Wall/)
    expect(placeStructure(m, ctx(d), { type: 'wall', x: 6, y: 2 }).error).toBeNull()
    const placed = placeStructure(m, ctx(d), { type: 'wall', x: 6, y: 2 }).map
    expect(placeUnit(placed, ctx(d), { type: 'infantry', side: 'blue', x: 6, y: 2 }).error).toMatch(/cannot stand on a Wall/)
    // a unit standing on a passable structure is fine
    expect(placeUnit(m, ctx(d), { type: 'infantry', side: 'blue', x: 7, y: 5 }).error).toBeNull() // on the city
  })
})

describe('finding', () => {
  it('structureAt covers every square of the footprint; indexByOrigin only the corner', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const c = ctx(d)
    for (const [x, y] of [[0, 4], [1, 4], [0, 5], [1, 5]]) expect(structureAt(m, c, x, y)).toMatchObject({ type: 'hq', x: 0, y: 4 })
    expect(structureAt(m, c, 2, 4)).toBeUndefined()
    expect(structureIndexAt(m, c, 1, 5)).toBe(indexByOrigin(m, 0, 4))
    expect(indexByOrigin(m, 1, 5)).toBe(-1)
  })
})

describe('moving, owning, changing, removing', () => {
  it('moves by an offset; refuses what does not fit', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const ok = moveStructureBy(m, ctx(d), { x: 0, y: 4 }, 0, -2)
    expect(ok.error).toBeNull()
    expect(ok.map.structures![indexByOrigin(m, 0, 4)]).toMatchObject({ type: 'hq', x: 0, y: 2, side: 'blue' })
    expect(moveStructureBy(m, ctx(d), { x: 0, y: 4 }, 0, 0).map).toBe(m)
    expect(moveStructureBy(m, ctx(d), { x: 0, y: 4 }, -1, 0).error).toMatch(/2 × 2/)
    expect(moveStructureBy(m, ctx(d), { x: 0, y: 4 }, 1, -1).error).toMatch(/overlap the Factory/) // (1..2, 3..4) hits the factory at (2,3)
    expect(moveStructureBy(m, ctx(d), { x: 0, y: 4 }, 0, 1).error).toMatch(/overlap the City/) // (0..1, 5..6) hits the city at (1,6)
    expect(moveStructureBy(m, ctx(d), { x: 9, y: 9 }, 1, 0).map).toBe(m) // no structure there
  })
  it('a structure does not collide with itself when it moves one square', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(moveStructureBy(m, ctx(d), { x: 13, y: 4 }, -1, 0).error).toBeNull() // red hq, overlapping its own old squares
  })
  it('sets and clears the owner; same object when nothing changes; refuses unknown sides', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const out = setStructureOwner(m, { x: 7, y: 5 }, 'red')
    expect(out.structures![indexByOrigin(m, 7, 5)].side).toBe('red')
    const cleared = setStructureOwner(out, { x: 7, y: 5 }, null)
    expect(cleared.structures![indexByOrigin(m, 7, 5)]).not.toHaveProperty('side')
    expect(setStructureOwner(m, { x: 7, y: 5 }, null)).toBe(m)
    expect(setStructureOwner(m, { x: 0, y: 4 }, 'blue')).toBe(m)
    expect(setStructureOwner(m, { x: 7, y: 5 }, 'ghost')).toBe(m)
    expect(setStructureOwner(m, { x: 8, y: 8 }, 'red')).toBe(m)
  })
  it('changes the kind in place when the new footprint fits', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(changeStructureKind(m, ctx(d), { x: 13, y: 1 }, 'hq').map.structures![indexByOrigin(m, 13, 1)].type).toBe('hq')
    const edge = placeStructure(m, ctx(d), { type: 'city', x: 14, y: 9 }).map
    expect(changeStructureKind(edge, ctx(d), { x: 14, y: 9 }, 'hq').error).toMatch(/2 × 2/)
    expect(changeStructureKind(m, ctx(d), { x: 7, y: 5 }, 'city').map).toBe(m)
  })
  it('removes by any square of the footprint', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const out = removeStructureAt(m, ctx(d), 1, 5)
    expect(out.structures).toHaveLength(m.structures!.length - 1)
    expect(indexByOrigin(out, 0, 4)).toBe(-1)
    expect(removeStructureAt(m, ctx(d), 8, 8)).toBe(m)
  })
  it('unknown keys on a placed structure survive an owner change', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const i = indexByOrigin(m, 7, 5)
    const tagged = { ...m, structures: m.structures!.map((s, j) => (j === i ? { ...s, __keep: 1 } : s)) }
    expect((setStructureOwner(tagged, { x: 7, y: 5 }, 'red').structures![i] as Record<string, unknown>).__keep).toBe(1)
  })
})

describe('when the kind changes under placed structures', () => {
  it('growing a kind makes misfits that the checks point at; shrinking fixes them', () => {
    let d = eco()
    expect(misfits(d.maps.twin_rivers!, ctx(d))).toEqual([])
    d = setSize(d, 'city', 3, 3) // the cities are crowded together and near the edge
    const bad = misfits(d.maps.twin_rivers!, ctx(d))
    expect(bad.length).toBeGreaterThan(0)
    const w = mapWarnings(d, d.maps.twin_rivers!).filter((x) => x.key.startsWith('misfit:'))
    expect(w).toHaveLength(bad.length)
    expect(w[0].text).toMatch(/does not fit/)
    d = setSize(d, 'city', 1, 1)
    expect(misfits(d.maps.twin_rivers!, ctx(d))).toEqual([])
  })
  it('fitProblem ignores the structure being moved', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const i = indexByOrigin(m, 0, 4)
    expect(fitProblem(m, ctx(d), 'hq', 0, 4)).toMatch(/overlap/)
    expect(fitProblem(m, ctx(d), 'hq', 0, 4, i)).toBeNull()
  })
})

describe('map checks with structures', () => {
  it('judge a unit with the structure\'s costs: a wall under a unit is flagged, a pier on water is fine', () => {
    let d = withWall()
    const u = d.maps.twin_rivers!.units![0] // blue infantry on (2,4)
    // the wall cannot be placed over it, but a structure may already be there in a hand-edited file:
    d = { ...d, maps: { ...d.maps, twin_rivers: { ...d.maps.twin_rivers!, structures: [...d.maps.twin_rivers!.structures!, { type: 'wall', x: u.x, y: u.y }] } } }
    expect(mapWarnings(d, d.maps.twin_rivers!).some((x) => x.key === `blocked:${u.x},${u.y}` && /Wall/.test(x.text))).toBe(true)
    // a unit on water with a pier (cost for its movement type) is legal
    const pier: Structure = { id: 'pier', name: 'Pier', moveCost: { foot: 1 } }
    let e: ProjectDoc = { ...eco(), structures: [...eco().structures!, pier] }
    e = editMap(e, 'twin_rivers', (m, c) => paintCells(m, c.terrain, [{ x: 6, y: 2 }], 'sea'))
    e = editMap(e, 'twin_rivers', (m) => ({ ...m, units: [...m.units!, { type: 'infantry', side: 'blue', x: 6, y: 2 }] }))
    const flagged = () => mapWarnings(e, e.maps.twin_rivers!).some((x) => x.key === 'blocked:6,2')
    expect(flagged()).toBe(true)
    e = editMap(e, 'twin_rivers', (m) => ({ ...m, structures: [...m.structures!, { type: 'pier', x: 6, y: 2 }] }))
    expect(flagged()).toBe(false)
  })
  it('painting terrain under a structure leaves the structure', () => {
    const d = eco()
    const out = editMap(d, 'twin_rivers', (m, c) => paintCells(m, c.terrain, [{ x: 7, y: 5 }], 'forest'))
    expect(out.maps.twin_rivers!.structures).toEqual(d.maps.twin_rivers!.structures)
  })
})

describe('resize', () => {
  it('lists structures that would no longer fit completely and removes exactly them; the others move with the map', () => {
    const d = eco(); const m = d.maps.twin_rivers! // 15 x 10, red hq (13,4) 2 x 2
    const types = d.structures!
    const loss = resizeLoss(m, types, 14, 10, 'nw') // column 14 goes: the red hq reaches it
    expect(loss.structures.map((s) => `${s.type}@${s.x},${s.y}`)).toContain('hq@13,4')
    const out = resizeMap(m, d.terrain!, types, 14, 10, 'nw')
    expect(out.structures).toHaveLength(m.structures!.length - loss.structures.length)
    expect(out.structures!.some((s) => s.type === 'hq' && s.x === 13)).toBe(false)
    const grown = resizeMap(m, d.terrain!, types, 17, 12, 'se') // offset (2, 2)
    expect(grown.structures![indexByOrigin(m, 0, 4)]).toMatchObject({ type: 'hq', x: 2, y: 6, side: 'blue' })
    expect(resizeLoss(m, types, 17, 12, 'se').structures).toEqual([])
  })
  it('a structure cut in half counts as lost', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(resizeLoss(m, d.structures!, 15, 5, 'nw').structures.some((s) => s.type === 'hq' && s.y === 4)).toBe(true) // rows 4..5, only row 4 stays
  })
})

describe('sides', () => {
  it('removing a side makes its structures nobody\'s on that map and keeps unknown keys', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    const three = { ...m, sides: [...m.sides!, { id: 'green', name: 'Green' }], structures: [...m.structures!, { type: 'city', x: 5, y: 2, side: 'green', __keep: 1 }] }
    const out = removeSide(three, 'green')
    expect(out.structures!.at(-1)).toEqual({ type: 'city', x: 5, y: 2, __keep: 1 })
  })
})

describe('new kinds', () => {
  it('a new kind can be placed right away', () => {
    let d = eco()
    d = addStructure(d, 'Tower')
    const m = d.maps.twin_rivers!
    expect(placeStructure(m, ctx(d), { type: 'tower', x: 5, y: 2 }).error).toBeNull()
  })
})
