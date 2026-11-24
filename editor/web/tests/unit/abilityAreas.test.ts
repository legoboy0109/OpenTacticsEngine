// Ability areas, heal + cure and a list of conditions (loader keys "area", "effect": { heal, cure }, "inflict": [ ... ]): the model helpers,
// every editor check, and that an ability without the new keys is read and written as before.
import { describe, expect, it } from 'vitest'
import { effectKind, setArea, setEffectKind, setRange, setTarget, setCure } from '../../src/model/abilities'
import { AREA_SHAPES, areaOf, hasArea, setAreaShape, setAreaSize, setAreaVertical } from '../../src/model/abilityArea'
import { areaChecks, effectPairChecks } from '../../src/model/abilityAreaChecks'
import { abilityChecks } from '../../src/model/abilityChecks'
import { alsoCures, setAlsoCures } from '../../src/model/abilityHealCure'
import { addInflict, inflictEntries, removeInflict, setInflictChance, setInflictCondition, setInflictTurns } from '../../src/model/abilityInflicts'
import { abilitySummary } from '../../src/model/abilityText'
import { canBeInflicted } from '../../src/model/conditionChecks'
import { abilityExtraProblems, abilityInflict, setAbilityInflict } from '../../src/model/fidelityAbility'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { Ability } from '../../src/model/srpgTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadConditionsFixture } from './fixtures'

const fx = (): ProjectDoc => switchOnFeature(loadConditionsFixture(), 'abilities')
const withHeight = (d: ProjectDoc): ProjectDoc => ({ ...d, game: { ...d.game!, rules: { ...(d.game!.rules ?? {}), height: {} } } })
const heal = (extra: Partial<Ability> = {}): Ability => ({ id: 'mend', name: 'Mend', target: 'ally', effect: { heal: 10 }, ...extra })
const withAbility = (d: ProjectDoc, a: Ability): ProjectDoc => ({ ...d, abilities: [a] })
const probs = (d: ProjectDoc, a: Ability) => abilityChecks(withAbility(d, a)).map((c) => c.text).filter((t) => !t.includes('nobody has'))
const areaProbs = (d: ProjectDoc, a: Ability) => areaChecks(d, a)

describe('effectKind with the new keys', () => {
  it('heal + cure reads as a heal; an inflict list with no effect is "none"', () => {
    expect(effectKind(heal({ effect: { heal: 5, cure: 'all' } }))).toBe('heal')
    expect(effectKind({ id: 'x', name: 'X', inflict: [{ condition: 'a' }, { condition: 'b' }] })).toBe('none')
    expect(effectKind({ id: 'x', name: 'X', inflict: { condition: 'a' } })).toBe('none')
    expect(effectKind({ id: 'x', name: 'X' })).toBeNull()
  })
})

describe('the area', () => {
  it('starts as a burst, with no radius, and goes again; unknown keys stay', () => {
    const a = setArea(heal(), true)
    expect(a.area).toEqual({ shape: 'burst' })
    expect(hasArea(a)).toBe(true)
    expect(setArea(a, true)).toBe(a)
    expect(setArea(a, false)).toEqual(heal())
    const odd = { ...a, area: { shape: 'burst', future: 1 } }
    expect(setAreaVertical(odd, 2).area).toEqual({ shape: 'burst', future: 1, vertical: 2 })
  })
  it('shapes: radius and cross keep a size, line has a length, burst none', () => {
    let a = setArea(heal(), true)
    a = setAreaShape(a, 'radius')
    expect(a.area).toEqual({ shape: 'radius', radius: 2 })
    a = setAreaSize(a, 4)
    expect(setAreaShape(a, 'cross').area).toEqual({ shape: 'cross', radius: 4 })
    a = setAreaShape(a, 'line')
    expect(a.area).toEqual({ shape: 'line', length: 3 })
    expect(setAreaSize(a, 99).area).toEqual({ shape: 'line', length: 8 })
    expect(setAreaSize(a, 0).area).toEqual({ shape: 'line', length: 1 })
    expect(setAreaShape(a, 'burst').area).toEqual({ shape: 'burst' })
    expect(setAreaSize(setArea(heal(), true), 3).area).toEqual({ shape: 'burst' })  // a burst has no size
    expect(areaOf(a)).toEqual({ shape: 'line', size: 3, vertical: undefined })
    expect(AREA_SHAPES).toHaveLength(4)
  })
  it('vertical: empty = no key (no limit), 0 is written (same level only)', () => {
    let a = setArea(heal(), true)
    a = setAreaVertical(a, 3)
    expect(a.area).toEqual({ shape: 'burst', vertical: 3 })
    expect(setAreaVertical(a, 0).area).toEqual({ shape: 'burst', vertical: 0 })
    expect(setAreaVertical(a, undefined).area).toEqual({ shape: 'burst' })
    expect(setAreaVertical(a, 5000).area).toEqual({ shape: 'burst', vertical: 999 })
    expect(setAreaVertical(heal(), 3)).toEqual(heal())  // no area: nothing to set
  })
  it('turning it on for an ability aimed at itself picks "ally"; push cannot have one', () => {
    expect(setArea(heal({ target: 'self' }), true).target).toBe('ally')
    expect(setArea({ id: 'p', name: 'P', target: 'unit', effect: { push: 1 } }, true)).toEqual({ id: 'p', name: 'P', target: 'unit', effect: { push: 1 } })
  })
  it('the area goes when the effect becomes one that cannot have it', () => {
    const d = fx()
    let a = setRange(setArea(heal(), true), 0, 2)
    expect(a.range).toEqual([0, 2])
    const p = setEffectKind(d, a, 'push')
    expect(p.area).toBeUndefined()
    expect(p.range).toBeUndefined()  // push has a fixed range
    a = setEffectKind(d, a, 'cure')
    expect(a.area).toEqual({ shape: 'burst' })  // cure keeps it
    expect(a.range).toEqual([0, 2])
    const s = setEffectKind(d, a, 'summon')
    expect(s.area).toBeUndefined()
    expect(s.range).toEqual([1, 2])  // a summon starts at 1..2 and the 0 is gone
    expect(setEffectKind({ ...d, weapons: [{ id: 'bow', name: 'Bow' } as never] }, a, 'strike').area).toBeUndefined()
    const none = setEffectKind(d, a, 'none')
    expect(none.area).toEqual({ shape: 'burst' })
    expect(none.inflict).toBeTruthy()
  })
  it('switching to heal from an area cure never leaves "self" as the target', () => {
    const d = fx()
    const a = setEffectKind(d, { id: 'c', name: 'C', target: 'enemy', effect: { cure: 'all' }, area: { shape: 'burst' } }, 'heal')
    expect(a.target).toBe('ally')
  })
  it('self and a free square cannot be picked while there is an area', () => {
    const a = setArea(heal(), true)
    expect(setTarget(a, 'self')).toBe(a)
    expect(setTarget(a, 'enemy' as never)).toBe(a)  // a heal never takes an enemy
    expect(setTarget(a, 'unit').target).toBe('unit')
  })
  it('the range may start at 0 only with an area, and goes back to 1 without', () => {
    const plain = setRange(heal(), 0, 3)
    expect(plain.range).toEqual([1, 3])
    let a = setRange(setArea(heal(), true), 0, 0)
    expect(a.range).toEqual([0, 0])
    a = setRange(a, 0, 3)
    expect(a.range).toEqual([0, 3])
    expect(setArea(a, false).range).toEqual([1, 3])
    expect(setArea(setRange(setArea(heal(), true), 0, 1), false).range).toBeUndefined()  // [1, 1] is the default: not written
    expect(setRange(setArea(heal(), true), 1, 1).range).toBeUndefined()
  })
})

describe('heal + cure', () => {
  it('"also cures" adds a cure from the first condition (or harmful) and removes it again', () => {
    const d = fx()
    const on = setAlsoCures(d, heal(), true)
    expect(on.effect).toEqual({ heal: 10, cure: [d.conditions![0].id] })
    expect(alsoCures(on)).toBe(true)
    expect(effectKind(on)).toBe('heal')
    expect(setAlsoCures(d, on, true)).toBe(on)
    expect(setAlsoCures(d, on, false)).toEqual(heal())
    expect(setAlsoCures({ ...d, conditions: [] }, heal(), true).effect).toEqual({ heal: 10, cure: 'harmful' })
    expect(setAlsoCures(d, { id: 'p', name: 'P', effect: { push: 1 } }, true).effect).toEqual({ push: 1 })
  })
  it('the cure part is edited through setCure and the heal stays', () => {
    const d = fx()
    const a = setCure(setAlsoCures(d, heal(), true), 'helpful')
    expect(a.effect).toEqual({ heal: 10, cure: 'helpful' })
  })
  it('only heal and cure may go together', () => {
    expect(effectPairChecks(heal({ effect: { heal: 5, cure: 'all' } }))).toEqual([])
    expect(effectPairChecks(heal({ effect: { heal: 5, push: 1 } }))).toEqual(['Mend: it can do only one thing. Only healing and curing may go together.'])
    expect(effectPairChecks(heal({ effect: { cure: 'all', push: 1 } }))).toHaveLength(1)
  })
  it('the cure part gets the cure checks and the summary says both', () => {
    const d = fx()
    const bad = heal({ effect: { heal: 5, cure: ['nope'] } })
    expect(probs(d, bad)).toContain('Mend: the condition “nope” does not exist.')
    expect(probs(d, heal({ effect: { heal: 5, cure: [] } }))).toContain('Mend: it cures nothing yet. Pick at least one condition, or all of them.')
    expect(probs({ ...d, conditions: [] }, heal({ effect: { heal: 5, cure: 'harmful' } }))).toContain('Mend: it cures every harmful condition, but the game has no conditions yet.')
    expect(probs(d, heal({ effect: { heal: 5, cure: 'all' } }))).toEqual([])
    expect(abilitySummary(d, heal({ effect: { heal: 5, cure: 'all' } }))).toMatch(/^Heals an ally next to it by 5, then cures every condition of them/)
  })
})

describe('inflict as a list', () => {
  const [c1, c2] = (() => { const d = fx(); return [d.conditions![0].id, d.conditions![1].id] })()
  const base = (): Ability => ({ id: 'curse', name: 'Curse', target: 'enemy', range: [1, 3], inflict: { condition: c1 } })
  it('reads both forms as a list', () => {
    expect(inflictEntries(base())).toEqual([{ condition: c1, turns: undefined, chance: undefined }])
    expect(inflictEntries({ ...base(), inflict: [{ condition: c1, turns: 2 }, { condition: c2, chance: '50' }] })).toHaveLength(2)
    expect(inflictEntries({ id: 'x', name: 'X' })).toEqual([])
  })
  it('one entry is written back as an object, two as a list', () => {
    const d = fx()
    let a = addInflict(d, base())
    expect(a.inflict).toEqual([{ condition: c1 }, { condition: c2 }])
    a = removeInflict(a, 0)
    expect(a.inflict).toEqual({ condition: c2 })  // not [ { ... } ]
    expect(removeInflict(a, 0)).not.toHaveProperty('inflict')
    expect(removeInflict(base(), 5)).toEqual(base())
  })
  it('edits an entry without touching the others or its unknown keys', () => {
    const d = fx()
    let a: Ability = { ...base(), inflict: [{ condition: c1, future: 1 }, { condition: c2 }] }
    a = setInflictTurns(a, 0, 3)
    a = setInflictChance(a, 1, '60')
    expect(a.inflict).toEqual([{ condition: c1, future: 1, turns: 3 }, { condition: c2, chance: 60 }])
    a = setInflictChance(setInflictTurns(a, 0, undefined), 1, 'a.mag + 40')
    expect(a.inflict).toEqual([{ condition: c1, future: 1 }, { condition: c2, chance: 'a.mag + 40' }])
    expect((setInflictCondition(a, 1, c1).inflict as unknown[])[1]).toEqual({ condition: c1, chance: 'a.mag + 40' })
    expect(setInflictCondition(a, 0, '').inflict).toEqual({ condition: c2, chance: 'a.mag + 40' })
    expect(setInflictCondition({ id: 'x', name: 'X' }, 0, c1).inflict).toEqual({ condition: c1 })
    expect(d).toBeTruthy()
  })
  it('add gives a condition that is not used yet, and nothing at 8 or when all are used', () => {
    const d = fx()
    const few: ProjectDoc = { ...d, conditions: d.conditions!.slice(0, 2) }
    const two = addInflict(few, base())
    expect(inflictEntries(two)).toHaveLength(2)
    expect(addInflict(few, two)).toBe(two)
  })
  it('the old single-entry helpers still work, and a list is checked entry by entry', () => {
    const d = fx()
    const a = setAbilityInflict(base(), c2, 4)
    expect(a.inflict).toEqual({ condition: c2, turns: 4 })
    expect(abilityInflict(a)).toEqual({ condition: c2, turns: 4, chance: undefined })
    expect(abilityExtraProblems(d, base())).toEqual([])
    const list = { ...base(), inflict: [{ condition: c1 }, { condition: 'frost' }, { condition: c1, turns: 0 }] }
    const p = abilityExtraProblems(d, list)
    expect(p).toContain('Condition 2: The condition "frost" does not exist (Conditions screen).')
    expect(p).toContain(`It gives "${c1}" twice. List each condition once.`)
    expect(p).toContain('Condition 3: Turns must be a whole number from 1 to 99.')
    expect(abilityExtraProblems(d, { ...base(), inflict: [] })).toEqual(['The conditions it gives must be one entry or a list of 1 to 8.'])
    expect(abilityExtraProblems(d, { ...base(), inflict: Array.from({ length: 9 }, () => ({ condition: c1 })) })[0]).toBe('The conditions it gives must be one entry or a list of 1 to 8.')
    expect(abilityExtraProblems(d, { ...base(), inflict: [{ condition: c1 }, 3 as never] })).toContain('Condition 2: "Gives a condition" must name a condition.')
    expect(abilityExtraProblems(d, { ...base(), inflict: [{ condition: c1, chance: 0 }] })).toContain('Condition 1: The chance must be a percent from 1 to 100 or a formula (a = the user, b = the target).')
  })
  it('a condition only a list entry gives still counts as given, and the summary names them all', () => {
    const d = withAbility(fx(), { ...base(), inflict: [{ condition: c1 }, { condition: c2, turns: 2 }] })
    expect(canBeInflicted(d, c2)).toBe(true)
    expect(abilitySummary(d, d.abilities![0])).toMatch(/^Gives .+ and .+ for 2 turns to an enemy next to it/)
  })
})

describe('the area checks (each says what the loader refuses)', () => {
  const d = fx()
  const area = (a: Record<string, unknown>, over: Partial<Ability> = {}): Ability => heal({ area: a, ...over })
  const one = (a: Ability, dd = d) => areaProbs(dd, a)
  it('a valid area and an ability with none are clean', () => {
    expect(one(area({ shape: 'burst' }))).toEqual([])
    expect(one(area({ shape: 'radius', radius: 2 }))).toEqual([])
    expect(one(area({ shape: 'cross', radius: 8 }, { range: [0, 3] }))).toEqual([])
    expect(one(area({ shape: 'line', length: 3 }, { range: [0, 0] }))).toEqual([])
    expect(one(area({ shape: 'burst', vertical: 2 }), withHeight(d))).toEqual([])
    expect(one(heal())).toEqual([])
  })
  it('shape, radius and length', () => {
    expect(one(area({}))).toEqual(['Mend: the area needs a shape (burst, radius, cross or line).'])
    expect(one(area({ shape: 'blob' }))).toEqual(['Mend: the shape of the area must be burst, radius, cross or line.'])
    expect(one(area({ shape: 'radius' }))).toEqual(['Mend: an area of shape “radius” needs a radius from 1 to 8.'])
    expect(one(area({ shape: 'cross', radius: 9 }))).toEqual(['Mend: the radius of the area must be a whole number from 1 to 8.'])
    expect(one(area({ shape: 'burst', radius: 2 }))).toEqual(['Mend: a radius goes only with the shapes “radius” and “cross”.'])
    expect(one(area({ shape: 'line' }))).toEqual(['Mend: an area of shape “line” needs a length from 1 to 8.'])
    expect(one(area({ shape: 'line', length: 0 }))).toEqual(['Mend: the length of the area must be a whole number from 1 to 8.'])
    expect(one(area({ shape: 'radius', radius: 2, length: 2 }))).toEqual(['Mend: a length goes only with the shape “line”.'])
    expect(one(heal({ area: 'big' as never }))).toEqual(['Mend: the area must be a shape with a size.'])
  })
  it('keys that are for weapons, unknown keys', () => {
    expect(one(area({ shape: 'burst', friendlyFire: true }))).toEqual(['Mend: the area has no “friendly fire” setting. The target (ally, enemy or unit) says who in the area is affected.'])
    expect(one(area({ shape: 'burst', apExtra: 1 }))).toEqual(['Mend: the area has no extra action point cost. That is for weapons.'])
    expect(one(area({ shape: 'burst', zap: 1 }))).toEqual(['Mend: the area has an unknown setting “zap”. An ability’s area takes a shape, a radius or length, and a vertical reach.'])
  })
  it('vertical needs the height rules and 0 to 999', () => {
    expect(one(area({ shape: 'burst', vertical: 2 }))).toEqual(['Mend: a vertical reach needs the height rules to be switched on for the game.'])
    expect(one(area({ shape: 'burst', vertical: 1000 }), withHeight(d))).toEqual(['Mend: the vertical reach of the area must be a whole number from 0 to 999.'])
  })
  it('the effect and the target', () => {
    expect(one({ id: 's', name: 'S', target: 'unit', effect: { strike: 'x' }, area: { shape: 'burst' } })).toEqual(['S: a strike takes its area from the weapon. Leave out the area.'])
    for (const e of [{ push: 1 }, { pull: true }, { swap: true }, { summon: { type: 'a' } }, { unlock: true as const }, { steal: true as const }, { refresh: true as const }] as Ability['effect'][])
      expect(one({ id: 'p', name: 'P', target: 'ally', effect: e, area: { shape: 'burst' } })).toEqual(['P: an area goes with healing, curing (or both), reviving or an ability that only gives a condition. It cannot push, pull, swap, summon, unlock, steal or refresh.'])
    const needsUnit = 'Mend: an area needs the target to be an ally, an enemy or any unit (who in the area is affected). A reach of 0 to 0 centres it on the unit.'
    expect(one(area({ shape: 'burst' }, { target: 'self' }))).toEqual([needsUnit])
    expect(one({ id: 'c', name: 'C', target: 'cell', effect: { cure: 'all' }, area: { shape: 'burst' } })).toContain(needsUnit.replace('Mend', 'C'))
    expect(one({ id: 'c', name: 'C', target: 'enemy', effect: { cure: 'all' }, area: { shape: 'burst' } })).toEqual([])
    expect(one({ id: 'i', name: 'I', target: 'enemy', inflict: [{ condition: 'a' }], area: { shape: 'burst' } })).toEqual([])
  })
  it('the reach: from 1 without an area, from 0 with one', () => {
    expect(one(heal({ range: [0, 2] }))).toEqual(['Mend: the reach must be two whole numbers from 1 to 20, the first not above the second.'])
    expect(one(area({ shape: 'burst' }, { range: [0, 2] }))).toEqual([])
    expect(one(area({ shape: 'burst' }, { range: [3, 2] }))).toEqual(['Mend: the reach must be two whole numbers from 0 to 20 (0 is the unit’s own square), the first not above the second.'])
    expect(one(heal({ range: [1, 21] }))).toHaveLength(1)
  })
  it('a charged ability keeps the square; an "only if" about the target is refused', () => {
    const charged = { ...area({ shape: 'burst' }), chargeSpeed: 2, chargeTargets: 'unit' } as Ability
    expect(one(charged)).toEqual(['Mend: a charged ability with an area keeps the square it aims at. “Who is hit” must be the square (or left out).'])
    expect(one({ ...charged, chargeTargets: 'cell' } as Ability)).toEqual([])
    expect(one(area({ shape: 'burst' }, { if: [{ hp: { op: '<=', percent: 50 }, who: 'foe' }] }))).toEqual(['Mend: an area ability has no single target unit, so its “only if” cannot be about the target.'])
    expect(one(area({ shape: 'burst' }, { if: [{ hp: { op: '<=', percent: 50 } }] }))).toEqual([])
    expect(one(heal({ if: [{ hp: { op: '<=', percent: 50 }, who: 'foe' }] }))).toEqual([])  // without an area it is fine
  })
  it('the checks are part of abilityChecks', () => {
    expect(probs(d, area({ shape: 'burst' }, { target: 'self' }))).toHaveLength(1)
  })
})

describe('the summary of an area ability', () => {
  it('names who and where', () => {
    const d = fx()
    expect(abilitySummary(d, setAreaShape(setArea(heal(), true), 'cross'))).toMatch(/^Heals every ally \(itself too\) in a cross reaching 2 squares from the square it aims at by 10/)
    expect(abilitySummary(d, setArea(heal({ target: 'unit' }), true))).toContain('every unit in the square it aims at and the ones next to it')
  })
})

describe('an ability with none of the new keys is as before', () => {
  it('the same checks and the same writes', () => {
    const d = fx()
    const a = heal({ range: [1, 3] })
    expect(probs(d, a)).toEqual([])
    expect(setRange(a, 2, 4).range).toEqual([2, 4])
    expect(setRange(a, 0, 4).range).toEqual([1, 4])
    expect(setEffectKind(d, a, 'cure').area).toBeUndefined()
    expect(Object.keys(setEffectKind(d, a, 'cure'))).not.toContain('area')
    expect(probs(d, { id: 'p', name: 'P', target: 'unit', effect: { push: 1 } })).toEqual([])
    expect(abilitySummary(d, a)).toBe('Heals an ally next to it by 10; uses the unit’s action.')
  })
})
