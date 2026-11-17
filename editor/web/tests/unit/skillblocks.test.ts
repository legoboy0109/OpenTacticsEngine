// The general skill blocks (docs/SRPG.md "More skill blocks"): what the block model offers, what its checks say (they mirror the loader's
// refusals) and the sentence a rule adds up to.
import { describe, expect, it } from 'vitest'
import { newCondition, newEffect, offeredConditions, offeredEffects, setAura, setWhen, weaponKinds } from '../../src/model/skillBlocks'
import { ruleIssues } from '../../src/model/skillChecks'
import { condsFor, effectsFor } from '../../src/model/skillRules'
import { blockSummary } from '../../src/model/skillText'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => {
  const d = switchOnFeature(loadDemo('demo-srpg'), 'skills')
  return { ...d, weapons: (d.weapons ?? []).map((w, i) => (i === 0 ? { ...w, damageKind: 'magic' } : w)) }
}
const issues = (b: object, d = base()) => ruleIssues(d, b as never)

describe('skill blocks: conditions', () => {
  it('"weaponKind" is offered only when a weapon has a kind, and its check names a kind nobody has', () => {
    const d = base()
    expect(weaponKinds(d)).toEqual(['magic'])
    expect(offeredConditions(d, { when: 'combat' })).toContain('weaponKind')
    expect(offeredConditions({ ...d, weapons: d.weapons!.map((w) => ({ ...w, damageKind: undefined })) }, { when: 'combat' })).not.toContain('weaponKind')
    expect(issues({ when: 'combat', if: [{ weaponKind: 'magic', who: 'foe' }], do: [{ damage: 2 }] })).toEqual([])
    expect(issues({ when: 'combat', if: [{ weaponKind: 'ice' }], do: [{ damage: 2 }] })[0]).toMatch(/no weapon has/)
  })
  it('"turn" checks mirror the loader', () => {
    const ok = (turn: object) => issues({ when: 'combat', if: [{ turn }], do: [{ damage: 1 }] })
    expect(ok({ odd: true })).toEqual([])
    expect(ok({ even: true })).toEqual([])
    expect(ok({ from: 3, to: 6 })).toEqual([])
    expect(ok({ from: 4 })).toEqual([])
    expect(ok({ every: 3, offset: 1 })).toEqual([])
    expect(ok({ from: 6, to: 3 })[0]).toMatch(/not be later/)
    expect(ok({ every: 1 })[0]).toMatch(/from 2 to 1000/)
    expect(ok({ every: 3, offset: 3 })[0]).toMatch(/below “every”/)
    expect(ok({})[0]).toMatch(/Pick odd turns/)
    expect(issues({ when: 'always', if: [{ turn: { odd: true } }], do: [{ stats: { str: 1 } }] })).toEqual([])  // allowed everywhere
  })
  it('"counterattack" belongs to strike and struck rules only', () => {
    const c = { counterattack: false }
    expect(issues({ when: 'strike', if: [c], do: [{ damage: 3 }] })).toEqual([])
    expect(issues({ when: 'struck', if: [c], do: [{ damageTaken: -1 }] })).toEqual([])
    expect(issues({ when: 'combat', if: [c], do: [{ damage: 3 }] })[0]).toMatch(/strikes or is struck/)
    expect(condsFor('combat', false)).not.toContain('counterattack')
    expect(condsFor('strike', false)).toContain('counterattack')
    // Changing a strike rule to a combat rule drops the condition it can no longer ask.
    expect(setWhen({ when: 'strike', if: [c], do: [{ damage: 3 }] }, 'combat', base()).if).toBeUndefined()
  })
  it('"carries" and "inventory"', () => {
    const d = base()
    const item = d.items![0].id
    expect(issues({ when: 'combat', if: [{ carries: item }], do: [{ damage: 1 }] }, d)).toEqual([])
    expect(issues({ when: 'combat', if: [{ carries: 'nothing' }], do: [{ damage: 1 }] }, d)[0]).toMatch(/does not exist/)
    for (const inventory of ['full', 'notFull', { op: '>=', value: 2 }]) expect(issues({ when: 'combat', if: [{ inventory, who: 'foe' }], do: [{ damage: 1 }] }, d)).toEqual([])
    expect(issues({ when: 'combat', if: [{ inventory: 'half' }], do: [{ damage: 1 }] }, d)[0]).toMatch(/“full”/)
    expect(newCondition('carries', d)).toEqual({ carries: item })
  })
})

describe('skill blocks: effects', () => {
  it('follow-up, triangle and the single blocks are offered where the loader allows them', () => {
    expect(effectsFor('combat', false)).toEqual(expect.arrayContaining(['followUp', 'followShift', 'trianglePercent']))
    expect(effectsFor('combat', true)).not.toContain('followUp')
    expect(effectsFor('always', false)).toEqual(expect.arrayContaining(['weaponXpPercent', 'noCarryPenalty', 'cancelEffective']))
    expect(effectsFor('turnStart', false)).toContain('cure')
    expect(effectsFor('strike', false)).not.toContain('cure')
    expect(offeredEffects(base(), { when: 'combat' })).toContain('followUp')
  })
  it('follow-up: force or forbid, aimed at the foe too', () => {
    expect(issues({ when: 'combat', do: [{ followUp: 'forbid', to: 'foe' }] })).toEqual([])
    expect(issues({ when: 'combat', do: [{ followUp: 'sometimes' }] })[0]).toMatch(/forced or forbidden/)
    expect(issues({ when: 'combat', do: [{ followShift: 500 }] })[0]).toMatch(/from -100 to 100/)
    expect(issues({ when: 'always', do: [{ followUp: 'force' }] })[0]).toMatch(/cannot be used/)
  })
  it('a combat number may be a formula; an empty one is flagged; stats formulas only in combat', () => {
    expect(issues({ when: 'combat', do: [{ damage: 'turn * 2' }, { hit: '-b.def' }, { damagePercent: '100 + a.spd' }] })).toEqual([])
    expect(issues({ when: 'strike', do: [{ damage: 'turn' }] })).toEqual([])
    expect(issues({ when: 'combat', do: [{ damage: ' ' }] })[0]).toMatch(/formula is empty/)
    expect(issues({ when: 'combat', do: [{ stats: { str: 'turn' } }] })).toEqual([])
    expect(issues({ when: 'always', do: [{ stats: { str: 'turn' } }] })[0]).toMatch(/Only a combat bonus/)
    expect(issues({ when: 'combat', do: [{ strikes: 'turn' }] })[0]).toMatch(/must be from -7 to 7/)
  })
  it('move again may be limited to actions other than attacking', () => {
    expect(issues({ when: 'always', do: [{ moveAgain: { notAfterAttack: true } }] })).toEqual([])
    expect(issues({ when: 'always', do: [{ moveAgain: { x: 1 } }] })[0]).toMatch(/on or not there/)
  })
  it('cure: all, or conditions that exist', () => {
    const d = { ...base(), conditions: [{ id: 'poison', name: 'Poison', turns: 3, blocks: [{ when: 'always', do: [{ cannotMove: true }] }] }] } as ProjectDoc
    expect(issues({ when: 'turnStart', do: [{ cure: 'all' }, { cure: ['poison'] }] }, d)).toEqual([])
    expect(issues({ when: 'turnStart', do: [{ cure: ['nothing'] }] }, d)[0]).toMatch(/does not exist/)
    expect(issues({ when: 'turnStart', do: [{ cure: [] }] }, d)[0]).toMatch(/cures nothing/)
    expect(newEffect('cure', d, 'turnStart')).toEqual({ cure: ['poison'] })
    expect(newEffect('cure', base(), 'turnStart')).toEqual({ cure: 'all' })
  })
})

describe('skill blocks: an aura limited to a kind of unit', () => {
  it('keeps "match", checks it against unit types and tags, and says it in words', () => {
    const d = base()
    const tag = d.units!.find((u) => u.tags?.length)?.tags?.[0] ?? d.units![0].id
    const b = setAura({ when: 'combat', do: [{ hit: 5 }] }, { to: 'allies', within: 2, match: tag }, d)
    expect(b.aura).toEqual({ to: 'allies', within: 2, match: tag })
    expect(issues(b, d)).toEqual([])
    expect(issues({ ...b, aura: { to: 'allies', within: 2, match: 'nobody' } }, d)[0]).toMatch(/no unit type or tag/)
    expect(blockSummary(d, b)).toMatch(/allies that are .* within 2 squares get hit \+5/)
  })
})

describe('skill blocks: the sentences', () => {
  const d = base()
  it('read well', () => {
    expect(blockSummary(d, { when: 'combat', if: [{ turn: { odd: true } }], do: [{ damage: 'turn' }] })).toBe('In combat, if the turn is odd: damage from the formula turn.')
    expect(blockSummary(d, { when: 'combat', do: [{ followUp: 'forbid', to: 'foe' }] })).toBe('In combat: the foe never makes a follow-up strike.')
    expect(blockSummary(d, { when: 'always', do: [{ moveAgain: { notAfterAttack: true } }] })).toBe('Always: can move again after acting, but not after attacking.')
    expect(blockSummary(d, { when: 'strike', if: [{ counterattack: false }], do: [{ damage: 3 }] })).toBe('When it strikes, if the foe cannot counterattack: damage +3.')
    expect(blockSummary(d, { when: 'combat', if: [{ weaponKind: 'magic', who: 'foe' }], do: [{ damageTaken: -2 }] })).toBe('In combat, if the foe’s weapon deals magic damage: damage taken -2.')
    expect(blockSummary(d, { when: 'turnStart', do: [{ cure: 'all' }] })).toBe('At the start of its turn: is cured of every condition.')
  })
})
