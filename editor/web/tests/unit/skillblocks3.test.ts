// The third set of skill blocks (docs/SRPG.md "Skill blocks, third set"): what the block model offers, what its checks say (they mirror the loader's
// refusals), the sentence a rule adds up to, and the item / ability / staff keys that came with it.
import { describe, expect, it } from 'vitest'
import { stealOf, setSteal } from '../../src/model/abilities'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilitySummary } from '../../src/model/abilityText'
import { itemChecks } from '../../src/model/itemChecks'
import { setItemKind, setItemNumber, setStatNumber } from '../../src/model/items'
import { auraOf, newCondition, newEffect, offeredConditions, offeredEffects, setAura, setWhen } from '../../src/model/skillBlocks'
import { ruleIssues } from '../../src/model/skillChecks'
import { condsFor, effectsFor } from '../../src/model/skillRules'
import { blockSummary } from '../../src/model/skillText'
import { lightOf, setLightOn, setLightShrink, setLightRadius } from '../../src/model/staffEffects'
import { staffChecks } from '../../src/model/staffChecks'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { Ability } from '../../src/model/srpgTypes'
import type { Extra, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => {
  const d = switchOnFeature(loadDemo('demo-srpg'), 'skills')
  return {
    ...d,
    weapons: (d.weapons ?? []).map((w, i) => (i === 0 ? { ...w, damageKind: 'magic', versus: { ...(w.versus ?? {}), armored: 2 } } : w)),
  }
}
const issues = (b: object, d = base()) => ruleIssues(d, b as never)

describe('aura receiver tests', () => {
  it('auraOf reads the keys it knows and setAura writes only the ones that are set', () => {
    expect(auraOf({ to: 'enemies', within: 3, weapon: 'sword', test: '' })).toEqual({ to: 'enemies', within: 3, weapon: 'sword' })
    const d = base()
    const b = setAura({ when: 'combat', do: [{ damage: 2 }] }, { to: 'allies', within: 2, weapon: d.weapons![0].id, weaponKind: 'magic', test: 'b.level >= 2' }, d)
    expect(b.aura).toEqual({ to: 'allies', within: 2, weapon: d.weapons![0].id, weaponKind: 'magic', test: 'b.level >= 2' })
    const cleared = setAura(b, { to: 'allies', within: 2, weapon: undefined, weaponKind: undefined, test: undefined }, d)
    expect(cleared.aura).toEqual({ to: 'allies', within: 2 })
  })
  it('checks name an unknown weapon, kind or an empty formula', () => {
    const d = base()
    const at = (aura: object) => issues({ when: 'combat', aura, do: [{ damage: 1 }] }, d)
    expect(at({ to: 'allies', within: 2, weapon: d.weapons![0].id, weaponKind: 'magic', test: 'b.level >= 2' })).toEqual([])
    expect(at({ to: 'allies', within: 2, weapon: 'lance of nothing' })[0]).toMatch(/no weapon or weapon group/)
    expect(at({ to: 'allies', within: 2, weaponKind: 'ice' })[0]).toMatch(/which no weapon has/)
    expect(at({ to: 'allies', within: 2, test: '  ' })[0]).toMatch(/formula the receivers must pass is empty/)
  })
  it('the sentence says who gets it', () => {
    const d = base()
    const text = blockSummary(d, { when: 'combat', aura: { to: 'allies', within: 3, weaponKind: 'magic', test: 'b.str >= 3' }, do: [{ hit: 10 }] })
    expect(text).toContain('allies that have a weapon dealing magic damage and pass the formula b.str >= 3 within 3 squares get hit +10')
  })
})

describe('the strike condition', () => {
  it('is offered in strike and struck rules only and its checks mirror the loader', () => {
    const d = base()
    expect(condsFor('strike', false)).toContain('strike')
    expect(condsFor('struck', false)).toContain('strike')
    expect(condsFor('combat', false)).not.toContain('strike')
    expect(offeredConditions(d, { when: 'strike' })).toContain('strike')
    expect(newCondition('strike', d)).toEqual({ strike: 'first' })
    for (const strike of ['first', 'followUp', 3, { op: '>=', value: 2 }]) expect(issues({ when: 'strike', if: [{ strike }], do: [{ damage: 2 }] })).toEqual([])
    expect(issues({ when: 'combat', if: [{ strike: 'first' }], do: [{ damage: 2 }] })[0]).toMatch(/strikes or is struck/)
    expect(issues({ when: 'strike', if: [{ strike: 0 }], do: [{ damage: 2 }] })[0]).toMatch(/the first, a follow-up, or a number/)
    expect(issues({ when: 'strike', if: [{ strike: 'last' }], do: [{ damage: 2 }] })[0]).toMatch(/the first, a follow-up, or a number/)
    // Changing the rule to a combat rule drops the condition it can no longer ask.
    expect(setWhen({ when: 'strike', if: [{ strike: 'first' }], do: [{ damage: 3 }] }, 'combat', d).if).toBeUndefined()
  })
  it('the sentences', () => {
    const d = base()
    expect(blockSummary(d, { when: 'strike', if: [{ strike: 'first' }], do: [{ damage: 5 }] })).toBe('When it strikes, if it is the first strike of the fight: damage +5.')
    expect(blockSummary(d, { when: 'strike', if: [{ strike: 'followUp' }], do: [{ damage: 5 }] })).toContain('a follow-up strike')
    expect(blockSummary(d, { when: 'struck', if: [{ strike: 3 }], do: [{ damageTaken: -1 }] })).toContain('strike number 3')
    expect(blockSummary(d, { when: 'strike', if: [{ strike: { op: '>=', value: 2 } }], do: [{ damage: 1 }] })).toContain('the strike number is at least 2')
  })
})

describe('recoil', () => {
  it('belongs to strike rules, with its own amount forms', () => {
    const d = base()
    expect(effectsFor('strike', false)).toContain('recoil')
    expect(effectsFor('combat', false)).not.toContain('recoil')
    expect(offeredEffects(d, { when: 'strike' })).toContain('recoil')
    expect(newEffect('recoil', d, 'strike')).toEqual({ recoil: 2 })
    for (const recoil of [3, 'w.might', 'dealt / 2', { instead: true }, { amount: 'dealt * 2', instead: true }]) expect(issues({ when: 'strike', do: [{ recoil }] })).toEqual([])
    expect(issues({ when: 'strike', do: [{ recoil: 0 }] })[0]).toMatch(/whole number of 1 or more/)
    expect(issues({ when: 'strike', do: [{ recoil: ' ' }] })[0]).toMatch(/formula is empty/)
    expect(issues({ when: 'strike', do: [{ recoil: { instead: 3 } }] })[0]).toMatch(/either on or off/)
    expect(issues({ when: 'combat', do: [{ recoil: 2 }] })[0]).toMatch(/cannot be used/)
  })
  it('the sentences', () => {
    const d = base()
    expect(blockSummary(d, { when: 'strike', chance: 30, do: [{ recoil: 4 }] })).toContain('the wielder loses 4 HP (it can be defeated)')
    expect(blockSummary(d, { when: 'strike', do: [{ recoil: { instead: true } }] })).toContain('turns on its wielder, who takes an amount from the formula dealt instead of the foe')
  })
})

describe('cancelEffective with tags', () => {
  it('lists the tags weapons use under versus', () => {
    const d = base()
    expect(issues({ when: 'always', do: [{ cancelEffective: true }] })).toEqual([])
    expect(issues({ when: 'always', do: [{ cancelEffective: ['armored'] }] }, d)).toEqual([])
    expect(issues({ when: 'always', do: [{ cancelEffective: ['dragon'] }] }, d)[0]).toMatch(/no effectiveness tag/)
    expect(issues({ when: 'always', do: [{ cancelEffective: [] }] }, d)[0]).toMatch(/cancels nothing yet/)
    expect(issues({ when: 'always', do: [{ cancelEffective: false }] }, d)[0]).toMatch(/either on or not there/)
    expect(blockSummary(d, { when: 'always', do: [{ cancelEffective: ['armored'] }] })).toContain('effective against armored')
  })
})

describe('healed in a staffUse formula', () => {
  it('a staffUse heal may be a formula and the help names healed', () => {
    const d = base()
    expect(issues({ when: 'staffUse', do: [{ heal: 'healed / 2' }] }, d)).toEqual([])
    expect(blockSummary(d, { when: 'staffUse', do: [{ heal: 'healed / 2' }] })).toContain('an amount from the formula healed / 2')
  })
})

describe('steal reads the item', () => {
  const steal = (s: object): Ability => ({ id: 'pick', name: 'Pick', target: 'enemy', effect: { steal: s } } as never)
  it('keeps the reason only next to a formula', () => {
    let a = steal({})
    a = setSteal(a, { if: 'it.weight < a.str', reason: 'too heavy' })
    expect(stealOf(a)).toEqual({ if: 'it.weight < a.str', reason: 'too heavy' })
    a = setSteal(a, { if: '' })
    expect(a.effect?.steal).toBe(true)  // the reason went with the formula
    expect(abilitySummary(base(), steal({ if: 'it.weight < a.str', reason: 'too heavy' }))).toContain('otherwise “too heavy”')
  })
  it('checks refuse a reason with no formula or an empty one', () => {
    const d = { ...base(), abilities: [steal({ reason: 'no' }), steal({ if: 'it.weight < 3', reason: ' ' })] }
    const out = abilityChecks(d).map((c) => c.text)
    expect(out.some((t) => /goes with an “only if” formula/.test(t))).toBe(true)
    expect(out.some((t) => /a few words, or left out/.test(t))).toBe(true)
  })
})

describe('item numbers and a staff bonus', () => {
  it('a usable item has props; switching to a weapon drops them', () => {
    let d = base()
    const id = (d.items ?? []).find((i) => i.kind === 'usable')!.id
    d = setItemNumber(d, id, 'weight', 2)
    expect(d.items!.find((i) => i.id === id)!.props).toEqual({ weight: 2 })
    d = setItemKind(d, id, 'weapon')
    expect(d.items!.find((i) => i.id === id)!.props).toBeUndefined()
  })
  it('a weapon item with props is a check; an empty staff bonus is too', () => {
    const d = base()
    const weaponItem = (d.items ?? []).find((i) => i.kind === 'weapon')
    if (weaponItem) {
      const bad = { ...d, items: d.items!.map((i) => (i.id === weaponItem.id ? { ...i, props: { weight: 1 } } : i)) }
      expect(itemChecks(bad).some((c) => /belong on the Weapons screen/.test(c.text))).toBe(true)
    }
    const staff = (d.items ?? []).find((i) => i.kind === 'staff')
    if (staff) {
      const empty = { ...d, items: d.items!.map((i) => (i.id === staff.id ? { ...i, bonus: { [d.game!.stats![0].id]: 0 } } : i)) }
      expect(itemChecks(empty).some((c) => /changes no stat/.test(c.text))).toBe(true)
      const set = setStatNumber(d, staff.id, 'bonus', d.game!.stats![1].id, 2)
      expect(set.items!.find((i) => i.id === staff.id)!.bonus).toEqual({ [d.game!.stats![1].id]: 2 })
    }
  })
})

describe('a light that shrinks', () => {
  const withLight = (): { d: ProjectDoc; id: string } => {
    const d0 = base()
    const d = { ...d0, items: [...(d0.items ?? []), { id: 'torch', name: 'Torch', kind: 'staff', group: 'staff', uses: 3, range: 6, effects: [{ heal: 5 }] } as never] }
    const staff = d.items.find((i) => i.kind === 'staff')!
    return { d: setLightRadius(setLightOn(d, staff.id, true), staff.id, 4), id: staff.id }
  }
  it('shrink is written beside radius and turns, kept below the radius, and removed when empty', () => {
    let { d, id } = withLight()
    d = setLightShrink(d, id, 2)
    expect(lightOf(d.items!.find((i) => i.id === id)!)).toMatchObject({ on: true, radius: 4, shrink: 2 })
    d = setLightShrink(d, id, 9)  // too big: kept below the radius
    expect(lightOf(d.items!.find((i) => i.id === id)!).shrink).toBe(3)
    d = setLightShrink(d, id, undefined)
    expect(lightOf(d.items!.find((i) => i.id === id)!).shrink).toBeUndefined()
  })
  it('the check refuses a shrink that is not below the radius', () => {
    const { d, id } = withLight()
    const bad = { ...d, items: d.items!.map((i) => (i.id === id ? { ...i, effects: [{ light: { radius: 3, shrink: 3 } }] as Extra[] } : i)) }
    expect(staffChecks(bad).some((c) => /shrink must be a whole number of 1 or more and less than its radius/.test(c.text))).toBe(true)
    const ok = { ...d, items: d.items!.map((i) => (i.id === id ? { ...i, effects: [{ light: { radius: 3, shrink: 1 } }] as Extra[] } : i)) }
    expect(staffChecks(ok).some((c) => /shrink/.test(c.text))).toBe(false)
  })
})
