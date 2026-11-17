// Doors, chests, places and keys in the editor (docs/SRPG.md section 10, docs/EDITOR_UI.md "Doors, chests and places"): the parts of a structure kind, keys,
// unlock staves and abilities, the placed details, the Events pieces, the rules card, the checks and the delete follow-through. Uses the engine's fixture
// tests/fixtures/srpg-doors (which the real loader accepts).
import { describe, expect, it } from 'vitest'
import { abilityChecks } from '../../src/model/abilityChecks'
import { setEffectKind, setUnlock, unlockOf } from '../../src/model/abilities'
import { setKeyOn, setKeyOpens } from '../../src/model/itemKeys'
import { deleteItem } from '../../src/model/items'
import { changePlaceId, fitPlaceKeys, setContents, setGoesFor, setPlacedLocked, suggestPlaceId } from '../../src/model/mapPlaces'
import { placeChecks } from '../../src/model/placeChecks'
import { reconcilePlaces, placeItemUsageLines, placeKindUsageLines } from '../../src/model/placeRefs'
import { setPlaceActions, placeActions } from '../../src/model/placeRules'
import { placeProblem, placesForCommand, placesForEvent } from '../../src/model/placeStory'
import {
  fittingKeys, lockedByDefault, partBlock, placeKindOf, setBlocksSight, setDoorHp, setDoorStat, setLockedByDefault, setOpensKind, setPlaceFrom, setPlaceKeys, setPlacePart, setVisitBecomes,
  setVisitRepeat, setVisitRuin, setWays, twinKinds, waysOf, fromOf,
} from '../../src/model/places'
import { staffChecks } from '../../src/model/staffChecks'
import { setLightOn } from '../../src/model/staffEffects'
import { setUnlockStaffKinds, setUnlockStaffOn, unlockBlock, unlockStaffOf } from '../../src/model/staffUnlock'
import { newCommand, newTrigger } from '../../src/model/storyDefaults'
import { commandText, triggerText } from '../../src/model/storySummary'
import { deleteStructure, addStructure } from '../../src/model/structures'
import { structureUsageLines } from '../../src/model/structuresRefs'
import { itemUsageLines } from '../../src/model/itemRefs'
import { storyChecks } from '../../src/model/storyChecks'
import { triggerKind, commandKind } from '../../src/model/storyTypes'
import type { Command } from '../../src/model/storyTypes'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { parseLoose } from './fixtures'
import { readFileSync } from 'node:fs'
import path from 'node:path'

const fixture = (): ProjectDoc => {
  const root = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-doors')
  const read = (rel: string) => parseLoose(readFileSync(path.join(root, rel), 'utf8'))
  const game = read('game.json')
  const maps: ProjectDoc['maps'] = {}
  for (const m of game.maps) maps[m.id] = read(m.file)
  return {
    game, terrain: read('data/terrain.json').terrain, structures: read('data/structures.json').structures, items: read('data/items.json').items, research: null,
    weapons: read('data/weapons.json').weapons, units: read('data/units.json').units, abilities: read('data/abilities.json').abilities, maps,
  } as unknown as ProjectDoc
}
const kind = (d: ProjectDoc, id: string) => d.structures!.find((s) => s.id === id)!
const item = (d: ProjectDoc, id: string) => d.items!.find((i) => i.id === id)!
const blocking = (d: ProjectDoc) => placeChecks(d).filter((c) => !c.note).map((c) => c.text)

describe('the fixture', () => {
  it('has nothing the editor would refuse', () => {
    const d = fixture()
    expect(blocking(d)).toEqual([])
    expect(staffChecks(d).filter((c) => c.blocks)).toEqual([])
    expect(abilityChecks(d).filter((c) => /unlock|lockpick/.test(c.text))).toEqual([])
  })
  it('reads each kind as what it is', () => {
    const d = fixture()
    expect(placeKindOf(kind(d, 'gate'))).toBe('door')
    expect(placeKindOf(kind(d, 'chest'))).toBe('chest')
    expect(placeKindOf(kind(d, 'hamlet'))).toBe('visit')
    expect(placeKindOf(kind(d, 'gate_open'))).toBeNull()
    expect(waysOf(kind(d, 'gate'))).toEqual(['key', 'trait', 'break'])
    expect(waysOf(kind(d, 'strongbox'))).toEqual(['key'])
    expect(waysOf(kind(d, 'cell_door'))).toEqual(['key', 'trait'])
    expect(fromOf(kind(d, 'gate'))).toBe('next')
    expect(fromOf(kind(d, 'hamlet'))).toBe('on')
    expect(fromOf(kind(d, 'camp'))).toBe('either')
    expect(lockedByDefault(kind(d, 'latch_door'))).toBe(false)
    expect(lockedByDefault(kind(d, 'cell_door'))).toBe(true)
  })
})

describe('the three parts of a structure kind', () => {
  it('a door needs another kind of the same size to open into, and is greyed with the reason otherwise', () => {
    let d: ProjectDoc = { ...fixture(), structures: [{ id: 'gate', name: 'Gate', glyph: 'G', color: '#000' }] }
    expect(partBlock(d, kind(d, 'gate'), 'door')).toBe('twin')
    expect(partBlock(d, kind(d, 'gate'), 'chest')).toBe('twin')
    expect(partBlock(d, kind(d, 'gate'), 'visit')).toBe('')
    expect(setPlacePart(d, 'gate', 'door', true)).toBe(d)
    d = addStructure(d, 'Open gate', 'gate_open')
    expect(twinKinds(d, kind(d, 'gate')).map((s) => s.id)).toEqual(['gate_open'])
    d = setPlacePart(d, 'gate', 'door', true)
    expect(kind(d, 'gate').door).toEqual({ opens: 'gate_open' })
    // A shut door stops every movement type until the maker says otherwise (the loader refuses a door that stops nobody).
    expect(Object.values(kind(d, 'gate').moveCost!).every((c) => c === 0)).toBe(true)
    expect(placeChecks(d).filter((c) => c.section === 'structures' && !c.note)).toEqual([])
  })
  it('a kind is at most one of the three', () => {
    let d = fixture()
    expect(partBlock(d, kind(d, 'chest'), 'door')).toBe('other')
    expect(setPlacePart(d, 'chest', 'door', true)).toBe(d)
    d = setPlacePart(d, 'chest', 'chest', false)
    expect(kind(d, 'chest').chest).toBeUndefined()
    expect(partBlock(d, kind(d, 'chest'), 'door')).toBe('')
  })
  it('writes only what is not the default', () => {
    let d = fixture()
    d = setLockedByDefault(d, 'cell_door', false)
    expect(kind(d, 'cell_door').door!.locked).toBe(false)
    d = setLockedByDefault(d, 'cell_door', true)
    expect('locked' in kind(d, 'cell_door').door!).toBe(false)
    d = setPlaceFrom(d, 'cell_door', 'either')
    expect('from' in kind(d, 'cell_door').door!).toBe(false)
    d = setPlaceFrom(d, 'hamlet', 'on')
    expect('from' in kind(d, 'hamlet').visit!).toBe(false)
    d = setPlaceFrom(d, 'hamlet', 'next')
    expect(kind(d, 'hamlet').visit!.from).toBe('next')
  })
  it('ways: breaking is the hit points; a key list needs the key way; the default list is not written', () => {
    let d = fixture()
    d = setWays(d, 'cell_door', ['key', 'trait', 'break'])
    expect(kind(d, 'cell_door').door!.hp).toBe(20)
    expect('by' in kind(d, 'cell_door').door!).toBe(false)
    d = setDoorHp(d, 'cell_door', 7)
    d = setDoorStat(d, 'cell_door', 'def', 2)
    expect(kind(d, 'cell_door').door).toMatchObject({ hp: 7, stats: { def: 2 } })
    d = setWays(d, 'cell_door', ['trait'])
    expect(kind(d, 'cell_door').door).toMatchObject({ by: ['trait'] })
    expect(kind(d, 'cell_door').door!.hp).toBeUndefined()
    expect(kind(d, 'cell_door').door!.stats).toBeUndefined()
    expect(kind(d, 'cell_door').door!.keys).toBeUndefined() // "a key" is off, so the key list went with it
    d = setWays(d, 'cell_door', [])
    expect(kind(d, 'cell_door').door!.by).toEqual([])
    expect(waysOf(kind(d, 'cell_door'))).toEqual([])
    expect(blocking(d)).toEqual([])
    // A chest has no hit points to break.
    d = setWays(d, 'chest', ['key', 'trait', 'break'])
    expect(kind(d, 'chest').chest!.by).toBeUndefined()
    expect(waysOf(kind(d, 'chest'))).toEqual(['key', 'trait'])
  })
  it('keys: only items that fit are offered; an empty list is not written', () => {
    let d = fixture()
    expect(fittingKeys(d, kind(d, 'cell_door')).map((i) => i.id).sort()).toEqual(['brass_key', 'master_key'])
    expect(fittingKeys(d, kind(d, 'strongbox')).map((i) => i.id).sort()).toEqual(['master_key', 'strong_key']) // a master key opens any chest (the kind's own list narrows it further)
    d = setPlaceKeys(d, 'cell_door', [])
    expect('keys' in kind(d, 'cell_door').door!).toBe(false)
    d = setPlaceKeys(d, 'cell_door', ['brass_key'])
    expect(kind(d, 'cell_door').door!.keys).toEqual(['brass_key'])
  })
  it('a visit: becoming and repeating contradict each other, and a switched-off part leaves the kinds it named alone', () => {
    let d = fixture()
    expect(setVisitRepeat(d, 'hamlet', true)).toBe(d)
    d = setVisitBecomes(d, 'hamlet', '')
    d = setVisitRepeat(d, 'hamlet', true)
    expect(kind(d, 'hamlet').visit).toMatchObject({ repeat: true, ruin: 'hamlet_ruin' })
    expect(setVisitBecomes(d, 'hamlet', 'hamlet_shut')).not.toBe(d) // the select is greyed in the form; the checks catch a file that has both
    d = setVisitRuin(d, 'hamlet', '')
    expect('ruin' in kind(d, 'hamlet').visit!).toBe(false)
    d = setPlacePart(d, 'hamlet', 'visit', false)
    expect(kind(d, 'hamlet').visit).toBeUndefined()
    expect(d.structures!.some((s) => s.id === 'hamlet_ruin')).toBe(true)
  })
  it('blocks sight and opens into another kind', () => {
    let d = fixture()
    d = setBlocksSight(d, 'cell_door', false)
    expect('blocksSight' in kind(d, 'cell_door')).toBe(false)
    d = setOpensKind(d, 'gate', 'cell_door')
    expect(placeChecks(d).some((c) => /different size/.test(c.text))).toBe(true)
  })
})

describe('keys', () => {
  it('a new key opens any door and does not heal; switching it off leaves an ordinary item', () => {
    let d = fixture()
    d = { ...d, items: [...d.items!, { id: 'rusty', name: 'Rusty Key', kind: 'usable', heal: 10, uses: 2 }] }
    d = setKeyOn(d, 'rusty', true)
    expect(item(d, 'rusty')).toMatchObject({ opens: ['door'] })
    expect(item(d, 'rusty').heal).toBeUndefined()
    d = setKeyOpens(d, 'rusty', ['door', 'strongbox'])
    expect(item(d, 'rusty').opens).toEqual(['door', 'strongbox'])
    expect(setKeyOpens(d, 'rusty', [])).toBe(d) // an empty list would be refused by the loader
    d = setKeyOn(d, 'rusty', false)
    expect('opens' in item(d, 'rusty')).toBe(false)
  })
  it('a maker’s own numbers stay when it becomes a key', () => {
    let d = fixture()
    d = setKeyOn(d, 'lockpick', true) // heal 1: not the untouched default
    expect(item(d, 'lockpick').heal).toBe(1)
  })
  it('checks: a key for nothing placed, an unknown kind, a non-usable key', () => {
    let d = fixture()
    d = { ...d, items: [...d.items!, { id: 'odd', name: 'Odd Key', kind: 'usable', opens: ['hamlet'], uses: 1 }, { id: 'ghost', name: 'Ghost Key', kind: 'usable', opens: ['nothing'] }] }
    const texts = placeChecks(d).filter((c) => c.section === 'items').map((c) => c.text)
    expect(texts.some((t) => /Ghost Key: it opens “nothing”/.test(t))).toBe(true)
    expect(texts.some((t) => /Odd Key: it opens Hamlet, which is not a door or a chest/.test(t))).toBe(true)
    const none: ProjectDoc = { ...fixture(), maps: Object.fromEntries(Object.entries(fixture().maps).map(([k, m]) => [k, { ...m!, structures: [] }])) }
    expect(placeChecks(none).some((c) => c.note && /Brass Key: it is a key, but no door or chest it opens is placed/.test(c.text))).toBe(true)
  })
})

describe('unlock staves and abilities', () => {
  it('reads the fixture staves', () => {
    const d = fixture()
    expect(unlockStaffOf(item(d, 'unlock_staff'))).toEqual({ on: true, kinds: 'both', editable: true })
    expect(unlockStaffOf(item(d, 'wild_staff'))).toEqual({ on: true, kinds: 'door', editable: true })
  })
  it('switching it on replaces the other effects and drops target, area and conditions; off leaves a small heal', () => {
    let d = fixture()
    d = { ...d, items: [...d.items!, { id: 'wand', name: 'Wand', kind: 'staff', group: 'staff', uses: 3, target: 'foe', range: 3, area: 'all', if: [{ x: 1 }], effects: [{ heal: 5 }] }] }
    expect(unlockStaffOf(item(d, 'wand')).on).toBe(false)
    d = setUnlockStaffOn(d, 'wand', true)
    expect(item(d, 'wand')).toMatchObject({ effects: [{ unlock: true }], range: 3 })
    expect(['target', 'area', 'if'].some((k) => k in item(d, 'wand'))).toBe(false)
    expect(staffChecks(d).filter((c) => c.blocks)).toEqual([])
    d = setUnlockStaffKinds(d, 'wand', 'chest')
    expect(item(d, 'wand').effects).toEqual([{ unlock: ['chest'] }])
    d = setUnlockStaffKinds(d, 'wand', 'both')
    expect(item(d, 'wand').effects).toEqual([{ unlock: true }])
    d = setUnlockStaffOn(d, 'wand', false)
    expect(item(d, 'wand').effects).toEqual([{ heal: 10 }])
  })
  it('is refused while an effect the form cannot show would be lost, and does not mix with a light', () => {
    let d = fixture()
    d = { ...d, items: [...d.items!, { id: 'odd', name: 'Odd', kind: 'staff', group: 'staff', uses: 1, effects: [{ mystery: 1 }] }] }
    expect(unlockBlock(item(d, 'odd'))).toBe('unknown')
    expect(setUnlockStaffOn(d, 'odd', true)).toBe(d)
    // The light switch turns the unlock staff into a light staff (one effect each): never both.
    const lit = setLightOn(d, 'unlock_staff', true)
    expect(item(lit, 'unlock_staff').effects).toEqual([{ light: { radius: 2, turns: 1 } }])
  })
  it('checks an unlock staff: no target, area or conditions, one effect', () => {
    let d = fixture()
    d = { ...d, items: [...d.items!, { id: 'bad', name: 'Bad', kind: 'staff', group: 'staff', uses: 1, target: 'ally', effects: [{ unlock: true }, { heal: 1 }] }] }
    const t = staffChecks(d).filter((c) => c.blocks && c.ids.includes('bad')).map((c) => c.text).join(' ')
    expect(t).toMatch(/cannot be put together/)
    d = { ...d, items: d.items!.map((i) => (i.id === 'bad' ? { ...i, effects: [{ unlock: true }] } : i)) }
    expect(staffChecks(d).filter((c) => c.blocks && c.ids.includes('bad')).map((c) => c.text).join(' ')).toMatch(/who it is used on/)
    d = { ...d, items: d.items!.map((i) => (i.id === 'bad' ? { ...i, effects: [{ unlock: 5 }] } : i)) }
    expect(staffChecks(d).filter((c) => c.blocks && c.ids.includes('bad')).map((c) => c.text).join(' ')).toMatch(/doors, chests or both/)
  })
  it('an ability: the unlock effect, its lockpick, and the simplest form that says it', () => {
    let d = fixture()
    const pick = () => d.abilities!.find((a) => a.id === 'pick')!
    expect(unlockOf(pick())).toEqual({ kinds: 'both', needs: '' })
    expect(unlockOf(d.abilities!.find((a) => a.id === 'deft')!)).toEqual({ kinds: 'chest', needs: 'lockpick' })
    let a = setEffectKind(d, { id: 'x', name: 'X', target: 'unit', effect: { push: 1 } }, 'unlock')
    expect(a).toMatchObject({ target: 'cell', effect: { unlock: true } })
    a = setUnlock(a, { kinds: 'door' })
    expect(a.effect).toEqual({ unlock: ['door'] })
    a = setUnlock(a, { needs: 'lockpick' })
    expect(a.effect).toEqual({ unlock: { kinds: ['door'], needs: 'lockpick' } })
    a = setUnlock(a, { kinds: 'both' })
    expect(a.effect).toEqual({ unlock: { needs: 'lockpick' } })
    a = setUnlock(a, { needs: '' })
    expect(a.effect).toEqual({ unlock: true })
    d = { ...d, abilities: [...d.abilities!, { ...a, id: 'x', name: 'X', effect: { unlock: { needs: 'potion' } } }, { ...a, id: 'y', name: 'Y', effect: { unlock: { needs: 'nothing' } } }] }
    const texts = abilityChecks(d).map((c) => c.text)
    expect(texts.some((t) => /X: the lockpick Potion has unlimited uses|X: .*Potion has/.test(t) || /Potion/.test(t))).toBe(false) // potion has 1 use: fine
    expect(texts.some((t) => /Y: the lockpick “nothing” is not an item/.test(t))).toBe(true)
  })
})

describe('placed doors, chests and places', () => {
  const at = { x: 2, y: 0 }
  it('a story name follows into the events; a taken or empty-while-used name is refused', () => {
    const d = fixture()
    const r = changePlaceId(d, 'hall', { x: 5, y: 1 }, 'cell')
    expect(r.error).toBeNull()
    expect(r.doc.maps.hall!.structures![0].id).toBe('cell')
    expect(r.doc.maps.hall!.events![0].when).toEqual({ place: 'cell', is: 'opened' })
    expect(changePlaceId(d, 'hall', { x: 5, y: 1 }, 'd2').error).toMatch(/already has this name/)
    expect(changePlaceId(d, 'hall', { x: 5, y: 1 }, '').error).toMatch(/Events point at this name/)
    expect(changePlaceId(d, 'hall', { x: 5, y: 3 }, '').doc.maps.hall!.structures![1].id).toBeUndefined()
    expect(suggestPlaceId(d.maps.hall!, 'chest')).toBe('chest_1')
  })
  it('a command naming a place follows the rename too', () => {
    let d = fixture()
    d = { ...d, maps: { ...d.maps, hall: { ...d.maps.hall!, events: [...d.maps.hall!.events!, { id: 'e2', when: { battle: 'start' as const }, do: [{ openChest: 'c2', by: 'hero' } as unknown as Command, { unlockDoor: 'd2' }] }] } } }
    const r = changePlaceId(d, 'hall', { x: 2, y: 5 }, 'box').doc
    expect(r.maps.hall!.events![4].do[0]).toEqual({ openChest: 'box', by: 'hero' })
    expect(r.maps.hall!.events![4].do[1]).toEqual({ unlockDoor: 'd2' })
  })
  it('contents, lock, goes-for', () => {
    const d = fixture()
    let m = setContents(d.maps.hall!, at, ['potion', { item: 'heavy_axe', uses: 8 }, { resource: 'gold', amount: 50 }])
    expect(m.structures![2].contents).toHaveLength(3)
    m = setContents(m, at, [])
    expect('contents' in m.structures![2]).toBe(false)
    m = setPlacedLocked(m, { x: 5, y: 1 }, false)
    expect(m.structures![0].locked).toBe(false)
    m = setPlacedLocked(m, { x: 5, y: 1 }, undefined)
    expect('locked' in m.structures![0]).toBe(false)
    const mk = setGoesFor(d.maps.market!, { x: 1, y: 3 }, ['villages', 'chests'])
    expect(mk.units![2].goesFor).toEqual(['chests', 'villages'])
    expect('goesFor' in setGoesFor(mk, { x: 1, y: 3 }, []).units![2]).toBe(false)
  })
  it('a chest placed or changed into gets something inside; other keys go when the kind changes', () => {
    const d = fixture()
    let m: MapFile = { ...d.maps.hall!, structures: [{ type: 'chest', x: 0, y: 0 }, { type: 'hamlet', x: 1, y: 1, id: 'z', visitSides: ['blue'], visitUnits: ['hero'] }] }
    m = fitPlaceKeys(d, m, { x: 0, y: 0 })
    expect(m.structures![0].contents).toEqual(['iron_sword'])
    const swapped = fitPlaceKeys(d, { ...m, structures: [{ ...m.structures![0], type: 'hamlet', locked: true }, m.structures![1]] }, { x: 0, y: 0 })
    expect(swapped.structures![0].contents).toBeUndefined()
    expect(swapped.structures![0].locked).toBeUndefined()
    const toDoor = fitPlaceKeys(d, { ...m, structures: [m.structures![0], { ...m.structures![1], type: 'cell_door' }] }, { x: 1, y: 1 })
    expect(toDoor.structures![1].visitSides).toBeUndefined()
    expect(toDoor.structures![1].visitUnits).toBeUndefined()
  })
  it('checks: a chest with nothing inside, a bad name, bad visitors, a lock nothing opens, an unknown item', () => {
    let d = fixture()
    const hall = d.maps.hall!
    d = { ...d, maps: { ...d.maps, hall: { ...hall, structures: [
      { type: 'chest', x: 2, y: 0, id: 'c1' }, { type: 'chest', x: 4, y: 0, id: 'c1', contents: [] }, { type: 'chest', x: 6, y: 0, id: 'bad id', contents: ['ghost', { item: 'potion', uses: 5 }] },
      { type: 'hamlet', x: 1, y: 3, visitSides: ['green'], visitUnits: ['nobody'] }, { type: 'cell_door', x: 5, y: 1, locked: true, id: 'locked1' }, { type: 'camp', x: 8, y: 0, contents: ['potion'], locked: true },
    ], events: [] } } }
    const texts = placeChecks(d).filter((c) => c.mapId === 'hall').map((c) => c.text)
    const has = (re: RegExp) => expect(texts.some((t) => re.test(t)), String(re)).toBe(true)
    has(/Chest at 3, 1: a chest needs contents/)
    has(/Chest at 5, 1: a chest with nothing in it/)
    has(/story name “c1” is used by more than one structure/)
    has(/story name may use only letters/)
    has(/“ghost”, which is not an item of this game/)
    has(/Potion has 1 uses|Potion has/)
    has(/side “green” visit, which is not a side of this map/)
    has(/“nobody” visit, which is neither a named unit/)
    has(/Camp at 9, 1: it has contents, but only a chest holds things/)
    has(/Camp at 9, 1: “locked” only belongs to a door or a chest/)
    // The cell door's keys exist in this game, so a locked one is fine; take them away and it is a note.
    expect(texts.some((t) => /Cell Door at 6, 2: it is locked, but nothing/.test(t))).toBe(false)
    const noKeys: ProjectDoc = { ...d, items: d.items!.filter((i) => !/key/.test(i.id) && i.kind !== 'staff'), abilities: [] }
    const strict = placeChecks({ ...noKeys, structures: noKeys.structures!.map((s) => (s.id === 'cell_door' ? { ...s, door: { opens: 'cell_door_open' } } : s)) })
    expect(strict.some((c) => c.note && c.mapId === 'hall' && /Cell Door at 6, 2: it is locked, but nothing in the game can open it/.test(c.text))).toBe(true)
  })
  it('checks: goes-for with nothing to go for, and on a human side', () => {
    const d = fixture()
    const hall = d.maps.hall!
    const m = { ...hall, structures: [], units: [{ type: 'raider', side: 'red', x: 7, y: 2, goesFor: ['chests', 'villages', 'doors', 'steal'] }, { type: 'fighter', side: 'blue', x: 4, y: 1, goesFor: ['chests'] }] } as unknown as MapFile
    const texts = placeChecks({ ...d, maps: { ...d.maps, hall: m } }).filter((c) => c.mapId === 'hall').map((c) => c.text)
    expect(texts).toContain('The unit at 8, 3: it goes for chests, but this map has no chest.')
    expect(texts).toContain('The unit at 8, 3: it goes for villages, but this map has no place that can be visited.')
    expect(texts).toContain('The unit at 8, 3: it goes for doors, but this map has no door.')
    expect(texts.some((t) => /The unit at 5, 2: “goes for” tells the computer what to do, but this unit is on a side a person plays/.test(t))).toBe(true)
    expect(texts.some((t) => /steal/.test(t))).toBe(false) // steal needs no place
  })
})

describe('Events: the place trigger and the five commands', () => {
  const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps.hall! })
  it('lists only the places that fit, and starts valid', () => {
    const d = fixture()
    const c = ctx(d)
    expect(placesForEvent(d, c.map, 'opened').map((p) => p.id)).toEqual(['d1', 'd2', 'c1', 'c2'])
    expect(placesForEvent(d, c.map, 'visited').map((p) => p.id)).toEqual(['v1', 'v2', 'v3', 'v4'])
    expect(placesForEvent(d, c.map, 'destroyed').map((p) => p.id)).toEqual(['v1', 'v3', 'v4']) // camp has no ruin
    expect(placesForCommand(d, c.map, 'openChest').map((p) => p.id)).toEqual(['c1', 'c2'])
    expect(placesForCommand(d, c.map, 'closeDoor').map((p) => p.id)).toEqual(['d1', 'd2'])
    expect(placesForCommand(d, c.map, 'unlockDoor').map((p) => p.id)).toEqual(['d1', 'd2', 'c1', 'c2'])
    expect(newTrigger(c, 'place')).toEqual({ place: 'd1', is: 'opened' })
    expect(newCommand(c, 'openChest')).toEqual({ openChest: 'c1' })
    expect(newCommand(c, 'openDoor')).toEqual({ openDoor: 'd1' })
    expect(triggerKind({ place: 'd1', is: 'opened' })).toBe('place')
    expect(commandKind({ lockDoor: 'd1' })).toBe('lockDoor')
  })
  it('says them in plain words', () => {
    const d = fixture()
    const c = ctx(d)
    expect(triggerText(c, { place: 'd1', is: 'opened' })).toBe('The door “d1” (Cell Door) is opened')
    expect(triggerText(c, { place: 'v4', is: 'destroyed', by: { side: 'red' } })).toBe('The place “v4” (Hamlet) is destroyed by any unit of side red')
    expect(commandText(c, { openDoor: 'd1' })).toBe('Opens the door “d1” (Cell Door)')
    expect(commandText(c, { openChest: 'c1', by: 'hero' } as unknown as Command)).toMatch(/^Opens the chest “c1” \(Chest\), the contents go to /)
    expect(commandText(c, { unlockDoor: 'gone' })).toBe('Unlocks “gone”')
  })
  it('the loader’s rules: wrong kind, missing place, cannot be destroyed', () => {
    const d = fixture()
    const m = d.maps.hall!
    expect(placeProblem(d, m, 'd1', { command: 'openDoor' })).toBeNull()
    expect(placeProblem(d, m, 'c1', { command: 'openDoor' })).toMatch(/is not a door/)
    expect(placeProblem(d, m, 'd1', { command: 'openChest' })).toMatch(/is not a chest/)
    expect(placeProblem(d, m, 'c1', { command: 'unlockDoor' })).toBeNull()
    expect(placeProblem(d, m, 'v2', { is: 'destroyed' })).toMatch(/cannot be destroyed/)
    expect(placeProblem(d, m, 'v1', { is: 'opened' })).toMatch(/is not a door or a chest/)
    expect(placeProblem(d, m, 'nope', { is: 'opened' })).toMatch(/not the story name/)
    expect(placeProblem(d, m, '', { command: 'openDoor' })).toMatch(/pick a door/)
  })
  it('a map check says an event names a place that is gone', () => {
    const d = fixture()
    const m = { ...d.maps.hall!, structures: d.maps.hall!.structures!.filter((s) => s.id !== 'd1') }
    const texts = placeChecks({ ...d, maps: { ...d.maps, hall: m } }).filter((c) => c.mapId === 'hall').map((c) => c.text)
    expect(texts.some((t) => /Event “e_door”, “When”: “d1” is not the story name of a door, chest or place of this map/.test(t))).toBe(true)
    // The ordinary story checks stay quiet about it (placeChecks says it).
    expect(storyChecks({ ...d, maps: { ...d.maps, hall: m } }, 'hall').some((c) => /d1/.test(c.text))).toBe(false)
  })
})

describe('the rules card', () => {
  it('writes only Automatic, and removes the block again', () => {
    let d = fixture()
    expect(placeActions(d)).toBe('menu')
    d = setPlaceActions(d, 'auto')
    expect(d.game!.rules!.places).toEqual({ actions: 'auto' })
    expect(placeActions(d)).toBe('auto')
    d = setPlaceActions(d, 'menu')
    expect(d.game!.rules!.places).toBeUndefined()
  })
})

describe('delete follow-through (one Undo step: pure functions plus the store’s reconcile)', () => {
  it('a structure kind that a door opens into: the door part goes, a place loses its becomes, keys stop listing it', () => {
    const before = fixture()
    const after = reconcilePlaces(before, deleteStructure(before, 'cell_door_open'))
    expect(kind(after, 'cell_door').door).toBeUndefined()
    expect('blocksSight' in kind(after, 'cell_door')).toBe(false)
    expect(kind(after, 'latch_door').door).toBeUndefined() // latch_door also opens into it
    expect(kind(after, 'gate').door).toBeDefined()
    const b2 = reconcilePlaces(before, deleteStructure(before, 'hamlet_ruin'))
    expect(kind(b2, 'hamlet').visit).toEqual({ becomes: 'hamlet_shut' })
    const lines = structureUsageLines(before, 'cell_door_open').map((l) => l.text)
    expect(lines.some((t) => /Door “Cell Door” opens into it/.test(t))).toBe(true)
    const withKey = { ...before, items: before.items!.map((i) => (i.id === 'strong_key' ? i : i)) }
    const after3 = reconcilePlaces(withKey, deleteStructure(withKey, 'strongbox'))
    expect(item(after3, 'strong_key').opens).toBeUndefined()
    expect(placeKindUsageLines(before, 'strongbox').some((l) => /Key “Strong Key” opens it/.test(l.text))).toBe(true)
  })
  it('an item: the key lists, the contents of placed chests and a lockpick follow', () => {
    const before = fixture()
    const del = deleteItem(before, 'master_key')
    const after = reconcilePlaces(before, del)
    expect(kind(after, 'cell_door').door!.keys).toEqual(['brass_key'])
    const potion = reconcilePlaces(before, deleteItem(before, 'potion'))
    expect(potion.maps.hall!.structures![2].contents).toEqual([{ resource: 'gold', amount: 50 }])
    expect(potion.maps.market!.structures![0].contents).toBeUndefined() // a chest left with nothing: the check says it needs contents
    const pick = reconcilePlaces(before, deleteItem(before, 'lockpick'))
    expect(pick.abilities!.find((a) => a.id === 'deft')!.effect).toEqual({ unlock: ['chest'] })
    const lines = itemUsageLines(before, 'potion').map((l) => l.text)
    expect(lines.some((t) => /holds it \(it is taken out\)/.test(t))).toBe(true)
    expect(placeItemUsageLines(before, 'lockpick').some((l) => /Ability “Deft Hands” needs it as a lockpick/.test(l.text))).toBe(true)
    expect(placeItemUsageLines(before, 'master_key').some((l) => /Door “Cell Door” lists it as a key/.test(l.text))).toBe(true)
  })
  it('a resource: amounts of it leave the chests; nothing changed returns the same document', () => {
    const before = fixture()
    const same = reconcilePlaces(before, { ...before })
    expect(same).not.toBe(before)
    expect(reconcilePlaces(before, before)).toBe(before)
    const noGold = { ...before, game: { ...before.game!, resources: before.game!.resources!.filter((r) => r.id !== 'gold') } }
    expect(reconcilePlaces(before, noGold).maps.hall!.structures![2].contents).toEqual(['potion'])
  })
})
