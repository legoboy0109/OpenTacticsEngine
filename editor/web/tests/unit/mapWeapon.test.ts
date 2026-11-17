// Map weapons in the editor (docs/SRPG.md "Map weapons", docs/EDITOR_UI.md): the structure card's edits, the checks that mirror the loader, the placed
// structure's own uses, "goes for: weapons" and the delete follow-through. Uses the engine's fixture tests/fixtures/srpg-mapweapon.
import { describe, expect, it } from 'vitest'
import { GOES_FOR, setGoesFor } from '../../src/model/mapPlaces'
import {
  byTypeIds, mapWeaponChecks, placedWithWeapon, setPlacedUses, setWeapon, setWeaponBecomes, setWeaponBy, setWeaponIgnoreRank, setWeaponUses, starterWeapon, unitTags,
  weaponItems, weaponTwins,
} from '../../src/model/mapWeapon'
import { mapWeaponItemUsageLines, mapWeaponKindUsageLines, mapWeaponUnitUsageLines, reconcileMapWeapons } from '../../src/model/mapWeaponRefs'
import { deleteItem } from '../../src/model/items'
import { deleteStructure } from '../../src/model/structures'
import { mapWarnings } from '../../src/model/mapChecks'
import type { ProjectDoc } from '../../src/model/types'
import { parseLoose } from './fixtures'
import { readFileSync } from 'node:fs'
import path from 'node:path'

const fixture = (): ProjectDoc => {
  const root = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-mapweapon')
  const read = (rel: string) => parseLoose(readFileSync(path.join(root, rel), 'utf8'))
  const game = read('game.json')
  const maps: ProjectDoc['maps'] = {}
  for (const m of game.maps) maps[m.id] = read(m.file)
  return {
    game, terrain: read('data/terrain.json').terrain, structures: read('data/structures.json').structures, items: read('data/items.json').items, research: null,
    weapons: read('data/weapons.json').weapons, units: read('data/units.json').units, maps,
  } as unknown as ProjectDoc
}
const kind = (d: ProjectDoc, id: string) => d.structures!.find((s) => s.id === id)!
const texts = (d: ProjectDoc) => mapWeaponChecks(d).filter((c) => !c.note).map((c) => c.text)

describe('the fixture', () => {
  it('has nothing the editor would refuse', () => {
    expect(texts(fixture())).toEqual([])
  })
  it('lists the weapon items and tags', () => {
    const d = fixture()
    expect(weaponItems(d).map((i) => i.id)).toEqual(['sword', 'short_bow', 'ballista_bolt'])
    expect(starterWeapon(d)).toBe('sword')
    expect(unitTags(d)).toEqual(['archer'])
  })
  it('works out who may use a weapon: the types and every type with a tag', () => {
    const d = fixture()
    expect(byTypeIds(d, kind(d, 'ballista'))).toEqual(['archer'])
    expect(byTypeIds(d, kind(d, 'catapult'))).toBeNull()
  })
})

describe('editing the card', () => {
  it('switching the weapon on gives a weapon, off removes everything that belongs with it', () => {
    let d = fixture()
    d = setWeapon(d, 'tower', 'ballista_bolt')
    expect(kind(d, 'tower').weapon).toBe('ballista_bolt')
    d = setWeaponUses(d, 'tower', 5)
    d = setWeaponBy(d, 'tower', ['knight'], ['archer'])
    d = setWeaponIgnoreRank(d, 'tower', true)
    expect(kind(d, 'tower')).toMatchObject({ uses: 5, by: { types: ['knight'], tags: ['archer'] }, ignoreRank: true })
    expect(texts(d)).toEqual([])
    d = setWeapon(d, 'tower', undefined)
    expect(kind(d, 'tower')).not.toHaveProperty('weapon')
    for (const k of ['uses', 'by', 'ignoreRank', 'becomes']) expect(kind(d, 'tower')).not.toHaveProperty(k)
  })
  it('keeps uses in range and lets an empty box mean no limit', () => {
    let d = fixture()
    d = setWeaponUses(d, 'ballista', 0)
    expect(kind(d, 'ballista').uses).toBe(1)
    d = setWeaponUses(d, 'ballista', 123456)
    expect(kind(d, 'ballista').uses).toBe(9999)
    d = setWeaponUses(d, 'ballista', undefined)
    expect(kind(d, 'ballista')).not.toHaveProperty('uses')
  })
  it('empty who lists remove "by"', () => {
    let d = setWeaponBy(fixture(), 'ballista', [], [])
    expect(kind(d, 'ballista')).not.toHaveProperty('by')
    d = setWeaponBy(d, 'ballista', ['knight'], [])
    expect(kind(d, 'ballista').by).toEqual({ types: ['knight'] })
  })
  it('offers only kinds of the same size for "becomes"', () => {
    let d = fixture()
    expect(weaponTwins(d, kind(d, 'ballista')).map((s) => s.id).sort()).toEqual(['ballista_wreck', 'catapult', 'tower'])
    d = { ...d, structures: d.structures!.map((s) => (s.id === 'tower' ? { ...s, width: 2 } : s)) }
    expect(weaponTwins(d, kind(d, 'ballista')).map((s) => s.id)).not.toContain('tower')
    d = setWeaponBecomes(d, 'catapult', 'ballista_wreck')
    expect(kind(d, 'catapult').becomes).toBe('ballista_wreck')
    d = setWeaponBecomes(d, 'catapult', '')
    expect(kind(d, 'catapult')).not.toHaveProperty('becomes')
  })
})

describe('the checks mirror the loader', () => {
  const broken = (patch: Record<string, unknown>, id = 'ballista'): ProjectDoc => {
    const d = fixture()
    return { ...d, structures: d.structures!.map((s) => (s.id === id ? { ...s, ...patch } : s)) }
  }
  it('an unknown or non-weapon item', () => {
    expect(texts(broken({ weapon: 'nope' }))[0]).toMatch(/not an item of this game/)
    expect(texts(broken({ weapon: 'potion' }))[0]).toMatch(/not a weapon item/)
  })
  it('uses out of range, not a whole number', () => {
    for (const bad of [0, 10000, 2.5, 'many']) expect(texts(broken({ uses: bad }))[0]).toMatch(/whole number from 1 to 9999/)
  })
  it('keys without a weapon', () => {
    const d = broken({ uses: 3 }, 'tower')
    expect(texts(d)[0]).toMatch(/belongs with a weapon/)
  })
  it('a who list that is empty, names an unknown type or tag, or cannot stand there', () => {
    expect(texts(broken({ by: {} }))[0]).toMatch(/list of who may use it is empty/)
    expect(texts(broken({ by: { types: ['dragon'] } }))[0]).toMatch(/unit type “dragon”/)
    expect(texts(broken({ by: { tags: ['flying'] } }))[0]).toMatch(/tag “flying”/)
    expect(texts(broken({ moveCost: { foot: 0 } }))[0]).toMatch(/none of the unit types|no kind of unit/)
  })
  it('a kind nobody can stand on', () => {
    expect(texts(broken({ moveCost: { foot: 0 } }, 'catapult'))[0]).toMatch(/no kind of unit can stand on it/)
  })
  it('becomes: unknown, itself, a different size', () => {
    expect(texts(broken({ becomes: 'nowhere' }))[0]).toMatch(/is not a structure kind/)
    expect(texts(broken({ becomes: 'ballista' }))[0]).toMatch(/is the structure itself/)
    const d = fixture()
    const wide = { ...d, structures: d.structures!.map((s) => (s.id === 'ballista_wreck' ? { ...s, width: 2 } : s)) }
    expect(texts(wide)[0]).toMatch(/different size/)
  })
  it('a placed structure\'s own uses', () => {
    const d = fixture()
    const field = d.maps.field!
    const bad = (uses: unknown, type = 'ballista') => ({ ...d, maps: { ...d.maps, field: { ...field, structures: field.structures!.map((p) => (p.id === 'b2' ? { ...p, type, uses } : p)) } } }) as ProjectDoc
    expect(texts(bad(0))[0]).toMatch(/whole number from 1 to 9999/)
    expect(texts(bad(2, 'tower'))[0]).toMatch(/only belong to a structure whose kind has a weapon/)
    expect(texts(bad(2))).toEqual([])
  })
  it('the map\'s own list shows them with their square', () => {
    const d = fixture()
    const field = d.maps.field!
    const withBad = { ...d, maps: { ...d.maps, field: { ...field, structures: field.structures!.map((p) => (p.id === 'b2' ? { ...p, uses: 0 } : p)) } } } as ProjectDoc
    const list = mapWarnings(withBad, withBad.maps.field!)
    expect(list.some((c) => /whole number from 1 to 9999/.test(c.text) && c.cell?.x === 4 && c.cell?.y === 2)).toBe(true)
  })
})

describe('a placed structure', () => {
  it('sets and clears its own uses', () => {
    const d = fixture()
    const field = d.maps.field!
    let m = setPlacedUses(field, { x: 4, y: 1 }, 7)
    expect(m.structures!.find((p) => p.id === 'b1')!.uses).toBe(7)
    m = setPlacedUses(m, { x: 4, y: 1 }, undefined)
    expect(m.structures!.find((p) => p.id === 'b1')).not.toHaveProperty('uses')
    expect(placedWithWeapon(d, field).map((p) => p.id)).toEqual(['c1', 'b1', 'b2'])
  })
})

describe('goes for weapons', () => {
  it('is one of the goals and kept in a fixed order', () => {
    expect(GOES_FOR).toEqual(['chests', 'villages', 'doors', 'steal', 'walls', 'weapons'])
    const d = fixture()
    const siege = d.maps.siege!
    const m = setGoesFor(siege, { x: 8, y: 1 }, ['weapons', 'doors'])
    expect(m.units!.find((u) => u.id === 'g1')!.goesFor).toEqual(['doors', 'weapons'])
  })
})

describe('deleting what a weapon points at', () => {
  it('a weapon item: the kinds lose their weapon and the placed uses go', () => {
    const d = fixture()
    expect(mapWeaponItemUsageLines(d, 'ballista_bolt').map((u) => u.text).join('|')).toMatch(/Ballista.*Catapult/)
    const after = deleteItem(d, 'ballista_bolt')
    const clean = reconcileMapWeapons(d, after)
    for (const id of ['ballista', 'catapult']) expect(kind(clean, id)).not.toHaveProperty('weapon')
    expect(kind(clean, 'ballista')).not.toHaveProperty('by')
    expect(clean.maps.field!.structures!.find((p) => p.id === 'b2')).not.toHaveProperty('uses')
    expect(texts(clean)).toEqual([])
  })
  it('a structure kind it becomes', () => {
    const d = fixture()
    expect(mapWeaponKindUsageLines(d, 'ballista_wreck')).toHaveLength(1)
    const clean = reconcileMapWeapons(d, deleteStructure(d, 'ballista_wreck'))
    expect(kind(clean, 'ballista')).not.toHaveProperty('becomes')
    expect(kind(clean, 'ballista').weapon).toBe('ballista_bolt')
  })
  it('a unit type that was the only one allowed: the limit goes with it', () => {
    const d = setWeaponBy(fixture(), 'ballista', ['archer'], [])
    expect(mapWeaponUnitUsageLines(d, 'archer')[0].text).toMatch(/only it may use/)
    const after = { ...d, units: d.units!.filter((u) => u.id !== 'archer') }
    const clean = reconcileMapWeapons(d, after)
    expect(kind(clean, 'ballista')).not.toHaveProperty('by')
  })
  it('a unit type among several is just taken off the list', () => {
    let d = setWeaponBy(fixture(), 'ballista', ['archer', 'knight'], [])
    d = { ...d }
    const after = { ...d, units: d.units!.filter((u) => u.id !== 'knight') }
    expect(kind(reconcileMapWeapons(d, after), 'ballista').by).toEqual({ types: ['archer'] })
  })
  it('does nothing when nothing it points at was deleted (the same document comes back)', () => {
    const d = fixture()
    expect(reconcileMapWeapons(d, d)).toBe(d)
    const edited = { ...d, items: d.items!.slice() }
    expect(reconcileMapWeapons(d, edited)).toBe(edited)
  })
})
