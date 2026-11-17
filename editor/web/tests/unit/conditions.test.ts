// Status conditions in the editor (docs/SRPG.md section 9, docs/EDITOR_UI.md "Conditions"): the list, every duration form and every block of the engine's
// fixture round-tripping, the four condition-only effects as switches, the starter set (one Undo step, same as the template's file), where conditions are used
// (staff, usable item, weapon, skill), the delete follow-through, and every check. Uses the engine's fixture tests/fixtures/srpg-conditions.
import { readFileSync } from 'node:fs'
import path from 'node:path'
import { describe, expect, it } from 'vitest'
import { conditionChecks, siteChecks } from '../../src/model/conditionChecks'
import {
  cannotActOf, forbidOf, hasOwn, isControlBlock, ownBlock, ruleIndexes, setBerserk, setCannotAct, setCannotActOption, setCannotMove, setForbidGroups, setForbidKinds, setForbidOn,
} from '../../src/model/conditionBehaviour'
import { conditionUsageLines, deleteCondition } from '../../src/model/conditionRefs'
import {
  addCondition, addConditionBlock, blocksOfCondition, durationOf, duplicateCondition, moveCondition, removeConditionBlock, setDuration, setEndsWhenStruck, setReplaces, setTag, setTurns, setWearOff,
  tagOf, updateCondition,
} from '../../src/model/conditions'
import {
  itemCureOf, itemInflictOf, setItemCureIds, setItemCureMode, setItemInflictCondition, setItemInflictOn, setItemInflictTurns, setWeaponChanceFormula, setWeaponChanceKind, setWeaponChancePercent,
  setWeaponInflictCondition, setWeaponInflictOn, setWeaponInflictTurns, weaponInflictOf,
} from '../../src/model/conditionSites'
import { conditionSummary } from '../../src/model/conditionText'
import { itemChecks } from '../../src/model/itemChecks'
import { ruleIssues } from '../../src/model/skillChecks'
import { amountOf, withAmount } from '../../src/model/skillAmount'
import { staffChecks } from '../../src/model/staffChecks'
import { cureOf, inflictOf, setCureAll, setCureIds, setCureOn, setInflictCondition, setInflictOn, setInflictTurns } from '../../src/model/staffConditions'
import { setLightOn } from '../../src/model/staffEffects'
import { keptEffectCount } from '../../src/model/staffFx'
import { STARTER_CONDITIONS, installStarterConditions } from '../../src/model/starterConditions'
import { switchOffFeature, switchOnFeature } from '../../src/model/srpgFeatures'
import { cleanStat } from '../../src/model/references'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import type { Condition } from '../../src/model/srpgTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadConditionsFixture, loadTemplate, loadTemplateById, parseLoose } from './fixtures'

const fx = (): ProjectDoc => loadConditionsFixture()
const cond = (d: ProjectDoc, id: string): Condition => d.conditions!.find((c) => c.id === id)!
const item = (d: ProjectDoc, id: string) => d.items!.find((i) => i.id === id)!
const weapon = (d: ProjectDoc, id: string) => d.weapons!.find((w) => w.id === id)!
const blocking = (d: ProjectDoc) => [...conditionChecks(d), ...siteChecks(d)].filter((c) => c.blocks).map((c) => c.text)

describe('the fixture', () => {
  it('has nothing the editor would refuse (conditions, their sites, staves, items, skill rules)', () => {
    const d = fx()
    expect(blocking(d)).toEqual([])
    expect(staffChecks(d).filter((c) => c.blocks)).toEqual([])
    expect(itemChecks(d)).toEqual([])
    for (const s of d.skills ?? []) for (const b of s.blocks ?? []) expect(ruleIssues(d, b), s.id).toEqual([])
    for (const c of d.conditions!) for (const b of blocksOfCondition(c)) expect(ruleIssues(d, b, true), c.id).toEqual([])
  })
  it('reads every duration form', () => {
    const d = fx()
    expect(durationOf(cond(d, 'poison'))).toBe('turns')
    expect(durationOf(cond(d, 'mark'))).toBe('cured')
    expect(durationOf(cond(d, 'scar'))).toBe('mapEnd')
    expect(cond(d, 'fleeting').wearOff).toBe(99)
  })
  it('reads the amounts of every hurt and heal form (number, percent, formula, can defeat)', () => {
    const d = fx()
    const first = (id: string) => (blocksOfCondition(cond(d, id))[0].do as Array<Record<string, unknown>>)[0]
    const hurt = (id: string) => amountOf(first(id), 'hurt')
    expect(hurt('fever')).toMatchObject({ kind: 'number', value: 5, canKill: false })
    expect(hurt('poison')).toMatchObject({ kind: 'percent', value: 10, canKill: false })
    expect(hurt('formula_burn')).toMatchObject({ kind: 'formula', text: 'a.mag + 1' })
    expect(hurt('deadly')).toMatchObject({ kind: 'number', value: 100, canKill: true })
    const e = first('poison')
    expect(withAmount(e, 'hurt', { kind: 'number', value: 7, text: '', canKill: false })).toMatchObject({ hurt: 7, to: 'self' })
    expect(withAmount(e, 'hurt', { kind: 'percent', value: 20, text: '', canKill: true })).toMatchObject({ hurt: { percent: 20, canKill: true } })
    expect(withAmount({ heal: 4 }, 'heal', { kind: 'formula', value: 0, text: 'a.level', canKill: false })).toEqual({ heal: 'a.level' })
  })
  it('summarises a condition in words', () => {
    const d = fx()
    expect(conditionSummary(d, cond(d, 'poison'))).toMatch(/^Lasts 3 of the unit’s own turns\./)
    expect(conditionSummary(d, cond(d, 'sleep'))).toMatch(/It ends when the unit is struck\./)
    expect(conditionSummary(d, cond(d, 'chill'))).toMatch(/removes Ember/)
    expect(conditionSummary(d, cond(d, 'mark'))).toMatch(/^Lasts until it is cured\./)
    expect(conditionSummary(d, cond(d, 'poison'))).toMatch(/loses 10% of its maximum HP \(never below 1\)/)
  })
})

describe('the list', () => {
  const base = (): ProjectDoc => ({ ...loadTemplate('strategy-rpg'), conditions: [] })
  it('a new condition is valid as it stands and lowers a stat other than hit points', () => {
    const d = addCondition(base(), 'Dizzy')
    expect(d.conditions).toHaveLength(1)
    expect(d.conditions![0]).toMatchObject({ id: 'dizzy', name: 'Dizzy', turns: 3 })
    expect(blocking(d)).toEqual([])
    const stat = Object.keys(((d.conditions![0].blocks![0].do as never[])[0] as { stats: object }).stats)[0]
    expect(stat).not.toBe(d.game!.roles.hp)
  })
  it('duplicates beside the original, moves, and keeps ids unique', () => {
    let d = fx()
    d = duplicateCondition(d, 'poison')
    expect(d.conditions!.map((c) => c.id).slice(0, 3)).toEqual(['poison', 'poison_copy', 'sleep'])
    expect(new Set(d.conditions!.map((c) => c.id)).size).toBe(d.conditions!.length)
    d = moveCondition(d, 0, 1)
    expect(d.conditions![0].id).toBe('poison_copy')
  })
  it('exactly one of turns and until is ever written', () => {
    let d = fx()
    d = setDuration(d, 'poison', 'cured')
    expect(cond(d, 'poison')).not.toHaveProperty('turns')
    expect(cond(d, 'poison').until).toBe('cured')
    d = setDuration(d, 'poison', 'mapEnd')
    expect(cond(d, 'poison').until).toBe('mapEnd')
    d = setDuration(d, 'poison', 'turns')
    expect(cond(d, 'poison')).toMatchObject({ turns: 3 })
    expect(cond(d, 'poison')).not.toHaveProperty('until')
    d = setTurns(d, 'poison', 500)
    expect(cond(d, 'poison').turns).toBe(99)
    expect(blocking(d)).toEqual([])
  })
  it('wear-off, ends-when-struck, replaces and the tag edit only their own keys', () => {
    let d = fx()
    d = setWearOff(d, 'poison', 25)
    expect(cond(d, 'poison').wearOff).toBe(25)
    d = setWearOff(d, 'poison', undefined)
    expect(cond(d, 'poison')).not.toHaveProperty('wearOff')
    d = setEndsWhenStruck(d, 'poison', true)
    expect(cond(d, 'poison').endsWhenStruck).toBe(true)
    d = setEndsWhenStruck(d, 'poison', false)
    expect(cond(d, 'poison')).not.toHaveProperty('endsWhenStruck')
    d = setReplaces(d, 'poison', ['poison', 'ember'])
    expect(cond(d, 'poison').replaces).toEqual(['ember'])
    d = setTag(d, 'poison', 'Pois0n')
    expect(cond(d, 'poison').tag).toBe('Pois')
    d = setTag(d, 'poison', '')
    expect(cond(d, 'poison')).not.toHaveProperty('tag')
    expect(tagOf(cond(d, 'poison'))).toBe('Po')
    expect(blocking(d)).toEqual([])
  })
  it('the last rule cannot be deleted; rules are added up to eight', () => {
    let d = fx()
    d = removeConditionBlock(d, 'poison', 0)
    expect(blocksOfCondition(cond(d, 'poison'))).toHaveLength(1)
    for (let i = 0; i < 10; i++) d = addConditionBlock(d, 'poison')
    expect(blocksOfCondition(cond(d, 'poison'))).toHaveLength(8)
  })
})

describe('the four condition-only effects as switches', () => {
  it('finds each wherever the file has it and leaves other keys alone', () => {
    const d = fx()
    expect(cannotActOf(cond(d, 'sleep'))).toEqual({ on: true, counter: false, dodge: false })
    expect(cannotActOf(cond(d, 'daze'))).toEqual({ on: true, counter: true, dodge: true })
    expect(hasOwn(cond(d, 'bind'), 'cannotMove')).toBe(true)
    expect(forbidOf(cond(d, 'silence'))).toEqual({ on: true, kinds: ['staff'], groups: ['tome'] })
    expect(hasOwn(cond(d, 'rage'), 'berserk')).toBe(true)
    // rage: one block holds only berserk (a control block); the other is a stat rule and stays a card
    expect(ruleIndexes(cond(d, 'rage'))).toEqual([1])
    expect(isControlBlock(blocksOfCondition(cond(d, 'rage'))[0])).toBe(true)
  })
  it('cannot act and acts-on-its-own are never on together (greyed with the reason, and refused by the model)', () => {
    let d = fx()
    expect(ownBlock(cond(d, 'rage'), 'cannotAct')).toBe('berserk')
    expect(setCannotAct(d, 'rage', true)).toBe(d)
    expect(ownBlock(cond(d, 'sleep'), 'berserk')).toBe('cannotAct')
    expect(setBerserk(d, 'sleep', true)).toBe(d)
    d = setCannotAct(d, 'stun', true)
    expect(cannotActOf(cond(d, 'stun')).on).toBe(true)
    expect(blocking(d)).toEqual([])
  })
  it('cannot act: counter and dodge write the object form, both off the plain true', () => {
    let d = fx()
    d = setCannotActOption(d, 'sleep', 'counter', true)
    expect(blocksOfCondition(cond(d, 'sleep'))[0].do).toEqual([{ cannotAct: { counter: true } }, { cannotMove: true }])
    d = setCannotActOption(d, 'sleep', 'dodge', true)
    expect(cannotActOf(cond(d, 'sleep'))).toEqual({ on: true, counter: true, dodge: true })
    d = setCannotActOption(d, 'sleep', 'counter', false)
    d = setCannotActOption(d, 'sleep', 'dodge', false)
    expect(blocksOfCondition(cond(d, 'sleep'))[0].do).toEqual([{ cannotAct: true }, { cannotMove: true }])
  })
  it('switching on goes into a block of its own kind; switching off drops an empty block; the last effect stays', () => {
    let d = fx()
    d = setBerserk(d, 'curse', true)
    expect(blocksOfCondition(cond(d, 'curse'))).toHaveLength(2)
    expect(blocking(d)).toEqual([])
    d = setBerserk(d, 'curse', false)
    expect(blocksOfCondition(cond(d, 'curse'))).toHaveLength(1)
    expect(ownBlock(cond(d, 'berserk'), 'berserk')).toBe('last')
    expect(setBerserk(d, 'berserk', false)).toBe(d)
    d = setCannotMove(d, 'berserk', true)
    d = setBerserk(d, 'berserk', false)
    expect(hasOwn(cond(d, 'berserk'), 'cannotMove')).toBe(true)
  })
  it('forbid: starts with staves, kinds and groups edit, and a forbid that names nothing is flagged', () => {
    let d = setForbidOn(fx(), 'stun', true)
    expect(forbidOf(cond(d, 'stun'))).toEqual({ on: true, kinds: ['staff'], groups: [] })
    d = setForbidKinds(d, 'stun', ['weapon', 'usable'])
    d = setForbidGroups(d, 'stun', ['tome'])
    expect(forbidOf(cond(d, 'stun'))).toEqual({ on: true, kinds: ['weapon', 'usable'], groups: ['tome'] })
    expect(blocking(d)).toEqual([])
    d = setForbidKinds(setForbidGroups(d, 'stun', []), 'stun', [])
    expect(blocking(d).join(' ')).toMatch(/forbids nothing/)
    d = setForbidGroups(d, 'stun', ['nonsense'])
    expect(blocking(d).join(' ')).toMatch(/“nonsense” is no weapon or staff group/)
  })
})

describe('the starter set', () => {
  const none = (): ProjectDoc => switchOnFeature(switchOffFeature(fx(), 'conditions'), 'conditions')
  it('adds all six in one edit, in one Undo step', () => {
    const before = loadTemplate('strategy-rpg')
    const empty = { ...before, conditions: [] }
    const r = installStarterConditions(empty)
    expect(r.added).toEqual(['Poison', 'Sleep', 'Silence', 'Berserk', 'Stun', 'Curse'])
    expect(r.doc.conditions!.map((c) => c.id)).toEqual(['poison', 'sleep', 'silence', 'berserk', 'stun', 'curse'])
    let h = initHistory<ProjectDoc>(empty)
    h = pushEdit(h, r.doc, undefined, 0)
    expect(undo(h).present.conditions).toEqual([])
    expect(blocking(r.doc)).toEqual([])
  })
  it('skips ids the game already has, and says so', () => {
    const d = installStarterConditions(none()).doc
    const again = installStarterConditions(updateCondition(d, 'poison', { name: 'Venom' }))
    expect(again.added).toEqual([])
    expect(again.existing).toHaveLength(6)
    expect(again.doc).toBe(updateCondition(d, 'poison', { name: 'Venom' }) as never === again.doc ? again.doc : again.doc)
    expect(cond(again.doc, 'poison').name).toBe('Venom')
    const fewer = { ...d, conditions: d.conditions!.filter((c) => c.id !== 'stun') }
    const filled = installStarterConditions(fewer)
    expect(filled.added).toEqual(['Stun'])
    expect(filled.doc.conditions).toHaveLength(6)
  })
  it('is the template’s file (the curse uses the game’s own stats)', () => {
    const file = parseLoose(readFileSync(path.resolve(import.meta.dirname, '../../../templates/strategy-rpg/data/conditions.json'), 'utf8')).conditions as Condition[]
    const installed = installStarterConditions({ ...loadTemplate('strategy-rpg'), conditions: [] }).doc.conditions!
    expect(installed).toEqual(file)
    expect(STARTER_CONDITIONS.map((c) => c.id)).toEqual(file.map((c) => c.id))
  })
  it('skips the curse in a game with no attack or defense stat to lower', () => {
    const d = { ...none(), game: { ...none().game!, stats: [{ id: 'hp', name: 'HP' }], roles: { hp: 'hp', move: 'hp' }, formulas: {} } }
    const r = installStarterConditions(d as ProjectDoc)
    expect(r.missingStat).toEqual(['Curse'])
    expect(r.added).toHaveLength(5)
  })
})

describe('staff parts: inflicts and cures', () => {
  it('reads the fixture’s staves', () => {
    const d = fx()
    expect(inflictOf(item(d, 'sleep_staff'))).toEqual({ on: true, condition: 'sleep', turns: 2, editable: true })
    expect(cureOf(item(d, 'cure_staff'))).toEqual({ on: true, all: false, ids: ['poison'], editable: true })
    expect(cureOf(item(d, 'purify_staff')).all).toBe(true)
    expect(keptEffectCount(item(d, 'mend_cure_staff'))).toBe(0) // heal and cure are both parts of the form: nothing is left over
  })
  it('switches inflict on (the first condition), sets condition and turns, drops turns for a condition that does not count them', () => {
    let d = fx()
    d = setInflictOn(d, 'cure_staff', true)
    expect(item(d, 'cure_staff').effects).toEqual([{ cure: ['poison'] }, { inflict: { condition: 'poison' } }])
    d = setInflictTurns(d, 'cure_staff', 4)
    expect(inflictOf(item(d, 'cure_staff')).turns).toBe(4)
    d = setInflictCondition(d, 'cure_staff', 'mark')
    expect(item(d, 'cure_staff').effects![1]).toEqual({ inflict: { condition: 'mark' } })
    expect(blocking(d)).toEqual([])
    expect(staffChecks(d).filter((c) => c.blocks)).toEqual([])
    d = setInflictOn(d, 'cure_staff', false)
    expect(item(d, 'cure_staff').effects).toEqual([{ cure: ['poison'] }])
  })
  it('the last effect cannot be switched off, a second one of a kind is not made, four is the most', () => {
    let d = fx()
    expect(setInflictOn(d, 'poison_staff', false)).toBe(d)
    expect(setCureOn(d, 'cure_staff', false)).toBe(d)
    expect(setInflictOn(d, 'poison_staff', true)).toBe(d)
    d = setCureOn(d, 'poison_staff', true)
    expect(item(d, 'poison_staff').effects).toHaveLength(2)
    d = { ...d, items: d.items!.map((i) => (i.id === 'poison_staff' ? { ...i, target: 'ally', effects: [{ heal: 1 }, { repair: 'full' }, { inflict: { condition: 'poison' } }, { cure: 'all' }] } : i)) }
    expect(staffChecks(d).filter((c) => c.blocks)).toEqual([])
    expect(setCureOn({ ...d, items: d.items!.map((i) => (i.id === 'poison_staff' ? { ...i, effects: [{ heal: 1 }, { repair: 'full' }, { inflict: { condition: 'poison' } }] } : i)) }, 'poison_staff', true).items!.find((i) => i.id === 'poison_staff')!.effects).toHaveLength(4)
  })
  it('cure: list or every condition; the list is never emptied', () => {
    let d = setCureAll(fx(), 'cure_staff', true)
    expect(item(d, 'cure_staff').effects).toEqual([{ cure: 'all' }])
    d = setCureAll(d, 'cure_staff', false)
    expect(item(d, 'cure_staff').effects).toEqual([{ cure: ['poison'] }])
    d = setCureIds(d, 'cure_staff', ['poison', 'sleep'])
    expect(cureOf(item(d, 'cure_staff')).ids).toEqual(['poison', 'sleep'])
    expect(setCureIds(d, 'cure_staff', [])).toBe(d)
  })
  it('area and heal go along with them; a light staff has neither (and switching light on replaces them)', () => {
    const d = fx()
    expect(item(d, 'sleep_staff').area).toEqual({ radius: 1 })
    expect(staffChecks(d).filter((c) => c.blocks)).toEqual([])
    expect(setInflictOn(setLightOn(d, 'cure_staff', true), 'cure_staff', true)).toEqual(setLightOn(d, 'cure_staff', true))
    expect(setLightOn(d, 'cure_staff', true).items!.find((i) => i.id === 'cure_staff')!.effects).toEqual([{ light: { radius: 2, turns: 1 } }])
  })
  it('an effect the form does not know is kept untouched', () => {
    let d = fx()
    d = { ...d, items: d.items!.map((i) => (i.id === 'cure_staff' ? { ...i, effects: [{ cure: ['poison'] }, { zzz: 1 }] } : i)) }
    d = setInflictOn(d, 'cure_staff', true)
    expect(item(d, 'cure_staff').effects).toEqual([{ cure: ['poison'] }, { zzz: 1 }, { inflict: { condition: 'poison' } }])
    expect(keptEffectCount(item(d, 'cure_staff'))).toBe(1)
  })
  it('mirrors the loader: unknown condition, two of a kind, empty or repeated cure list, turns on a condition that does not count them', () => {
    const bad = (effects: Array<Record<string, unknown>>): string[] => {
      const d = fx()
      return staffChecks({ ...d, items: d.items!.map((i) => (i.id === 'cure_staff' ? { ...i, effects } : i)) }).filter((c) => c.blocks).map((c) => c.text)
    }
    expect(bad([{ inflict: { condition: 'nothing' } }]).join()).toMatch(/the condition “nothing” does not exist/)
    expect(bad([{ inflict: { condition: 'poison' } }, { inflict: { condition: 'sleep' } }]).join()).toMatch(/only one “inflicts a condition”/)
    expect(bad([{ cure: ['poison'] }, { cure: 'all' }]).join()).toMatch(/only one “cures”/)
    expect(bad([{ cure: [] }]).join()).toMatch(/needs a list of conditions/)
    expect(bad([{ cure: ['poison', 'poison'] }]).join()).toMatch(/listed twice/)
    expect(bad([{ cure: ['nothing'] }]).join()).toMatch(/does not exist/)
    expect(bad([{ inflict: { condition: 'mark', turns: 2 } }]).join()).toMatch(/does not count turns/)
    expect(bad([{ inflict: { condition: 'poison', turns: 0 } }]).join()).toMatch(/from 1 to 99/)
    expect(bad([{ inflict: { condition: 'poison', extra: 1 } }]).join()).toMatch(/only a condition and optional turns/)
    expect(bad([{ inflict: { condition: 'poison' } }, { light: { radius: 1 } }]).join()).toMatch(/light/)
    // a game with no conditions says so
    const d = switchOffFeature(fx(), 'conditions')
    expect(staffChecks(d).filter((c) => c.blocks).map((c) => c.text).join()).toMatch(/the game has no conditions yet/)
  })
})

describe('usable items and weapons', () => {
  it('item: cures none / list / all, and inflicts with optional turns', () => {
    let d = fx()
    expect(itemCureOf(item(d, 'antidote'))).toEqual({ mode: 'list', ids: ['poison'] })
    expect(itemCureOf(item(d, 'tonic')).mode).toBe('all')
    d = setItemCureMode(d, 'antidote', 'all')
    expect(item(d, 'antidote').cure).toBe('all')
    d = setItemCureMode(d, 'antidote', 'list')
    expect(item(d, 'antidote').cure).toEqual(['poison'])
    d = setItemCureIds(d, 'antidote', ['poison', 'sleep'])
    expect(item(d, 'antidote').cure).toEqual(['poison', 'sleep'])
    d = setItemCureIds(d, 'antidote', [])
    expect(item(d, 'antidote')).not.toHaveProperty('cure')
    d = setItemInflictOn(d, 'antidote', true)
    expect(item(d, 'antidote').inflict).toEqual({ condition: 'poison' })
    d = setItemInflictTurns(d, 'antidote', 2)
    expect(item(d, 'antidote').inflict).toEqual({ condition: 'poison', turns: 2 })
    d = setItemInflictCondition(d, 'antidote', 'mark')
    expect(item(d, 'antidote').inflict).toEqual({ condition: 'mark' })
    expect(itemInflictOf(item(d, 'antidote'))).toMatchObject({ on: true, condition: 'mark' })
    expect(itemChecks(d)).toEqual([])
    d = setItemInflictOn(d, 'antidote', false)
    expect(item(d, 'antidote')).not.toHaveProperty('inflict')
  })
  it('item: a usable item may have only a cure; an accessory with one is refused; an unknown or repeated condition is refused', () => {
    const d = fx()
    const texts = (i: object) => itemChecks({ ...d, items: [...d.items!, { id: 'x', name: 'X', ...i } as never] }).map((c) => c.text).join()
    expect(texts({ kind: 'usable', cure: ['poison'] })).toBe('')
    expect(texts({ kind: 'accessory', bonus: { str: 1 }, cure: ['poison'] })).toMatch(/only a usable item can cure/)
    expect(texts({ kind: 'usable', cure: ['nothing'] })).toMatch(/“nothing” does not exist/)
    expect(texts({ kind: 'usable', cure: ['poison', 'poison'] })).toMatch(/twice/)
    expect(texts({ kind: 'usable', inflict: { condition: 'mark', turns: 2 } })).toMatch(/does not count turns/)
    expect(texts({ kind: 'usable', heal: 1, inflict: { condition: 'nothing' } })).toMatch(/does not exist/)
  })
  it('weapon: on, condition, every hit / percent / formula, turns', () => {
    let d = fx()
    expect(weaponInflictOf(weapon(d, 'venom_blade'))).toMatchObject({ on: true, condition: 'poison', chanceKind: 'always', turns: 2 })
    expect(weaponInflictOf(weapon(d, 'lull_blade'))).toMatchObject({ chanceKind: 'formula', formula: 'a.mag * 25' })
    d = setWeaponInflictOn(d, 'short_sword', true)
    expect(weapon(d, 'short_sword').inflict).toEqual({ condition: 'poison', chance: 30 })
    d = setWeaponChancePercent(d, 'short_sword', 45)
    expect(weapon(d, 'short_sword').inflict).toEqual({ condition: 'poison', chance: 45 })
    d = setWeaponChanceKind(d, 'short_sword', 'formula')
    expect((weapon(d, 'short_sword').inflict as { chance: string }).chance).toMatch(/^60 \+ a\.\w+ - b\.\w+ \* 5$/)
    d = setWeaponChanceFormula(d, 'short_sword', 'a.mag * 20')
    d = setWeaponInflictTurns(d, 'short_sword', 3)
    expect(weapon(d, 'short_sword').inflict).toEqual({ condition: 'poison', chance: 'a.mag * 20', turns: 3 })
    d = setWeaponChanceKind(d, 'short_sword', 'always')
    expect(weapon(d, 'short_sword').inflict).toEqual({ condition: 'poison', turns: 3 })
    d = setWeaponInflictCondition(d, 'short_sword', 'mark')
    expect(weapon(d, 'short_sword').inflict).toEqual({ condition: 'mark' })
    expect(blocking(d)).toEqual([])
    d = setWeaponInflictOn(d, 'short_sword', false)
    expect(weapon(d, 'short_sword')).not.toHaveProperty('inflict')
  })
  it('weapon: unknown condition, bad chance and turns on an uncounted condition are refused', () => {
    const d = fx()
    const texts = (inflict: object) => siteChecks({ ...d, weapons: d.weapons!.map((w) => (w.id === 'stick' ? { ...w, inflict: inflict as never } : w)) }).map((c) => c.text).join()
    expect(texts({ condition: 'poison', chance: 30 })).toBe('')
    expect(texts({ condition: 'nothing' })).toMatch(/does not exist/)
    expect(texts({ condition: 'poison', chance: 0 })).toMatch(/percent from 1 to 100/)
    expect(texts({ condition: 'poison', chance: '' })).toMatch(/percent from 1 to 100/)
    expect(texts({ condition: 'mark', turns: 2 })).toMatch(/does not count turns/)
    expect(texts({ condition: 'poison', nope: 1 })).toMatch(/only a condition, a chance and optional turns/)
  })
})

describe('skill rules about conditions', () => {
  it('immune and inflict effects and the condition test are offered and checked', () => {
    const d = fx()
    expect(ruleIssues(d, { when: 'always', do: [{ immune: ['poison'] }] })).toEqual([])
    expect(ruleIssues(d, { when: 'always', do: [{ immune: [] }] }).join()).toMatch(/immune to nothing/)
    expect(ruleIssues(d, { when: 'always', do: [{ immune: ['nothing'] }] }).join()).toMatch(/“nothing” does not exist/)
    expect(ruleIssues(d, { when: 'strike', chance: 30, do: [{ inflict: { condition: 'poison', turns: 2 } }] })).toEqual([])
    expect(ruleIssues(d, { when: 'struck', do: [{ inflict: { condition: 'mark', turns: 2 } }] }).join()).toMatch(/does not count turns/)
    expect(ruleIssues(d, { when: 'turnStart', do: [{ inflict: { condition: 'poison' } }] }).join()).toMatch(/cannot be used/)
    expect(ruleIssues(d, { when: 'combat', if: [{ condition: 'poison', who: 'foe' }], do: [{ damage: 2 }] })).toEqual([])
    expect(ruleIssues(d, { when: 'combat', if: [{ condition: 'nothing' }], do: [{ damage: 2 }] }).join()).toMatch(/does not exist/)
    expect(ruleIssues(d, { when: 'always', do: [{ cannotAct: true }] }).join()).toMatch(/belongs to a status condition/)
    expect(ruleIssues(d, { when: 'always', do: [{ cannotAct: true }] }, true)).toEqual([])
  })
  it('a condition cannot change hit points, and cannot both stop acting and act on its own', () => {
    const d = fx()
    const hp = d.game!.roles.hp
    expect(ruleIssues(d, { when: 'always', do: [{ stats: { [hp]: 1 } }] }, true).join()).toMatch(/cannot change hit points/)
    const clash = { ...d, conditions: d.conditions!.map((c) => (c.id === 'stun' ? { ...c, blocks: [{ when: 'always', do: [{ cannotAct: true }, { berserk: true }] }] } : c)) }
    expect(blocking(clash).join()).toMatch(/contradict each other/)
  })
  it('hurt: self only at the start of a turn, foe only after combat, amounts legal', () => {
    const d = fx()
    expect(ruleIssues(d, { when: 'turnStart', do: [{ hurt: { percent: 10 }, to: 'self' }] }, true)).toEqual([])
    expect(ruleIssues(d, { when: 'afterCombat', do: [{ hurt: 3, to: 'self' }] }).join()).toMatch(/start of a turn/)
    expect(ruleIssues(d, { when: 'turnStart', do: [{ hurt: { percent: 150 }, to: 'self' }] }).join()).toMatch(/1 to 100/)
    expect(ruleIssues(d, { when: 'turnStart', do: [{ hurt: { formula: ' ' }, to: 'self' }] }).join()).toMatch(/formula is empty/)
    expect(ruleIssues(d, { when: 'turnStart', do: [{ heal: 'a.level * 2' }] })).toEqual([])
  })
})

describe('mirrors the loader on the condition itself', () => {
  const t = (patch: object): string => {
    const d = fx()
    return blocking({ ...d, conditions: d.conditions!.map((c) => (c.id === 'stun' ? ({ ...c, ...patch } as Condition) : c)) }).join(' | ')
  }
  it('flags a missing or doubled duration, out-of-range numbers, a bad tag, and replaces that names nothing or itself', () => {
    expect(t({ turns: undefined })).toMatch(/needs to say how long it lasts/)
    expect(t({ until: 'cured' })).toMatch(/both a number of turns and “until”/)
    expect(t({ turns: 0 })).toMatch(/turns must be a whole number from 1 to 99/)
    expect(t({ turns: 100 })).toMatch(/from 1 to 99/)
    expect(t({ turns: undefined, until: 'forever' })).toMatch(/until” cured or until the map ends/)
    expect(t({ wearOff: 100 })).toMatch(/from 1 to 99/)
    expect(t({ wearOff: 0 })).toMatch(/from 1 to 99/)
    expect(t({ tag: '' })).toMatch(/1 to 4 characters/)
    expect(t({ tag: 'Toolong' })).toMatch(/1 to 4 characters/)
    expect(t({ replaces: ['stun'] })).toMatch(/replaces itself/)
    expect(t({ replaces: ['nothing'] })).toMatch(/“nothing”, which is not a condition/)
    expect(t({ blocks: [] })).toMatch(/at least one rule/)
    expect(t({ blocks: Array.from({ length: 9 }, () => ({ when: 'always', do: [{ cannotMove: true }] })) })).toMatch(/more than 8 rules/)
    expect(t({ name: '' })).toMatch(/needs a name/)
  })
  it('flags a duplicate id and a rule the loader would refuse', () => {
    const d = fx()
    expect(blocking({ ...d, conditions: [...d.conditions!, { ...cond(d, 'stun') }] }).join()).toMatch(/same id/)
    expect(t({ blocks: [{ when: 'turnStart', do: [{ cannotMove: true }] }] })).toMatch(/cannot be used in a rule for the start of a turn/)
    expect(t({ blocks: [{ when: 'always', do: [{ hit: 5 }] }] })).toMatch(/cannot be used in a rule that always applies/)
  })
})

describe('worth a look', () => {
  it('a condition nothing can inflict; an until-cured condition nothing cures', () => {
    let d = fx()
    const notes = (x: ProjectDoc) => conditionChecks(x).filter((c) => !c.blocks).map((c) => c.text)
    d = addCondition(d, 'Lonely')
    d = setDuration(d, 'lonely', 'cured')
    expect(notes(d).join('|')).toMatch(/Lonely: nothing can put this on a unit yet/)
    expect(notes(d).filter((t) => /Lonely/.test(t)).join('|')).not.toMatch(/nothing cures it/) // purify_staff cures every condition
    d = { ...d, items: d.items!.filter((i) => i.id !== 'purify_staff' && i.id !== 'tonic' && i.id !== 'cleanse_staff') }
    expect(notes(d).join('|')).toMatch(/Lonely: it lasts until it is cured, but nothing cures it/)
    // a staff that inflicts it removes the first note; "cure all" removes the second
    d = setInflictCondition(setInflictOn(d, 'cure_staff', true), 'cure_staff', 'lonely')
    expect(notes(d).filter((t) => /Lonely/.test(t)).join('|')).not.toMatch(/nothing can put this/)
    d = setCureOn(d, 'poison_staff', true)
    d = setCureAll(d, 'poison_staff', true)
    expect(notes(d).filter((t) => /Lonely/.test(t)).join('|')).not.toMatch(/nothing cures it/)
  })
  it('another condition replacing it counts as a cure; an event command counts as both', () => {
    let d = fx()
    d = setDuration(d, 'ember', 'cured')
    expect(conditionChecks(d).filter((c) => !c.blocks && /Ember: it lasts/.test(c.text))).toEqual([]) // chill replaces ember
    d = addCondition(d, 'Lonely')
    const map = d.maps[Object.keys(d.maps)[0]]!
    d = { ...d, maps: { ...d.maps, [Object.keys(d.maps)[0]]: { ...map, events: [...(map.events ?? []), { id: 'e1', when: { battle: 'start' }, do: [{ addCondition: 'lonely', to: {} }] }] } } }
    expect(conditionChecks(d).filter((c) => !c.blocks && /Lonely/.test(c.text))).toEqual([])
  })
})

describe('deleting a condition', () => {
  it('lists every place that uses it', () => {
    const d = fx()
    const lines = conditionUsageLines(d, 'poison').map((u) => u.text).join('\n')
    expect(lines).toMatch(/Staff “Poison Staff” inflicts it/)
    expect(lines).toMatch(/Staff “Cure Staff” cures it/)
    expect(lines).toMatch(/Item “Antidote” cures it/)
    expect(lines).toMatch(/Weapon “Venom Blade” may inflict it/)
  })
  it('removes or cleans each use in one edit, and what is left loads', () => {
    const d = fx()
    const gone = deleteCondition(d, 'poison')
    expect(gone.conditions!.some((c) => c.id === 'poison')).toBe(false)
    expect(blocking(gone)).toEqual([])
    expect(staffChecks(gone).filter((c) => c.blocks)).toEqual([])
    // (the antidote only cured poison: it now does nothing, and the delete question said so)
    expect(itemChecks(gone).map((c) => c.text)).toEqual([expect.stringMatching(/^Antidote: a usable item needs to heal or raise/)])
    expect(conditionUsageLines(d, 'poison').map((u) => u.text).join('\n')).toMatch(/Antidote” cures it.*the item then does nothing/)
    // the staff that only inflicted it heals instead; the usable item that only cured it loses the key (it then needs something else: the item check says so)
    expect(item(gone, 'poison_staff').effects).toEqual([{ heal: 10 }])
    expect(item(gone, 'poison_staff').target).toBeUndefined()
    expect(item(gone, 'antidote')).not.toHaveProperty('cure')
    expect(item(gone, 'mend_cure_staff').effects).toEqual([{ heal: 5 }])
    expect(weapon(gone, 'venom_blade')).not.toHaveProperty('inflict')
    // untouched sections keep their identity
    expect(gone.units).toBe(d.units)
  })
  it('immune lists, inflict effects and the rules that test the condition (skills and other conditions), and replaces', () => {
    let d = fx()
    d = { ...d, skills: [...(d.skills ?? []), { id: 'ward', name: 'Ward', blocks: [{ when: 'always', do: [{ immune: ['ember', 'poison'] }] }, { when: 'strike', chance: 30, do: [{ inflict: { condition: 'ember' } }, { damage: 1 }] }, { when: 'combat', if: [{ condition: 'ember', who: 'foe' }], do: [{ damage: 2 }] }] }] }
    const lines = conditionUsageLines(d, 'ember').map((u) => u.text).join('\n')
    expect(lines).toMatch(/Skill “Ward” makes its owner immune to it/)
    expect(lines).toMatch(/Skill “Ward”, rule 2, inflicts it/)
    expect(lines).toMatch(/Skill “Ward”, rule 3, only applies when the unit has it/)
    expect(lines).toMatch(/Condition “Chill” replaces it/)
    const gone = deleteCondition(d, 'ember')
    const ward = gone.skills!.find((s) => s.id === 'ward')!
    expect(ward.blocks).toEqual([{ when: 'always', do: [{ immune: ['poison'] }] }, { when: 'strike', chance: 30, do: [{ damage: 1 }] }])
    expect(cond(gone, 'chill')).not.toHaveProperty('replaces')
    expect(blocking(gone)).toEqual([])
  })
  it('the requirement goes from an ability or a staff that asked about it', () => {
    let d = fx()
    d = { ...d, abilities: [{ id: 'cleanse', name: 'Cleanse', target: 'ally', effect: { heal: 'full' }, if: [{ condition: 'poison' }, { hp: { op: '<=', percent: 50 } }] }] }
    const gone = deleteCondition(d, 'poison')
    expect(gone.abilities![0].if).toEqual([{ hp: { op: '<=', percent: 50 } }])
  })
  it('deleting a stat takes it out of conditions too; a condition left with no rule is deleted with its uses', () => {
    const d = fx()
    const mov = cleanStat(d, 'mov')
    expect(cond(mov, 'chill')).toBeUndefined()
    expect(blocking(mov)).toEqual([])
    const def = cleanStat(d, 'def')
    expect(cond(def, 'mark')).toBeUndefined()
    expect(cond(def, 'curse').blocks).toEqual([{ when: 'always', do: [{ stats: { str: -2 } }] }])
  })
  it('turning the feature off removes the conditions and every use', () => {
    const off = switchOffFeature(fx(), 'conditions')
    expect(off.conditions).toBeNull()
  })
})

describe('the template', () => {
  it('has the starter set and nothing refused', () => {
    const d = loadTemplateById('strategy-rpg')
    expect(d.conditions!.map((c) => c.id)).toEqual(['poison', 'sleep', 'silence', 'berserk', 'stun', 'curse'])
    expect(blocking(d)).toEqual([])
  })
})
