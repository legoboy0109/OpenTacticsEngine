// New engine keys in the editor: strike abilities, heal percent / formula, spend, unit resources (game.json), the weapon's hit formula,
// an item's "teaches" and rules.abilityPoints (docs/SRPG.md, docs/JOBS.md). Checks must say what the loader refuses; edits must write no defaults.
import { describe, expect, it } from 'vitest'
import {
  addAbility, healKind, setEffectKind, setHealKind, setHealValue, setSpend, setStrikeWeapon, setTarget, strikeWeapon,
} from '../../src/model/abilities'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilitySummary } from '../../src/model/abilityText'
import { healChecks, spendChecks, strikeChecks } from '../../src/model/abilityStrikeChecks'
import { abilityPointsChecks, setAbilityPointsOn, updateAbilityPoints } from '../../src/model/abilityPoints'
import { findUsages, cleanStat } from '../../src/model/references'
import { removeResource } from '../../src/model/rulesLists'
import { setItemKind } from '../../src/model/items'
import { deleteAbility, deleteSkill, abilityUsageLines } from '../../src/model/srpgRefs'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { itemChecks } from '../../src/model/itemChecks'
import { addTeach, dropTeach, pointsFor, removeTeach, setTeachEntry, setTeachPoints, teachChecks, teachesOf } from '../../src/model/teaches'
import {
  addUnitResource, dropPoolsOfStat, moveUnitResource, newUnitResourceId, removeUnitResource, unitResourceChecks, updateUnitResource,
} from '../../src/model/unitResources'
import { deleteWeapon, setWeaponHit, weaponHitChecks } from '../../src/model/weapons'
import type { Ability } from '../../src/model/srpgTypes'
import type { AbilityPointRules, Item, ProjectDoc, Weapon } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => {
  const d = switchOnFeature(loadDemo('demo-srpg'), 'abilities')
  const weapons: Weapon[] = [...(d.weapons ?? []), { id: 'fire_spell', name: 'Fire', range: [1, 4] }]
  return { ...d, weapons }
}
const stat = (d: ProjectDoc) => d.game!.stats.find((s) => s.id !== d.game!.roles.hp && s.id !== d.game!.roles.move)?.id ?? d.game!.stats[0].id
const withAbility = (d: ProjectDoc, a: Ability): ProjectDoc => ({ ...d, abilities: [...(d.abilities ?? []), a] })
const one = (d: ProjectDoc): Ability => d.abilities![d.abilities!.length - 1]
const accessory = (id: string, teaches?: Item['teaches']): Item => ({ id, name: id, kind: 'accessory', bonus: { [''] : 0 }, skills: [], ...(teaches ? { teaches } : {}) } as Item)

describe('abilities that strike', () => {
  it('switching to strike takes target, range and a free cost away, and points at the first weapon', () => {
    let d = addAbility(base(), 'Blast')
    d = { ...d, abilities: [{ ...one(d), range: [1, 3], cost: 'free' }] }
    const a = setEffectKind(d, one(d), 'strike')
    expect(a.effect).toEqual({ strike: d.weapons![0].id })
    expect(a.target).toBeUndefined()
    expect(a.range).toBeUndefined()
    expect(a.cost).toBeUndefined()
    expect(setTarget(a, 'enemy')).toBe(a)  // a strike takes no target
  })
  it('switching away from a strike gives the target back', () => {
    const d = addAbility(base(), 'Blast')
    const s = setEffectKind(d, one(d), 'strike')
    const back = setEffectKind(d, s, 'push')
    expect(back.target).toBe('unit')
    expect(back.effect).toEqual({ push: 1 })
  })
  it('picks the weapon and reads it back', () => {
    let d = addAbility(base(), 'Blast')
    d = { ...d, abilities: [setStrikeWeapon(setEffectKind(d, one(d), 'strike'), 'fire_spell')] }
    expect(strikeWeapon(one(d))).toBe('fire_spell')
    expect(abilitySummary(d, one(d))).toMatch(/ttacks with Fire/)
    expect(abilityChecks(d).filter((c) => !c.text.includes('nobody has'))).toEqual([])
  })
  it('refuses an unknown weapon, a left-over target or range, a free cost and a weapon that charges', () => {
    const d = base()
    const ok: Ability = { id: 'b', name: 'Blast', effect: { strike: 'fire_spell' } }
    expect(strikeChecks(d, ok)).toEqual([])
    expect(strikeChecks(d, { ...ok, effect: { strike: 'nope' } })[0]).toMatch(/not a weapon of this game/)
    expect(strikeChecks(d, { ...ok, effect: { strike: '' } })[0]).toMatch(/pick the weapon/)
    expect(strikeChecks(d, { ...ok, target: 'enemy' })[0]).toMatch(/Leave out the target and the range/)
    expect(strikeChecks(d, { ...ok, range: [1, 2] })[0]).toMatch(/Leave out the target and the range/)
    expect(strikeChecks(d, { ...ok, cost: 'free' })[0]).toMatch(/cannot be free/)
    const charging = { ...d, weapons: d.weapons!.map((w) => (w.id === 'fire_spell' ? { ...w, chargeSpeed: 20 } : w)) }
    expect(strikeChecks(charging, ok)[0]).toMatch(/charges by itself/)
  })
  it('the shape checks do not ask a strike for a target', () => {
    const d = withAbility(base(), { id: 'b', name: 'Blast', effect: { strike: 'fire_spell' } })
    expect(abilityChecks(d).filter((c) => c.ids?.includes('b') && !c.text.includes('nobody has'))).toEqual([])
  })
})

describe('healing by percent and formula', () => {
  const heal = (h: unknown): Ability => ({ id: 'h', name: 'Cure', target: 'unit', effect: { heal: h as number } })
  it('tells the kinds apart and starts each with a valid value', () => {
    expect(healKind(heal(10))).toBe('amount')
    expect(healKind(heal('full'))).toBe('full')
    expect(healKind(heal({ percent: 30 }))).toBe('percent')
    expect(healKind(heal('a.mag * 3'))).toBe('formula')
    for (const k of ['amount', 'full', 'percent', 'formula'] as const) expect(healChecks(setHealKind(heal(10), k))).toEqual([])
  })
  it('keeps percent within 1 to 100 and the formula as typed', () => {
    expect(setHealValue(heal({ percent: 30 }), 150).effect).toEqual({ heal: { percent: 100 } })
    expect(setHealValue(heal({ percent: 30 }), 0).effect).toEqual({ heal: { percent: 1 } })
    expect(setHealValue(heal('a.mag'), 'a.mag * 3').effect).toEqual({ heal: 'a.mag * 3' })
    expect(setHealValue(heal(10), 'x')).toEqual(heal(10))  // a number heals by setEffectAmount, not here
  })
  it('refuses what the loader refuses', () => {
    expect(healChecks(heal({ percent: 0 }))).toHaveLength(1)
    expect(healChecks(heal({ percent: 101 }))).toHaveLength(1)
    expect(healChecks(heal({ percent: 5, extra: 1 }))).toHaveLength(1)
    expect(healChecks(heal(''))).toHaveLength(1)
    expect(healChecks(heal(0))).toHaveLength(1)
    expect(healChecks(heal(1000))).toHaveLength(1)
    expect(healChecks(heal(true))).toHaveLength(1)
    expect(healChecks(heal({ percent: 100 }))).toEqual([])
  })
  it('says it in the summary', () => {
    const d = base()
    expect(abilitySummary(d, heal({ percent: 30 }))).toMatch(/30% of its maximum/)
    expect(abilitySummary(d, heal('a.mag * 3'))).toMatch(/formula \(a\.mag \* 3\)/)
  })
})

describe('what an ability spends', () => {
  const pool = (d: ProjectDoc): ProjectDoc => addUnitResource(d, 'MP')
  it('writes amounts 1 to 999 and removes the key when empty', () => {
    let a: Ability = { id: 'x', name: 'X', effect: { push: 1 } }
    a = setSpend(a, 'mp', 6)
    expect(a.spend).toEqual({ mp: 6 })
    expect(setSpend(a, 'mp', 5000).spend).toEqual({ mp: 999 })
    expect(setSpend(a, 'mp', 0).spend).toEqual({ mp: 1 })
    expect('spend' in setSpend(a, 'mp', undefined)).toBe(false)
  })
  it('accepts unit resources and side resources, refuses unknown ids and bad amounts', () => {
    let d = pool(base())
    d = { ...d, game: { ...d.game!, resources: [{ id: 'gold', name: 'Gold' }] } }
    const a: Ability = { id: 'x', name: 'X', effect: { push: 1 }, spend: { mp: 6, gold: 2 } }
    expect(spendChecks(d, a)).toEqual([])
    expect(spendChecks(d, { ...a, spend: { nothing: 1 } })[0]).toMatch(/not a resource of this game/)
    expect(spendChecks(d, { ...a, spend: { mp: 0 } })[0]).toMatch(/1 to 999/)
    expect(spendChecks(d, { ...a, spend: { mp: 1.5 } })[0]).toMatch(/1 to 999/)
    expect(spendChecks(d, { ...a, spend: [] as unknown as Record<string, number> })[0]).toMatch(/must list resources/)
  })
  it('shows the price in the summary with the pool’s name', () => {
    const d = pool(base())
    expect(abilitySummary(d, { id: 'x', name: 'X', effect: { push: 1 }, target: 'unit', spend: { mp: 6 } })).toMatch(/costs 6 MP/)
  })
  it('a resource that is deleted leaves the abilities that paid in it', () => {
    let d: ProjectDoc = { ...base(), game: { ...base().game!, resources: [{ id: 'gold', name: 'Gold' }] } }
    d = withAbility(d, { id: 'x', name: 'X', effect: { push: 1 }, target: 'unit', spend: { gold: 3 } })
    expect(findUsages(d, 'resource', 'gold').some((u) => u.section === 'abilities')).toBe(true)
    d = removeResource(d, 'gold')
    expect(one(d).spend).toBeUndefined()
  })
})

describe('unit resources (game.json)', () => {
  it('adds a pool on a stat that is not hit points or movement, with no defaults written', () => {
    const d = addUnitResource(base(), 'MP')
    expect(d.game!.unitResources).toEqual([{ id: 'mp', name: 'MP', max: stat(d) }])
  })
  it('never makes an id equal to a side resource, nor adds a fifth', () => {
    let d: ProjectDoc = { ...base(), game: { ...base().game!, resources: [{ id: 'mp', name: 'Funds' }] } }
    expect(newUnitResourceId(d, 'MP')).toBe('mp_2')
    d = addUnitResource(d, 'A')
    for (const n of ['B', 'C', 'D', 'E']) d = addUnitResource(d, n)
    expect(d.game!.unitResources).toHaveLength(4)
  })
  it('keeps start 100 and regeneration 0 out of the file, and removes an empty block', () => {
    let d = addUnitResource(base(), 'MP')
    d = updateUnitResource(d, 'mp', { start: 50, perTurn: 2, percent: 10 })
    expect(d.game!.unitResources![0]).toMatchObject({ start: 50, regen: { perTurn: 2, percent: 10 } })
    d = updateUnitResource(d, 'mp', { start: 100, perTurn: 0, percent: 0 })
    expect(d.game!.unitResources![0]).toEqual({ id: 'mp', name: 'MP', max: stat(d) })
    d = updateUnitResource(d, 'mp', { start: 500, perTurn: 5000 })
    expect(d.game!.unitResources![0]).toMatchObject({ regen: { perTurn: 999 } })
    expect('start' in d.game!.unitResources![0]).toBe(false)  // 500 clamps to 100, the default
  })
  it('moves, and a deleted pool takes the key and what abilities paid in it', () => {
    let d = addUnitResource(addUnitResource(base(), 'MP'), 'Rage')
    d = moveUnitResource(d, 1, 0)
    expect(d.game!.unitResources!.map((r) => r.id)).toEqual(['rage', 'mp'])
    d = withAbility(d, { id: 'x', name: 'X', effect: { push: 1 }, target: 'unit', spend: { mp: 3, rage: 1 } })
    d = removeUnitResource(d, 'mp')
    expect(one(d).spend).toEqual({ rage: 1 })
    d = removeUnitResource(d, 'rage')
    expect(d.game!.unitResources).toBeUndefined()
    expect(one(d).spend).toBeUndefined()
  })
  it('a stat that goes takes its pools with it', () => {
    let d = addUnitResource(base(), 'MP')
    expect(findUsages(d, 'stat', stat(d)).some((u) => u.text.includes('Unit resource'))).toBe(true)
    d = cleanStat(d, stat(d))
    expect(d.game!.unitResources).toBeUndefined()
    expect(dropPoolsOfStat(addUnitResource(base(), 'MP'), 'zzz').game!.unitResources).toHaveLength(1)
  })
  it('checks what the loader refuses', () => {
    const d = base()
    const s = stat(d)
    const check = (list: unknown) => unitResourceChecks({ ...d, game: { ...d.game!, unitResources: list as never } })
    expect(check(undefined)).toEqual([])
    expect(check([{ id: 'mp', name: 'MP', max: s, start: 100, regen: { perTurn: 1, percent: 5 } }])).toEqual([])
    expect(check([{ id: 'a b', max: s }])[0]).toMatch(/letters, digits/)
    expect(check([{ id: 'mp', max: s }, { id: 'mp', max: s }])[0]).toMatch(/same id/)
    expect(check([{ id: 'mp', name: 'Much too long', max: s }])[0]).toMatch(/1 to 8/)
    expect(check([{ id: 'mp' }])[0]).toMatch(/pick the stat/)
    expect(check([{ id: 'mp', max: 'nope' }])[0]).toMatch(/not a stat/)
    expect(check([{ id: 'mp', max: s, start: 101 }])[0]).toMatch(/0 to 100/)
    expect(check([{ id: 'mp', max: s, regen: { perTurn: 1000 } }])[0]).toMatch(/0 to 999/)
    expect(check([{ id: 'mp', max: s, regen: { percent: 101 } }])[0]).toMatch(/0 to 100/)
    expect(check(['a', 'b', 'c', 'd', 'e'].map((id) => ({ id, max: s })))[0]).toMatch(/at most 4/)
    const withSide = { ...d, game: { ...d.game!, resources: [{ id: 'mp', name: 'Funds' }], unitResources: [{ id: 'mp', max: s }] } }
    expect(unitResourceChecks(withSide)[0]).toMatch(/also a resource of the sides/)
  })
})

describe('a weapon’s hit formula', () => {
  it('is written when typed and removed when emptied', () => {
    let d = setWeaponHit(base(), 'fire_spell', 'a.mag + 70 - b.res')
    expect(d.weapons!.find((w) => w.id === 'fire_spell')!.hit).toBe('a.mag + 70 - b.res')
    d = setWeaponHit(d, 'fire_spell', '   ')
    expect('hit' in d.weapons!.find((w) => w.id === 'fire_spell')!).toBe(false)
  })
  it('refuses a hit that is not text', () => {
    const d = base()
    expect(weaponHitChecks(d)).toEqual([])
    const bad = { ...d, weapons: d.weapons!.map((w) => (w.id === 'fire_spell' ? { ...w, hit: 5 as unknown as string } : w)) }
    expect(weaponHitChecks(bad)[0]).toMatch(/must be text/)
  })
  it('a weapon that goes takes the abilities that strike with it', () => {
    let d = withAbility(base(), { id: 'b', name: 'Blast', effect: { strike: 'fire_spell' } })
    expect(findUsages(d, 'weapon', 'fire_spell').some((u) => u.section === 'abilities')).toBe(true)
    d = deleteWeapon(d, 'fire_spell')
    expect(d.abilities!.some((a) => a.id === 'b')).toBe(false)
  })
})

describe('items that teach', () => {
  const doc = (): ProjectDoc => {
    let d = base()
    d = withAbility(d, { id: 'first_aid', name: 'First Aid', target: 'unit', effect: { heal: 10 } })
    d = { ...d, skills: [...(d.skills ?? []), { id: 'counter', name: 'Counter', blocks: [] }] }
    return { ...d, items: [...(d.items ?? []), accessory('ring'), { id: 'sword', name: 'Sword', kind: 'weapon', weapon: d.weapons![0].id } as Item] }
  }
  it('adds an entry for something it does not teach yet, with the points other items already give it', () => {
    let d = addTeach(doc(), 'ring')
    expect(teachesOf(d.items!.find((i) => i.id === 'ring')!)).toEqual([{ ability: expect.any(String), points: 100 }])
    d = setTeachPoints(d, 'ring', 0, 40)
    const id = teachesOf(d.items!.find((i) => i.id === 'ring')!)[0].ability!
    expect(pointsFor(d, 'ability', id, 'sword')).toBe(40)
    d = addTeach(d, 'sword')
    d = setTeachEntry(d, 'sword', 0, 'ability', id)
    expect(teachesOf(d.items!.find((i) => i.id === 'sword')!)[0].points).toBe(40)
  })
  it('stops at 8 entries and removes the key when the last entry goes', () => {
    let d = doc()
    for (let n = 0; n < 12; n++) d = addTeach(d, 'ring')
    expect(teachesOf(d.items!.find((i) => i.id === 'ring')!).length).toBeLessThanOrEqual(8)
    for (let n = 0; n < 8; n++) d = removeTeach(d, 'ring', 0)
    expect('teaches' in d.items!.find((i) => i.id === 'ring')!).toBe(false)
  })
  it('only weapons and accessories teach, and changing the kind drops the entries', () => {
    expect(addTeach(addTeach(doc(), 'ring'), 'ring')).not.toBe(doc())
    let d = addTeach(doc(), 'ring')
    d = setItemKind(d, 'ring', 'usable')
    expect('teaches' in d.items!.find((i) => i.id === 'ring')!).toBe(false)
  })
  it('checks mirror the loader', () => {
    const d = doc()
    const put = (items: Item[]): ProjectDoc => ({ ...d, items })
    const ring = (t: unknown): Item => ({ ...accessory('ring'), teaches: t as never })
    expect(teachChecks(put([ring([{ ability: 'first_aid', points: 100 }, { skill: 'counter', points: 5 }])]))).toEqual([])
    expect(teachChecks(put([ring([])]))[0]).toMatch(/1 to 8/)
    expect(teachChecks(put([ring(Array.from({ length: 9 }, () => ({ ability: 'first_aid', points: 1 })))]))[0]).toMatch(/1 to 8/)
    expect(teachChecks(put([ring([{ ability: 'first_aid', skill: 'counter', points: 5 }])]))[0]).toMatch(/one of them/)
    expect(teachChecks(put([ring([{ points: 5 }])]))[0]).toMatch(/one of them/)
    expect(teachChecks(put([ring([{ ability: 'first_aid', points: 0 }])]))[0]).toMatch(/1 to 9999/)
    expect(teachChecks(put([ring([{ ability: 'first_aid', points: 10000 }])]))[0]).toMatch(/1 to 9999/)
    expect(teachChecks(put([ring([{ ability: 'nope', points: 5 }])]))[0]).toMatch(/not an ability/)
    expect(teachChecks(put([ring([{ skill: 'nope', points: 5 }])]))[0]).toMatch(/not a skill/)
    expect(teachChecks(put([ring([{ ability: 'first_aid', points: 5 }, { ability: 'first_aid', points: 5 }])]))[0]).toMatch(/twice/)
    const usable: Item = { id: 'potion', name: 'Potion', kind: 'usable', heal: 10, teaches: [{ ability: 'first_aid', points: 5 }] }
    expect(teachChecks(put([usable]))[0]).toMatch(/only a weapon or an accessory/)
    const two = put([ring([{ ability: 'first_aid', points: 100 }]), { ...accessory('dagger'), teaches: [{ ability: 'first_aid', points: 80 }] }])
    expect(teachChecks(two)[0]).toMatch(/costs 80 points here but 100 on ring/)
    const both = put([ring([{ ability: 'first_aid', points: 100 }, { skill: 'first_aid', points: 100 }])])
    expect(teachChecks({ ...both, skills: [...both.skills!, { id: 'first_aid', name: 'FA', blocks: [] }] }).some((t) => /as a skill and as an ability/.test(t))).toBe(true)
  })
  it('the items screen sees the checks', () => {
    const d = doc()
    const bad = { ...d, items: d.items!.map((i) => (i.id === 'ring' ? { ...i, teaches: [{ ability: 'nope', points: 5 }] } : i)) }
    expect(itemChecks(bad).some((c) => c.section === 'items' && /not an ability/.test(c.text))).toBe(true)
  })
  it('a deleted ability or skill leaves the items that taught it', () => {
    let d = doc()
    d = { ...d, items: d.items!.map((i) => (i.id === 'ring' ? { ...i, teaches: [{ ability: 'first_aid', points: 5 }, { skill: 'counter', points: 9 }] } : i)) }
    expect(abilityUsageLines(d, 'first_aid').some((u) => u.section === 'items')).toBe(true)
    const noAbility = deleteAbility(d, 'first_aid')
    expect(teachesOf(noAbility.items!.find((i) => i.id === 'ring')!)).toEqual([{ skill: 'counter', points: 9 }])
    const none = deleteSkill(noAbility, 'counter')
    expect('teaches' in none.items!.find((i) => i.id === 'ring')!).toBe(false)
    expect(dropTeach(d, 'skill', 'unknown')).toBe(d)
  })
})

describe('rules.abilityPoints', () => {
  const teaching = (): ProjectDoc => {
    const d = withAbility(base(), { id: 'first_aid', name: 'First Aid', target: 'unit', effect: { heal: 10 } })
    return { ...d, items: [{ ...accessory('ring'), teaches: [{ ability: 'first_aid', points: 5 }] }] }
  }
  it('switching on writes only perBattle, off removes the block', () => {
    let d = setAbilityPointsOn(teaching(), true)
    expect(d.game!.rules!.abilityPoints).toEqual({ perBattle: 10 })
    d = setAbilityPointsOn(d, false)
    expect('abilityPoints' in d.game!.rules!).toBe(false)
  })
  it('never writes defaults and clamps to 0 to 9999', () => {
    let d = setAbilityPointsOn(teaching(), true)
    d = updateAbilityPoints(d, { name: 'AP', perAction: 0, perDefeat: 3, perBattle: 99999 })
    expect(d.game!.rules!.abilityPoints).toEqual({ perBattle: 9999, perDefeat: 3 })
    d = updateAbilityPoints(d, { name: 'Pts', perBattle: -5 })
    expect(d.game!.rules!.abilityPoints).toEqual({ name: 'Pts', perBattle: 0, perDefeat: 3 })
    expect(updateAbilityPoints(base(), { perBattle: 3 })).toEqual(base())  // not switched on: nothing to change
  })
  it('refuses a block when no item teaches, and bad names or numbers', () => {
    const on = setAbilityPointsOn(teaching(), true)
    expect(abilityPointsChecks(on)).toEqual([])
    expect(abilityPointsChecks(setAbilityPointsOn(base(), true))[0]).toMatch(/no item teaches anything/)
    expect(abilityPointsChecks(base())).toEqual([])
    const set = (ap: AbilityPointRules): ProjectDoc => ({ ...on, game: { ...on.game!, rules: { ...on.game!.rules!, abilityPoints: ap } } })
    expect(abilityPointsChecks(set({ name: '' }))[0]).toMatch(/1 to 8/)
    expect(abilityPointsChecks(set({ name: 'Ninechars' }))[0]).toMatch(/1 to 8/)
    expect(abilityPointsChecks(set({ perBattle: 10000 }))[0]).toMatch(/0 to 9999/)
    expect(abilityPointsChecks(set({ perAction: -1 }))[0]).toMatch(/0 to 9999/)
    expect(abilityPointsChecks(set({ perDefeat: 1.5 }))[0]).toMatch(/0 to 9999/)
  })
})
