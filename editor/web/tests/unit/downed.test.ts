// "Downed units and revive": the game rule (rules.downed), an ability's effect.revive, a usable item's revive, a staff's { revive } effect and a
// condition's autoRevive. The model helpers, every editor check (each mirrors one loader refusal), the sentences, and that a game that does not use
// any of it is read and written as before.
import { describe, expect, it } from 'vitest'
import { effectKind, setEffectKind, setTarget } from '../../src/model/abilities'
import { areaChecks } from '../../src/model/abilityAreaChecks'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilitySummary } from '../../src/model/abilityText'
import { conditionChecks } from '../../src/model/conditionChecks'
import {
  downedAfterBattle, downedRemoveAfter, gameHasDowned, isReviveStaff, reviveAmountProblem, reviveKindOf, reviveStart, reviveWithNumber, reviveWithText,
  setAbilityReviveKind, setAbilityReviveValue, setAutoRevive, setDownedAfterBattle, setDownedOn, setDownedRemoveAfter, setItemRevive, setItemReviveValue,
  setStaffReviveOn, setStaffReviveValue, staffRevive,
} from '../../src/model/downed'
import { downedRuleChecks } from '../../src/model/downedChecks'
import { itemChecks } from '../../src/model/itemChecks'
import { setStaffTarget } from '../../src/model/staff'
import { staffChecks } from '../../src/model/staffChecks'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { Ability } from '../../src/model/srpgTypes'
import type { Item, ProjectDoc } from '../../src/model/types'
import { loadConditionsFixture } from './fixtures'

const base = (): ProjectDoc => switchOnFeature(loadConditionsFixture(), 'abilities')
const on = (d: ProjectDoc): ProjectDoc => setDownedOn(d, true)
const revive = (extra: Partial<Ability> = {}): Ability => ({ id: 'rise', name: 'Rise', target: 'ally', effect: { revive: { percent: 50 } }, ...extra })
const withAbility = (d: ProjectDoc, a: Ability): ProjectDoc => ({ ...d, abilities: [a] })
const probs = (d: ProjectDoc, a: Ability) => abilityChecks(withAbility(d, a)).map((c) => c.text).filter((t) => !t.includes('nobody has'))
const withItem = (d: ProjectDoc, i: Item): ProjectDoc => ({ ...d, items: [i] })
// The fixture's units carry the items the tests replace, so "not an item of this game" notes are noise here.
const itemProbs = (d: ProjectDoc) => itemChecks(d).map((c) => c.text).filter((t) => !t.includes('is not an item of this game'))
const staff = (extra: Partial<Item> = {}): Item => ({ id: 'rod', name: 'Rod', kind: 'staff', uses: 5, group: 'staff', range: 2, effects: [{ revive: 'full' }], ...extra })
const staffProbs = (d: ProjectDoc) => staffChecks(d).filter((c) => c.blocks).map((c) => c.text)
const withCondition = (d: ProjectDoc, c: object): ProjectDoc => ({ ...d, conditions: [{ id: 'ward', name: 'Ward', turns: 3, blocks: [], ...c } as never] })
const condProbs = (d: ProjectDoc) => conditionChecks(d).filter((c) => c.blocks).map((c) => c.text)
const NEEDS = 'it needs the “Downed units” rule to be switched on (Rules screen).'

describe('rules.downed', () => {
  it('is off by default; on writes an empty block, off removes it with everything in it', () => {
    const d = base()
    expect(gameHasDowned(d)).toBe(false)
    expect(downedAfterBattle(d)).toBe('return')
    expect(downedRemoveAfter(d)).toBe(0)
    const o = on(d)
    expect(o.game!.rules!.downed).toEqual({})
    expect(gameHasDowned(o)).toBe(true)
    expect(setDownedOn(o, true)).toBe(o)
    const set = setDownedRemoveAfter(setDownedAfterBattle(o, 'fallen'), 3)
    expect(set.game!.rules!.downed).toEqual({ afterBattle: 'fallen', removeAfter: 3 })
    const off = setDownedOn(set, false)
    expect('downed' in off.game!.rules!).toBe(false)
    expect(JSON.stringify(off.game)).toBe(JSON.stringify(d.game))  // byte for byte as before
  })
  it('defaults are not written; the settings do nothing while the rule is off; other keys stay', () => {
    const o = on(base())
    const odd = { ...o, game: { ...o.game!, rules: { ...o.game!.rules, downed: { future: 1 } } } } as ProjectDoc
    expect(setDownedAfterBattle(odd, 'fallen').game!.rules!.downed).toEqual({ future: 1, afterBattle: 'fallen' })
    expect(setDownedAfterBattle(setDownedAfterBattle(odd, 'fallen'), 'return').game!.rules!.downed).toEqual({ future: 1 })
    expect(setDownedRemoveAfter(setDownedRemoveAfter(o, 5), 0).game!.rules!.downed).toEqual({})
    expect(setDownedRemoveAfter(o, 500).game!.rules!.downed).toEqual({ removeAfter: 99 })
    const d = base()
    expect(setDownedAfterBattle(d, 'fallen')).toBe(d)
    expect(setDownedRemoveAfter(d, 4)).toBe(d)
  })
  it('checks: keys, afterBattle, removeAfter', () => {
    const rule = (downed: unknown): ProjectDoc => { const o = base(); return { ...o, game: { ...o.game!, rules: { ...o.game!.rules, downed } } } as ProjectDoc }
    expect(downedRuleChecks(base())).toEqual([])
    expect(downedRuleChecks(rule({}))).toEqual([])
    expect(downedRuleChecks(rule({ afterBattle: 'fallen', removeAfter: 99 }))).toEqual([])
    expect(downedRuleChecks(rule({ removeAfter: 0 }))).toEqual([])
    expect(downedRuleChecks(rule({ zap: 1 }))).toEqual(['Downed units: there is no setting “zap”. The rule takes “afterBattle” and “removeAfter”.'])
    expect(downedRuleChecks(rule({ afterBattle: 'gone' }))).toHaveLength(1)
    for (const bad of [100, -1, 1.5, '3']) expect(downedRuleChecks(rule({ removeAfter: bad }))).toHaveLength(1)
    expect(downedRuleChecks(rule(true))).toHaveLength(1)
  })
})

describe('the amount of HP', () => {
  it('kinds, starts and numbers', () => {
    expect(['full', { percent: 5 }, 'a.mag', 7].map(reviveKindOf)).toEqual(['full', 'percent', 'formula', 'amount'])
    expect(reviveStart('amount')).toBe(10)
    expect(reviveStart('percent')).toEqual({ percent: 50 })
    expect(reviveWithNumber('percent', 500)).toEqual({ percent: 100 })
    expect(reviveWithNumber('amount', 0)).toBe(1)
    expect(reviveWithNumber('amount', 5000)).toBe(999)
  })
  it('the loader’s rules', () => {
    for (const ok of [1, 999, 'full', { percent: 1 }, { percent: 100 }]) expect(reviveAmountProblem(ok, false)).toBeNull()
    for (const bad of [0, 1000, 1.5, 'a.mag', { percent: 0 }, { percent: 101 }, { percent: 5, x: 1 }, [], true]) expect(reviveAmountProblem(bad, false)).toBe('bad')
    expect(reviveAmountProblem('a.mag * 2', true)).toBeNull()
    expect(reviveAmountProblem('most', true)).toBe('bare')
    expect(reviveAmountProblem('', true)).toBe('bare')
    expect(reviveAmountProblem({ percent: 0 }, true)).toBe('bad')
  })
  it('sentence parts', () => {
    expect(reviveWithText({ percent: 50 })).toBe('with 50% of its HP')
    expect(reviveWithText(10)).toBe('with 10 HP')
    expect(reviveWithText('full')).toBe('with all its HP')
    expect(reviveWithText('a.mag')).toBe('')
  })
})

describe('an ability with revive', () => {
  const d = on(base())
  it('is a kind of its own; changing to it picks an ally; a revive alone is clean', () => {
    expect(effectKind(revive())).toBe('revive')
    const a = setEffectKind(d, { id: 'x', name: 'X', target: 'enemy', effect: { push: 1 } }, 'revive')
    expect(a.effect).toEqual({ revive: { percent: 50 } })
    expect(a.target).toBe('ally')
    expect(setTarget(revive(), 'enemy').target).toBe('ally')  // the picker does not offer it
    expect(probs(d, revive())).toEqual([])
    expect(probs(d, revive({ target: 'unit' }))).toEqual([])
  })
  it('the kind and value setters', () => {
    const a = setAbilityReviveKind(revive(), 'full')
    expect(a.effect).toEqual({ revive: 'full' })
    expect(setAbilityReviveKind(a, 'formula').effect).toEqual({ revive: 'a.mag * 2' })
    expect(setAbilityReviveValue(a, 10).effect).toEqual({ revive: 10 })
    expect(setAbilityReviveKind({ id: 'h', name: 'H', effect: { heal: 5 } }, 'full').effect).toEqual({ heal: 5 })
  })
  it('needs the rule', () => {
    expect(probs(base(), revive())).toEqual([`Rise: ${NEEDS}`])
  })
  it('is the only effect', () => {
    expect(probs(d, revive({ effect: { revive: 'full', heal: 5 } }))).toEqual(['Rise: reviving must be the only thing it does. It cannot go together with healing, curing, pushing and the like.'])
    expect(probs(d, revive({ effect: { revive: 'full', push: 1 } }))).toHaveLength(1)
    expect(probs(d, revive({ effect: { revive: 'full', cure: 'all' } }))).toHaveLength(1)
  })
  it('the target is an ally or any unit', () => {
    const msg = 'Rise: reviving works on an ally or any unit (a downed unit), not on an enemy, itself or a square.'
    for (const t of ['enemy', 'self', 'cell'] as const) expect(probs(d, revive({ target: t }))).toContain(msg)
    expect(probs(d, revive({ target: undefined }))).toEqual([])  // the default is "unit"
  })
  it('no inflict, no charge', () => {
    expect(probs(d, revive({ inflict: { condition: 'poison' } }))).toEqual(['Rise: reviving cannot also give a condition. Make a second ability for the condition.'])
    expect(probs(d, { ...revive(), chargeSpeed: 3 } as Ability)).toEqual(['Rise: reviving cannot be a charged ability.'])
  })
  it('the amount: number, full, percent, formula; a bare word is refused', () => {
    for (const ok of [1, 999, 'full', { percent: 100 }, 'a.mag * 2']) expect(probs(d, revive({ effect: { revive: ok } }))).toEqual([])
    for (const bad of [0, 1000, { percent: 0 }, { percent: 101 }, 2.5, true as never])
      expect(probs(d, revive({ effect: { revive: bad } }))).toEqual(['Rise: reviving must give back a whole number from 1 to 999, “full” or a percent from 1 to 100, or a formula.'])
    expect(probs(d, revive({ effect: { revive: 'most' } }))).toEqual(['Rise: “most” is not a formula. Write “full”, or a formula that reads the units, such as “a.mag * 2”.'])
  })
  it('an area is allowed (the area check accepts a revive, and still refuses a push)', () => {
    const withArea = revive({ area: { shape: 'burst' } })
    expect(areaChecks(d, withArea)).toEqual([])
    expect(probs(d, withArea)).toEqual([])
    expect(probs(d, revive({ area: { shape: 'radius', radius: 2 }, range: [0, 3] }))).toEqual([])
    expect(areaChecks(d, { id: 'p', name: 'P', target: 'ally', effect: { push: 1 }, area: { shape: 'burst' } }))
      .toEqual(['P: an area goes with healing, curing (or both), reviving or an ability that only gives a condition. It cannot push, pull, swap, summon, unlock, steal or refresh.'])
  })
  it('the sentence', () => {
    const t = (a: Ability) => abilitySummary(d, a)
    expect(t(revive())).toBe('Revives a downed ally next to it with 50% of its HP; uses the unit’s action.')
    expect(t(revive({ target: 'unit', effect: { revive: 10 } }))).toContain('Revives a downed unit next to it with 10 HP')
    expect(t(revive({ effect: { revive: 'full' } }))).toContain('with all its HP')
    expect(t(revive({ effect: { revive: 'a.mag' } }))).toMatch(/^Revives a downed ally next to it;/)
    expect(t(revive({ area: { shape: 'burst' } }))).toContain('Revives every downed ally (itself too) in the square it aims at')
  })
})

describe('a usable item with revive', () => {
  const item = (extra: Partial<Item> = {}): Item => ({ id: 'phoenix', name: 'Phoenix', kind: 'usable', uses: 1, target: 'ally', revive: { percent: 50 }, ...extra })
  const d = on(base())
  it('switching on takes away what it cannot go with and uses an ally; off goes back to a small heal', () => {
    const start: Item = { id: 'phoenix', name: 'Phoenix', kind: 'usable', uses: 1, heal: 10, raise: { str: 1 }, cure: 'all', promotes: true, target: 'self' }
    const o = setItemRevive(withItem(d, start), 'phoenix', true)
    expect(o.items![0]).toEqual({ id: 'phoenix', name: 'Phoenix', kind: 'usable', uses: 1, target: 'ally', revive: { percent: 50 } })
    expect(setItemRevive(o, 'phoenix', true)).toEqual(o)
    expect(setItemReviveValue(o, 'phoenix', 'full').items![0].revive).toBe('full')
    expect(setItemRevive(o, 'phoenix', false).items![0]).toEqual({ id: 'phoenix', name: 'Phoenix', kind: 'usable', uses: 1, target: 'ally', heal: 10 })
    expect(itemProbs(o)).toEqual([])
  })
  it('only on a usable item', () => {
    const w: Item = { id: 'w', name: 'W', kind: 'accessory', bonus: { str: 1 } }
    expect(setItemRevive(withItem(d, w), 'w', true).items![0]).toBe(w)
  })
  it('needs the rule', () => {
    expect(itemProbs(withItem(base(), item()))).toEqual([`Phoenix: ${NEEDS}`])
  })
  it('refuses a kind that is not usable', () => {
    expect(itemProbs(withItem(d, item({ kind: 'accessory', bonus: { str: 1 } })))).toContain('Phoenix: only a usable item can revive (a staff lists “revive” among its effects).')
  })
  it('cannot go with heal, raise, cure, inflict, promotes, opens, light', () => {
    const clashes: Array<[Partial<Item>, string]> = [
      [{ heal: 5 }, 'healing'], [{ raise: { str: 1 } }, 'raising stats'], [{ cure: 'all' }, 'curing'], [{ inflict: { condition: 'poison' } }, 'giving a condition'],
      [{ promotes: true }, 'promoting'], [{ opens: ['door'] }, 'opening doors and chests'], [{ light: { radius: 2 } }, 'giving light'],
    ]
    for (const [extra, word] of clashes) expect(itemProbs(withItem(d, item(extra)))).toContain(`Phoenix: reviving cannot go together with ${word}.`)
  })
  it('needs target ally', () => {
    const msg = 'Phoenix: reviving is used on a downed neighbour, so the item must be used on an ally.'
    expect(itemProbs(withItem(d, item({ target: 'self' })))).toContain(msg)
    expect(itemProbs(withItem(d, item({ target: undefined })))).toContain(msg)
    expect(itemProbs(withItem(d, item()))).toEqual([])
  })
  it('the amount has no formula', () => {
    for (const ok of [1, 999, 'full', { percent: 100 }]) expect(itemProbs(withItem(d, item({ revive: ok as never })))).toEqual([])
    for (const bad of [0, 1000, 'a.mag', 'most', { percent: 0 }, { percent: 101 }])
      expect(itemProbs(withItem(d, item({ revive: bad as never })))).toEqual(['Phoenix: reviving must be a whole number from 1 to 999, “full” or a percent from 1 to 100 (no formula).'])
  })
  it('a revive alone is enough for a usable item (no "needs to heal" note)', () => {
    expect(itemProbs(withItem(d, item()))).toEqual([])
  })
})

describe('a staff with revive', () => {
  const d = on(base())
  it('switching on makes revive the only effect and fixes a target that cannot be used; off goes back to a heal', () => {
    const start = staff({ effects: [{ heal: 10 }, { cure: 'all' }], target: 'self', range: undefined })
    const o = setStaffReviveOn(withItem(d, start), 'rod', true)
    expect(o.items![0].effects).toEqual([{ revive: { percent: 50 } }])
    expect(o.items![0].target).toBeUndefined()
    expect(o.items![0].range).toBe(2)
    expect(isReviveStaff(o.items![0])).toBe(true)
    expect(staffRevive(o.items![0])).toEqual({ percent: 50 })
    expect(setStaffReviveValue(o, 'rod', 'a.mag * 2').items![0].effects).toEqual([{ revive: 'a.mag * 2' }])
    expect(setStaffReviveOn(o, 'rod', false).items![0].effects).toEqual([{ heal: 10 }])
    expect(staffProbs(o)).toEqual([])
  })
  it('the staff target cannot be changed to foe or self once it revives', () => {
    const o = withItem(d, staff())
    expect(setStaffTarget(o, 'rod', 'foe')).toBe(o)
    expect(setStaffTarget(o, 'rod', 'self')).toBe(o)
    expect(setStaffTarget(o, 'rod', 'team').items![0].target).toBe('team')
  })
  it('needs the rule', () => {
    expect(staffProbs(withItem(base(), staff()))).toEqual([expect.stringContaining(NEEDS)])
  })
  it('must be the only effect', () => {
    expect(staffProbs(withItem(d, staff({ effects: [{ revive: 'full' }, { heal: 5 }] })))).toEqual([expect.stringContaining('reviving must be the only effect of the staff.')])
  })
  it('target ally, team or any; not foe or self', () => {
    for (const t of [undefined, 'ally', 'team', 'any'] as const) expect(staffProbs(withItem(d, staff({ target: t })))).toEqual([])
    for (const t of ['foe', 'self'] as const) expect(staffProbs(withItem(d, staff({ target: t, range: t === 'self' ? undefined : 2 })))).toEqual([expect.stringContaining('cannot be used on foes or on the user')])
  })
  it('the amount: the same forms as heal, formula too, a bare word refused', () => {
    for (const ok of [1, 999, 'full', { percent: 100 }, 'a.mag * 2']) expect(staffProbs(withItem(d, staff({ effects: [{ revive: ok }] })))).toEqual([])
    for (const bad of [0, 1000, { percent: 101 }, 2.5]) expect(staffProbs(withItem(d, staff({ effects: [{ revive: bad }] })))).toEqual([expect.stringContaining('reviving must give back')])
    expect(staffProbs(withItem(d, staff({ effects: [{ revive: 'most' }] })))).toEqual([expect.stringContaining('“most” is not a formula')])
  })
})

describe('a condition with autoRevive', () => {
  const d = on(base())
  it('the key is set and removed; the condition needs no rule of its own', () => {
    const c = withCondition(d, {})
    expect(condProbs(c)).toEqual(['Ward: it needs at least one rule (something it does).'])
    const o = setAutoRevive(c, 'ward', { percent: 25 })
    expect(o.conditions![0].autoRevive).toEqual({ percent: 25 })
    expect(condProbs(o)).toEqual([])
    expect(condProbs(withCondition(d, { autoRevive: 10, blocks: undefined }))).toEqual([])
    expect('autoRevive' in setAutoRevive(o, 'ward', undefined).conditions![0]).toBe(false)
  })
  it('needs the rule', () => {
    expect(condProbs(withCondition(base(), { autoRevive: 10 }))).toEqual([`Ward: ${NEEDS}`])
  })
  it('the amount has no formula', () => {
    for (const ok of [1, 999, 'full', { percent: 1 }]) expect(condProbs(withCondition(d, { autoRevive: ok }))).toEqual([])
    for (const bad of [0, 1000, 'a.mag', { percent: 101 }]) expect(condProbs(withCondition(d, { autoRevive: bad }))).toEqual(['Ward: the HP it stands the unit up with must be a whole number from 1 to 999, “full” or a percent from 1 to 100 (no formula).'])
  })
})

describe('a game that does not use any of it', () => {
  it('has no new keys and no new problems, and survives an edit unchanged', () => {
    const d = base()
    const before = JSON.stringify(d)
    expect(abilityChecks(d).filter((c) => /revive|Downed|reviv/i.test(c.text))).toEqual([])
    expect(itemChecks(d).filter((c) => /reviv|Downed/i.test(c.text))).toEqual([])
    expect(conditionChecks(d).filter((c) => /reviv|Downed/i.test(c.text))).toEqual([])
    expect(downedRuleChecks(d)).toEqual([])
    expect(JSON.stringify(setDownedOn(setDownedOn(d, true), false).game)).toBe(JSON.stringify(d.game))
    expect(JSON.stringify(d)).toBe(before)
    expect(/revive|downed|autoRevive/i.test(JSON.stringify(d))).toBe(false)
  })
  it('a round trip keeps the keys it was given and unknown ones next to them', () => {
    const d = withCondition(withItem(withAbility(on(base()), revive({ area: { shape: 'burst' } })), item()), { autoRevive: 'full', mystery: 1 })
    const again = JSON.parse(JSON.stringify(d)) as ProjectDoc
    expect(again).toEqual(d)
    expect(again.game!.rules!.downed).toEqual({})
    expect(again.abilities![0].effect).toEqual({ revive: { percent: 50 } })
    expect(again.conditions![0]).toMatchObject({ autoRevive: 'full', mystery: 1 })
  })
})

function item(): Item { return { id: 'phoenix', name: 'Phoenix', kind: 'usable', uses: 1, target: 'ally', revive: 'full' } }
