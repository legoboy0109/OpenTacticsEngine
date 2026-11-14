import { describe, expect, it } from 'vitest'
import { mapWarnings } from '../../src/model/mapChecks'
import { ctxOf, editMap, type MapCtx } from '../../src/model/mapCtx'
import { paintCells } from '../../src/model/mapPaint'
import { addSide, canRemoveSide, removeSide, setSideResource, sideRemovalLoss, updateSide } from '../../src/model/mapSides'
import { setDefeatCondition, setFeature } from '../../src/model/rules'
import { setHeadquarters } from '../../src/model/structures'
import { removeStat } from '../../src/model/stats'
import { deleteUnit } from '../../src/model/units'
import { unitAt } from '../../src/model/mapUnits'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const srpg = () => loadDemo('demo-srpg')
const eco = () => loadDemo('demo-economy')
const ctx = (d: ProjectDoc): MapCtx => ctxOf(d)!
const clone = <T,>(v: T): T => JSON.parse(JSON.stringify(v))

describe('sides', () => {
  it('adds a computer side with a fresh name, colour and id', () => {
    const m = addSide(srpg().maps.bridge!)
    expect(m.sides).toHaveLength(3)
    expect(m.sides![2]).toMatchObject({ id: 'green', name: 'Green', controller: 'ai' })
    expect(new Set(m.sides!.map((s) => s.color)).size).toBe(3)
    expect(addSide(m).sides![3].id).toBe('yellow')
  })
  it('removing a side removes its units and makes its structures neutral; the minimum is two', () => {
    const d = eco(); const m = d.maps.twin_rivers!
    expect(canRemoveSide(m)).toBe(false)
    expect(removeSide(m, 'red')).toBe(m)
    const three = addSide(m)
    expect(sideRemovalLoss(three, 'red')).toEqual({ units: 3, structures: 3 })
    const out = removeSide(three, 'red')
    expect(out.sides!.map((s) => s.id)).toEqual(['blue', 'green'])
    expect(out.units!.every((u) => u.side !== 'red')).toBe(true)
    expect(out.structures).toHaveLength(m.structures!.length) // none are deleted
    expect(out.structures!.every((o) => o.side !== 'red')).toBe(true)
    expect(out.structures!.filter((o) => o.side === undefined)).toHaveLength(m.structures!.filter((o) => o.side === undefined || o.side === 'red').length)
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
    m = { ...m, structures: m.structures!.map((o) => (o.side === 'red' ? { type: o.type, x: o.x, y: o.y } : o)) }
    expect(mapWarnings({ ...d, maps: { twin_rivers: m } }, m).some((x) => x.key === 'no-units:red')).toBe(true)
  })
  it('leader and headquarters rules', () => {
    let d = eco()
    expect(mapWarnings(d, d.maps.twin_rivers!).map((x) => x.key.split(':')[0])).not.toContain('no-leader')
    d = setDefeatCondition(setDefeatCondition(d, 'leaderDead', true), 'lostHq', true)
    const keys = mapWarnings(d, d.maps.twin_rivers!).map((x) => x.key)
    expect(keys).toEqual(expect.arrayContaining(['no-leader:blue', 'no-leader:red']))
    expect(keys.some((k) => k.startsWith('no-hq'))).toBe(false) // both own an HQ structure (tagged hq)
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
