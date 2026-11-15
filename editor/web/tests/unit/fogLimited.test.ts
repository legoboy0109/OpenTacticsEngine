// Extra sight for some unit types only (docs/ECONOMY.md "Fog of war"): the model (plain number or { amount, units }), the checks
// that mirror the loader, the "what a side sees" aid, and the clean-up when a unit type is deleted (one undo step).
import { describe, expect, it, vi } from 'vitest'
import type { Api, PutBody, PutResult, Result } from '../../src/api/client'
import { bonusAmount, bonusFor, bonusUnits, setVision, setVisionBonus, setVisionUnits } from '../../src/model/fog'
import { fogChecks } from '../../src/model/fogChecks'
import { seenAtStart } from '../../src/model/fogSight'
import { editMap } from '../../src/model/mapCtx'
import { findUsages } from '../../src/model/references'
import { reconcileEconomy } from '../../src/model/researchRefs'
import { deleteUnit, setUnitTags } from '../../src/model/units'
import { unitKeyOptions } from '../../src/model/unitKeys'
import { buildBody } from '../../src/store/autosave'
import { DocumentStore } from '../../src/store/documentStore'
import type { ProjectDoc, ServerProject } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => loadDemo('demo-economy')
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const mountain = (d: ProjectDoc) => d.terrain!.find((t) => t.id === 'mountain')!

describe('the model', () => {
  it('the demo mountain is for infantry and mech only', () => {
    const m = mountain(base())
    expect(bonusAmount(m)).toBe(2)
    expect(bonusUnits(m)).toEqual(['infantry', 'mech'])
  })
  it('a plain number means every unit; an object means the listed unit types and labels', () => {
    const d = base()
    const tank = d.units!.find((u) => u.id === 'tank')!
    const infantry = d.units!.find((u) => u.id === 'infantry')!
    expect(bonusFor({ visionBonus: 3 }, tank)).toBe(3)
    expect(bonusUnits({ visionBonus: 3 })).toBeUndefined()
    expect(bonusFor({ visionBonus: { amount: 3, units: ['infantry'] } }, tank)).toBe(0)
    expect(bonusFor({ visionBonus: { amount: 3, units: ['infantry'] } }, infantry)).toBe(3)
    expect(bonusFor({ visionBonus: { amount: 3, units: ['heavy'] } }, { ...tank, tags: ['heavy'] })).toBe(3) // a label
    expect(bonusFor({}, tank)).toBe(0)
  })
  it('limiting writes the object, going back to “all” writes the plain number again', () => {
    let d = setVisionBonus(base(), 'terrain', 'plain', 2)
    d = setVisionUnits(d, 'terrain', 'plain', ['mech'])
    expect(d.terrain!.find((t) => t.id === 'plain')!.visionBonus).toEqual({ amount: 2, units: ['mech'] })
    expect(setVisionUnits(d, 'terrain', 'plain', undefined).terrain!.find((t) => t.id === 'plain')!.visionBonus).toBe(2)
    // a limited bonus of 0 with no unit ticked yet goes back to no key at all
    const zero = setVisionBonus(base(), 'terrain', 'plain', 0)
    expect('visionBonus' in setVisionUnits(zero, 'terrain', 'plain', undefined).terrain!.find((t) => t.id === 'plain')!).toBe(false)
  })
  it('changing the amount keeps the list; a plain 0 removes the key, a limited 0 keeps the list', () => {
    let d = setVisionUnits(setVisionBonus(base(), 'structure', 'city', 1), 'structure', 'city', ['infantry'])
    d = setVisionBonus(d, 'structure', 'city', 4)
    expect(d.structures!.find((s) => s.id === 'city')!.visionBonus).toEqual({ amount: 4, units: ['infantry'] })
    expect(setVisionBonus(d, 'structure', 'city', 99).structures!.find((s) => s.id === 'city')!.visionBonus).toEqual({ amount: 10, units: ['infantry'] })
    expect(setVisionBonus(d, 'structure', 'city', 0).structures!.find((s) => s.id === 'city')!.visionBonus).toEqual({ amount: 0, units: ['infantry'] })
    const plain = setVisionBonus(setVisionBonus(base(), 'structure', 'city', 3), 'structure', 'city', 0)
    expect('visionBonus' in plain.structures!.find((s) => s.id === 'city')!).toBe(false)
  })
  it('the choices are the unit types, then the labels, then entries the game no longer has', () => {
    const d = setUnitTags(base(), 'tank', ['heavy'])
    const options = unitKeyOptions(d, ['ghost', 'tank'], (t) => `Label ${t}`, (e) => `${e} gone`)
    expect(options.map((o) => o.value)).toEqual([...d.units!.map((u) => u.id), 'heavy', 'ghost'])
    expect(options.find((o) => o.value === 'heavy')!.label).toBe('Label heavy')
    expect(options.find((o) => o.value === 'ghost')!.label).toBe('ghost gone')
  })
  it('the object is saved as written (buildBody)', () => {
    const d = setVisionUnits(setVisionBonus(base(), 'terrain', 'plain', 2), 'terrain', 'plain', ['mech'])
    expect(JSON.stringify(buildBody(d, base(), 'r1', false))).toContain('"visionBonus":{"amount":2,"units":["mech"]}')
  })
})

describe('checks mirror the loader', () => {
  const withBonus = (visionBonus: unknown) => ({ ...base(), terrain: base().terrain!.map((t, i) => (i === 0 ? { ...t, visionBonus: visionBonus as never } : t)) })
  const texts = (d: ProjectDoc) => fogChecks(d).map((c) => c.text)
  it('the demo and good limited bonuses have none', () => {
    expect(fogChecks(base())).toEqual([])
    expect(fogChecks(withBonus({ amount: 3, units: ['infantry', 'tank'] }))).toEqual([])
    expect(fogChecks(withBonus({ amount: 0, units: ['infantry'] }))).toEqual([])
  })
  it('names an amount outside 0..10, an empty list, an unknown entry, a missing part', () => {
    expect(texts(withBonus({ amount: 11, units: ['infantry'] })).join()).toMatch(/extra sight must be a whole number from 0 to 10/)
    expect(texts(withBonus({ amount: 1.5, units: ['infantry'] })).join()).toMatch(/whole number/)
    expect(texts(withBonus({ amount: 2, units: [] })).join()).toMatch(/none is ticked/)
    expect(texts(withBonus({ amount: 2 })).join()).toMatch(/none is ticked/)
    expect(texts(withBonus({ units: ['infantry'] })).join()).toMatch(/whole number from 0 to 10/)
    expect(texts(withBonus({ amount: 2, units: ['ghost'] })).join()).toMatch(/“ghost”, which is neither a unit type nor a label/)
    expect(fogChecks(withBonus({ amount: 2, units: ['ghost'] }))[0].section).toBe('terrain')
  })
  it('a label some unit type has is fine', () => {
    const d = setUnitTags(base(), 'tank', ['heavy'])
    expect(fogChecks({ ...d, terrain: d.terrain!.map((t, i) => (i === 0 ? { ...t, visionBonus: { amount: 1, units: ['heavy'] } } : t)) })).toEqual([])
  })
})

describe('what a side sees at the start', () => {
  /** A 9 x 3 plain map: blue has one unit of `type` on (4,1) (a mountain-like square), red one far away. */
  function scene(type: string, limit?: string[]): ProjectDoc {
    let d = base()
    const m = mapId(d)
    d = { ...d, units: d.units!.map((u) => ({ ...u, vision: 1 })), structures: [] }
    d = setVisionBonus(d, 'terrain', 'mountain', 2)
    if (limit) d = setVisionUnits(d, 'terrain', 'mountain', limit)
    return editMap(d, m, (map) => ({
      ...map, grid: { ...map.grid, width: 9, height: 3 }, legend: { p: 'plain', m: 'mountain' }, tiles: ['ppppppppp', 'ppppmpppp', 'ppppppppp'], heights: undefined,
      sides: [{ id: 'blue', name: 'Blue' }, { id: 'red', name: 'Red' }],
      units: [{ type, side: 'blue', x: 4, y: 1 }, { type: 'tank', side: 'red', x: 8, y: 2 }],
      structures: [], owners: undefined,
    }))
  }
  const count = (d: ProjectDoc) => seenAtStart(d, d.maps[mapId(d)]!, 'blue').reduce((n, v) => n + v, 0)
  it('a bonus for infantry only applies to an infantry and not to a mech', () => {
    expect(count(scene('infantry', ['infantry']))).toBe(count(scene('infantry'))) // same as the plain form
    expect(count(scene('infantry', ['infantry']))).toBeGreaterThan(count(scene('mech', ['infantry'])))
    expect(count(scene('mech', ['infantry']))).toBe(count(scene('mech', ['tank']))) // nothing extra either way
    expect(count(scene('mech', ['mech']))).toBe(count(scene('infantry'))) // each gets it when named
  })
  it('a label applies to the unit types that carry it', () => {
    const d = setUnitTags(scene('mech', ['soft']), 'mech', ['soft'])
    expect(count(d)).toBe(count(scene('mech')))
    expect(count(scene('mech', ['soft']))).toBeLessThan(count(d))
  })
  it('the limit works for structures too', () => {
    const d = setVision(scene('infantry'), 'infantry', 1)
    const withTower = { ...d, structures: [{ ...base().structures![0], visionBonus: { amount: 3, units: ['mech'] } as never }] }
    const placed = editMap(withTower, mapId(d), (map) => ({ ...map, structures: [{ type: withTower.structures![0].id, x: 4, y: 1 }] }))
    const noTower = count(placed)
    const forMech = editMap({ ...placed, structures: [{ ...placed.structures![0], visionBonus: { amount: 3, units: ['infantry'] } as never }] }, mapId(d), (m) => m)
    expect(count(forMech)).toBeGreaterThan(noTower)
  })
})

describe('clean-up when a unit type is deleted', () => {
  const doc = () => {
    let d = setVisionUnits(setVisionBonus(base(), 'structure', 'city', 2), 'structure', 'city', ['mech', 'tank'])
    d = setVisionUnits(setVisionBonus(d, 'terrain', 'plain', 1), 'terrain', 'plain', ['tank'])
    return d
  }
  it('the entry is taken off the list; a list that loses all its entries takes the bonus with it (never “all units”)', () => {
    const d = doc()
    const after = reconcileEconomy(d, deleteUnit(d, 'tank'))
    expect(after.structures!.find((s) => s.id === 'city')!.visionBonus).toEqual({ amount: 2, units: ['mech'] })
    expect('visionBonus' in after.terrain!.find((t) => t.id === 'plain')!).toBe(false)
    // the mountain still lists infantry and mech
    expect(bonusUnits(mountain(after))).toEqual(['infantry', 'mech'])
  })
  it('an entry that is also a label of another unit type stays', () => {
    const d = setUnitTags(doc(), 'recon', ['tank'])
    const after = reconcileEconomy(d, deleteUnit(d, 'tank'))
    expect(bonusUnits(after.terrain!.find((t) => t.id === 'plain')!)).toEqual(['tank'])
  })
  it('the delete confirmation names where it is used', () => {
    const lines = findUsages(doc(), 'unit', 'tank').map((u) => u.text).join('\n')
    expect(lines).toMatch(/Terrain “Plain” gives extra sight to it only \(the extra sight is removed\)/)
    expect(lines).toMatch(/Structure “City” gives it extra sight \(it is taken off that list\)/)
  })
  it('nothing else changes: the same document comes back when no list names the unit', () => {
    const d = base()
    const after = deleteUnit(d, 'artillery')
    expect(reconcileEconomy(d, after).terrain).toEqual(after.terrain)
  })

  type PutFn = (body: PutBody) => Promise<Result<PutResult>>
  const fake = (project: ServerProject, put: PutFn) => ({
    getProject: vi.fn(async () => ({ ok: true, value: project }) as Result<ServerProject>),
    putProject: vi.fn(async (_id: string, body: PutBody) => put(body)),
    listArt: vi.fn(async () => ({ ok: true, value: [] })),
    listModels: vi.fn(async () => ({ ok: true, value: [] })),
    listAudio: vi.fn(async () => ({ ok: true, value: [] })),
  }) as unknown as Api
  it('is one undo step through the store', async () => {
    const d = doc()
    const store = new DocumentStore(fake({ ...d, id: 'g', revision: 'r0', problems: [] }, async () => ({ ok: true, value: { revision: 'r1', problems: [] } })), 'g')
    await store.load()
    store.edit((x) => deleteUnit(x, 'tank'))
    const after = store.getState().doc
    expect(after.structures!.find((s) => s.id === 'city')!.visionBonus).toEqual({ amount: 2, units: ['mech'] })
    expect('visionBonus' in after.terrain!.find((t) => t.id === 'plain')!).toBe(false)
    store.undo()
    expect(store.getState().doc).toEqual({ ...d })
  })
})
