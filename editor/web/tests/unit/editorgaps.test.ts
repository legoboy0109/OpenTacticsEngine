// The editor-gap pass: class locks (`wieldedByTypes`), a character's own weapon groups (`wields`), `rankXp: 0`, the shared starting-ranks and
// goes-for controls on the story "Add a unit" command, and the preparation marker on the campaign. Engine fixture: tests/fixtures/srpg-locks.
import { describe, expect, it } from 'vitest'
import { setCharacterWields } from '../../src/model/characters'
import { lockChecks } from '../../src/model/lockChecks'
import { dropUnitTypeLocks, setGearWielderTypes, typeLockUsageLines, typeLocksShown, wielderTypesOf } from '../../src/model/locks'
import { placeChecks } from '../../src/model/placeChecks'
import { rankChecks } from '../../src/model/rankChecks'
import { renameGroupRefs } from '../../src/model/rankRefs'
import { setGearRankXp, wieldsOf } from '../../src/model/ranks'
import { findUsages } from '../../src/model/references'
import { buildTimeline } from '../../src/model/timelineGraph'
import { deleteUnit } from '../../src/model/units'
import type { ProjectDoc } from '../../src/model/types'
import { loadLocksFixture, loadPrepFixture } from './fixtures'

const fx = (): ProjectDoc => loadLocksFixture()
const weapon = (d: ProjectDoc, id: string) => d.weapons!.find((w) => w.id === id)!

describe('class locks (wieldedByTypes)', () => {
  it('writes the chosen unit types, and removes the key when none are left', () => {
    const d = setGearWielderTypes(fx(), 'weapon', 'steel_sword', ['knight'])
    expect(weapon(d, 'steel_sword').wieldedByTypes).toEqual(['knight'])
    expect(wielderTypesOf(weapon(d, 'steel_sword'))).toEqual(['knight'])
    expect('wieldedByTypes' in weapon(setGearWielderTypes(d, 'weapon', 'steel_sword', []), 'steel_sword')).toBe(false)
  })
  it('the list shows whenever weapons are carried, with or without characters', () => {
    expect(typeLocksShown(fx())).toBe(true)
    expect(typeLocksShown({ ...fx(), characters: null })).toBe(true)
    const fixed = fx()
    expect(typeLocksShown({ ...fixed, game: { ...fixed.game!, rules: { ...fixed.game!.rules, items: { weapons: 'fixed' } } } as ProjectDoc['game'] })).toBe(false)
  })
  it('deleting a unit type takes it off every list (an emptied list loses its key), and the delete question names the weapon', () => {
    const d = setGearWielderTypes(setGearWielderTypes(fx(), 'weapon', 'steel_sword', ['knight']), 'weapon', 'bronze_sword', ['knight', 'archer'])
    expect(typeLockUsageLines(d, 'knight').map((u) => u.text).join('\n')).toMatch(/Weapon “Steel Sword” is locked to some classes/)
    const cut = dropUnitTypeLocks(d, 'knight')
    expect('wieldedByTypes' in weapon(cut, 'steel_sword')).toBe(false)
    expect(weapon(cut, 'bronze_sword').wieldedByTypes).toEqual(['archer'])
    expect(findUsages(d, 'unit', 'knight').map((u) => u.text).join('\n')).toMatch(/locked to some classes/)
    const gone = deleteUnit(d, 'knight')
    expect('wieldedByTypes' in weapon(gone, 'steel_sword')).toBe(false)
    expect(weapon(gone, 'bronze_sword').wieldedByTypes).toEqual(['archer'])
  })
  it('checks: an unknown class, and a list where no class can use the weapon group', () => {
    const base = setGearWielderTypes(fx(), 'weapon', 'steel_sword', ['archer']) // an archer cannot use a sword
    expect(lockChecks(base).map((c) => c.text).join('\n')).toMatch(/Nobody on the list can ever wield Steel Sword: none of those classes can use its group/)
    const both = setGearWielderTypes(base, 'weapon', 'steel_sword', ['archer', 'knight'])
    expect(lockChecks(both)).toEqual([])
    const unknown = setGearWielderTypes(fx(), 'weapon', 'steel_sword', ['ghost'])
    expect(lockChecks(unknown).map((c) => c.text)).toEqual(['Steel Sword is locked to the class “ghost”, which is not in this game.'])
  })
  it('a character on one list and a class on the other: the lock passes by either, so one usable entry is enough', () => {
    const d = setGearWielderTypes(fx(), 'weapon', 'dawn_blade', ['archer']) // dawn_blade is locked to kell (a fighter, can use swords)
    expect(lockChecks(d)).toEqual([])
  })
})

describe('a character\'s own weapon groups (wields)', () => {
  it('writes the list, and an empty list removes the key', () => {
    const d = setCharacterWields(fx(), 'mara', ['sword'])
    expect(d.characters!.find((c) => c.id === 'mara')!.wields).toEqual(['sword'])
    expect('wields' in setCharacterWields(d, 'mara', []).characters!.find((c) => c.id === 'mara')!).toBe(false)
  })
  it('the groups a unit can wield are its class\'s plus the character\'s (what the rank pickers offer)', () => {
    const d = setCharacterWields(fx(), 'mara', ['sword'])
    expect(wieldsOf(fx(), 'archer', 'mara')).toEqual(['bow'])
    expect(wieldsOf(d, 'archer', 'mara')).toEqual(['bow', 'sword'])
    expect(wieldsOf(d, 'archer')).toEqual(['bow'])
  })
  it('renaming a group follows into characters', () => {
    const d = renameGroupRefs(setCharacterWields(fx(), 'mara', ['sword']), 'sword', 'blade')
    expect(d.characters!.find((c) => c.id === 'mara')!.wields).toEqual(['blade'])
  })
})

describe('rankXp 0', () => {
  it('0 is kept (it trains nothing), 1 is the default and not written, the checks accept 0..99', () => {
    const d = setGearRankXp(fx(), 'weapon', 'steel_sword', 0)
    expect(weapon(d, 'steel_sword').rankXp).toBe(0)
    expect('rankXp' in weapon(setGearRankXp(d, 'weapon', 'steel_sword', 1), 'steel_sword')).toBe(false)
    expect(rankChecks(d).filter((c) => /experience per use/.test(c.text))).toEqual([])
    expect(rankChecks(setGearRankXp(fx(), 'weapon', 'steel_sword', 100)).map((c) => c.text).join()).toMatch(/from 0 to 99/)
    expect(rankChecks(setGearRankXp(fx(), 'weapon', 'steel_sword', -1)).map((c) => c.text).join()).toMatch(/from 0 to 99/)
  })
})

describe('goes-for on an Add a unit command', () => {
  const withCommand = (side: string, goesFor: unknown): ProjectDoc => {
    const d = fx()
    const id = d.game!.maps[0].id
    const m = d.maps[id]!
    const events = [{ id: 'reinforce', when: { turn: 1 }, do: [{ addUnit: { type: 'grunt', side, x: 4, y: 0, goesFor } }] }]
    return { ...d, maps: { ...d.maps, [id]: { ...m, events } } } as ProjectDoc
  }
  it('a computer unit may go for something; a person\'s unit, or an empty list, is flagged', () => {
    expect(placeChecks(withCommand('red', ['chests'])).filter((c) => !c.note)).toEqual([])
    expect(placeChecks(withCommand('blue', ['steal'])).map((c) => c.text).join()).toMatch(/Event .*the unit it adds: “goes for” tells the computer/)
    expect(placeChecks(withCommand('red', [])).map((c) => c.text).join()).toMatch(/the unit it adds: “goes for” needs at least one/)
  })
  it('a goal with nothing of that kind on the map is only a note', () => {
    const notes = placeChecks(withCommand('red', ['doors'])).filter((c) => c.note)
    expect(notes.map((c) => c.text).join()).toMatch(/it goes for doors, but this map has no door/)
  })
})

describe('the campaign marks a chapter whose map has a preparation screen', () => {
  it('the level box says so', () => {
    const d = loadPrepFixture()
    const t = buildTimeline(d)!
    const levels = t.nodes.filter((n) => n.kind === 'level')
    expect(levels.some((n) => n.hasPrep)).toBe(true)
    expect(levels.filter((n) => n.hasPrep).every((n) => !!d.maps[n.mapId!]?.prep)).toBe(true)
  })
})
