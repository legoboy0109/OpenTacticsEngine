// The four small additions in the editor's model: the actor condition and the storeItems command (docs/EVENTS.md), the fading condition
// (docs/SRPG.md "Effects that fade") and marks between strike blocks (docs/SRPG.md "Linking two strike blocks"): what the pickers offer,
// defaults, sentences, the checks that mirror the loader, and how deletes reach them.
import { describe, expect, it } from 'vitest'
import { campaignChecks } from '../../src/model/campaignChecks'
import { reconcileCampaign } from '../../src/model/campaignRefs'
import { conditionChecks } from '../../src/model/conditionChecks'
import { conditionSummary } from '../../src/model/conditionText'
import { addCondition as addStatus, hasScalableEffect, setDuration, setScale, updateCondition } from '../../src/model/conditions'
import { addItem, deleteItem } from '../../src/model/items'
import { setStorageMode as setStorage } from '../../src/model/itemRules'
import { newCondition, newEffect, offeredConditions, offeredEffects, setWhen } from '../../src/model/skillBlocks'
import { ruleIssues } from '../../src/model/skillChecks'
import { condsFor, effectsFor } from '../../src/model/skillRules'
import { blockSummary } from '../../src/model/skillText'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { newCondition as newStoryCondition, newCommand } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText, conditionText } from '../../src/model/storySummary'
import { commandKind, conditionKind, type Command, type Condition } from '../../src/model/storyTypes'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id

// ---- marks ----

describe('marks between strike blocks', () => {
  const d = switchOnFeature(loadDemo('demo-srpg'), 'skills')
  it('offers "marked" where the loader allows it and "mark" in strike and struck rules only', () => {
    for (const w of ['strike', 'struck', 'defeat', 'afterCombat']) expect(offeredConditions(d, { when: w, do: [] }), w).toContain('marked')
    for (const w of ['always', 'turnStart', 'combat', 'staffUse']) expect(offeredConditions(d, { when: w, do: [] }), w).not.toContain('marked')
    expect(offeredConditions(d, { when: 'combat', aura: { to: 'allies', within: 1 }, do: [] })).not.toContain('marked')
    expect(offeredEffects(d, { when: 'strike', do: [] })).toContain('mark')
    expect(offeredEffects(d, { when: 'struck', do: [] })).toContain('mark')
    for (const w of ['always', 'combat', 'afterCombat', 'defeat'] as const) expect(effectsFor(w, false), w).not.toContain('mark')
    expect(condsFor('strike', false)).not.toContain('marked')  // the contract list the older checks pin stays as it was
  })
  it('starts both from the same name so a pair links at once', () => {
    expect(newCondition('marked', d)).toEqual({ marked: 'mark' })
    expect(newEffect('mark', d, 'strike')).toEqual({ mark: 'mark' })
  })
  it('mirrors the loader: the name, where it may be asked, and a linked pair is clean', () => {
    const first = { when: 'strike', if: [{ strike: 'first' }], chance: 30, do: [{ damage: 3 }, { mark: 'hit' }] }
    const second = { when: 'strike', if: [{ strike: 2 }, { marked: 'hit' }], do: [{ damage: 5 }] }
    expect(ruleIssues(d, first)).toEqual([])
    expect(ruleIssues(d, second)).toEqual([])
    expect(ruleIssues(d, { when: 'afterCombat', if: [{ marked: 'hit', is: false }], do: [{ heal: 2 }] })).toEqual([])
    expect(ruleIssues(d, { when: 'combat', if: [{ marked: 'hit' }], do: [{ hit: 5 }] }).join(' ')).toContain('“Marked” can only be asked')
    expect(ruleIssues(d, { when: 'turnStart', if: [{ marked: 'hit' }], do: [{ heal: 2 }] }).join(' ')).toContain('“Marked” can only be asked')
    expect(ruleIssues(d, { when: 'strike', if: [{ marked: '' }], do: [{ hit: 5 }] }).join(' ')).toContain('name of 1 to 40 characters')
    expect(ruleIssues(d, { when: 'strike', if: [{ marked: 'x'.repeat(41) }], do: [{ hit: 5 }] }).join(' ')).toContain('name of 1 to 40 characters')
    expect(ruleIssues(d, { when: 'strike', if: [{ marked: 'a', is: 'no' }], do: [{ hit: 5 }] }).join(' ')).toContain('yes or no')
    expect(ruleIssues(d, { when: 'strike', do: [{ mark: '' }] }).join(' ')).toContain('name of 1 to 40 characters')
    expect(ruleIssues(d, { when: 'combat', do: [{ mark: 'a' }] }).join(' ')).toContain('cannot be used')
  })
  it('moving a rule to another point drops what that point cannot ask or leave', () => {
    const b = { when: 'strike', if: [{ marked: 'hit' }, { strike: 2 }], do: [{ damage: 3 }, { mark: 'hit' }] }
    const moved = setWhen(b, 'struck', d) as { if?: unknown[]; do: unknown[] }
    expect(moved.if).toEqual([{ marked: 'hit' }, { strike: 2 }])  // a struck rule may ask both
    const combat = setWhen(b, 'combat', d) as { if?: unknown[]; do: unknown[] }
    expect(combat.if ?? []).toEqual([])
    expect(JSON.stringify(combat.do)).not.toContain('mark')
  })
  it('reads as a sentence', () => {
    expect(blockSummary(d, { when: 'strike', if: [{ marked: 'hit' }], do: [{ damage: 5 }] })).toContain('it has the mark “hit”')
    expect(blockSummary(d, { when: 'strike', if: [{ marked: 'hit', is: false }], do: [{ damage: 5 }] })).toContain('has not the mark “hit”')
    expect(blockSummary(d, { when: 'strike', chance: 30, do: [{ mark: 'hit' }] })).toContain('leaves the mark “hit”')
  })
})

// ---- a condition whose effects fade ----

describe('a condition that fades', () => {
  const withStatus = (): { doc: ProjectDoc; id: string } => {
    let doc = switchOnFeature(loadDemo('demo-srpg'), 'conditions')
    doc = addStatus(doc, 'Ward', 'ward')
    return { doc, id: 'ward' }
  }
  it('is set and cleared, and a condition that never counts cannot fade', () => {
    let { doc } = withStatus()
    expect(doc.conditions![0].scale).toBeUndefined()
    doc = setScale(doc, 'ward', true)
    expect(doc.conditions![0].scale).toBe('turnsLeft')
    expect(setScale(doc, 'ward', false).conditions![0].scale).toBeUndefined()
    expect(Object.keys(setScale(doc, 'ward', false).conditions![0])).not.toContain('scale')
    const cured = setDuration(doc, 'ward', 'cured')
    expect(cured.conditions![0].scale).toBeUndefined()
    expect(cured.conditions![0].until).toBe('cured')
  })
  it('knows which rules have a number to fade', () => {
    const { doc } = withStatus()
    expect(hasScalableEffect(doc.conditions![0])).toBe(true)  // the new condition lowers a stat, or cannot move: see below
  })
  it('mirrors the loader: counted conditions only, and something to scale', () => {
    let { doc } = withStatus()
    const stat = Object.keys((doc.conditions![0].blocks![0].do as Array<{ stats?: object }>)[0].stats ?? {})[0]
    doc = updateCondition(doc, 'ward', { scale: 'turnsLeft' })
    const issues = () => conditionChecks(doc).map((c) => c.text).join(' | ')
    if (stat) expect(issues()).not.toContain('fade')
    doc = updateCondition(doc, 'ward', { turns: undefined, until: 'cured' })
    expect(issues()).toContain('only for a condition that counts turns')
    doc = updateCondition(doc, 'ward', { turns: 3, until: undefined, blocks: [{ when: 'combat', do: [{ hit: 5 }] }] })
    expect(issues()).toContain('no rule that always applies has a number to fade')
    doc = updateCondition(doc, 'ward', { blocks: [{ when: 'always', do: [{ vision: 1 }] }] })
    expect(issues()).not.toContain('fade')
    doc = updateCondition(doc, 'ward', { scale: 'always' as never })
    expect(issues()).toContain('“turnsLeft”')
  })
  it('says so in the condition’s sentence', () => {
    let { doc } = withStatus()
    doc = updateCondition(doc, 'ward', { scale: 'turnsLeft', blocks: [{ when: 'always', do: [{ vision: 1 }] }] })
    expect(conditionSummary(doc, doc.conditions![0])).toContain('as many times as it has turns left')
  })
})

// ---- the actor condition and storeItems ----

function mapWith(d: ProjectDoc, events: unknown[]): ProjectDoc {
  const m = d.maps[mapId(d)]!
  const next: MapFile = { ...m, units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'gorm' } : u)), events: events as MapFile['events'] }
  return { ...d, maps: { ...d.maps, [mapId(d)]: next } }
}
const problems = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text).join(' | ')
const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps[mapId(d)]! })

describe('the actor condition', () => {
  const d = mapWith(loadDemo('demo-economy'), [])
  it('is its own kind, starts on the default unit filter and reads as a sentence', () => {
    const c = newStoryCondition(ctx(d), 'actor')
    expect(conditionKind(c)).toBe('actor')
    expect(c.actor).toBeDefined()
    expect(conditionText(ctx(d), { actor: { unit: 'gorm' } } as Condition)).toBe('Infantry is the unit that set this off')
  })
  it('warns when nothing sets the event off with a unit, and stays quiet when something does', () => {
    const asks: Condition[] = [{ actor: { unit: 'gorm' } } as Condition]
    const turn = mapWith(d, [{ id: 'a', when: { turn: 2 }, if: asks, do: [{ setSwitch: 'x' }] }])
    expect(problems(turn)).toContain('never true')
    const nested = mapWith(d, [{ id: 'a', when: { battle: 'start' }, do: [{ if: asks, then: [{ endTurn: true }] }] }])
    expect(problems(nested)).toContain('never true')
    const reach = mapWith(d, [{ id: 'a', when: { reach: { x: 1, y: 1, w: 1, h: 1 } }, if: asks, do: [{ setSwitch: 'x' }] }])
    expect(problems(reach)).not.toContain('never true')
  })
  it('names a unit that is not on the map, and follows a rename or delete like every unit filter', () => {
    const bad = mapWith(d, [{ id: 'a', when: { reach: { x: 1, y: 1, w: 1, h: 1 } }, if: [{ actor: { unit: 'nobody' } }], do: [{ setSwitch: 'x' }] }])
    expect(problems(bad)).toContain('“nobody” is not a named unit')
  })
  it('a campaign scene refuses it', () => {
    const base = loadDemo('demo-srpg')
    const ch = base.campaign!.chapters[0]
    const doc = { ...base, campaign: { ...base.campaign!, chapters: [{ ...ch, after: [{ if: [{ actor: { unit: 'x' } }], then: [] } as Command] }, ...base.campaign!.chapters.slice(1)] } }
    expect(campaignChecks(doc).map((c) => c.text).join(' | ')).toContain('no unit that set it off')
  })
})

describe('the storeItems command', () => {
  const stocked = (storage: 'off' | 'anywhere'): ProjectDoc => {
    let doc = addItem(addItem(loadDemo('demo-economy'), 'Potion', 'potion'), 'Ring', 'ring')
    doc = setStorage(doc, storage)
    return mapWith(doc, [{ id: 'e', when: { battle: 'start' }, do: [{ storeItems: { unit: 'gorm' }, keep: ['ring'] }] }])
  }
  const cmds = (d: ProjectDoc): Command[] => d.maps[mapId(d)]!.events![0].do
  it('is its own kind with a default, and reads as a sentence', () => {
    const d = stocked('anywhere')
    const c = newCommand(ctx(d), 'storeItems')
    expect(commandKind(c)).toBe('storeItems')
    expect(c.storeItems).toBeDefined()
    expect(commandText(ctx(d), { storeItems: { unit: 'gorm' }, keep: ['ring'] })).toBe('Sends Infantry’s items to the storage, except Ring')
    expect(commandText(ctx(d), { storeItems: 'gorm' })).toBe('Sends Infantry’s items to the storage')
  })
  it('mirrors the loader: a game with no storage refuses it, one with storage does not', () => {
    expect(problems(stocked('off'))).toContain('no shared storage')
    expect(problems(stocked('anywhere'))).toBe('')
  })
  it('names items and units that exist', () => {
    const d = mapWith(stocked('anywhere'), [{ id: 'e', when: { battle: 'start' }, do: [{ storeItems: { unit: 'nobody' }, keep: ['nothing'] }] }])
    const p = problems(d)
    expect(p).toContain('“nobody” is not a named unit')
    expect(p).toContain('“nothing” is not an item')
  })
  it('a deleted item leaves the keep list and the command stays', () => {
    const d = stocked('anywhere')
    expect(cmds(deleteItem(d, 'ring'))).toEqual([{ storeItems: { unit: 'gorm' } }])
    expect(cmds(deleteItem(d, 'potion'))).toEqual([{ storeItems: { unit: 'gorm' }, keep: ['ring'] }])
  })
  it('works in a campaign scene on a roster id, and the checks say when there is no storage', () => {
    const base = setStorage(addItem(loadDemo('demo-srpg'), 'Ring', 'ring'), 'anywhere')
    const unit = base.maps[mapId(base)]!.units!.find((u) => u.id)!.id!
    const scene = (doc: ProjectDoc, commands: Command[]): ProjectDoc => {
      const ch = doc.campaign!.chapters[0]
      return { ...doc, campaign: { ...doc.campaign!, chapters: [{ ...ch, after: commands }, ...doc.campaign!.chapters.slice(1)] } }
    }
    const ok = scene(base, [{ storeItems: unit, keep: ['ring'] }, { setSwitch: 'x' } as Command])
    const errors = (doc: ProjectDoc) => campaignChecks(doc).filter((c) => c.level === 'error').map((c) => c.text).join(' | ')
    expect(errors(ok)).not.toContain('cannot use this command')
    expect(errors(ok)).not.toContain('no shared storage')
    expect(errors(scene(setStorage(base, 'off'), [{ storeItems: unit }]))).toContain('no shared storage')
    expect(errors(scene(base, [{ storeItems: '' }]))).toContain('pick a unit')
    const after = reconcileCampaign(ok, deleteItem(ok, 'ring'))
    expect(after.campaign!.chapters[0].after).toEqual([{ storeItems: unit }, { setSwitch: 'x' }])
  })
})
