// E3: the Abilities model (docs/SRPG.md "Menu abilities and summoning", "Editor"): what each effect needs, that the target
// follows the effect, that defaults are never written, the plain summary, the conditions rows and the editor-side checks.
import { describe, expect, it } from 'vitest'
import {
  TARGETS_FOR, addAbility, duplicateAbility, effectKind, moveAbility, setAfterMove, setCooldown, setCost, setEffectAmount, setEffectKind, setRange,
  setSummon, setTarget, setUses,
} from '../../src/model/abilities'
import { abilityChecks } from '../../src/model/abilityChecks'
import { COND_KINDS, changeKind, condKind, conditionText, newCondition, setWho } from '../../src/model/abilityConditions'
import { abilitySummary } from '../../src/model/abilityText'
import type { Ability } from '../../src/model/srpgTypes'
import { deleteAbility, abilityUsageLines } from '../../src/model/srpgRefs'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => switchOnFeature(loadDemo('demo-srpg'), 'abilities')
const first = (d: ProjectDoc): Ability => d.abilities![0]

describe('abilities: the list', () => {
  it('a new ability pushes a neighbour one square and is valid as it stands', () => {
    const d = addAbility(base(), 'Shove')
    expect(first(d)).toEqual({ id: 'shove', name: 'Shove', target: 'unit', effect: { push: 1 } })
    expect(abilityChecks(d).filter((c) => !c.text.includes('nobody has'))).toEqual([])
  })
  it('duplicates after the original with a new id, moves, and deletes with everything that names it', () => {
    let d = addAbility(addAbility(base(), 'Shove'), 'Heal')
    d = duplicateAbility(d, 'shove')
    expect(d.abilities!.map((a) => a.id)).toEqual(['shove', 'shove_copy', 'heal'])
    d = moveAbility(d, 2, 0)
    expect(d.abilities![0].id).toBe('heal')
    const unit = d.units![0]
    d = { ...d, units: [{ ...unit, abilities: ['shove'] }, ...d.units!.slice(1)] }
    expect(abilityUsageLines(d, 'shove')).toHaveLength(1)
    d = deleteAbility(d, 'shove')
    expect(d.units![0].abilities).toBeUndefined()
  })
})

describe('abilities: target and effect stay a pair the loader accepts', () => {
  const kinds = Object.keys(TARGETS_FOR) as Array<keyof typeof TARGETS_FOR>
  it('every effect starts valid, with a target it accepts', () => {
    let d = addAbility(base(), 'Thing')
    for (const k of kinds) {
      const a = setEffectKind(d, first(d), k)
      d = { ...d, abilities: [a] }
      expect(effectKind(a)).toBe(k)
      if (k === 'strike') expect(a.target).toBeUndefined()  // the weapon gives a strike its target
      else expect(TARGETS_FOR[k]).toContain(a.target)
    }
  })
  it('changing the effect keeps the target when it still fits and repairs it when not', () => {
    const d = addAbility(base(), 'Thing')
    const heal = setEffectKind(d, first(d), 'heal')
    expect(heal.target).toBe('unit')
    const summon = setEffectKind(d, heal, 'summon')
    expect(summon.target).toBe('cell')
    expect(summon.range).toEqual([1, 2])
    const back = setEffectKind(d, summon, 'swap')
    expect(back.target).toBe('ally')
    expect(back.range).toBeUndefined()
  })
  it('a target the effect refuses is not accepted', () => {
    const d = addAbility(base(), 'Thing')
    expect(setTarget(first(d), 'cell')).toBe(first(d))
    expect(setTarget(first(d), 'enemy').target).toBe('enemy')
    const heal = setEffectKind(d, first(d), 'heal')
    expect(setTarget(heal, 'enemy')).toBe(heal)
    expect(setTarget(heal, 'self').target).toBe('self')
  })
  it('range: written only when it is not [1, 1], never for moves or for "itself", and min never exceeds max', () => {
    const d = addAbility(base(), 'Thing')
    const heal = setEffectKind(d, first(d), 'heal')
    expect(setRange(heal, 1, 3).range).toEqual([1, 3])
    expect(setRange(heal, 1, 1).range).toBeUndefined()
    expect(setRange(heal, 3, 2).range).toEqual([3, 3])
    expect(setRange(setTarget(heal, 'self'), 1, 3).range).toBeUndefined()
    expect(setRange(first(d), 1, 3).range).toBeUndefined()
    expect(setRange(heal, 1, 99).range).toEqual([1, 20])
  })
  it('amounts are kept inside the loader’s limits', () => {
    const d = addAbility(base(), 'Thing')
    expect(setEffectAmount(first(d), 9).effect).toEqual({ push: 5 })
    expect(setEffectAmount(first(d), 0).effect).toEqual({ push: 1 })
    const heal = setEffectKind(d, first(d), 'heal')
    expect(setEffectAmount(heal, 5000).effect).toEqual({ heal: 999 })
    expect(setEffectAmount(heal, 'full').effect).toEqual({ heal: 'full' })
  })
})

describe('abilities: defaults are never written', () => {
  it('cost, uses, wait and "only before moving"', () => {
    const d = addAbility(base(), 'Thing')
    let a = setCost(first(d), true)
    expect(a.cost).toBe('free')
    expect(setCost(a, false)).not.toHaveProperty('cost')
    a = setUses(setCooldown(setAfterMove(a, false), 3), 500)
    expect(a).toMatchObject({ uses: 99, cooldown: 3, afterMove: false })
    a = setAfterMove(a, true)
    expect(a).not.toHaveProperty('afterMove')
    expect(setUses(a, undefined)).not.toHaveProperty('uses')
  })
  it('a summon writes only what differs from the engine’s defaults, and unknown keys stay', () => {
    const d = addAbility(base(), 'Call')
    const unit = d.units![0].id
    let a = setEffectKind(d, first(d), 'summon')
    expect(a.effect).toEqual({ summon: { type: unit } })
    a = setSummon(a, { level: 4, limit: 2, vanish: false, lifetime: 3, canAct: true, givesXp: false })
    expect(a.effect!.summon).toEqual({ type: unit, level: 4, limit: 2, vanish: false, lifetime: 3, canAct: true, givesXp: false })
    a = setSummon(a, { level: 'summoner', limit: 1, vanish: true, lifetime: 0, canAct: false, givesXp: true })
    expect(a.effect!.summon).toEqual({ type: unit })
    const odd: Ability = { ...a, effect: { summon: { type: unit, mystery: 7 } } }
    expect(setSummon(odd, { limit: 3 }).effect!.summon).toEqual({ type: unit, mystery: 7, limit: 3 })
  })
})

describe('abilities: the summary sentence', () => {
  it('says what it does, the cost and the limits in plain words', () => {
    const d = addAbility(base(), 'Shove')
    expect(abilitySummary(d, first(d))).toBe('Pushes a unit next to it 1 square away; uses the unit’s action.')
    const a = setUses(setCooldown(setCost(setEffectAmount(first(d), 2), true), 2), 2)
    expect(abilitySummary(d, a)).toBe('Pushes a unit next to it 2 squares away; free (the unit can still act); 2 times per battle; then waits 2 turns.')
  })
  it('a summon names its unit type and the square', () => {
    const d = addAbility(base(), 'Call')
    const a = setSummon(setEffectKind(d, first(d), 'summon'), { limit: 2 })
    expect(abilitySummary(d, a)).toMatch(/^Summons up to 2 .+ on a free square 1 to 2 squares away; uses the unit’s action\.$/)
  })
  it('adds its conditions', () => {
    const d = addAbility(base(), 'Shove')
    expect(abilitySummary(d, { ...first(d), if: [{ hp: { op: '>', percent: 50 } }] })).toMatch(/Only if its HP is above 50%\.$/)
  })
})

describe('abilities: "Only if" rows', () => {
  it('each kind starts as a row the loader accepts and says its words', () => {
    for (const k of COND_KINDS) {
      const c = newCondition(k)
      expect(condKind(c)).toBe(k)
      expect(conditionText(c)).not.toMatch(/another condition/)
    }
    expect(conditionText({ formula: 'a.level > 3' })).toMatch(/kept as it is/)
  })
  it('"the target" is kept across kinds that can have it and dropped for the others', () => {
    const foe = setWho(newCondition('hp'), true)
    expect(foe.who).toBe('foe')
    expect(changeKind(foe, 'unit').who).toBe('foe')
    expect(changeKind(foe, 'round')).not.toHaveProperty('who')
    expect(setWho(foe, false)).not.toHaveProperty('who')
    expect(conditionText(foe)).toBe('the target’s HP is at most 50%')
  })
})

describe('abilities: editor-side checks', () => {
  it('an ability nobody has is flagged until a unit type, a character or a skill hands it out', () => {
    let d = addAbility(base(), 'Shove')
    expect(abilityChecks(d).map((c) => c.text)).toEqual(['Shove: nobody has this ability yet. Give it to a unit type on the Units screen.'])
    const u = d.units![0]
    expect(abilityChecks({ ...d, units: [{ ...u, abilities: ['shove'] }, ...d.units!.slice(1)] })).toEqual([])
    d = { ...d, skills: [{ id: 's', name: 'S', blocks: [{ when: 'always', do: [{ ability: 'shove' }] }] }] }
    expect(abilityChecks(d)).toEqual([])
  })
  it('says the loader’s refusals in plain words', () => {
    const d = addAbility(base(), 'Call')
    const bad = (a: Ability) => abilityChecks({ ...d, abilities: [a] }).map((c) => c.text).filter((t) => !t.includes('nobody has'))
    expect(bad({ id: 'x', name: 'X', target: 'cell', effect: { push: 1 } })[0]).toMatch(/needs a unit next to it/)
    expect(bad({ id: 'x', name: 'X', target: 'enemy', effect: { heal: 5 } })[0]).toMatch(/healing cannot be used/)
    expect(bad({ id: 'x', name: 'X', target: 'unit', effect: { summon: { type: 'nope' } } })[0]).toMatch(/needs a free square/)
    expect(bad({ id: 'x', name: 'X', target: 'cell', effect: { summon: { type: 'nope' } } })[0]).toMatch(/“nope”\) is not a unit type/)
    expect(bad({ id: 'x', name: 'X', target: 'cell', effect: { summon: {} } })[0]).toMatch(/choose which unit type/)
    expect(bad({ id: 'x', name: 'X', target: 'unit', effect: {} })[0]).toMatch(/does not say what it does/)
    expect(bad({ id: 'x', name: 'X', target: 'unit', range: [1, 2], effect: { swap: true } })[0]).toMatch(/right next to it/)
  })
})
