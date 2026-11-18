import { describe, expect, it } from 'vitest'
import { attackWeaponChoice, menuOptionProblems, menuUnavailable, setAttackWeaponChoice, setMenuUnavailable } from '../../src/model/menuOptions'
import type { ProjectDoc } from '../../src/model/types'

const doc = (extra: Record<string, unknown> = {}, weapons: 'fixed' | 'carried' = 'carried'): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [], rules: { items: { weapons } }, ...extra },
  terrain: [], structures: null, weapons: null, items: null, research: null, units: null, maps: {}, scripts: {},
} as unknown as ProjectDoc)

describe('menu options (game.json menuUnavailable, attackWeaponChoice)', () => {
  it('read as the defaults when the keys are missing or are a word the loader refuses', () => {
    expect(menuUnavailable(doc())).toBe('grey')
    expect(attackWeaponChoice(doc())).toBe('equipped')
    expect(menuUnavailable(doc({ menuUnavailable: 'gray' }))).toBe('grey')
    expect(attackWeaponChoice(doc({ attackWeaponChoice: 'ask' }))).toBe('equipped')
  })
  it('write the non-default word and no key for the default', () => {
    let d = setMenuUnavailable(doc(), 'hide')
    expect(d.game!.menuUnavailable).toBe('hide')
    d = setAttackWeaponChoice(d, 'pick')
    expect(d.game!.attackWeaponChoice).toBe('pick')
    d = setMenuUnavailable(d, 'grey')
    d = setAttackWeaponChoice(d, 'equipped')
    expect('menuUnavailable' in d.game!).toBe(false)
    expect('attackWeaponChoice' in d.game!).toBe(false)
    const same = doc()
    expect(setMenuUnavailable(same, 'grey')).toBe(same) // nothing to change: the same document
    expect(setAttackWeaponChoice(same, 'equipped')).toBe(same)
  })
  it('keeps the keys it does not know', () => {
    const d = setMenuUnavailable(doc({ somethingElse: 3 }), 'hide')
    expect((d.game as unknown as Record<string, unknown>).somethingElse).toBe(3)
  })
  it('says what the loader refuses, and notes that choosing a weapon needs carried weapons', () => {
    expect(menuOptionProblems(doc())).toEqual([])
    expect(menuOptionProblems(doc({ menuUnavailable: 'gray' }))).toHaveLength(1)
    expect(menuOptionProblems(doc({ attackWeaponChoice: 'ask' }))).toHaveLength(1)
    expect(menuOptionProblems(doc({ attackWeaponChoice: 'pick' }))).toEqual([])
    expect(menuOptionProblems(doc({ attackWeaponChoice: 'pick' }, 'fixed'))).toHaveLength(1)
  })
})
