import { describe, expect, it } from 'vitest'
import { mapWarnings } from '../../src/model/mapChecks'
import { ctxOf, editMap, type MapCtx } from '../../src/model/mapCtx'
import { terrainAt } from '../../src/model/mapGrid'
import { ownerAt, ownershipMatters, setOwners } from '../../src/model/mapOwners'
import { paintCells } from '../../src/model/mapPaint'
import { ANCHORS, resizeLoss, resizeMap, resizeOffset } from '../../src/model/mapResize'
import { addSide, canRemoveSide, removeSide, setSideResource, sideRemovalLoss, updateSide } from '../../src/model/mapSides'
import { changeUnitType, moveMapUnit, placeUnit, removeMapUnit, setUnitStat, unitAt, updateMapUnit } from '../../src/model/mapUnits'
import { setDefeatCondition, setFeature } from '../../src/model/rules'
import { setHeadquarters, setMoveCost } from '../../src/model/terrain'
import { removeStat } from '../../src/model/stats'
import { deleteUnit } from '../../src/model/units'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const srpg = () => loadDemo('demo-srpg')
const eco = () => loadDemo('demo-economy')
const ctx = (d: ProjectDoc): MapCtx => ctxOf(d)!
const clone = <T,>(v: T): T => JSON.parse(JSON.stringify(v))

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

describe('sides', () => {
  it('adds a computer side with a fresh name, colour and id', () => {
    const m = addSide(srpg().maps.bridge!)
    expect(m.sides).toHaveLength(3)
    expect(m.sides![2]).toMatchObject({ id: 'green', name: 'Green', controller: 'ai' })
    expect(new Set(m.sides!.map((s) => s.color)).size).toBe(3)
    expect(addSide(m).sides![3].id).toBe('yellow')
  })
  it('removing a side removes its units and owners; the minimum is two', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(canRemoveSide(m)).toBe(false)
    expect(removeSide(m, 'red')).toBe(m)
    const three = addSide(m)
    expect(sideRemovalLoss(three, 'red')).toEqual({ units: 3, owners: 3 })
    const out = removeSide(three, 'red')
    expect(out.sides!.map((s) => s.id)).toEqual(['blue', 'green'])
    expect(out.units!.every((u) => u.side !== 'red')).toBe(true)
    expect(out.owners!.every((o) => o.side !== 'red')).toBe(true)
  })
  it('edits name (id stays), colour, controller, team and resources', () => {
    const d = eco(); let m = d.maps.twin_rivers!
    m = updateSide(m, 'blue', { name: 'Azure', color: '#112233', controller: 'ai', team: 'allies' })
    expect(m.sides![0]).toMatchObject({ id: 'blue', name: 'Azure', color: '#112233', controller: 'ai', team: 'allies' })
    m = updateSide(m, 'blue', { team: '' })
    expect('team' in m.sides![0]).toBe(false)
    m = setSideResource(m, ctx(d), 'blue', 'funds', 5000)
    expect(m.sides![0].resources).toEqual({ funds: 5000 })
    expect(setSideResource(m, ctx(d), 'blue', 'gems', 1)).toBe(m) // not a resource of this game
    m = setSideResource(m, ctx(d), 'blue', 'funds', undefined)
    expect('resources' in m.sides![0]).toBe(false)
  })
})

describe('warnings', () => {
  it('flags a unit on impassable terrain without deleting it, and clears when fixed', () => {
    let d = srpg()
    const u = d.maps.bridge!.units![0]
    d = editMap(d, 'bridge', (m, c) => paintCells(m, c.terrain, [{ x: u.x, y: u.y }], 'water'))
    expect(unitAt(d.maps.bridge!, u.x, u.y)).toBeDefined()
    const w = mapWarnings(d, d.maps.bridge!)
    expect(w.find((x) => x.key === `blocked:${u.x},${u.y}`)?.cell).toEqual({ x: u.x, y: u.y })
    d = editMap(d, 'bridge', (m, c) => paintCells(m, c.terrain, [{ x: u.x, y: u.y }], 'plain'))
    expect(mapWarnings(d, d.maps.bridge!).some((x) => x.key.startsWith('blocked'))).toBe(false)
  })
  it('a side without units (and without a producing building) is flagged', () => {
    const d = srpg()
    const m = addSide(d.maps.bridge!)
    const w = mapWarnings({ ...d, maps: { bridge: m } }, m)
    expect(w.map((x) => x.key)).toContain('no-units:green')
  })
  it('with Building units on, an owned producing building is a way to get units', () => {
    let d = eco()
    d = setFeature(d, 'production', true)
    let m = d.maps.twin_rivers!
    m = { ...m, units: m.units!.filter((u) => u.side !== 'red') } // red keeps its factory
    expect(mapWarnings({ ...d, maps: { twin_rivers: m } }, m).some((x) => x.key === 'no-units:red')).toBe(false)
    m = { ...m, owners: m.owners!.filter((o) => o.side !== 'red') }
    expect(mapWarnings({ ...d, maps: { twin_rivers: m } }, m).some((x) => x.key === 'no-units:red')).toBe(true)
  })
  it('leader and headquarters rules', () => {
    let d = eco()
    expect(mapWarnings(d, d.maps.twin_rivers!).map((x) => x.key.split(':')[0])).not.toContain('no-leader')
    d = setDefeatCondition(setDefeatCondition(d, 'leaderDead', true), 'lostHq', true)
    const keys = mapWarnings(d, d.maps.twin_rivers!).map((x) => x.key)
    expect(keys).toEqual(expect.arrayContaining(['no-leader:blue', 'no-leader:red']))
    expect(keys.some((k) => k.startsWith('no-hq'))).toBe(false) // both own an HQ ("H" is tagged hq)
    d = setHeadquarters(d, 'hq', false)
    expect(mapWarnings(d, d.maps.twin_rivers!).map((x) => x.key)).toEqual(expect.arrayContaining(['no-hq:blue', 'no-hq:red']))
    const m = d.maps.twin_rivers!
    const lead = { ...m, units: m.units!.map((u, i) => (i === 0 || i === 3 ? { ...u, leader: true } : u)) }
    expect(mapWarnings(d, lead).some((x) => x.key.startsWith('no-leader'))).toBe(false)
  })
  it('fewer than two sides', () => {
    const d = srpg(); const m = { ...d.maps.bridge!, sides: [d.maps.bridge!.sides![0]] }
    expect(mapWarnings(d, m).map((x) => x.key)).toContain('few-sides')
  })
})

describe('reference clean-up with the grid model', () => {
  it('deleting a unit type removes it from maps; deleting a stat removes overrides; both leave valid maps', () => {
    const d = srpg()
    const next = deleteUnit(d, 'archer')
    expect(next.maps.bridge!.units!.some((u) => u.type === 'archer')).toBe(false)
    const hp = removeStat(clone(d), 'str')
    expect(hp.maps.bridge!.units!.every((u) => !u.stats || !('str' in u.stats))).toBe(true)
  })
})
