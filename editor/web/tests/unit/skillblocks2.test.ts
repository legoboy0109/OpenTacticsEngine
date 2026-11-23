// The second set of skill and ability blocks (docs/SRPG.md "Skill and ability blocks, second set"): what the model offers, what its checks
// say (they mirror the loader's refusals), the plain sentences, and the hand-edit-only keys of the first set that now have controls.
import { describe, expect, it } from 'vitest'
import {
  TARGETS_FOR, REFRESH_MAX, addAbility, setCure, setEffectKind, setRange, setRefreshPerTurn, setSteal, setTarget, stealOf, updateAbility,
} from '../../src/model/abilities'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilitySummary } from '../../src/model/abilityText'
import { gameHasSteal, setStealable } from '../../src/model/items'
import { gameClassSkills, setClassSkillsRule } from '../../src/model/promotion'
import { newEffect, newBlock, offeredEffects, setWhen } from '../../src/model/skillBlocks'
import { ruleIssues } from '../../src/model/skillChecks'
import { WHENS, condsFor, effectsFor } from '../../src/model/skillRules'
import { blockSummary, conditionText } from '../../src/model/skillText'
import { addSkill, allSkillTags } from '../../src/model/skills'
import { deleteSkill } from '../../src/model/srpgRefs'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { MapUnit, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => addSkill(switchOnFeature(switchOnFeature(loadDemo('demo-srpg'), 'skills'), 'abilities'), 'First')
const issues = (b: object, d = base()) => ruleIssues(d, b as never)

describe('staffUse and the new skill effects', () => {
  it('"staffUse" is a point of its own: heal and cure only, conditions as at a turn start', () => {
    expect(WHENS).toContain('staffUse')
    expect([...effectsFor('staffUse', false)].sort()).toEqual(['cure', 'heal'])
    expect(condsFor('staffUse', false)).toEqual(condsFor('turnStart', false))
    expect(issues({ when: 'staffUse', do: [{ heal: 3 }] })).toEqual([])
    expect(issues({ when: 'staffUse', chance: 50, do: [{ heal: { percent: 20 } }] })).toEqual([])
    expect(issues({ when: 'staffUse', do: [{ damage: 3 }] })[0]).toMatch(/cannot be used/)
    expect(issues({ when: 'staffUse', do: [{ heal: 3, to: 'allies', within: 1 }] })[0]).toMatch(/goes to the unit that used it/)
  })
  it('changing a rule to "staffUse" turns an ally heal into a self heal and drops what does not fit', () => {
    const b = setWhen({ when: 'turnStart', do: [{ heal: 5, to: 'allies', within: 2 }, { hurt: 3 }] }, 'staffUse', base())
    expect(b.do).toEqual([{ heal: 5 }])
    expect(newBlock(base(), 'staffUse').do).toEqual([{ heal: 5 }])
  })
  it('nullifySkills, staffRange and carryBonus are made valid and offered at the right points only', () => {
    const d = base()
    for (const k of ['nullifySkills', 'staffRange', 'carryBonus'] as const) expect(newEffect(k, d, 'combat')).not.toBeNull()
    expect(offeredEffects(d, { when: 'combat' })).toContain('nullifySkills')
    expect(offeredEffects(d, { when: 'always' })).toEqual(expect.arrayContaining(['staffRange', 'carryBonus']))
    expect(offeredEffects(d, { when: 'combat' })).not.toContain('staffRange')
    expect(effectsFor('combat', true)).not.toContain('nullifySkills')
    expect(issues({ when: 'combat', do: [{ nullifySkills: true }] })).toEqual([])
    expect(issues({ when: 'strike', do: [{ nullifySkills: true }] })[0]).toMatch(/cannot be used/)
  })
  it('nullifySkills checks: unknown skills and tags are said in plain words', () => {
    const d = base()
    const first = d.skills![0].id
    expect(issues({ when: 'combat', do: [{ nullifySkills: [first] }] }, d)).toEqual([])
    expect(issues({ when: 'combat', do: [{ nullifySkills: ['nobody'] }] }, d)[0]).toMatch(/does not exist/)
    expect(issues({ when: 'combat', do: [{ nullifySkills: [] }] }, d)[0]).toMatch(/nothing yet/)
    expect(issues({ when: 'combat', do: [{ nullifySkills: { tag: 'bonus' } }] }, d)[0]).toMatch(/No skill has the tag/)
    const tagged: ProjectDoc = { ...d, skills: d.skills!.map((s, i) => (i === 0 ? { ...s, tags: ['bonus'] } : s)) }
    expect(allSkillTags(tagged)).toEqual(['bonus'])
    expect(issues({ when: 'combat', do: [{ nullifySkills: { tag: 'bonus' } }] }, tagged)).toEqual([])
    expect(issues({ when: 'combat', do: [{ nullifySkills: 'x' }] })[0]).toMatch(/switches off every skill/)
  })
  it('staffRange and carryBonus checks mirror the loader', () => {
    const d = base()
    expect(issues({ when: 'always', do: [{ staffRange: 2 }] }, d)).toEqual([])
    expect(issues({ when: 'always', do: [{ staffRange: 0 }] }, d)[0]).toMatch(/from 1 to 10/)
    expect(issues({ when: 'always', do: [{ staffRange: { amount: 2, group: 'flute' } }] }, d)[0]).toMatch(/no staff group/)
    expect(issues({ when: 'always', do: [{ carryBonus: 3 }] }, d)).toEqual([])
    expect(issues({ when: 'always', do: [{ carryBonus: 500 }] }, d)[0]).toMatch(/from -100 to 100/)
  })
  it('the sentences', () => {
    const d = base()
    expect(blockSummary(d, { when: 'combat', do: [{ nullifySkills: true }] })).toBe('In combat: the foe’s skills do not work in this fight.')
    expect(blockSummary(d, { when: 'staffUse', do: [{ heal: 3 }] })).toBe('After using a staff: heals 3 HP.')
    expect(blockSummary(d, { when: 'always', do: [{ staffRange: 2 }] })).toBe('Always: staves reach 2 squares further.')
    expect(blockSummary(d, { when: 'always', do: [{ carryBonus: 2 }] })).toMatch(/carrying capacity \+2/)
  })
  it('the terrain test takes a list; the sentence and the checks follow', () => {
    const d = base()
    const a = d.terrain![0].id
    expect(issues({ when: 'combat', if: [{ terrain: [a] }], do: [{ damage: 1 }] }, d)).toEqual([])
    expect(issues({ when: 'combat', if: [{ terrain: [a, 'lava'] }], do: [{ damage: 1 }] }, d)[0]).toMatch(/“lava”/)
    expect(issues({ when: 'combat', if: [{ terrain: [] }], do: [{ damage: 1 }] }, d)[0]).toMatch(/at least one/)
    expect(conditionText(d, { terrain: [a, 'cover'] })).toContain(' or ')
  })
})

describe('abilities: steal, refresh, move past, cure', () => {
  it('each effect accepts the targets the loader accepts, and changing the effect repairs the target', () => {
    expect(TARGETS_FOR.steal).toEqual(['enemy'])
    expect(TARGETS_FOR.refresh).toEqual(['ally'])
    expect(TARGETS_FOR.cure).toEqual(['self', 'ally', 'enemy', 'unit'])
    expect(TARGETS_FOR.movePast).toEqual(['ally', 'enemy', 'unit'])
    let d = addAbility(base(), 'Pick')
    let a = d.abilities!.find((x) => x.id === 'pick')!
    a = setEffectKind(d, a, 'steal')
    expect(a).toMatchObject({ target: 'enemy', effect: { steal: true } })
    expect(setTarget(a, 'ally')).toBe(a)  // refused: a steal works on an enemy
    a = setEffectKind(d, a, 'refresh')
    expect(a).toMatchObject({ target: 'ally', effect: { refresh: true } })
    a = setEffectKind(d, a, 'movePast')
    expect(a).toMatchObject({ target: 'ally', effect: { movePast: true } })
    a = setEffectKind(d, a, 'cure')
    expect(a.target).toBe('ally')
    d = updateAbility(d, 'pick', a)
    expect(a.effect).toEqual(d.conditions?.[0] ? { cure: [d.conditions[0].id] } : { cure: 'all' })
  })
  it('a steal or a move past has no range of its own: it is not written', () => {
    const d = base()
    const a = setEffectKind(d, { id: 'x', name: 'X', target: 'unit', effect: { push: 1 } }, 'steal')
    expect(setRange(a, 1, 3).range).toBeUndefined()
  })
  it('steal options: the defaults are written as plain true, "equipped" needs "weapons"', () => {
    const d = base()
    let a = setEffectKind(d, { id: 'x', name: 'X', target: 'enemy', effect: { push: 1 } }, 'steal')
    a = setSteal(a, { if: 'a.spd > b.spd' })
    expect(a.effect).toEqual({ steal: { if: 'a.spd > b.spd' } })
    a = setSteal(a, { weapons: true })
    a = setSteal(a, { equipped: true })
    expect(stealOf(a)).toEqual({ if: 'a.spd > b.spd', weapons: true, equipped: true })
    a = setSteal(a, { weapons: false })
    expect(stealOf(a)).toEqual({ if: 'a.spd > b.spd' })  // equipped goes with weapons
    a = setSteal(a, { if: '' })
    expect(a.effect).toEqual({ steal: true })
  })
  it('refresh per turn: 1 is written as true', () => {
    const d = base()
    let a = setEffectKind(d, { id: 'x', name: 'X', target: 'ally', effect: { push: 1 } }, 'refresh')
    a = setRefreshPerTurn(a, 3)
    expect(a.effect).toEqual({ refresh: { perTurn: 3 } })
    expect(setRefreshPerTurn(a, 99).effect).toEqual({ refresh: { perTurn: REFRESH_MAX } })
    expect(setRefreshPerTurn(a, 1).effect).toEqual({ refresh: true })
  })
  it('cure: all or a list', () => {
    const d = base()
    const a = setEffectKind(d, { id: 'x', name: 'X', target: 'ally', effect: { push: 1 } }, 'cure')
    expect(setCure(a, ['poison']).effect).toEqual({ cure: ['poison'] })
    expect(setCure(a, 'all').effect).toEqual({ cure: 'all' })
  })
  it('the checks say what the loader refuses', () => {
    const d = base()
    const mk = (a: object) => abilityChecks({ ...d, abilities: [{ id: 'x', name: 'X', ...a }] as never }).filter((c) => !c.text.includes('nobody has')).map((c) => c.text)
    expect(mk({ target: 'enemy', effect: { steal: true } })).toEqual([])
    expect(mk({ target: 'ally', effect: { steal: true } })[0]).toMatch(/stealing works on an enemy/)
    expect(mk({ target: 'enemy', range: [1, 2], effect: { steal: true } })[0]).toMatch(/right next to it/)
    expect(mk({ target: 'enemy', effect: { steal: { equipped: true } } })[0]).toMatch(/weapon in hand only if/)
    expect(mk({ target: 'enemy', effect: { steal: { if: ' ' } } })[0]).toMatch(/formula of the steal is empty/)
    expect(mk({ target: 'ally', effect: { refresh: true } })).toEqual([])
    expect(mk({ target: 'enemy', effect: { refresh: true } })[0]).toMatch(/refreshing works on an ally/)
    expect(mk({ target: 'ally', effect: { refresh: { perTurn: 0 } } })[0]).toMatch(/from 1 to 9/)
    expect(mk({ target: 'ally', effect: { cure: [] } })[0]).toMatch(/cures nothing yet/)
    expect(mk({ target: 'ally', effect: { cure: ['gout'] } })[0]).toMatch(/does not exist/)
    expect(mk({ target: 'enemy', effect: { cure: 'all' } })).toEqual([])  // a dispel (docs/SRPG.md "Ability conditions")
    expect(mk({ target: 'cell', effect: { cure: 'all' } })[0]).toMatch(/curing works on a unit, not a square/)
    expect(mk({ target: 'unit', effect: { movePast: true } })).toEqual([])
    expect(mk({ target: 'cell', effect: { movePast: true } })[0]).toMatch(/needs a unit next to it/)
  })
  it('the summary sentences', () => {
    const d = base()
    const s = (a: object) => abilitySummary(d, { id: 'x', name: 'X', ...a } as never)
    expect(s({ target: 'enemy', effect: { steal: true } })).toBe('Takes an item it carries (not weapons) from an enemy next to it; uses the unit’s action.')
    expect(s({ target: 'enemy', effect: { steal: { if: 'a.spd > b.spd', weapons: true } } })).toMatch(/weapons too .* \(only if a\.spd > b\.spd\)/)
    expect(s({ target: 'ally', effect: { refresh: { perTurn: 2 } } })).toMatch(/can be refreshed 2 times a turn/)
    expect(s({ target: 'unit', effect: { movePast: true } })).toMatch(/Moves to the far side of a unit next to it/)
    expect(s({ target: 'self', effect: { cure: 'all' } })).toMatch(/Cures every condition of itself/)
  })
})

describe('the keys that were hand-edit only', () => {
  it('an item can be marked "cannot be stolen": written as stealable false, the default is not written', () => {
    const d = base()
    const id = d.items![0].id
    const off = setStealable(d, id, false)
    expect(off.items!.find((i) => i.id === id)!.stealable).toBe(false)
    expect('stealable' in setStealable(off, id, true).items!.find((i) => i.id === id)!).toBe(false)
    expect(gameHasSteal(d)).toBe(false)
    expect(gameHasSteal({ ...d, abilities: [{ id: 's', name: 'S', target: 'enemy', effect: { steal: true } }] })).toBe(true)
  })
  it('the promotion rule classSkills: "keep" is the default and is not written; an empty block goes', () => {
    const d = base()
    expect(gameClassSkills(d)).toBe('keep')
    const r = setClassSkillsRule(d, 'replace')
    expect(gameClassSkills(r)).toBe('replace')
    expect(r.game!.rules!.promotion).toMatchObject({ classSkills: 'replace' })
    const back = setClassSkillsRule({ ...r, game: { ...r.game!, rules: { ...r.game!.rules, promotion: { classSkills: 'replace' } } } }, 'keep')
    expect(back.game!.rules!.promotion).toBeUndefined()
  })
  it('deleting a skill also takes it off the units placed on a map and off addUnit commands', () => {
    const d = base()
    const id = d.skills![0].id
    const mapId = Object.keys(d.maps)[0]
    const m = d.maps[mapId]!
    const unit: MapUnit = { ...(m.units![0] as MapUnit), skills: [id, 'keep_me'] }
    const withUnit = { ...d, maps: { ...d.maps, [mapId]: { ...m, units: [unit, ...m.units!.slice(1)], events: [...(m.events ?? []), { id: 'ev', when: {}, do: [{ addUnit: { type: unit.type, side: unit.side, x: 0, y: 0, skills: [id] } }] }] as never } } }
    const gone = deleteSkill(withUnit, id)
    const out = gone.maps[mapId]!
    expect(out.units![0].skills).toEqual(['keep_me'])
    expect(JSON.stringify(out.events)).not.toContain('"skills"')
  })
})
