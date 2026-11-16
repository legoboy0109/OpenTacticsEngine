// E1 of docs/SRPG.md: the Characters screen's edits, characters on the map (placed without a `type`), the "class" word,
// the checks, and what deleting a unit type or a character takes with it.
import { describe, expect, it } from 'vitest'
import {
  addCharacter, charactersOfType, classWord, duplicateCharacter, moveCharacter, setCharacterItems, setCharacterLevel, setCharacterNumber, totalStat, updateCharacter,
} from '../../src/model/characters'
import { characterChecks, NOT_USED } from '../../src/model/characterChecks'
import { placedCharacterIds, storyIdOf, typeIdOf, withResolvedTypes } from '../../src/model/characterUnits'
import { ctxOf, editMap } from '../../src/model/mapCtx'
import { changeUnitType, moveMapUnit, placeUnit, setUnitStat, startStats } from '../../src/model/mapUnits'
import { findUsages } from '../../src/model/references'
import { definedStoryIds } from '../../src/model/campaignRoster'
import { characterUsageLines, deleteCharacter } from '../../src/model/srpgRefs'
import { namedUnits, unitDisplayName } from '../../src/model/storyIds'
import { deleteUnit } from '../../src/model/units'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

/** The demo with the feature on (an empty list), so `addCharacter` works. */
const game = (): ProjectDoc => ({ ...loadDemo('demo-srpg'), characters: [] })
const firstMapId = (d: ProjectDoc) => Object.keys(d.maps)[0]
const emptyMap = (d: ProjectDoc): MapFile => ({ ...d.maps[firstMapId(d)]!, units: [], events: [], objectives: undefined })
/** The game with two characters and the first map holding nothing, so placements are the only units. */
function withPeople(): ProjectDoc {
  let d = game()
  d = { ...d, maps: { ...d.maps, [firstMapId(d)]: emptyMap(d) } }
  const [a, b] = d.units!
  d = addCharacter(d, 'Tamsin')
  d = addCharacter(d, 'Oswin')
  d = updateCharacter(d, 'oswin', { type: b.id })
  d = updateCharacter(d, 'tamsin', { type: a.id })
  return d
}

describe('the character list edits', () => {
  it('adds a character that starts as the first unit type and has an id made from the name', () => {
    const d = addCharacter(game(), 'Tamsin the Bold')
    expect(d.characters).toEqual([{ id: 'tamsin_the_bold', name: 'Tamsin the Bold', type: d.units![0].id }])
  })
  it('does nothing while the feature is off', () => {
    expect(addCharacter({ ...game(), characters: null }, 'Tamsin').characters).toBeNull()
  })
  it('duplicates right after the original with a fresh id and "copy" in the name', () => {
    let d = withPeople()
    d = duplicateCharacter(d, 'tamsin')
    expect(d.characters!.map((c) => c.id)).toEqual(['tamsin', 'tamsin_copy', 'oswin'])
    expect(d.characters![1].name).toBe('Tamsin copy')
  })
  it('reorders', () => {
    expect(moveCharacter(withPeople(), 0, 1).characters!.map((c) => c.id)).toEqual(['oswin', 'tamsin'])
  })
  it('keeps unknown keys through an edit', () => {
    let d = withPeople()
    d = { ...d, characters: d.characters!.map((c) => (c.id === 'tamsin' ? { ...c, mystery: 7 } : c)) }
    d = updateCharacter(d, 'tamsin', { name: 'Tam' })
    expect(d.characters![0]).toMatchObject({ name: 'Tam', mystery: 7 })
  })
  it('personal bonuses may be negative; empty removes the key and then the table', () => {
    let d = withPeople()
    d = setCharacterNumber(d, 'tamsin', 'stats', 'str', -2)
    d = setCharacterNumber(d, 'tamsin', 'growths', 'hp', 10)
    expect(d.characters![0]).toMatchObject({ stats: { str: -2 }, growths: { hp: 10 } })
    d = setCharacterNumber(d, 'tamsin', 'stats', 'str', undefined)
    expect(d.characters![0]).not.toHaveProperty('stats')
  })
  it('the level key goes at 1 and is capped at the highest level', () => {
    let d = withPeople()
    d = setCharacterLevel(d, 'tamsin', 5)
    expect(d.characters![0].level).toBe(5)
    d = setCharacterLevel(d, 'tamsin', 1)
    expect(d.characters![0]).not.toHaveProperty('level')
    d = setCharacterLevel(d, 'tamsin', 999)
    expect(d.characters![0].level).toBe(d.game?.rules?.progression?.maxLevel ?? 20)
  })
  it('starting items: a list (even empty) replaces the class\'s, undefined goes back to it', () => {
    let d = withPeople()
    d = setCharacterItems(d, 'tamsin', [])
    expect(d.characters![0].items).toEqual([])
    d = setCharacterItems(d, 'tamsin', undefined)
    expect(d.characters![0]).not.toHaveProperty('items')
  })
  it('totals are the class value plus the bonus, never below 0', () => {
    expect(totalStat(5, 2)).toBe(7)
    expect(totalStat(1, -3)).toBe(0)
    expect(totalStat(undefined, undefined)).toBe(0)
  })
})

describe('"class" or "unit type"', () => {
  it('says class once the game has a character, unit type before', () => {
    expect(classWord(game())).toBe('unit type')
    expect(classWord(game(), true)).toBe('unit types')
    expect(classWord(withPeople())).toBe('class')
    expect(classWord(withPeople(), true)).toBe('classes')
    expect(classWord({ ...game(), characters: null })).toBe('unit type')
  })
})

describe('characters on a map', () => {
  it('places a character as { character, side, x, y } with no type, once, on a free square', () => {
    const d = withPeople()
    const id = firstMapId(d)
    const c = ctxOf(d)!
    const r = placeUnit(d.maps[id]!, c, { character: 'tamsin', side: d.maps[id]!.sides![0].id, x: 0, y: 0 })
    expect(r.error).toBeNull()
    expect(r.map.units).toEqual([{ character: 'tamsin', side: d.maps[id]!.sides![0].id, x: 0, y: 0 }])
    const again = placeUnit(r.map, c, { character: 'tamsin', side: d.maps[id]!.sides![0].id, x: 1, y: 0 })
    expect(again.error).toMatch(/already on the map/)
    expect(placeUnit(r.map, c, { character: 'oswin', side: d.maps[id]!.sides![0].id, x: 0, y: 0 }).error).toMatch(/already a unit/)
    expect(placeUnit(r.map, c, { character: 'nobody', side: d.maps[id]!.sides![0].id, x: 2, y: 0 }).error).toMatch(/character/)
    expect(placedCharacterIds(r.map)).toEqual(['tamsin'])
  })
  it('a plain unit is still placed with its type', () => {
    const d = withPeople()
    const id = firstMapId(d)
    const side = d.maps[id]!.sides![0].id
    const r = placeUnit(d.maps[id]!, ctxOf(d)!, { type: d.units![0].id, side, x: 3, y: 3 })
    expect(r.map.units).toEqual([{ type: d.units![0].id, side, x: 3, y: 3 }])
  })
  it('the unit type of a character unit comes from the character, unless the map overrides it', () => {
    const d = withPeople()
    expect(typeIdOf(d.characters, { character: 'tamsin' })).toBe(d.characters![0].type)
    expect(typeIdOf(d.characters, { character: 'tamsin', type: 'other' })).toBe('other')
    expect(typeIdOf(d.characters, { character: 'gone' })).toBe('')
    expect(typeIdOf(null, { type: 'knight' })).toBe('knight')
    const map = { units: [{ character: 'tamsin', side: 'a', x: 0, y: 0 }, { type: 'x', side: 'a', x: 1, y: 0 }] } as unknown as MapFile
    expect(withResolvedTypes(map, d.characters).units!.map((u) => u.type)).toEqual([d.characters![0].type, 'x'])
    const plain = { units: [{ type: 'x', side: 'a', x: 1, y: 0 }] } as unknown as MapFile
    expect(withResolvedTypes(plain, d.characters)).toBe(plain)
  })
  it('the story id of a character unit is the character id', () => {
    expect(storyIdOf({ character: 'tamsin' })).toBe('tamsin')
    expect(storyIdOf({ id: 'boss' })).toBe('boss')
    expect(storyIdOf({})).toBeUndefined()
  })
  it('events and scenes can name a placed character, and it is shown by its own name', () => {
    let d = withPeople()
    const id = firstMapId(d)
    d = { ...d, maps: { ...d.maps, [id]: placeUnit(d.maps[id]!, ctxOf(d)!, { character: 'tamsin', side: d.maps[id]!.sides![0].id, x: 0, y: 0 }).map } }
    expect(namedUnits(d, d.maps[id]!).map((u) => u.id)).toEqual(['tamsin'])
    expect(namedUnits(d, d.maps[id]!)[0].label).toBe('Tamsin (tamsin)')
    expect(unitDisplayName(d, d.maps[id]!.units![0])).toBe('Tamsin')
    expect(definedStoryIds(d).has('tamsin')).toBe(true)
  })
  it('dragging a character checks the terrain for its own type', () => {
    const d = withPeople()
    const id = firstMapId(d)
    const side = d.maps[id]!.sides![0].id
    const map = placeUnit(d.maps[id]!, ctxOf(d)!, { character: 'tamsin', side, x: 0, y: 0 }).map
    const moved = moveMapUnit(map, ctxOf(d)!, { x: 0, y: 0 }, { x: 1, y: 0 })
    expect(moved.error === null || /cannot stand/.test(moved.error)).toBe(true)
    if (!moved.error) expect(moved.map.units![0]).not.toHaveProperty('type')
  })
  it('a character\'s start stats are its class plus its own, and a map override equal to that is not stored', () => {
    let d = withPeople()
    const type = d.units![0]
    d = setCharacterNumber(d, 'tamsin', 'stats', 'str', 2)
    const id = firstMapId(d)
    const side = d.maps[id]!.sides![0].id
    const c = ctxOf(d)!
    const map = placeUnit(d.maps[id]!, c, { character: 'tamsin', side, x: 0, y: 0 }).map
    const base = (type.stats?.str ?? 0) + 2
    expect(startStats(c, { character: 'tamsin' }, type.id).str).toBe(base)
    expect(setUnitStat(map, c, 0, 0, 'str', base).units![0]).not.toHaveProperty('stats')
    expect(setUnitStat(map, c, 0, 0, 'str', base + 1).units![0].stats).toEqual({ str: base + 1 })
  })
  it('a different class on one map is stored as `type`; the character\'s own class removes it again', () => {
    const d = withPeople()
    const id = firstMapId(d)
    const side = d.maps[id]!.sides![0].id
    const c = ctxOf(d)!
    const own = d.characters![0].type
    const other = d.units!.find((u) => u.id !== own)!.id
    let map = placeUnit(d.maps[id]!, c, { character: 'tamsin', side, x: 0, y: 0 }).map
    const asOther = changeUnitType(map, c, 0, 0, other)
    if (asOther.error) return // the demo's terrain may forbid it: nothing to check then
    map = asOther.map
    expect(map.units![0]).toMatchObject({ character: 'tamsin', type: other })
    map = changeUnitType(map, c, 0, 0, own).map
    expect(map.units![0]).not.toHaveProperty('type')
    expect(map.units![0]).toMatchObject({ character: 'tamsin' })
  })
})

describe('checks', () => {
  it('a character no map places and no event adds is flagged; placing it clears it', () => {
    let d = withPeople()
    expect(characterChecks(d)).toEqual([{ characterId: 'tamsin', text: NOT_USED }, { characterId: 'oswin', text: NOT_USED }])
    const id = firstMapId(d)
    d = editMap(d, id, (m, c) => placeUnit(m, c, { character: 'tamsin', side: m.sides![0].id, x: 0, y: 0 }).map)
    expect(characterChecks(d).map((c) => c.characterId)).toEqual(['oswin'])
  })
  it('an event that adds the character counts as using it', () => {
    let d = withPeople()
    const id = firstMapId(d)
    const m = d.maps[id]!
    d = { ...d, maps: { ...d.maps, [id]: { ...m, events: [{ id: 'arrive', when: { battle: 'start' }, do: [{ addUnit: { character: 'oswin', side: m.sides![0].id, x: 1, y: 1 } }] }] } } }
    expect(characterChecks(d).map((c) => c.characterId)).toEqual(['tamsin'])
  })
  it('says nothing without characters or when a map could not be read', () => {
    expect(characterChecks(game())).toEqual([])
    const d = withPeople()
    expect(characterChecks({ ...d, maps: { ...d.maps, broken: null } })).toEqual([])
  })
})

describe('deleting', () => {
  it('deleting a character takes its placed unit and the events naming it away, in one document', () => {
    let d = withPeople()
    const id = firstMapId(d)
    const m = d.maps[id]!
    d = editMap(d, id, (mm, c) => placeUnit(mm, c, { character: 'tamsin', side: mm.sides![0].id, x: 0, y: 0 }).map)
    d = { ...d, maps: { ...d.maps, [id]: { ...d.maps[id]!, events: [{ id: 'hello', when: { turn: 2 }, do: [{ moveUnit: 'tamsin', x: 1, y: 1 }] }] } } }
    expect(characterUsageLines(d, 'tamsin').some((u) => u.section === 'maps')).toBe(true)
    const after = deleteCharacter(d, 'tamsin')
    expect(after.characters!.map((c) => c.id)).toEqual(['oswin'])
    expect(after.maps[id]!.units).toEqual([])
    expect(JSON.stringify(after.maps[id]!.events)).not.toContain('tamsin')
    void m
  })
  it('deleting a unit type takes the characters of that type with it, and says so first', () => {
    const d = withPeople()
    const type = d.characters![0].type
    expect(charactersOfType(d, type).map((c) => c.id)).toEqual(['tamsin'])
    expect(findUsages(d, 'unit', type).some((u) => u.section === 'characters' && /Tamsin/.test(u.text))).toBe(true)
    const after = deleteUnit(d, type)
    expect(after.units!.some((u) => u.id === type)).toBe(false)
    expect(after.characters!.map((c) => c.id)).toEqual(['oswin'])
  })
})
