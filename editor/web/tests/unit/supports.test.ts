// E4 of docs/SRPG.md: the Supports screen's edits (pairs, levels, bonus, conversation, rules), the checks, and the story pieces
// that name a pair (command "Add support points", condition "support level", trigger "a support level is reached"): their
// defaults, sentences, checks, and what deleting a character or a pair, or giving a pair another character, does to them.
import { describe, expect, it } from 'vitest'
import { addCharacter } from '../../src/model/characters'
import { campaignChecks } from '../../src/model/campaignChecks'
import { reconcileCampaign } from '../../src/model/campaignRefs'
import { sceneAsMap } from '../../src/model/campaign'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { deleteCharacter, deletePairClean, pairUsageLines, retargetPair } from '../../src/model/srpgRefs'
import { newCommand, newCondition, newTrigger } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { commandKind, conditionKind, triggerKind, type StoryEvent } from '../../src/model/storyTypes'
import { commandText, conditionText, triggerText } from '../../src/model/storySummary'
import { supportChecks } from '../../src/model/supportChecks'
import { talkBase } from '../../src/model/supportTalk'
import {
  addLevel, addPair, canAddLevel, deletePair, levelName, movePair, nextFreePair, pairKey, pairsOf, partnerChoices, removeLevel, setBonus, setConversations,
  setLevelNames, setLevelPoints, setMaxLevels, setPairStart, setPointSource, setRange, setTalk, STAT_PREFIX,
} from '../../src/model/supports'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id
/** The demo with three characters (Tamsin, Oswin, Brin), the feature on, and the first map holding none of them. */
function world(): ProjectDoc {
  let d: ProjectDoc = { ...loadDemo('demo-srpg'), characters: [], supports: null }
  d = { ...d, maps: { ...d.maps, [mapId(d)]: { ...d.maps[mapId(d)]!, units: [], events: [], objectives: undefined } } }
  for (const n of ['Tamsin', 'Oswin', 'Brin']) d = addCharacter(d, n)
  return switchOnFeature(d, 'supports')
}
const place = (d: ProjectDoc, ...ids: string[]): ProjectDoc => {
  const m = d.maps[mapId(d)]!
  return { ...d, maps: { ...d.maps, [mapId(d)]: { ...m, units: [...(m.units ?? []), ...ids.map((c, i) => ({ character: c, side: 'blue', x: i, y: 0 }))] } } }
}
const withEvents = (d: ProjectDoc, events: StoryEvent[]): ProjectDoc => ({ ...d, maps: { ...d.maps, [mapId(d)]: { ...d.maps[mapId(d)]!, events } } })
const two = (): ProjectDoc => addPair(world())

describe('pairs', () => {
  it('a new pair is the first two characters that are not a pair, with one level of 20 points and nothing else', () => {
    const d = two()
    expect(d.supports).toEqual({ pairs: [{ a: 'tamsin', b: 'oswin', levels: [{ points: 20 }] }] })
    expect(nextFreePair(d)).toEqual(['tamsin', 'brin'])
    expect(pairsOf(addPair(d))[1]).toMatchObject({ a: 'tamsin', b: 'brin' })
  })
  it('no pair can be added with fewer than two characters or when every two are already a pair', () => {
    const one = switchOnFeature({ ...loadDemo('demo-srpg'), characters: [], supports: null }, 'supports')
    expect(addPair(addCharacter(one, 'Solo'))).toEqual(addCharacter(one, 'Solo'))
    let d = world()
    for (let i = 0; i < 3; i++) d = addPair(d)
    expect(pairsOf(d)).toHaveLength(3)
    expect(addPair(d)).toBe(d)
  })
  it('a slot offers every character except the other one and those that would make a pair that exists', () => {
    const d = addPair(addPair(world())) // tamsin-oswin, tamsin-brin
    expect(partnerChoices(d, 0, 'a').map((c) => c.id)).toEqual(['tamsin', 'brin']) // not oswin (the other one); brin and oswin are no pair yet
    expect(partnerChoices(d, 0, 'b').map((c) => c.id)).toEqual(['oswin']) // brin is taken: tamsin and brin are pair 2
    const lone = two()
    expect(partnerChoices(lone, 0, 'b').map((c) => c.id)).toEqual(['oswin', 'brin'])
  })
  it('the key names the pair; the start is not written when 0; reorder and delete', () => {
    let d = addPair(two())
    expect(pairKey(pairsOf(d)[0])).toBe('tamsin.oswin')
    d = setPairStart(d, 0, 15)
    expect(pairsOf(d)[0].start).toBe(15)
    d = setPairStart(d, 0, 0)
    expect(pairsOf(d)[0]).not.toHaveProperty('start')
    d = movePair(d, 0, 1)
    expect(pairKey(pairsOf(d)[0])).toBe('tamsin.brin')
    expect(pairsOf(deletePair(d, 0))).toHaveLength(1)
  })
  it('keeps keys it does not know', () => {
    const d = two()
    const odd = { ...d, supports: { ...d.supports!, mystery: 1, pairs: [{ ...d.supports!.pairs[0], note: 'x' }] } }
    expect(setPairStart(odd, 0, 5).supports).toMatchObject({ mystery: 1, pairs: [{ note: 'x', start: 5 }] })
  })
})

describe('levels', () => {
  it('a new level asks for 20 more than the last, up to 5 levels', () => {
    let d = two()
    for (let i = 0; i < 4; i++) d = addLevel(d, 0)
    expect(pairsOf(d)[0].levels.map((l) => l.points)).toEqual([20, 40, 60, 80, 100])
    expect(canAddLevel(pairsOf(d)[0])).toBe(false)
    expect(addLevel(d, 0)).toBe(d)
  })
  it('levels always rise: a value not above the one before is raised, later levels move up with it', () => {
    let d = addLevel(addLevel(two(), 0), 0) // 20 40 60
    d = setLevelPoints(d, 0, 1, 10)
    expect(pairsOf(d)[0].levels.map((l) => l.points)).toEqual([20, 21, 60])
    d = setLevelPoints(d, 0, 0, 70)
    expect(pairsOf(d)[0].levels.map((l) => l.points)).toEqual([70, 71, 72])
    d = setLevelPoints(d, 0, 0, 0)
    expect(pairsOf(d)[0].levels[0].points).toBe(0)
  })
  it('the last level cannot be removed; others can', () => {
    let d = addLevel(two(), 0)
    d = removeLevel(d, 0, 0)
    expect(pairsOf(d)[0].levels).toEqual([{ points: 40 }])
    expect(removeLevel(d, 0, 0)).toBe(d)
  })
  it('names: a level is its number until the rules name it; adding a level extends the names so the loader accepts them', () => {
    let d = addLevel(two(), 0)
    expect(levelName(d.supports!.rules, 1)).toBe('2')
    d = setLevelNames(d, ['C', ''])
    expect(d.supports!.rules!.levelNames).toEqual(['C', '2'])
    d = addLevel(d, 0)
    expect(d.supports!.rules!.levelNames).toEqual(['C', '2', '3'])
    expect(levelName(d.supports!.rules, 0)).toBe('C')
    expect(setLevelNames(d, ['', '', '']).supports).not.toHaveProperty('rules')
  })
})

describe('bonus and conversation', () => {
  it('numbers and stats are written under bonus; removing the last one removes bonus', () => {
    let d = setBonus(two(), 0, 0, 'hit', 5)
    d = setBonus(d, 0, 0, `${STAT_PREFIX}def`, 1)
    d = setBonus(d, 0, 0, 'damageTaken', -2)
    expect(pairsOf(d)[0].levels[0].bonus).toEqual({ hit: 5, stats: { def: 1 }, damageTaken: -2 })
    d = setBonus(setBonus(setBonus(d, 0, 0, 'hit', undefined), 0, 0, `${STAT_PREFIX}def`, undefined), 0, 0, 'damageTaken', undefined)
    expect(pairsOf(d)[0].levels[0]).toEqual({ points: 20 })
  })
  it('an empty conversation removes talk', () => {
    let d = setTalk(two(), 0, 0, [{ say: 'Hello.', speaker: 'tamsin' }])
    expect(pairsOf(d)[0].levels[0].talk).toEqual([{ say: 'Hello.', speaker: 'tamsin' }])
    d = setTalk(d, 0, 0, [])
    expect(pairsOf(d)[0].levels[0]).not.toHaveProperty('talk')
  })
  it('the speakers of a conversation are all characters, wherever they stand', () => {
    expect((talkBase(world()).units ?? []).map((u) => u.character)).toEqual(['tamsin', 'oswin', 'brin'])
  })
})

describe('rules: a default is never written', () => {
  it('point sources are off when empty or 0; range 3, "both" and no limit are not written; rules goes when empty', () => {
    let d = setPointSource(two(), 'endTurnAdjacent', 1)
    d = setPointSource(d, 'fightTogether', 2)
    expect(d.supports!.rules).toEqual({ points: { endTurnAdjacent: 1, fightTogether: 2 } })
    d = setPointSource(setPointSource(d, 'endTurnAdjacent', 0), 'fightTogether', undefined)
    expect(d.supports).not.toHaveProperty('rules')
    d = setRange(d, 3)
    d = setConversations(d, 'both')
    expect(d.supports).not.toHaveProperty('rules')
    d = setMaxLevels(setConversations(setRange(d, 5), 'camp'), 4)
    expect(d.supports!.rules).toEqual({ range: 5, conversations: 'camp', maxLevels: 4 })
  })
  it('keeps keys of rules it does not know', () => {
    const d = { ...two(), supports: { pairs: [], rules: { mystery: true } } as never }
    expect(setRange(d, 4).supports!.rules).toEqual({ mystery: true, range: 4 })
  })
})

describe('checks', () => {
  it('"These two never meet" until a map has both (placed, or added by an event)', () => {
    let d = two()
    expect(supportChecks(d).map((c) => c.text)).toContain('These two never meet: no map has both of them.')
    d = place(d, 'tamsin')
    expect(supportChecks(d).some((c) => c.text.startsWith('These two never meet'))).toBe(true)
    expect(supportChecks(place(d, 'oswin')).some((c) => c.text.startsWith('These two never meet'))).toBe(false)
    const added = withEvents(place(d), [{ id: 'e', when: { battle: 'start' }, do: [{ addUnit: { character: 'oswin', side: 'blue', x: 3, y: 3 } }, { addUnit: { character: 'tamsin', side: 'blue', x: 4, y: 3 } }] }])
    expect(supportChecks(added).some((c) => c.text.startsWith('These two never meet'))).toBe(false)
  })
  it('a level that no source of points can reach; a story that gives points, or a source, or a start that reaches it, clears it', () => {
    const base = place(two(), 'tamsin', 'oswin')
    const text = (d: ProjectDoc) => supportChecks(d).map((c) => c.text).join('|')
    expect(text(base)).toContain('Level 1 can never be reached')
    expect(text(setPointSource(base, 'heal', 2))).toBe('')
    expect(text(setPairStart(base, 0, 20))).toBe('')
    expect(text(withEvents(base, [{ id: 'e', when: { battle: 'start' }, do: [{ addSupport: ['oswin', 'tamsin'], by: 5 }] }]))).toBe('')
    expect(text(withEvents(base, [{ id: 'e', when: { battle: 'start' }, do: [{ addSupport: ['oswin', 'tamsin'], by: -5 }] }]))).toContain('can never be reached')
    // Level names make the message speak of the level as the game does.
    expect(text(setLevelNames(base, ['C']))).toContain('Level C can never be reached')
  })
  it('conversations only between chapters without a campaign', () => {
    let d = setPointSource(place(two(), 'tamsin', 'oswin'), 'heal', 1)
    d = setTalk(setConversations(d, 'camp'), 0, 0, [{ say: 'Hi' }])
    expect(supportChecks({ ...d, campaign: null }).map((c) => c.text).join()).toContain('only played between chapters')
    expect(supportChecks(setConversations(d, 'both')).map((c) => c.text).join()).not.toContain('between chapters')
  })
  it('nothing for a game without pairs, and nothing about meeting while a map cannot be read', () => {
    expect(supportChecks(world())).toEqual([])
    const d = two()
    expect(supportChecks({ ...d, maps: { ...d.maps, broken: null } }).some((c) => c.text.startsWith('These two never'))).toBe(false)
  })
})

describe('story: command, condition, trigger', () => {
  const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps[mapId(d)]! })
  it('start with the first pair, valid at once; with no pair they are empty ids', () => {
    const d = two()
    expect(newCommand(ctx(d), 'addSupport')).toEqual({ addSupport: ['tamsin', 'oswin'], by: 10 })
    expect(newCondition(ctx(d), 'support')).toEqual({ support: ['tamsin', 'oswin'], op: '>=', value: 1 })
    expect(newTrigger(ctx(d), 'support')).toEqual({ support: ['tamsin', 'oswin'] })
    expect(newCommand(ctx(world()), 'addSupport').addSupport).toEqual(['', ''])
  })
  it('the editor finds the kind of each', () => {
    expect(commandKind({ addSupport: ['a', 'b'], by: 1 })).toBe('addSupport')
    expect(conditionKind({ support: ['a', 'b'], value: 1 })).toBe('support')
    expect(triggerKind({ support: ['a', 'b'], level: 2 })).toBe('support')
  })
  it('read as plain sentences, with the level named as the game names it', () => {
    let d = setLevelNames(addLevel(two(), 0), ['C', 'B'])
    const c = ctx(d)
    expect(commandText(c, { addSupport: ['tamsin', 'oswin'], by: 10 })).toBe('Gives 10 support points to Tamsin and Oswin')
    expect(commandText(c, { addSupport: ['tamsin', 'oswin'], by: -1 })).toBe('Takes 1 support point from Tamsin and Oswin')
    expect(conditionText(c, { support: ['tamsin', 'oswin'], op: '>=', value: 2 })).toBe('Tamsin and Oswin’s support level is at least 2')
    expect(triggerText(c, { support: ['tamsin', 'oswin'], level: 2 })).toBe('Tamsin and Oswin reach support level B')
    expect(triggerText(c, { support: ['tamsin', 'oswin'] })).toBe('Tamsin and Oswin reach a support level')
    d = two()
    expect(commandText(ctx(d), { addSupport: ['', ''], by: 3 })).toBe('Gives 3 support points to two characters')
  })
  const story = (d: ProjectDoc): ProjectDoc => place(withEvents(d, [
    { id: 'bond', when: { support: ['oswin', 'tamsin'], level: 1 }, do: [{ say: 'Bonded.' }] },
    { id: 'gift', when: { battle: 'start' }, if: [{ support: ['tamsin', 'oswin'], op: '>=', value: 1 }], do: [{ addSupport: ['tamsin', 'oswin'], by: 5 }, { say: 'ok' }] },
    { id: 'other', when: { battle: 'start' }, do: [{ addSupport: ['tamsin', 'brin'], by: 5 }] },
    { id: 'solo', when: { battle: 'start' }, do: [{ addSupport: ['tamsin', 'oswin'], by: 2 }, { say: 'kept' }] },
  ]), 'tamsin', 'oswin')
  it('a pair the Supports screen does not declare, a missing character, a level that does not exist, numbers out of range', () => {
    const d = story(addPair(world())) // only tamsin-oswin is a pair
    const t = storyChecks(d, mapId(d)).map((c) => c.text)
    expect(t.some((x) => x.includes('Tamsin and Brin are not a support pair'))).toBe(true)
    expect(t.filter((x) => x.includes('not a support pair'))).toHaveLength(1)
    const bad = withEvents(d, [
      { id: 'a', when: { support: ['tamsin', 'oswin'], level: 3 }, do: [{ say: 'x' }] },
      { id: 'b', when: { battle: 'start' }, if: [{ support: ['tamsin', 'oswin'], value: 9 }], do: [{ addSupport: ['tamsin', 'ghost'], by: 5 }] },
    ])
    const u = storyChecks(bad, mapId(bad)).map((c) => c.text).join('|')
    expect(u).toContain('have 1 level, so the level must be from 1 to 1')
    expect(u).toContain('the level must be from 0 to 5')
    expect(u).toContain('“ghost” is not a character of this game')
  })
  it('deleting the pair removes what was about it and lists it first; other pairs are untouched', () => {
    const d = story(addPair(addPair(world())))
    const lines = pairUsageLines(d, 'tamsin', 'oswin').map((l) => l.text).join('|')
    expect(lines).toContain('bond')
    expect(lines).toContain('Gives 5 support points to Tamsin and Oswin')
    const after = deletePairClean(d, 0)
    // 'bond' (its trigger) and 'gift' (its condition) go whole; 'solo' loses only the command about the pair.
    expect(after.maps[mapId(after)]!.events!.map((e) => e.id)).toEqual(['other', 'solo'])
    expect(after.maps[mapId(after)]!.events![1].do).toEqual([{ say: 'kept' }])
  })
  it('giving a pair another character: what the story said follows the pair', () => {
    const d = story(addPair(world()))
    const after = retargetPair(d, 0, 'b', 'brin')
    expect(pairKey(pairsOf(after)[0])).toBe('tamsin.brin')
    const ev = after.maps[mapId(after)]!.events!
    expect(ev[0].when.support).toEqual(['brin', 'tamsin'])
    expect(ev[1].if![0].support).toEqual(['brin', 'tamsin']) // a pair is named by its two characters; the order the editor writes is alphabetical
    expect(ev[1].do[0].addSupport).toEqual(['brin', 'tamsin'])
  })
  it('deleting a character removes its pairs and the story about them, on maps and in scenes', () => {
    let d = story(addPair(addPair(world())))
    d = { ...d, campaign: { chapters: [{ id: 'c1', map: mapId(d), before: [{ addSupport: ['oswin', 'tamsin'], by: 5 }, { say: 'x' }], after: [{ addSupport: ['tamsin', 'brin'], by: 1 }] }] } as never }
    expect(campaignChecks(d)).toEqual([])
    expect(sceneAsMap(d, 'c1', 'before').events![0].do).toHaveLength(2)
    // The document store runs `reconcileCampaign` after every edit; scenes are cleaned there.
    const after = reconcileCampaign(d, deleteCharacter(d, 'oswin'))
    expect(pairsOf(after).map(pairKey)).toEqual(['tamsin.brin'])
    expect(after.maps[mapId(after)]!.events!.map((e) => e.id)).toEqual(['other', 'solo'])
    expect(JSON.stringify(after.campaign)).not.toContain('oswin')
    expect(JSON.stringify(after.campaign)).toContain('"brin"')
  })
  it('a scene command about a pair that is not declared is a problem the Campaign screen shows', () => {
    const d = { ...addPair(world()), campaign: { chapters: [{ id: 'c1', map: mapId(world()), before: [{ addSupport: ['tamsin', 'brin'], by: 5 }] }] } as never }
    expect(campaignChecks(d).map((c) => c.text).join()).toContain('Tamsin and Brin are not a support pair')
  })
})
