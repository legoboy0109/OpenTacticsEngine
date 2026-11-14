import { describe, expect, it } from 'vitest'
import { gridProblems } from '../../src/model/mapGrid'
import { convertOldProject, convertPlan, dropEmptyOwners, isLegacyTerrain } from '../../src/model/structuresConvert'
import { loadDemo } from './fixtures'
import { oldProject } from './oldProject'

describe('converting an old project', () => {
  it('finds the old settings: capture / capturePoints, income, produces and the hq tag', () => {
    const d = oldProject()
    expect(d.terrain!.filter(isLegacyTerrain).map((t) => t.id)).toEqual(['city', 'factory', 'hq'])
    expect(isLegacyTerrain({ id: 'x', name: 'x', capturePoints: 5 })).toBe(true)
    expect(isLegacyTerrain({ id: 'x', name: 'x', tags: ['hq'] })).toBe(true)
    expect(isLegacyTerrain({ id: 'x', name: 'x', tags: ['wet'] })).toBe(false)
    expect(isLegacyTerrain({ id: 'x', name: 'x', income: {} })).toBe(true) // the key itself is refused by the engine
  })

  it('plans what will happen, in numbers', () => {
    const plan = convertPlan(oldProject())!
    expect(plan.kinds.map((k) => [k.terrainId, k.structureId, k.squares])).toEqual([['city', 'city', 6], ['factory', 'factory', 2], ['hq', 'hq', 2]])
    expect(plan.kinds[0].settings).toEqual(['can be captured', 'earns income'])
    expect(plan.kinds[2].settings).toContain('headquarters')
    expect(plan).toMatchObject({ placed: 10, ownersKept: 6, ownersDropped: 1, mapsWithOwners: 1 })
  })

  it('makes structure kinds with the same name, colour, letter and the moved settings', () => {
    const out = convertOldProject(oldProject())
    expect(out.structures).toEqual([
      { id: 'city', name: 'City', color: '#b7b7b7', glyph: 'c', capturePoints: 20, income: { funds: 1000 } },
      { id: 'factory', name: 'Factory', color: '#9a9a9a', glyph: 'F', capturePoints: 20, income: { funds: 1000 }, produces: ['infantry', 'tank', 'artillery'] },
      { id: 'hq', name: 'Headquarters', color: '#d4af37', glyph: 'H', capturePoints: 20, income: { funds: 1000 }, tags: ['hq'] },
    ])
  })

  it('leaves the ground rules on the terrain and takes only the moved settings off', () => {
    const out = convertOldProject(oldProject())
    for (const t of out.terrain!) {
      for (const k of ['capture', 'capturePoints', 'income', 'produces']) expect(t).not.toHaveProperty(k)
      expect(t.tags?.includes('hq') ?? false).toBe(false)
    }
    expect(out.terrain!.find((t) => t.id === 'city')).toEqual({ id: 'city', name: 'City', color: '#b7b7b7', glyph: 'c', props: { defense: 3 } })
    expect(out.terrain!.find((t) => t.id === 'forest')).toEqual(oldProject().terrain!.find((t) => t.id === 'forest'))
    expect('tags' in out.terrain!.find((t) => t.id === 'hq')!).toBe(false)
  })

  it('keeps other terrain tags', () => {
    const d = oldProject()
    d.terrain = d.terrain!.map((t) => (t.id === 'hq' ? { ...t, tags: ['hq', 'wet'] } : t))
    expect(convertOldProject(d).terrain!.find((t) => t.id === 'hq')!.tags).toEqual(['wet'])
  })

  it('puts one structure on every square of those terrains, owned as the old owners list said, and drops the list', () => {
    const out = convertOldProject(oldProject())
    const m = out.maps.crossroads!
    expect(m).not.toHaveProperty('owners')
    expect(m.structures).toHaveLength(10)
    expect(m.structures).toContainEqual({ type: 'hq', x: 1, y: 2, side: 'blue' })
    expect(m.structures).toContainEqual({ type: 'factory', x: 9, y: 3, side: 'red' })
    expect(m.structures).toContainEqual({ type: 'city', x: 5, y: 0 }) // nobody's
    expect(m.structures!.filter((s) => s.side).length).toBe(6)
    // the terrain under them is untouched and the map is still consistent
    expect(m.tiles).toEqual(oldProject().maps.crossroads!.tiles)
    expect(gridProblems(m, out.terrain!)).toEqual([])
    expect(m.units).toEqual(oldProject().maps.crossroads!.units)
  })

  it('copies a style\'s colour for the terrain to the structure and keeps the terrain colour', () => {
    const out = convertOldProject(oldProject())
    const night = out.game!.styles!.find((s) => s.id === 'night')!
    expect(night.structureColors).toEqual({ city: '#6a6a78', hq: '#8a7a30' })
    expect(night.terrainColors).toEqual({ plain: '#2f4f3a', city: '#6a6a78', hq: '#8a7a30' })
    expect(out.game!.styles!.find((s) => s.id === 'classic')).not.toHaveProperty('structureColors')
  })

  it('copies a still or animated picture to the structure (also per style, including "no picture") and not what a structure cannot show', () => {
    const g = convertOldProject(oldProject()).graphics!
    expect(g.structures).toEqual({ city: { sheet: 'tiles', cell: [3, 1] }, hq: { sheet: 'tiles', frames: [[0, 2], [1, 2]], frameMs: 300 } })
    expect(g.styles!.night.structures).toEqual({ city: { sheet: 'tiles', cell: [4, 1] }, hq: null })
    expect(g.terrain!.city).toEqual({ sheet: 'tiles', cell: [3, 1] }) // the terrain keeps its own
    expect(g.terrain!.road).toEqual(oldProject().graphics!.terrain!.road)
    expect(g.structures).not.toHaveProperty('factory')
  })

  it('is one edit and is finished: nothing left to convert, and doing it again changes nothing', () => {
    const out = convertOldProject(oldProject())
    expect(convertPlan(out)).toBeNull()
    expect(convertOldProject(out)).toBe(out)
  })

  it('does not touch a project that is already current', () => {
    for (const name of ['demo-srpg', 'demo-economy'] as const) {
      const d = loadDemo(name)
      expect(convertPlan(d)).toBeNull()
      expect(convertOldProject(d)).toBe(d)
    }
  })

  it('picks a free id when the structure list already has the terrain\'s id', () => {
    const d = oldProject()
    d.structures = [{ id: 'city', name: 'Existing city' }]
    const out = convertOldProject(d)
    expect(out.structures!.map((s) => s.id)).toEqual(['city', 'city_2', 'factory', 'hq'])
    expect(out.maps.crossroads!.structures!.some((s) => s.type === 'city_2')).toBe(true)
  })

  it('skips squares an existing structure already covers', () => {
    const d = oldProject()
    d.structures = [{ id: 'keep', name: 'Keep', width: 2, height: 2 }]
    d.maps.crossroads = { ...d.maps.crossroads!, structures: [{ type: 'keep', x: 5, y: 0 }] } // covers (5,0) (6,0) (5,1) (6,1)
    const out = convertOldProject(d)
    expect(out.maps.crossroads!.structures!.filter((s) => s.type === 'city')).toHaveLength(4) // 6 minus the two under the keep
    expect(out.maps.crossroads!.structures![0]).toEqual({ type: 'keep', x: 5, y: 0 })
  })

  it('a non-empty owners list with no building terrain is replaced too, and counted as dropped', () => {
    const d = convertOldProject(oldProject())
    d.maps.crossroads = { ...d.maps.crossroads!, owners: [{ x: 0, y: 0, side: 'blue' }] }
    const plan = convertPlan(d)!
    expect(plan.kinds).toEqual([])
    expect(plan).toMatchObject({ placed: 0, ownersKept: 0, ownersDropped: 1, mapsWithOwners: 1 })
    expect(convertOldProject(d).maps.crossroads).not.toHaveProperty('owners')
  })

  it('ignores damaged (null) maps and a terrain list that did not load', () => {
    const d = oldProject()
    d.maps.broken = null
    expect(convertOldProject(d).maps.broken).toBeNull()
    expect(convertPlan({ ...d, terrain: null })).toBeNull()
  })

  it('an unknown owner side is not carried over', () => {
    const d = oldProject()
    d.maps.crossroads = { ...d.maps.crossroads!, owners: [{ x: 1, y: 2, side: 'ghost' }] }
    const out = convertOldProject(d)
    expect(out.maps.crossroads!.structures).toContainEqual({ type: 'hq', x: 1, y: 2 })
  })
})

describe('the empty owners list', () => {
  it('is left out of a map; a non-empty one is never dropped silently', () => {
    const m = oldProject().maps.crossroads!
    expect(dropEmptyOwners({ ...m, owners: [] })).not.toHaveProperty('owners')
    expect(dropEmptyOwners(m)).toBe(m)
    expect(dropEmptyOwners({ ...m, owners: undefined })).toEqual({ ...m, owners: undefined })
    const noOwners = { ...m }
    delete noOwners.owners
    expect(dropEmptyOwners(noOwners)).toBe(noOwners)
  })
})
