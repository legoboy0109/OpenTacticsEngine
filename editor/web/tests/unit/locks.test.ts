// Character-locked weapons (docs/SRPG.md "Character-locked weapons", editor step): the list edit, what deleting, renaming a character or
// switching characters off does to the lists, the delete question, and the checks. Uses the engine's fixture tests/fixtures/srpg-locks.
import { describe, expect, it } from 'vitest'
import { lockChecks } from '../../src/model/lockChecks'
import { cleanAllLocks, gearLockedTo, lockCount, locksShown, renameCharacterLocks, setWielders, wieldersOf } from '../../src/model/locks'
import { characterUsageLines, deleteCharacter } from '../../src/model/srpgRefs'
import { switchOffFeature } from '../../src/model/srpgFeatures'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo, loadLocksFixture } from './fixtures'

const fx = (): ProjectDoc => loadLocksFixture()
const list = (d: ProjectDoc, id: string) => d.weapons!.find((w) => w.id === id)!.wieldedBy

describe('the list', () => {
  it('the fixture shows the card; the demos and a game without characters do not', () => {
    expect(locksShown(fx())).toBe(true)
    expect(locksShown({ ...fx(), characters: null })).toBe(false)
    expect(locksShown({ ...fx(), characters: [] })).toBe(false)
    expect(locksShown(loadDemo('demo-srpg'))).toBe(false)
  })
  it('writes the chosen ids, and removes the key when none are left', () => {
    const d = setWielders(fx(), 'steel_sword', ['mara'])
    expect(list(d, 'steel_sword')).toEqual(['mara'])
    expect(wieldersOf(d.weapons!.find((w) => w.id === 'steel_sword')!)).toEqual(['mara'])
    const none = setWielders(d, 'steel_sword', [])
    expect('wieldedBy' in none.weapons!.find((w) => w.id === 'steel_sword')!).toBe(false)
  })
  it('the weapons locked to a character', () => {
    expect(gearLockedTo(fx(), 'kell').map((w) => w.id)).toEqual(['dawn_blade', 'pair_blade', 'trio_blade'])
    expect(gearLockedTo(fx(), 'mara').map((w) => w.id)).toEqual(['trio_blade'])
  })
})

describe('a character goes, is renamed, or characters are switched off', () => {
  it('deleting a character takes it off every list; a list that empties loses its key; one Undo step', () => {
    const d = deleteCharacter(fx(), 'kell')
    expect(list(d, 'dawn_blade')).toBeUndefined()
    expect(list(d, 'pair_blade')).toEqual(['ser'])
    expect(list(d, 'trio_blade')).toEqual(['ser', 'mara'])
    const start = fx()
    let h = initHistory(start)
    h = pushEdit(h, deleteCharacter(h.present, 'kell'), undefined, 0)
    h = undo(h)
    expect(h.present.weapons).toEqual(start.weapons)
  })
  it('the delete question lists the locked weapons', () => {
    const lines = characterUsageLines(fx(), 'mara').filter((l) => l.section === 'weapons')
    expect(lines).toHaveLength(1)
    expect(lines[0].text).toContain('Trio Blade')
  })
  it('renaming an id updates the lists and leaves other weapons alone', () => {
    const start = fx()
    const d = renameCharacterLocks(start, 'kell', 'kelle')
    expect(list(d, 'pair_blade')).toEqual(['kelle', 'ser'])
    expect(d.weapons!.find((w) => w.id === 'bronze_sword')).toBe(start.weapons!.find((w) => w.id === 'bronze_sword'))
    expect(renameCharacterLocks(fx(), 'nobody', 'x').weapons!.map((w) => w.wieldedBy)).toEqual(fx().weapons!.map((w) => w.wieldedBy))
  })
  it('switching characters off removes every list and the count says how many weapons had one', () => {
    expect(lockCount(fx())).toBe(3)
    const d = switchOffFeature(fx(), 'characters')
    expect(d.characters).toBeNull()
    expect(JSON.stringify(d.weapons)).not.toContain('wieldedBy')
    expect(cleanAllLocks(d)).toBe(d)
  })
  it('a game without any list is returned as it was', () => {
    const d = loadDemo('demo-srpg')
    expect(deleteCharacter(d, 'x')).toEqual(d)
    expect(switchOffFeature(d, 'characters')).toEqual(d)
  })
})

describe('checks', () => {
  it('the fixture has none (every listed character has a class that wields the group)', () => {
    expect(lockChecks(fx())).toEqual([])
  })
  it('nobody on the list can ever wield it: their classes cannot use its group', () => {
    const d = setWielders(fx(), 'dawn_blade', ['mara']) // Mara is an archer; Dawn Blade is a sword
    const c = lockChecks(d)
    expect(c).toHaveLength(1)
    expect(c[0]).toMatchObject({ section: 'weapons', ids: ['dawn_blade'] })
    expect(c[0].text).toContain('Nobody on the list can ever wield Dawn Blade')
    expect(lockChecks(setWielders(fx(), 'dawn_blade', ['mara', 'kell']))).toEqual([]) // one who can is enough
  })
  it('an id that is no character is named; a list of only unknown ids says nothing more', () => {
    const c = lockChecks(setWielders(fx(), 'dawn_blade', ['ghost']))
    expect(c.map((x) => x.text)).toEqual(['Dawn Blade is locked to “ghost”, who is not a character of this game.'])
  })
  it('nothing when weapons are not carried (the loader ignores the key there)', () => {
    const d = fx()
    expect(lockChecks({ ...d, game: { ...d.game!, rules: { ...d.game!.rules!, items: { ...(d.game!.rules!.items ?? {}), weapons: 'fixed' } } } })).toEqual([])
  })
})
