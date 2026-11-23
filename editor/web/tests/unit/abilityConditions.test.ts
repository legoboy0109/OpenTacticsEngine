// Ability conditions in the editor (docs/SRPG.md "Ability conditions"): a condition's kind, cure lists by kind (abilities, usable items, staves, skill
// blocks), a cure ability on an enemy, the chance of an ability's inflict. The checks must say what the loader refuses; edits write no defaults and
// keep keys they do not show. Uses the engine's fixture tests/fixtures/srpg-conditions.
import { describe, expect, it } from 'vitest'
import { setCure as setAbilityCure, setTarget } from '../../src/model/abilities'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilitySummary } from '../../src/model/abilityText'
import { canBeCured, canBeInflicted, conditionChecks, kindOf, siteChecks } from '../../src/model/conditionChecks'
import { setConditionKind } from '../../src/model/conditions'
import { itemCureOf, setItemCureMode } from '../../src/model/conditionSites'
import { abilityExtraProblems, abilityInflict, setAbilityInflict, setAbilityInflictChance } from '../../src/model/fidelityAbility'
import { ruleIssues } from '../../src/model/skillChecks'
import { staffChecks } from '../../src/model/staffChecks'
import { cureOf, setCure as setStaffCure } from '../../src/model/staffConditions'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { Ability, Condition } from '../../src/model/srpgTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadConditionsFixture } from './fixtures'

const fx = (): ProjectDoc => switchOnFeature(loadConditionsFixture(), 'abilities')
const cond = (d: ProjectDoc, id: string): Condition => d.conditions!.find((c) => c.id === id)!
const withAbility = (d: ProjectDoc, a: Ability): ProjectDoc => ({ ...d, abilities: [...(d.abilities ?? []), a], units: d.units?.map((u, i) => (i === 0 ? { ...u, abilities: [...(u.abilities ?? []), a.id] } : u)) })
const texts = (d: ProjectDoc, id: string) => abilityChecks(d).filter((c) => c.ids?.includes(id)).map((c) => c.text)

describe('a condition’s kind', () => {
  it('is harmful unless it says helpful, and only helpful is written', () => {
    let d = fx()
    const id = d.conditions![0].id
    expect(kindOf(d, id)).toBe('harmful')
    d = setConditionKind(d, id, 'helpful')
    expect(cond(d, id).kind).toBe('helpful')
    expect(kindOf(d, id)).toBe('helpful')
    d = setConditionKind(d, id, 'harmful')
    expect('kind' in cond(d, id)).toBe(false)
  })
  it('a kind that is neither is refused, as the loader does', () => {
    const d = fx()
    const id = d.conditions![0].id
    const bad = { ...d, conditions: d.conditions!.map((c) => (c.id === id ? ({ ...c, kind: 'good' } as unknown as Condition) : c)) }
    expect(conditionChecks(bad).filter((c) => c.blocks).map((c) => c.text).join(' ')).toMatch(/kind must be harmful/)
  })
})

describe('cure lists by kind', () => {
  it('an ability may cure every harmful or helpful condition, on a foe too', () => {
    const d0 = fx()
    let a: Ability = { id: 'esuna', name: 'Esuna', target: 'ally', range: [1, 3], effect: { cure: 'all' } }
    a = setAbilityCure(a, 'harmful')
    expect(a.effect).toEqual({ cure: 'harmful' })
    a = setTarget(a, 'enemy')
    expect(a.target).toBe('enemy')  // a dispel (the loader takes a cure on an enemy)
    const d = withAbility(d0, a)
    expect(texts(d, 'esuna')).toEqual([])
    expect(abilitySummary(d, a)).toMatch(/every harmful condition/)
    const dispel = setAbilityCure(a, 'helpful')
    expect(abilitySummary(withAbility(d0, dispel), dispel)).toMatch(/takes every helpful condition away/i)
    const onCell = { ...a, target: 'cell' as const }
    expect(texts(withAbility(d0, onCell), 'esuna').join(' ')).toMatch(/curing works on a unit, not a square/)
  })
  it('a usable item: two more modes, written as the words', () => {
    const d0 = fx()
    const usable = d0.items!.find((i) => i.kind === 'usable')!
    let d = setItemCureMode(d0, usable.id, 'harmful')
    expect(d.items!.find((i) => i.id === usable.id)!.cure).toBe('harmful')
    expect(itemCureOf(d.items!.find((i) => i.id === usable.id)!).mode).toBe('harmful')
    expect(siteChecks(d).filter((c) => c.blocks)).toEqual([])
    d = setItemCureMode(d, usable.id, 'none')
    expect(d.items!.find((i) => i.id === usable.id)!.cure).toBeUndefined()
  })
  it('a staff: read and written as a kind, no list shown', () => {
    const d0 = fx()
    const staff = d0.items!.find((i) => i.kind === 'staff' && cureOf(i).on)!
    const d = setStaffCure(d0, staff.id, 'helpful')
    const c = cureOf(d.items!.find((i) => i.id === staff.id)!)
    expect(c).toMatchObject({ on: true, all: false, kind: 'helpful', editable: true })
    expect(staffChecks(d).filter((x) => x.blocks)).toEqual([])
  })
  it('a skill block’s cure takes the kinds', () => {
    const d = fx()
    expect(ruleIssues(d, { when: 'turnStart', do: [{ cure: 'harmful' }] })).toEqual([])
    expect(ruleIssues(d, { when: 'turnStart', do: [{ cure: 'cursed' }] }).join(' ')).toMatch(/cures nothing yet/)
  })
  it('a condition cured by a kind is not "never cured", and an ability that inflicts it counts', () => {
    let d = fx()
    const until = d.conditions!.find((c) => c.until === 'cured')!
    const before = canBeCured(d, until.id)
    d = { ...d, items: (d.items ?? []).filter((i) => !(i.kind === 'usable' && i.cure !== undefined) && !(i.kind === 'staff' && cureOf(i).on)) }
    d = { ...d, conditions: d.conditions!.map((c) => ({ ...c, replaces: undefined })) }
    if (!before || canBeCured(d, until.id)) return  // (the fixture cures it some other way too: nothing to compare)
    d = withAbility(d, { id: 'esuna', name: 'Esuna', target: 'ally', range: [1, 3], effect: { cure: 'harmful' } })
    expect(canBeCured(d, until.id)).toBe(true)
    const none = d.conditions!.find((c) => !canBeInflicted(fx(), c.id))
    if (none) expect(canBeInflicted(withAbility(fx(), { id: 'x', name: 'X', target: 'enemy', range: [1, 1], inflict: { condition: none.id } }), none.id)).toBe(true)
  })
})

describe('the chance of an ability’s condition', () => {
  const sleepy = (d: ProjectDoc): Ability => ({ id: 'lull', name: 'Lull', target: 'enemy', range: [1, 3], inflict: { condition: d.conditions![0].id } })
  it('a number or a formula, empty removes it, the condition and turns keep it', () => {
    const d = fx()
    let a = setAbilityInflictChance(sleepy(d), '60')
    expect(abilityInflict(a)?.chance).toBe(60)
    a = setAbilityInflictChance(a, 'a.mag + 40 - b.res')
    expect(abilityInflict(a)?.chance).toBe('a.mag + 40 - b.res')
    a = setAbilityInflict(a, d.conditions![0].id, undefined)
    expect(abilityInflict(a)?.chance).toBe('a.mag + 40 - b.res')  // kept when the condition is set again
    a = setAbilityInflictChance(a, '')
    expect('chance' in (a.inflict as object)).toBe(false)
    expect(abilityExtraProblems(d, a)).toEqual([])
  })
  it('out of range is refused, as the loader does', () => {
    const d = fx()
    const bad = { ...sleepy(d), inflict: { condition: d.conditions![0].id, chance: 0 } }
    expect(abilityExtraProblems(d, bad).join(' ')).toMatch(/percent from 1 to 100/)
    const bad2 = { ...sleepy(d), inflict: { condition: d.conditions![0].id, chance: 101 } }
    expect(abilityExtraProblems(d, bad2).join(' ')).toMatch(/percent from 1 to 100/)
    const ok = { ...sleepy(d), inflict: { condition: d.conditions![0].id, chance: 100 } }
    expect(abilityExtraProblems(d, ok)).toEqual([])
  })
})
