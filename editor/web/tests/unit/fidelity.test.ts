// The fidelity options in the editor's model (docs/SRPG.md section 11, docs/EVENTS.md): defaults are never written, edits keep
// unknown keys, and the checks mirror the loader's refusals.
import { describe, expect, it } from 'vitest'
import { abilityExtraProblems, abilityInflict, amountFromText, setAbilityInflict, setAbilityXp } from '../../src/model/fidelityAbility'
import { computerCap, computerCapProblems, setComputerCap } from '../../src/model/fidelityCaps'
import { computerGains, fidelityRuleProblems, hitRoll, setComputerGains, setHitRoll, setXpNoDamage, xpNoDamageText } from '../../src/model/fidelityRules'
import { setStaffXpMiss, xpMissText } from '../../src/model/staff'
import { COMMAND_KINDS, CONDITION_KINDS } from '../../src/model/storyTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadRanksFixture, loadStavesFixture } from './fixtures'

const progression = (d: ProjectDoc): ProjectDoc => ({ ...d, game: { ...d.game!, rules: { ...d.game!.rules, progression: { enabled: true } } } }) as ProjectDoc

describe('hit roll and experience rules', () => {
  it('writes only non-default values and removes them again', () => {
    const d = progression(loadRanksFixture())
    expect(hitRoll(d)).toBe('single')
    const avg = setHitRoll(d, 'average2')
    expect(avg.game!.rules!.hitRoll).toBe('average2')
    expect(setHitRoll(avg, 'single').game!.rules).not.toHaveProperty('hitRoll')
    expect(computerGains(d)).toBe(true)
    const off = setComputerGains(d, false)
    expect(off.game!.rules!.progression!.computerGains).toBe(false)
    expect(setComputerGains(off, true).game!.rules!.progression).not.toHaveProperty('computerGains')
  })
  it('xpNoDamage is a number when it looks like one, else a formula, and empty removes it', () => {
    const d = setXpNoDamage(progression(loadRanksFixture()), '3')
    expect(d.game!.rules!.progression!.xpNoDamage).toBe(3)
    expect(xpNoDamageText(d)).toBe('3')
    expect(setXpNoDamage(d, 'a.level + 1').game!.rules!.progression!.xpNoDamage).toBe('a.level + 1')
    expect(setXpNoDamage(d, '  ').game!.rules!.progression).not.toHaveProperty('xpNoDamage')
  })
  it('checks mirror the loader', () => {
    const d = progression(loadRanksFixture())
    expect(fidelityRuleProblems(d)).toEqual([])
    const bad = { ...d, game: { ...d.game!, rules: { ...d.game!.rules, hitRoll: 'triple', progression: { enabled: true, xpNoDamage: -2 } } } } as ProjectDoc
    expect(fidelityRuleProblems(bad).length).toBe(2)
  })
})

describe('computer caps', () => {
  it('sets, clears and checks a per-stat cap, keeping the file shape', () => {
    const d = loadRanksFixture()
    const stat = d.game!.stats[0].id
    const one = setComputerCap(d, stat, 120)
    expect(computerCap(one, stat)).toBe(120)
    expect(one.caps!.caps).toEqual(d.caps!.caps)
    expect(setComputerCap(one, stat, undefined).caps).not.toHaveProperty('computerCaps')
    expect(computerCapProblems(one)).toEqual([])
    expect(computerCapProblems(setComputerCap(d, stat, 0)).length).toBe(1)
    const unknown = { ...d, caps: { ...d.caps!, computerCaps: { nonesuch: 5 } } } as ProjectDoc
    expect(computerCapProblems(unknown)[0]).toContain('not a stat')
  })
})

describe('ability extras and staff xpMiss', () => {
  it('xp and inflict are written only when set, and checked', () => {
    const d = progression(loadStavesFixture())
    let a = { id: 'dance', name: 'Dance', target: 'ally' as const, effect: { refresh: true as const } }
    expect(amountFromText('12')).toBe(12)
    expect(amountFromText('a.level')).toBe('a.level')
    a = setAbilityXp(a, '10') as typeof a
    expect(a).toHaveProperty('xp', 10)
    expect(setAbilityXp(a, '')).not.toHaveProperty('xp')
    const inflicted = setAbilityInflict(a, 'poison', 2)
    expect(abilityInflict(inflicted)).toEqual({ condition: 'poison', turns: 2 })
    expect(setAbilityInflict(inflicted, '')).not.toHaveProperty('inflict')
    expect(abilityExtraProblems(d, inflicted).some((p) => p.includes('does not exist'))).toBe(true)
    const summon = { ...inflicted, target: 'cell' as const, effect: { summon: { type: 'x' } } }
    expect(abilityExtraProblems(d, summon).some((p) => p.includes('needs a unit'))).toBe(true)
  })
  it('a staff keeps xpMiss only while it has text', () => {
    const d = loadStavesFixture()
    const id = d.items!.find((i) => i.kind === 'staff')!.id
    const one = setStaffXpMiss(d, id, '2')
    expect(xpMissText(one.items!.find((i) => i.id === id)!)).toBe('2')
    expect(one.items!.find((i) => i.id === id)).toHaveProperty('xpMiss', 2)
    expect(setStaffXpMiss(one, id, '').items!.find((i) => i.id === id)).not.toHaveProperty('xpMiss')
  })
})

describe('story lists', () => {
  it('keeps the scene presentation commands last and appends the new kinds', () => {
    const last = COMMAND_KINDS.slice(-8)
    expect(last).toEqual(['background', 'fade', 'wait', 'shake', 'leave', 'clearStage', 'weather', 'narrate'])
    expect(COMMAND_KINDS).toContain('showUnit')
    expect(CONDITION_KINDS.slice(-3)).toEqual(['rosterAlive', 'rosterDead', 'inRoster'])
  })
})
