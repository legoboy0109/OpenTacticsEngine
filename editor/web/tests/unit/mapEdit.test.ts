import { describe, expect, it } from 'vitest'
import { ctxOf, type MapCtx } from '../../src/model/mapCtx'
import { terrainAt } from '../../src/model/mapGrid'
import { ownerAt, ownershipMatters, setOwners } from '../../src/model/mapOwners'
import { ANCHORS, resizeLoss, resizeMap, resizeOffset } from '../../src/model/mapResize'
import { changeUnitType, moveMapUnit, placeUnit, removeMapUnit, setUnitStat, unitAt, updateMapUnit } from '../../src/model/mapUnits'
import { setMoveCost } from '../../src/model/terrain'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const srpg = () => loadDemo('demo-srpg')
const eco = () => loadDemo('demo-economy')
const ctx = (d: ProjectDoc): MapCtx => ctxOf(d)!

describe('placing, moving and removing units', () => {
  it('places on a free passable square', () => {
    const d = srpg(); const m = d.maps.bridge!
    const r = placeUnit(m, ctx(d), { type: 'archer', side: 'red', x: 0, y: 0 })
    expect(r.error).toBeNull()
    expect(r.map.units!.at(-1)).toEqual({ type: 'archer', side: 'red', x: 0, y: 0 })
  })
  it('an occupied square selects the existing unit instead of replacing it', () => {
    const d = srpg(); const m = d.maps.bridge!
    const first = m.units![0]
    const r = placeUnit(m, ctx(d), { type: 'archer', side: 'red', x: first.x, y: first.y })
    expect(r.map).toBe(m)
    expect(r.existing).toEqual({ x: first.x, y: first.y })
  })
  it('refuses impassable terrain with an explanation, and out of bounds', () => {
    const d = srpg(); const m = d.maps.bridge!
    const water = m.tiles.map((row, y) => [row.indexOf('~'), y]).find(([x]) => x >= 0)!
    const r = placeUnit(m, ctx(d), { type: 'lord', side: 'blue', x: water[0], y: water[1] })
    expect(r.map).toBe(m)
    expect(r.error).toMatch(/cannot stand on River/)
    expect(placeUnit(m, ctx(d), { type: 'lord', side: 'blue', x: 99, y: 0 }).error).toMatch(/outside/)
  })
  it('moves a unit and refuses occupied / impassable targets', () => {
    const d = srpg(); const m = d.maps.bridge!
    const u = m.units![0]
    expect(moveMapUnit(m, ctx(d), u, { x: 0, y: 0 }).map.units![0]).toMatchObject({ x: 0, y: 0, name: u.name })
    const other = m.units![1]
    expect(moveMapUnit(m, ctx(d), u, other).error).toMatch(/already a unit/)
    const [wx, wy] = [m.tiles[0].indexOf('~'), 0]
    expect(moveMapUnit(m, ctx(d), u, { x: wx, y: wy }).error).toMatch(/cannot stand/)
  })
  it('removes a unit', () => {
    const m = srpg().maps.bridge!
    const u = m.units![2]
    const out = removeMapUnit(m, u.x, u.y)
    expect(out.units).toHaveLength(m.units!.length - 1)
    expect(unitAt(out, u.x, u.y)).toBeUndefined()
    expect(removeMapUnit(m, 0, 0)).toBe(m)
  })
  it('changing the type is refused where the new type cannot stand', () => {
    const d = srpg(); const m = d.maps.bridge!
    // Make "foot" units unable to cross forest, then try to turn a horse unit into a foot unit on forest.
    const d2 = setMoveCost(d, 'forest', 'foot', 0)
    const horse = d2.units!.find((x) => x.moveType === 'horse')!
    const foot = d2.units!.find((x) => x.moveType === 'foot')!
    const spot = { x: m.tiles[0].indexOf('f'), y: 0 }
    const placed = placeUnit(d2.maps.bridge!, ctx(d2), { type: horse.id, side: 'blue', ...spot })
    expect(placed.error).toBeNull()
    expect(changeUnitType(placed.map, ctx(d2), spot.x, spot.y, foot.id).error).toMatch(/cannot stand/)
  })
})

describe('unit properties', () => {
  it('stat overrides: only values that differ from the type are stored; clearing removes the key', () => {
    const d = srpg(); let m = d.maps.bridge!
    const u = m.units![2] // archer, no overrides
    const base = d.units!.find((x) => x.id === u.type)!.stats!
    expect(u.stats).toBeUndefined()
    m = setUnitStat(m, ctx(d), u.x, u.y, 'hp', base.hp + 5)
    expect(unitAt(m, u.x, u.y)!.stats).toEqual({ hp: base.hp + 5 })
    m = setUnitStat(m, ctx(d), u.x, u.y, 'str', base.str) // same as the base: not stored
    expect(unitAt(m, u.x, u.y)!.stats).toEqual({ hp: base.hp + 5 })
    m = setUnitStat(m, ctx(d), u.x, u.y, 'hp', undefined)
    expect('stats' in unitAt(m, u.x, u.y)!).toBe(false)
    m = setUnitStat(m, ctx(d), u.x, u.y, 'hp', base.hp) // typing the base value back also clears
    expect('stats' in unitAt(m, u.x, u.y)!).toBe(false)
  })
  it('name, level and leader are stored only when meaningful', () => {
    const d = srpg(); let m = d.maps.bridge!
    const u = m.units![0] // Eirik: name, level 3, leader
    m = updateMapUnit(m, u.x, u.y, { name: '  ', level: undefined, leader: false })
    const got = unitAt(m, u.x, u.y)!
    expect(['name', 'level', 'leader'].some((k) => k in got)).toBe(false)
    m = updateMapUnit(m, u.x, u.y, { name: 'Zed', level: 4, leader: true })
    expect(unitAt(m, u.x, u.y)).toMatchObject({ name: 'Zed', level: 4, leader: true })
  })
  it('changing the type drops overrides that equal the new base', () => {
    const d = srpg(); let m = d.maps.bridge!
    const u = m.units![0]
    const fighter = d.units!.find((x) => x.id === 'fighter')!
    m = setUnitStat(m, ctx(d), u.x, u.y, 'str', fighter.stats!.str)
    m = changeUnitType(m, ctx(d), u.x, u.y, 'fighter').map
    expect(unitAt(m, u.x, u.y)!.type).toBe('fighter')
    expect(unitAt(m, u.x, u.y)!.stats).not.toHaveProperty('str')
  })
  it('unknown keys on a unit survive', () => {
    const d = srpg(); const m0 = d.maps.bridge!
    const m1 = { ...m0, units: m0.units!.map((u, i) => (i === 0 ? { ...u, __keep: 1 } : u)) }
    const m2 = updateMapUnit(m1, m0.units![0].x, m0.units![0].y, { name: 'N' })
    expect((m2.units![0] as Record<string, unknown>).__keep).toBe(1)
  })
})

describe('owners', () => {
  it('sets, replaces and clears', () => {
    const d = eco(); let m = d.maps.twin_rivers!
    const before = m.owners!.length
    m = setOwners(m, [{ x: 7, y: 5 }], 'red')
    expect(ownerAt(m, 7, 5)).toBe('red')
    m = setOwners(m, [{ x: 7, y: 5 }], 'blue')
    expect(ownerAt(m, 7, 5)).toBe('blue')
    expect(m.owners).toHaveLength(before + 1)
    m = setOwners(m, [{ x: 7, y: 5 }], null)
    expect(ownerAt(m, 7, 5)).toBeUndefined()
    expect(m.owners).toHaveLength(before)
  })
  it('refuses unknown sides, ignores outside squares, same object when unchanged', () => {
    const m = eco().maps.twin_rivers!
    expect(setOwners(m, [{ x: 0, y: 0 }], 'ghost')).toBe(m)
    expect(setOwners(m, [{ x: 99, y: 0 }], 'red')).toBe(m)
    expect(setOwners(m, [{ x: 1, y: 4 }], 'blue')).toBe(m)
    expect(setOwners(m, [{ x: 5, y: 5 }], null)).toBe(m)
  })
  it('knows which terrain ownership matters on', () => {
    const t = eco().terrain!
    expect(ownershipMatters(t.find((x) => x.id === 'city'))).toBe(true)
    expect(ownershipMatters(t.find((x) => x.id === 'factory'))).toBe(true)
    expect(ownershipMatters(t.find((x) => x.id === 'forest'))).toBe(false)
  })
})

describe('resize', () => {
  const map = (): { d: ProjectDoc; m: MapFile } => { const d = srpg(); return { d, m: d.maps.bridge! } } // 13 x 9
  it('offset per anchor', () => {
    expect(resizeOffset(10, 10, 14, 12, 'nw')).toEqual({ dx: 0, dy: 0 })
    expect(resizeOffset(10, 10, 14, 12, 'se')).toEqual({ dx: 4, dy: 2 })
    expect(resizeOffset(10, 10, 14, 12, 'c')).toEqual({ dx: 2, dy: 1 })
    expect(resizeOffset(10, 10, 6, 6, 'ne')).toEqual({ dx: -4, dy: 0 })
    expect(resizeOffset(10, 10, 6, 6, 's')).toEqual({ dx: -2, dy: -4 })
  })
  for (const a of ANCHORS) {
    it(`growing with anchor ${a} keeps terrain and units together and pads with the first terrain`, () => {
      const { d, m } = map()
      const out = resizeMap(m, d.terrain!, 17, 12, a)
      const { dx, dy } = resizeOffset(13, 9, 17, 12, a)
      expect(out.grid).toMatchObject({ width: 17, height: 12 })
      expect(out.tiles).toHaveLength(12)
      for (const u of m.units!) {
        const moved = unitAt(out, u.x + dx, u.y + dy)!
        expect(moved.name).toBe(u.name)
        expect(terrainAt(out, u.x + dx, u.y + dy)).toBe(terrainAt(m, u.x, u.y))
      }
      expect(out.units).toHaveLength(m.units!.length)
      expect(terrainAt(out, dx === 0 ? 16 : 0, dy === 0 ? 11 : 0)).toBe(d.terrain![0].id)
    })
    it(`shrinking with anchor ${a} reports and removes exactly what is cropped`, () => {
      const { d, m } = map()
      const loss = resizeLoss(m, 6, 5, a)
      const out = resizeMap(m, d.terrain!, 6, 5, a)
      const { dx, dy } = resizeOffset(13, 9, 6, 5, a)
      const survivors = m.units!.filter((u) => u.x + dx >= 0 && u.x + dx < 6 && u.y + dy >= 0 && u.y + dy < 5)
      expect(out.units).toHaveLength(survivors.length)
      expect(loss.units).toHaveLength(m.units!.length - survivors.length)
      expect(out.units!.every((u) => u.x < 6 && u.y < 5)).toBe(true)
      expect(out.tiles.every((r) => r.length === 6)).toBe(true)
    })
  }
  it('crops owners, resizes heights, refuses out-of-range sizes', () => {
    const d = eco(); const m0 = d.maps.twin_rivers!
    const m = { ...m0, heights: m0.tiles.map((r) => [...r].map(() => 1)) }
    const loss = resizeLoss(m, 5, 5, 'nw')
    expect(loss.owners.length).toBeGreaterThan(0)
    const out = resizeMap(m, d.terrain!, 20, 12, 'nw')
    expect(out.heights).toHaveLength(12)
    expect(out.heights![0]).toHaveLength(20)
    expect(out.heights![0][0]).toBe(1)
    expect(out.heights![11][19]).toBe(0)
    expect(resizeMap(m, d.terrain!, 0, 5, 'nw')).toBe(m)
    expect(resizeMap(m, d.terrain!, 257, 5, 'nw')).toBe(m)
    expect(resizeMap(m, d.terrain!, 1, 1, 'c').tiles).toHaveLength(1)
  })
})
