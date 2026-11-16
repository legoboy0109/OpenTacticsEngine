// E2: the Skills model (docs/SRPG.md section 3, "Editor"): the pickers offer exactly what the tables of the contract allow,
// changing "when" repairs a rule, the sentence a rule adds up to, chance modes, the editor-side checks, accessory wording, and
// that deleting a skill takes it from everything that names it (including events) in one edit.
import { describe, expect, it } from 'vitest'
import { skillChecks, ruleIssues } from '../../src/model/skillChecks'
import { addCondition, addEffect, newCondition, newEffect, noConditionsWhy, offeredConditions, offeredEffects, setAura, setWhen } from '../../src/model/skillBlocks'
import { chanceMode, setChanceMode, statChance } from '../../src/model/skillChance'
import { COND_KINDS, EFFECT_KINDS, WHENS, condsFor, effectsFor, type CondKind, type EffectKind, type When } from '../../src/model/skillRules'
import { blockSummary, skillSummary } from '../../src/model/skillText'
import { addBlock, addSkill, changeWhen, duplicateSkill, moveBlock, moveSkill, removeBlock, updateBlock } from '../../src/model/skills'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { deleteSkill, skillUsageLines } from '../../src/model/srpgRefs'
import { itemChecks } from '../../src/model/itemChecks'
import type { Extra, ProjectDoc } from '../../src/model/types'
import { cleanStat } from '../../src/model/references'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => switchOnFeature(loadDemo('demo-srpg'), 'skills')

// The tables of docs/SRPG.md section 3, written out once more by hand: the picker rules must equal them.
const EFFECT_TABLE: Record<When, EffectKind[]> = {
  always: ['stats', 'growths', 'maxMoveCost', 'pass', 'moveAgain', 'xpPercent', 'ability'],
  turnStart: ['heal', 'hurt'],
  combat: ['stats', 'hit', 'crit', 'damage', 'avoid', 'critAvoid', 'damageTaken', 'damagePercent', 'damageTakenPercent', 'strikes', 'multiStrike', 'strikeFirst', 'counterAnyRange', 'noCounter'],
  strike: ['hit', 'crit', 'damage', 'damagePercent', 'drain', 'strikeAgain'],
  struck: ['avoid', 'critAvoid', 'damageTaken', 'damageTakenPercent', 'survive'],
  defeat: ['heal', 'actAgain'],
  afterCombat: ['heal', 'hurt', 'actAgain'],
}
const sorted = <T,>(l: T[]) => [...l].sort()

describe('skills: the pickers follow the contract tables', () => {
  it('offers the effects of each "when" and no others (aura: only what an aura gives)', () => {
    for (const w of WHENS) expect(sorted(effectsFor(w, false)), w).toEqual(sorted(EFFECT_TABLE[w]))
    expect(sorted(effectsFor('combat', true))).toEqual(sorted(['stats', 'hit', 'avoid', 'crit', 'critAvoid', 'damage', 'damageTaken']))
    expect(sorted(EFFECT_KINDS)).toEqual(sorted([...new Set(Object.values(EFFECT_TABLE).flat())]))
  })
  it('offers the conditions of each "when": no formula / foe points where there is no combat', () => {
    const all = sorted(COND_KINDS)
    expect(sorted(condsFor('always', false))).toEqual(sorted(all.filter((k) => !['formula', 'initiating', 'distance'].includes(k))))
    expect(sorted(condsFor('turnStart', false))).toEqual(sorted(all.filter((k) => !['initiating', 'distance'].includes(k))))
    expect(sorted(condsFor('combat', true))).toEqual(sorted(all.filter((k) => !['initiating', 'distance'].includes(k))))
    for (const w of ['combat', 'strike', 'struck', 'defeat', 'afterCombat'] as const) expect(sorted(condsFor(w, false))).toEqual(all)
  })
  it('offers only what the game can make: a weapon condition needs a weapon, an ability effect needs an ability', () => {
    const d = base()
    const block = { when: 'always', do: [{ stats: { str: 1 } }] }
    expect(offeredEffects(d, block)).not.toContain('ability')
    expect(offeredEffects({ ...d, abilities: [{ id: 'shove', name: 'Shove' }] }, block)).toContain('ability')
    expect(offeredConditions({ ...d, weapons: [] }, { when: 'combat', do: [] })).not.toContain('weapon')
    expect(offeredConditions(d, { when: 'combat', do: [] })).toContain('weapon')
  })
  it('does not offer growths or an ability to a rule with conditions, nor conditions to a rule that has them', () => {
    const d = base()
    expect(offeredEffects(d, { when: 'always', if: [{ hp: { op: '<=', percent: 50 } }], do: [{ pass: 'enemies' }] })).not.toContain('growths')
    expect(noConditionsWhy(d, { when: 'always', do: [{ growths: { str: 10 } }] })).toBe('growths')
    expect(noConditionsWhy(d, { when: 'always', do: [{ stats: { hp: 5 } }] })).toBe('hp')
    expect(noConditionsWhy(d, { when: 'combat', do: [{ crit: 5 }] })).toBeNull()
    const full = { when: 'combat', if: Array.from({ length: 8 }, () => ({ initiating: true })), do: [{ crit: 5 }] }
    expect(noConditionsWhy(d, full)).toBe('limit')
  })
  it('every effect and condition the editor can make passes its own checks in a rule that allows it', () => {
    const d = { ...base(), abilities: [{ id: 'shove', name: 'Shove' }] }
    for (const w of WHENS) for (const k of effectsFor(w, false)) {
      const e = newEffect(k, d, w)
      expect(e, `${w}/${k}`).not.toBeNull()
      expect(ruleIssues(d, { when: w, do: [e!] }), `${w}/${k}`).toEqual([])
    }
    for (const w of WHENS) for (const k of condsFor(w, false)) {
      const c = newCondition(k as CondKind, d)
      expect(c, `${w}/${k}`).not.toBeNull()
      const eff = w === 'always' ? { pass: 'enemies' } : newEffect(effectsFor(w, false).find((x) => x !== 'growths' && x !== 'ability')!, d, w)!
      expect(ruleIssues(d, { when: w, if: [c!], do: [eff] }), `${w}/${k}`).toEqual([])
    }
  })
})

describe('skills: changing "when" repairs the rule', () => {
  const d = base()
  it('drops the chance, the aura and what the new point does not allow, and keeps a rule with an effect', () => {
    const b = { when: 'strike', chance: 30, if: [{ initiating: true }, { hp: { op: '<=', percent: 50 } }], do: [{ damage: 3 }, { drain: 50 }] }
    expect(setWhen(b, 'always', d)).toEqual({ when: 'always', if: [{ hp: { op: '<=', percent: 50 } }], do: [{ stats: { str: 1 } }] })
    const aura = { when: 'combat', aura: { to: 'allies', within: 2 }, do: [{ hit: 10 }] }
    expect(setWhen(aura, 'strike', d)).toEqual({ when: 'strike', do: [{ hit: 10 }] })
  })
  it('a hurt effect loses its targets (each point has its own default); "to foe" goes outside combat', () => {
    expect(setWhen({ when: 'afterCombat', do: [{ hurt: 5, to: 'foe' }] }, 'turnStart', d).do).toEqual([{ hurt: 5 }])
    expect(setWhen({ when: 'combat', do: [{ strikes: -1, to: 'foe' }, { hit: 5 }] }, 'always', d).do).toEqual([{ stats: expect.any(Object) }])
    expect(setWhen({ when: 'combat', do: [{ hit: 5, to: 'foe' }] }, 'strike', d).do).toEqual([{ hit: 5 }])
  })
  it('an aura keeps only conditions about its owner', () => {
    const b = { when: 'combat', if: [{ initiating: true }, { hp: { op: '<=', percent: 50 }, who: 'foe' }, { round: { op: '>=', value: 2 } }], do: [{ hit: 5, to: 'foe' }] }
    const a = setAura(b, { to: 'allies', within: 2 }, d)
    expect(a.if).toEqual([{ round: { op: '>=', value: 2 } }])
    expect(a.do).toEqual([{ hit: 5 }])
    expect(setAura(a, null, d).aura).toBeUndefined()
  })
  it('keeps the file order of keys', () => {
    expect(Object.keys(addCondition({ do: [{ crit: 5 }], when: 'combat' }, { initiating: true }))).toEqual(['when', 'if', 'do'])
    expect(Object.keys(setChanceMode({ when: 'strike', do: [{ crit: 5 }] }, 'percent', ['str']))).toEqual(['when', 'chance', 'do'])
  })
})

describe('skills: the list and its rules', () => {
  it('a new skill is valid as it stands; duplicate, move and rule edits keep it so', () => {
    let d = addSkill(base(), 'Heavy Blow')
    expect(d.skills).toEqual([{ id: 'heavy_blow', name: 'Heavy Blow', blocks: [{ when: 'always', do: [{ stats: { str: 1 } }] }] }])
    d = addSkill(d, 'Second')
    d = duplicateSkill(d, 'heavy_blow')
    expect(d.skills!.map((s) => s.id)).toEqual(['heavy_blow', 'heavy_blow_copy', 'second'])
    d = moveSkill(d, 2, 0)
    expect(d.skills![0].id).toBe('second')
    d = addBlock(d, 'second')
    expect(d.skills![0].blocks).toHaveLength(2)
    expect(d.skills![0].blocks![1]).toEqual({ when: 'combat', do: [{ damage: 3 }] })
    d = moveBlock(d, 'second', 1, 0)
    expect(d.skills![0].blocks![0].when).toBe('combat')
    d = changeWhen(d, 'second', 0, 'strike')
    d = updateBlock(d, 'second', 0, (b) => addEffect(b, { drain: 50 }))
    expect(d.skills![0].blocks![0]).toEqual({ when: 'strike', do: [{ damage: 3 }, { drain: 50 }] })
    expect(skillChecks(d).filter((c) => !c.text.includes('nobody has'))).toEqual([])
    d = removeBlock(removeBlock(d, 'second', 0), 'second', 0)
    expect(d.skills![0].blocks).toHaveLength(1) // the last rule stays
  })
  it('a rule has at most 8 effects and a skill at most 8 rules', () => {
    let b: Extra = { when: 'combat', do: [{ crit: 1 }] }
    for (let i = 0; i < 12; i++) b = addEffect(b, { hit: i })
    expect((b.do as unknown[]).length).toBe(8)
    let d = addSkill(base(), 'Many')
    for (let i = 0; i < 12; i++) d = addBlock(d, 'many')
    expect(d.skills![0].blocks).toHaveLength(8)
  })
})

describe('skills: the sentence', () => {
  const d = base()
  it('reads like the contract examples', () => {
    expect(blockSummary(d, { when: 'combat', if: [{ initiating: true }], do: [{ damage: 3 }] })).toBe('In combat, if it started the fight: damage +3.')
    expect(blockSummary(d, { when: 'combat', if: [{ hp: { op: '<=', percent: 50 } }], do: [{ crit: 25 }] })).toBe('In combat, if its HP is at most 50%: critical +25.')
    expect(blockSummary(d, { when: 'strike', chance: 'a.skl', do: [{ damagePercent: 200 }] })).toBe('When it strikes, with a chance of its Skill percent: damage dealt 200%.')
    expect(blockSummary(d, { when: 'strike', chance: 30, do: [{ strikeAgain: true }] })).toBe('When it strikes, with a 30% chance: strikes once more.')
    expect(blockSummary(d, { when: 'combat', aura: { to: 'allies', within: 2 }, do: [{ hit: 10 }, { avoid: 10 }] })).toBe('In combat: allies within 2 squares get hit +10, avoid +10.')
    expect(blockSummary(d, { when: 'combat', if: [{ near: 'ally', within: 2, op: '=', count: 0 }], do: [{ damage: 3 }] })).toContain('no ally is within 2 squares')
    expect(blockSummary(d, { when: 'combat', if: [{ unit: 'knight', who: 'foe' }], do: [{ damagePercent: 150 }] })).toContain('the foe is')
    expect(blockSummary(d, { when: 'always', do: [{ stats: { str: 2, def: -1 } }] })).toBe('Always: Strength +2, Defense -1.')
    expect(blockSummary(d, { when: 'turnStart', do: [{ heal: 5, to: 'allies', within: 1 }] })).toBe('At the start of its turn: heals allies within 1 square by 5 HP.')
    expect(blockSummary(d, { when: 'turnStart', do: [{ hurt: 2 }] })).toContain('hurts enemies within 1 square')
  })
  it('joins the rules of a skill', () => {
    const s = { id: 'x', name: 'X', blocks: [{ when: 'always', do: [{ pass: 'enemies' }] }, { when: 'strike', do: [{ drain: 50 }] }] }
    expect(skillSummary(d, s)).toBe('Always: can move through enemy units. When it strikes: heals itself by 50% of the damage it deals.')
  })
})

describe('skills: chance', () => {
  it('reads the mode from the value and writes a formula that the loader reads back', () => {
    const stats = ['str', 'skl']
    expect(chanceMode(undefined, stats)).toBe('always')
    expect(chanceMode(30, stats)).toBe('percent')
    expect(chanceMode('a.skl', stats)).toBe('stat')
    expect(chanceMode('a.skl * 2', stats)).toBe('stat')
    expect(chanceMode('a.level * 2', stats)).toBe('custom')
    expect(statChance('a.skl * 2')).toEqual(['skl', 2])
    expect(statChance('a.skl')).toEqual(['skl', 1])
    expect(setChanceMode({ when: 'strike', chance: 30, do: [] }, 'always', stats).chance).toBeUndefined()
  })
})

describe('skills: what is wrong with a rule, in words', () => {
  const d = base()
  it('names the refused shapes of the loader', () => {
    expect(ruleIssues(d, { when: 'combat', chance: 30, do: [{ crit: 1 }] })[0]).toMatch(/cannot have a chance/)
    expect(ruleIssues(d, { when: 'always', do: [{ drain: 50 }] })[0]).toMatch(/cannot be used in a rule that always applies/)
    expect(ruleIssues(d, { when: 'always', if: [{ initiating: true }], do: [{ pass: 'enemies' }] })[0]).toMatch(/there is no combat/)
    expect(ruleIssues(d, { when: 'always', if: [{ round: { op: '>=', value: 2 } }], do: [{ growths: { str: 5 } }] })[0]).toMatch(/Growths cannot have a condition/)
    expect(ruleIssues(d, { when: 'combat', do: [{ stats: { hp: 5 } }] })[0]).toMatch(/hit points or movement/)
    expect(ruleIssues(d, { when: 'combat', do: [] })[0]).toMatch(/does nothing/)
    expect(ruleIssues(d, { when: 'combat', do: [{ stats: { nope: 1 } }] })[0]).toMatch(/not a stat/)
    expect(ruleIssues(d, { when: 'combat', do: [{ multiStrike: 9 }] })[0]).toMatch(/from 2 to 4/)
  })
  it('flags a skill nobody has, and clears it when a unit type has it', () => {
    let doc = addSkill(base(), 'Heavy Blow')
    expect(skillChecks(doc).map((c) => c.text)).toEqual(['Heavy Blow: nobody has this skill yet. Give it to a unit type, character, weapon or accessory.'])
    doc = { ...doc, units: doc.units!.map((u, i) => (i === 0 ? { ...u, learns: [{ level: 3, skill: 'heavy_blow' }] } : u)) }
    expect(skillChecks(doc)).toEqual([])
  })
})

describe('accessories: the wording of K1', () => {
  it('an accessory with skills and no bonus is fine; with neither it says "a bonus or a skill"', () => {
    const d = { ...base(), items: [{ id: 'ring', name: 'Ring', kind: 'accessory' as const, skills: ['x'] }, { id: 'band', name: 'Band', kind: 'accessory' as const }] }
    const texts = itemChecks(d).map((c) => c.text)
    expect(texts.some((t) => t.startsWith('Ring: an accessory needs'))).toBe(false)
    expect(texts).toContain('Band: an accessory needs a bonus to at least one stat, or at least one skill.')
  })
})

describe('skills: delete takes the skill from everything that names it', () => {
  it('unit types, learns, characters, weapons, accessories and events, in one edit', () => {
    let d = addSkill(addSkill(base(), 'Heavy Blow'), 'Keen')
    const mapId = d.game!.maps[0].id
    d = {
      ...d,
      units: d.units!.map((u, i) => (i === 0 ? { ...u, skills: ['heavy_blow', 'keen'], learns: [{ level: 3, skill: 'heavy_blow' }, { level: 4, skill: 'keen' }] } : u)),
      weapons: d.weapons!.map((w, i) => (i === 0 ? { ...w, skills: ['heavy_blow'] } : w)),
      items: [{ id: 'ring', name: 'Ring', kind: 'accessory' as const, skills: ['heavy_blow'] }],
      characters: [{ id: 'eirik', name: 'Eirik', type: d.units![0].id, skills: ['heavy_blow'], learns: [{ level: 2, skill: 'heavy_blow' }] }],
      maps: {
        ...d.maps,
        [mapId]: { ...d.maps[mapId]!, events: [{ id: 'teach', when: { battle: 'start' }, do: [{ teachSkill: 'heavy_blow', to: {} }, { say: 'Hi' }] },
          { id: 'ask', when: { battle: 'start' }, if: [{ hasSkill: 'heavy_blow' }], do: [{ say: 'x' }] }] },
      },
    }
    const lines = skillUsageLines(d, 'heavy_blow').map((l) => l.text)
    expect(lines.some((t) => t.includes('Unit'))).toBe(true)
    expect(lines.some((t) => t.includes('Character'))).toBe(true)
    expect(lines.some((t) => t.includes('Weapon'))).toBe(true)
    expect(lines.some((t) => t.includes('Item'))).toBe(true)
    expect(lines.some((t) => t.includes('Map'))).toBe(true)
    const after = deleteSkill(d, 'heavy_blow')
    expect(after.skills!.map((s) => s.id)).toEqual(['keen'])
    expect(after.units![0].skills).toEqual(['keen'])
    expect(after.units![0].learns).toEqual([{ level: 4, skill: 'keen' }])
    expect(after.weapons![0].skills).toBeUndefined()
    expect(after.items![0].skills).toBeUndefined()
    expect(after.characters![0].skills).toBeUndefined()
    expect(after.characters![0].learns).toBeUndefined()
    const events = after.maps[mapId]!.events!
    expect(events.map((e) => e.id)).toEqual(['teach'])
    expect(events[0].do).toEqual([{ say: 'Hi' }])
  })
  it('switching skills off removes them from the units that had them', () => {
    let d = addSkill(base(), 'Heavy Blow')
    d = { ...d, units: d.units!.map((u, i) => (i === 0 ? { ...u, skills: ['heavy_blow'] } : u)) }
    const off = switchOffSkills(d)
    expect(off.skills).toBeNull()
    expect(off.units![0].skills).toBeUndefined()
  })
})

import { switchOffFeature } from '../../src/model/srpgFeatures'
const switchOffSkills = (d: ProjectDoc) => switchOffFeature(d, 'skills')

describe('skills: deleting a stat', () => {
  it('takes the stat out of bonuses, drops what is left empty, zeroes a chance that reads it and deletes a skill with no rule left', () => {
    let d = base()
    d = { ...d, skills: [
      { id: 'a', name: 'A', blocks: [{ when: 'always', do: [{ stats: { str: 1, skl: 2 } }, { growths: { skl: 5 } }] }, { when: 'strike', chance: 'a.skl * 2', do: [{ drain: 50 }] }] },
      { id: 'b', name: 'B', blocks: [{ when: 'always', do: [{ growths: { skl: 5 } }] }] },
    ] }
    const out = cleanStat(d, 'skl')
    expect(out.skills!.map((s) => s.id)).toEqual(['a'])
    expect(out.skills![0].blocks).toEqual([{ when: 'always', do: [{ stats: { str: 1 } }] }, { when: 'strike', chance: 0, do: [{ drain: 50 }] }])
  })
})
