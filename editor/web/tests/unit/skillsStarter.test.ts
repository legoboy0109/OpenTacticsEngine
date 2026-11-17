// E2: the starter set (docs/SRPG.md "The starter set"): it equals the template's data/skills.json, uses every "when", every
// condition the game does not need ids for and every effect but "ability", and the installer maps stats, skips what exists
// and skips what needs a stat that is not chosen. Every skill passes the editor's own rule checks.
import { describe, expect, it } from 'vitest'
import { ruleIssues } from '../../src/model/skillChecks'
import { COND_KINDS, EFFECT_KINDS, WHENS, condKindOf, effectKindOf } from '../../src/model/skillRules'
import { blocksOf } from '../../src/model/skills'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { STARTER_SKILLS, guessStats, installStarter, resolveSkill, rolesNeeded } from '../../src/model/starterSkills'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo, loadTemplate } from './fixtures'

const TEMPLATE_STATS = { atk: 'str', skl: 'skl', spd: 'spd', def: 'def', hp: 'hp', move: 'mov' }
const ids = ['tough', 'light_step', 'sure_footing', 'slip_through', 'strike_and_move', 'fast_learner', 'gifted', 'opening_blow', 'hold_the_line', 'back_to_the_wall',
  'on_my_own', 'close_fighter', 'steady_aim', 'beat_them_to_it', 'double_strike', 'always_ready', 'clean_hit', 'braced', 'rallying_presence', 'unnerving_presence',
  'heavy_blow', 'draining_blow', 'extra_swing', 'glancing_hit', 'hang_on', 'second_charge', 'catch_breath', 'parting_jab', 'steady_recovery', 'caring_presence']

describe('the starter set', () => {
  it('has the thirty skills of the contract, in its order', () => {
    expect(STARTER_SKILLS.map((s) => s.id)).toEqual(ids)
    expect(new Set(STARTER_SKILLS.map((s) => s.name)).size).toBe(30)
  })
  it('equals the template’s data/skills.json once the stat ids are filled in', () => {
    const template = loadTemplate('strategy-rpg').skills
    expect(template).not.toBeNull()
    expect(STARTER_SKILLS.map((s) => resolveSkill(s, TEMPLATE_STATS))).toEqual(template)
  })
  it('uses every "when", every condition that needs no ids of the game and every effect but "ability" (and critAvoid, which the contract’s table of thirty does not hold either)', () => {
    const blocks = STARTER_SKILLS.flatMap(blocksOf)
    expect(new Set(blocks.map((b) => b.when))).toEqual(new Set(WHENS.filter((w) => w !== 'staffUse')))  // "staffUse" came later: the thirty are fixed
    const conds = new Set(blocks.flatMap((b) => ((b.if as object[]) ?? []).map((c) => condKindOf(c as never))))
    for (const k of COND_KINDS.filter((x) => !['weapon', 'terrain', 'structure', 'unit', 'carrying', 'round', 'formula', 'condition', 'weaponKind', 'turn', 'counterattack', 'carries', 'inventory', 'strike'].includes(x))) expect(conds.has(k), k).toBe(true)
    const effects = new Set(blocks.flatMap((b) => (b.do as object[]).map((e) => effectKindOf(e as never))))
    for (const k of EFFECT_KINDS.filter((x) => x !== 'ability' && x !== 'critAvoid' && !['cannotAct', 'cannotMove', 'forbid', 'berserk', 'immune', 'inflict', 'recoil', 'followUp', 'followShift', 'trianglePercent', 'weaponXpPercent', 'noCarryPenalty', 'cancelEffective', 'cure', 'nullifySkills', 'staffRange', 'carryBonus'].includes(x))) expect(effects.has(k), k).toBe(true)
  })
  it('every skill passes the editor’s checks in the template’s game', () => {
    const d = loadTemplate('strategy-rpg')
    for (const s of STARTER_SKILLS) for (const b of blocksOf(resolveSkill(s, TEMPLATE_STATS)!)) expect(ruleIssues(d, b), s.id).toEqual([])
  })
})

describe('the installer', () => {
  const doc = (): ProjectDoc => switchOnFeature(loadDemo('demo-srpg'), 'skills')
  it('guesses the four stats from the ids and the hit points / move stats from the game’s roles', () => {
    expect(guessStats(doc())).toEqual(TEMPLATE_STATS)
  })
  it('guesses from the combat formulas when the ids are unusual', () => {
    const d = doc()
    const stats = [{ id: 'hp', name: 'HP' }, { id: 'pow', name: 'Power' }, { id: 'acc', name: 'Accuracy' }, { id: 'agi2', name: 'Reflex' }, { id: 'arm', name: 'Armor' }, { id: 'mov', name: 'Move' }]
    const g = { ...d.game!, stats, roles: { hp: 'hp', move: 'mov' }, formulas: { damage: 'a.pow - b.arm', hit: 'a.acc * 2 - b.agi2' } }
    expect(guessStats({ ...d, game: g })).toEqual({ hp: 'hp', move: 'mov', atk: 'pow', def: 'arm', skl: 'acc', spd: 'agi2' })
  })
  it('adds the chosen skills, filling in the stats', () => {
    const r = installStarter(doc(), ['gifted', 'heavy_blow', 'tough'], TEMPLATE_STATS)
    expect(r.added).toEqual(['Tough', 'Gifted', 'Heavy Blow'])
    expect(r.doc.skills!.find((s) => s.id === 'heavy_blow')!.blocks).toEqual([{ when: 'strike', chance: 'a.skl', do: [{ damagePercent: 200 }] }])
    expect(r.doc.skills!.find((s) => s.id === 'gifted')!.blocks![0]).toEqual({ when: 'always', do: [{ growths: { str: 10, skl: 10, spd: 10, def: 10 } }] })
  })
  it('uses the stats the maker points at', () => {
    const r = installStarter(doc(), ['heavy_blow'], { ...TEMPLATE_STATS, skl: 'spd' })
    expect(r.doc.skills![0].blocks![0].chance).toBe('a.spd')
  })
  it('skips a skill whose id already exists (and lists it) and one whose stat is left unset', () => {
    const first = installStarter(doc(), ['heavy_blow'], TEMPLATE_STATS).doc
    const r = installStarter(first, ['heavy_blow', 'glancing_hit', 'extra_swing'], { ...TEMPLATE_STATS, def: undefined })
    expect(r.existing).toEqual(['Heavy Blow'])
    expect(r.missingStat).toEqual(['Glancing Hit'])
    expect(r.added).toEqual(['Extra Swing'])
    expect(r.doc.skills!.map((s) => s.id)).toEqual(['heavy_blow', 'extra_swing'])
  })
  it('a game without a hit points stat role skips Tough and Light Step', () => {
    const r = installStarter(doc(), ['tough', 'light_step', 'sure_footing'], { ...TEMPLATE_STATS, hp: undefined, move: undefined })
    expect(r.missingStat).toEqual(['Tough', 'Light Step'])
    expect(r.added).toEqual(['Sure Footing'])
  })
  it('adding nothing leaves the document as it was', () => {
    const d = doc()
    expect(installStarter(d, [], TEMPLATE_STATS).doc).toBe(d)
  })
  it('knows which roles a skill needs', () => {
    expect(rolesNeeded(STARTER_SKILLS.find((s) => s.id === 'gifted')!).sort()).toEqual(['atk', 'def', 'skl', 'spd'])
    expect(rolesNeeded(STARTER_SKILLS.find((s) => s.id === 'slip_through')!)).toEqual([])
  })
})
