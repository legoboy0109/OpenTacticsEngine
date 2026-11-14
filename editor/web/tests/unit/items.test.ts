import { describe, expect, it } from 'vitest'
import { buildBody } from '../../src/store/autosave'
import { itemChecks } from '../../src/model/itemChecks'
import { patchEntry, setEntryItem } from '../../src/model/itemGrants'
import { itemUsageLines } from '../../src/model/itemRefs'
import { setInventorySize, setStorageMode, setWeaponsMode } from '../../src/model/itemRules'
import {
  addItem, deleteItem, duplicateItem, newItemId, setHeal, setItemKind, setPrice, setStatNumber, setTarget, setUnlimited, updateItem,
} from '../../src/model/items'
import { mapWarnings } from '../../src/model/mapChecks'
import { problemTarget } from '../../src/model/problems'
import { cleanWeapon, findUsages } from '../../src/model/references'
import { removeResource } from '../../src/model/rulesLists'
import { addStat, removeStat } from '../../src/model/stats'
import { setSells } from '../../src/model/structures'
import { setUnitItems } from '../../src/model/units'
import type { Item, ProjectDoc } from '../../src/model/types'
import { editMap } from '../../src/model/mapCtx'
import { loadDemo } from './fixtures'

// The demo has only the health and movement stats; a third one can be deleted again.
const base = (): ProjectDoc => addStat(loadDemo('demo-economy'), 'Guard', 'guard')
const stat = (_d: ProjectDoc) => 'guard'
const res = (d: ProjectDoc) => d.game!.resources![0].id
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const weapon = (d: ProjectDoc) => d.weapons![0].id
const unitType = (d: ProjectDoc) => d.units![0].id
const carried = (d: ProjectDoc) => setWeaponsMode(d, 'carried')

/** A game with one item of each kind, a shop, starting items, a placed unit's own items and a side's storage. */
function rich(): ProjectDoc {
  let d = carried(base())
  d = addItem(d, 'Potion', 'potion')
  d = setPrice(d, 'potion', res(d), 100)
  d = addItem(d, 'Ring', 'ring')
  d = setItemKind(d, 'ring', 'accessory')
  d = setStatNumber(d, 'ring', 'bonus', stat(d), 2)
  d = addItem(d, 'Sword', 'sword')
  d = setItemKind(d, 'sword', 'weapon')
  d = updateItem(d, 'sword', { weapon: weapon(d), uses: 40 })
  d = setUnitItems(d, unitType(d), ['potion', { item: 'sword', uses: 5, drop: true }])
  const shop = d.structures![0].id
  d = setSells(d, shop, ['potion'])
  d = editMap(d, mapId(d), (m) => ({
    ...m,
    units: m.units!.map((u, i) => (i === 0 ? { ...u, items: ['ring'] } : u)),
    sides: m.sides!.map((s, i) => (i === 0 ? { ...s, storage: ['potion'] } : s)),
  }))
  d = setStorageMode(d, 'anywhere')
  return d
}
const checksOf = (d: ProjectDoc) => itemChecks(d).map((c) => c.text)

describe('the list', () => {
  it('the first item creates the list from "none" and is a valid healing item', () => {
    const d = addItem(base(), 'Healing Herb')
    expect(d.items).toEqual([{ id: 'healing_herb', name: 'Healing Herb', kind: 'usable', uses: 3, heal: 10 }])
    expect(newItemId(d, 'Healing Herb')).toBe('healing_herb_2')
    expect(itemChecks(d)).toEqual([])
  })
  it('duplicates beside the original with its own id; the id never changes with the name', () => {
    let d = updateItem(addItem(base(), 'Potion'), 'potion', { name: 'Elixir' })
    expect(d.items![0]).toMatchObject({ id: 'potion', name: 'Elixir' })
    d = duplicateItem(d, 'potion')
    expect(d.items!.map((i) => i.id)).toEqual(['potion', 'elixir_copy'])
  })
  it('a missing list is saved as an empty one after an undo back to "none"', () => {
    const saved = addItem(base(), 'Potion')
    expect(buildBody(base(), saved, 'r', false)!.items).toEqual([])
  })
})

describe('changing the kind drops the old kind’s fields', () => {
  it('usable -> accessory removes heal, raise, target and uses', () => {
    let d = addItem(base(), 'Potion', 'potion')
    d = setTarget(d, 'potion', 'ally')
    d = setStatNumber(d, 'potion', 'raise', stat(d), 1)
    d = setItemKind(d, 'potion', 'accessory')
    const it = d.items![0]
    expect(it.kind).toBe('accessory')
    for (const k of ['heal', 'raise', 'target', 'uses', 'weapon']) expect(it).not.toHaveProperty(k)
  })
  it('accessory -> weapon keeps nothing of the bonus and picks the first weapon; weapon -> usable drops weapon', () => {
    let d = setItemKind(addItem(carried(base()), 'X', 'x'), 'x', 'accessory')
    d = setStatNumber(d, 'x', 'bonus', stat(d), 3)
    d = setItemKind(d, 'x', 'weapon')
    expect(d.items![0]).toMatchObject({ kind: 'weapon', weapon: weapon(d), uses: 40 })
    expect(d.items![0]).not.toHaveProperty('bonus')
    d = setItemKind(d, 'x', 'usable')
    expect(d.items![0]).not.toHaveProperty('weapon')
    expect(d.items![0]).toMatchObject({ kind: 'usable', heal: 10 })
  })
  it('"itself" is not written; unlimited removes uses and brings back a default', () => {
    let d = addItem(base(), 'P', 'p')
    d = setTarget(setTarget(d, 'p', 'ally'), 'p', 'self')
    expect(d.items![0]).not.toHaveProperty('target')
    d = setUnlimited(d, 'p', true)
    expect(d.items![0]).not.toHaveProperty('uses')
    expect(setUnlimited(d, 'p', false).items![0].uses).toBe(3)
    expect(setHeal(d, 'p', 'full').items![0].heal).toBe('full')
  })
  it('an emptied price leaves no key (not sold)', () => {
    let d = setPrice(addItem(base(), 'P', 'p'), 'p', res(base()), 50)
    expect(d.items![0].price).toEqual({ [res(d)]: 50 })
    d = setPrice(d, 'p', res(d), undefined)
    expect(d.items![0]).not.toHaveProperty('price')
  })
})

describe('entries in a list', () => {
  it('an entry with nothing to say is a plain id; extra keys survive', () => {
    expect(patchEntry('a', { uses: 2 })).toEqual({ item: 'a', uses: 2 })
    expect(patchEntry({ item: 'a', uses: 2 }, { uses: undefined })).toBe('a')
    expect(patchEntry({ item: 'a', drop: true, note: 1 }, { drop: false })).toEqual({ item: 'a', note: 1 })
    expect(setEntryItem([{ item: 'a', uses: 2 }], 0, 'b')).toEqual(['b'])
  })
})

describe('rules: defaults are never written', () => {
  it('weapons fixed, size 5 and storage off leave no rules.items', () => {
    let d = setStorageMode(setInventorySize(carried(base()), 8), 'leader')
    expect(d.game!.rules!.items).toEqual({ weapons: 'carried', inventorySize: 8, storage: 'leader' })
    d = setStorageMode(setInventorySize(setWeaponsMode(d, 'fixed'), 5), 'off')
    expect(d.game!.rules).not.toHaveProperty('items')
  })
  it('the size stays within 1 to 20', () => {
    expect(setInventorySize(base(), 99).game!.rules!.items!.inventorySize).toBe(20)
    expect(setInventorySize(base(), 0).game!.rules!.items!.inventorySize).toBe(1)
  })
})

describe('deleting', () => {
  it('lists every place that holds the item, and removes them all in one step', () => {
    const d = rich()
    const lines = itemUsageLines(d, 'potion').map((u) => u.text).join('\n')
    expect(lines).toMatch(/starts with it/)
    expect(lines).toMatch(/sells it/)
    expect(lines).toMatch(/storage/)
    const gone = deleteItem(d, 'potion')
    expect(gone.items!.map((i) => i.id)).toEqual(['ring', 'sword'])
    expect(gone.units![0].items).toEqual([{ item: 'sword', uses: 5, drop: true }])
    expect(gone.structures![0]).not.toHaveProperty('sells')
    expect(gone.maps[mapId(d)]!.sides![0]).not.toHaveProperty('storage')
    expect(checksOf(gone)).toEqual([])
  })
  it('a placed unit left with no items keeps an empty own list', () => {
    const gone = deleteItem(rich(), 'ring')
    expect(gone.maps[mapId(gone)]!.units![0].items).toEqual([])
  })
  it('deleting the weapon of a weapon item deletes the item and its references', () => {
    const d = rich()
    expect(findUsages(d, 'weapon', weapon(d)).some((u) => u.section === 'items')).toBe(true)
    const gone = cleanWeapon(d, weapon(d))
    expect(gone.items!.map((i) => i.id)).toEqual(['potion', 'ring'])
    expect(gone.units![0].items).toEqual(['potion'])
  })
  it('deleting a stat or a resource takes it out of raise, bonus and price', () => {
    const d = rich()
    expect(d.items!.find((i) => i.id === 'ring')!.bonus).toBeTruthy()
    const noStat = removeStat(d, stat(d))
    expect(noStat.items!.find((i) => i.id === 'ring')).not.toHaveProperty('bonus')
    const noRes = removeResource(d, res(d))
    expect(noRes.items!.find((i) => i.id === 'potion')).not.toHaveProperty('price')
  })
})

describe('checks mirror the loader', () => {
  it('a fully valid game has none', () => expect(checksOf(rich())).toEqual([]))
  const text = (d: ProjectDoc) => checksOf(d).join('\n')
  it('usable with no effect, accessory with no bonus', () => {
    let d = rich()
    d = setHeal(d, 'potion', undefined)
    d = setStatNumber(d, 'ring', 'bonus', stat(d), undefined)
    expect(text(d)).toMatch(/Potion: a usable item needs to heal or raise/)
    expect(text(d)).toMatch(/Ring: an accessory needs a bonus/)
  })
  it('weapon items while weapons are fixed, or naming a missing weapon', () => {
    expect(text(setWeaponsMode(rich(), 'fixed'))).toMatch(/Sword: weapon items need weapons/)
    expect(text(updateItem(rich(), 'sword', { weapon: 'nope' }))).toMatch(/“nope” does not exist/)
  })
  it('unknown item ids anywhere', () => {
    const d = setSells(setUnitItems(rich(), unitType(rich()), ['ghost']), rich().structures![0].id, ['ghost'])
    expect(text(d)).toMatch(/starts with: “ghost” is not an item/)
    expect(text(d)).toMatch(/sells “ghost”/)
  })
  it('sells an item with no price', () => {
    expect(text(setSells(rich(), rich().structures![0].id, ['ring']))).toMatch(/sells Ring, which has no price/)
  })
  it('too many starting items, uses above the item’s own, uses on an unlimited item', () => {
    let d = setInventorySize(rich(), 1)
    expect(text(d)).toMatch(/2 items, but a unit can carry only 1/)
    d = setUnitItems(rich(), unitType(d), [{ item: 'sword', uses: 41 }, { item: 'ring', uses: 1 }])
    expect(text(d)).toMatch(/Sword has only 40 uses, but 41 are left/)
    expect(text(d)).toMatch(/Ring has unlimited uses/)
  })
  it('starting storage while storage is off shows on that map’s warnings', () => {
    const d = setStorageMode(rich(), 'off')
    const m = d.maps[mapId(d)]!
    expect(text(d)).toMatch(/shared storage is Off/)
    expect(mapWarnings(d, m).some((w) => /shared storage is Off/.test(w.text))).toBe(true)
  })
  it('an unknown stat in a bonus', () => {
    const d = updateItem(rich(), 'ring', { bonus: { nope: 1 } } as Partial<Item>)
    expect(text(d)).toMatch(/“nope” is not a stat/)
  })
})

describe('problems from the loader land on the right screen', () => {
  it('data/items.json belongs to the Items section', () => expect(problemTarget('data/items.json', null)).toEqual({ section: 'items' }))
})
