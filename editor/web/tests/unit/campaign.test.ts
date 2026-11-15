// The campaign in the editor's model: create / remove, rules, chapters, scenes (edited through the scene map), the roster,
// summaries, checks, the flow, and how the campaign follows deletes and renames of the rest of the game.
import { describe, expect, it } from 'vitest'
import type { Api, PutBody, Result, PutResult } from '../../src/api/client'
import {
  addChapter, createCampaign, deleteChapter, freeMaps, freeMode, moveChapter, removeCampaign, sceneAsMap, sceneOf, setCampaignName, setChapterMap,
  setChapterName, setChapterNext, setDefeatRule, setFallenRule, setFreeMap, setFreeMode, withScene,
} from '../../src/model/campaign'
import { campaignChecks } from '../../src/model/campaignChecks'
import { flowRows } from '../../src/model/campaignFlow'
import { campaignUsages, mapDeleteUsages, reconcileCampaign } from '../../src/model/campaignRefs'
import { humanSides, rosterUnits, sceneMap } from '../../src/model/campaignRoster'
import { deleteMap } from '../../src/model/maps'
import { removeResource } from '../../src/model/rulesLists'
import { addCommand, patchCommand, removeCommand } from '../../src/model/storyEdit'
import { newCommand } from '../../src/model/storyDefaults'
import { namedUnits } from '../../src/model/storyIds'
import { commandText, commandsSummary } from '../../src/model/storySummary'
import { removeStoryItem, storyItemUsages } from '../../src/model/storyLists'
import { changeUnitStoryId, removeSideAndStory, removeUnitAndStory } from '../../src/model/storyUnits'
import type { Command } from '../../src/model/storyTypes'
import type { ProjectDoc, ServerProject } from '../../src/model/types'
import { buildBody } from '../../src/store/autosave'
import { DocumentStore } from '../../src/store/documentStore'
import { loadDemo } from './fixtures'

const demo = (): ProjectDoc => loadDemo('demo-srpg')
const chapter = (d: ProjectDoc, id: string) => d.campaign!.chapters.find((c) => c.id === id)!
/** A game with no campaign yet: the demo without it. */
const bare = (): ProjectDoc => ({ ...demo(), campaign: null })

describe('the demo campaign', () => {
  it('is read as it is, round-trips, and has nothing to complain about', () => {
    const d = demo()
    expect(d.campaign!.chapters.map((c) => c.id)).toEqual(['ford', 'pass'])
    expect(JSON.parse(JSON.stringify(d.campaign))).toEqual(d.campaign)
    expect(campaignChecks(d)).toEqual([])
  })
  it('sends nothing to the helper when nothing was edited', () => {
    const d = demo()
    expect(buildBody(d, d, 'r', false)).toBeNull()
    // A full save of the unchanged campaign is the same object content (the helper skips byte-identical files).
    expect((buildBody(d, d, 'r', true) as PutBody).campaign).toEqual(d.campaign)
  })
})

describe('create and remove', () => {
  it('creates a one-chapter campaign on the first map, and only when there is none', () => {
    const d = createCampaign(bare())
    expect(d.campaign).toEqual({ name: 'Emblem Demo', chapters: [{ id: 'chapter_1', name: 'Chapter 1', map: d.game!.maps[0].id }] })
    expect(campaignChecks(d)).toEqual([])
    expect(createCampaign(d)).toBe(d)
  })
  it('removing it is written as `campaign: false`; undoing that writes the campaign back', () => {
    const d = demo()
    const gone = removeCampaign(d)
    expect(gone.campaign).toBeNull()
    expect((buildBody(gone, d, 'r', false) as PutBody).campaign).toBe(false)
    expect((buildBody(d, gone, 'r', false) as PutBody).campaign).toEqual(d.campaign)
    expect(buildBody(gone, gone, 'r', true)).not.toHaveProperty('campaign') // nothing to remove when there was none
  })
})

describe('rules and free battles', () => {
  it('writes a rule only when it differs from the default, and drops an emptied rules block', () => {
    let d = createCampaign(bare())
    d = setFallenRule(d, 'return')
    d = setDefeatRule(d, 'end')
    expect(d.campaign!.rules).toEqual({ fallen: 'return', onDefeat: 'end' })
    d = setFallenRule(d, 'gone')
    expect(d.campaign!.rules).toEqual({ onDefeat: 'end' })
    d = setDefeatRule(d, 'retry')
    expect(d.campaign).not.toHaveProperty('rules')
  })
  it('free battles: none (no key) / all / chosen maps in the game’s order', () => {
    let d = createCampaign(bare())
    expect(freeMode(d.campaign!)).toBe('none')
    d = setFreeMode(d, 'all')
    expect(d.campaign!.freeBattles).toBe('all')
    d = setFreeMode(d, 'chosen')
    expect(d.campaign!.freeBattles).toEqual([])
    const [a, b] = d.game!.maps.map((m) => m.id)
    d = setFreeMap(d, b, true)
    d = setFreeMap(d, a, true)
    expect(freeMaps(d.campaign!)).toEqual([a, b])
    d = setFreeMap(d, a, false)
    expect(d.campaign!.freeBattles).toEqual([b])
    d = setFreeMode(d, 'none')
    expect(d.campaign).not.toHaveProperty('freeBattles')
  })
  it('the name may be emptied (no key)', () => {
    const d = setCampaignName(createCampaign(bare()), '')
    expect(d.campaign).not.toHaveProperty('name')
  })
})

describe('chapters', () => {
  it('adds on the last chapter’s map, moves, renames and clears the name to no key', () => {
    let d = createCampaign(bare())
    d = addChapter(d, 'The Pass')
    expect(d.campaign!.chapters.map((c) => c.id)).toEqual(['chapter_1', 'the_pass'])
    d = moveChapter(d, 1, 0)
    expect(d.campaign!.chapters.map((c) => c.id)).toEqual(['the_pass', 'chapter_1'])
    d = setChapterName(d, 'the_pass', '')
    expect(chapter(d, 'the_pass')).not.toHaveProperty('name')
    d = setChapterMap(d, 'the_pass', d.game!.maps[1].id)
    expect(chapter(d, 'the_pass').map).toBe(d.game!.maps[1].id)
  })
  it('"next" set to the default is no key', () => {
    let d = setChapterNext(demo(), 'ford', 'pass')
    expect(chapter(d, 'ford').next).toBe('pass')
    d = setChapterNext(d, 'ford', undefined)
    expect(chapter(d, 'ford')).not.toHaveProperty('next')
  })
  it('deleting a chapter removes the "next" and the "go to" that named it (one edit)', () => {
    let d = setChapterNext(demo(), 'ford', 'pass')
    d = withScene(d, 'ford', 'after', {
      ...sceneAsMap(d, 'ford', 'after'),
      events: [{ id: 'x', when: { battle: 'start' }, do: [...sceneOf(d, 'ford', 'after'), { if: [{ switch: 'sera_joined' }], then: [{ goto: 'pass' }] }] }],
    })
    const gone = deleteChapter(d, 'pass')
    expect(gone.campaign!.chapters.map((c) => c.id)).toEqual(['ford'])
    expect(chapter(gone, 'ford')).not.toHaveProperty('next')
    expect(JSON.stringify(chapter(gone, 'ford').after)).not.toContain('goto')
  })
})

describe('scenes are edited with the map editor’s command operations', () => {
  it('adds, patches and removes commands; an emptied scene leaves its key out', () => {
    let d = createCampaign(bare())
    const edit = (fn: (m: ReturnType<typeof sceneAsMap>) => ReturnType<typeof sceneAsMap>) => { d = withScene(d, 'chapter_1', 'before', fn(sceneAsMap(d, 'chapter_1', 'before'))) }
    const eid = sceneAsMap(d, 'chapter_1', 'before').events![0].id
    const ctx = { doc: d, map: sceneMap(d), scene: { goto: false } }
    edit((m) => addCommand(m, eid, [], newCommand(ctx, 'say')))
    edit((m) => patchCommand(m, eid, [], 0, { say: 'Hello', speakerName: 'Narrator' }))
    expect(chapter(d, 'chapter_1').before).toEqual([{ say: 'Hello', speakerName: 'Narrator' }])
    expect(chapter(d, 'chapter_1')).not.toHaveProperty('after')
    edit((m) => removeCommand(m, eid, [], 0))
    expect(chapter(d, 'chapter_1')).not.toHaveProperty('before')
  })
  it('a new "If" in a scene starts from a switch, and a unit condition holds a plain roster id', () => {
    const d = demo()
    const ctx = { doc: d, map: sceneMap(d), scene: { goto: true } }
    expect(newCommand(ctx, 'if')).toEqual({ if: [{ switch: 'sera_joined' }], then: [] })
    expect(newCommand(ctx, 'goto')).toEqual({ goto: 'ford' })
  })
})

describe('the roster of the scenes', () => {
  it('is the named units of human sides of every map; sides are the human ones', () => {
    const d = demo()
    const ids = rosterUnits(d).map((u) => u.id)
    expect(ids).toEqual(expect.arrayContaining(['eirik', 'sera', 'kell']))
    expect(humanSides(d).map((s) => s.id)).toContain('blue')
    expect(namedUnits(d, sceneMap(d)).map((u) => u.id)).toEqual(ids)
  })
  it('does not include units of the computer’s sides', () => {
    const d = demo()
    const all = Object.values(d.maps).flatMap((m) => (m!.units ?? []).filter((u) => u.id))
    const ai = new Set(Object.values(d.maps).flatMap((m) => (m!.sides ?? []).filter((s) => s.controller === 'ai').map((s) => s.id)))
    expect(all.some((u) => ai.has(u.side) && u.id === 'gorm')).toBe(true)
    expect(rosterUnits(d).map((r) => r.id)).not.toContain('gorm')
    // sera starts on the computer's side in one map but is added to the player's side by an event of another: she is in
    expect(rosterUnits(d).map((r) => r.id)).toContain('sera')
  })
})

describe('plain sentences', () => {
  it('summarises the first two commands of a scene', () => {
    const d = demo()
    const ctx = { doc: d, map: sceneMap(d) }
    expect(commandsSummary(ctx, sceneOf(d, 'ford', 'before'), '')).toBe('Narrator says “Three days\' march, and the Red…”, Eirik says “Then that one is ours by nightf…”')
    expect(commandsSummary(ctx, [], 'none')).toBe('none')
  })
  it('says what each scene command does, including "go to"', () => {
    const d = demo()
    const ctx = { doc: d, map: sceneMap(d) }
    const after = sceneOf(d, 'ford', 'after')
    expect(commandText(ctx, after[2])).toMatch(/^Gives side .* 200 /)
    expect(commandText(ctx, { goto: 'pass' })).toBe('Goes on to chapter “The North Road”')
    expect(commandText(ctx, { if: [{ alive: 'sera' }], then: [] } as Command)).toMatch(/is alive/)
  })
})

describe('checks', () => {
  const texts = (d: ProjectDoc) => campaignChecks(d).map((c) => `${c.level}: ${c.text}`)
  it('a campaign without chapters', () => {
    const d = { ...createCampaign(bare()) }
    d.campaign = { ...d.campaign!, chapters: [] }
    expect(texts(d)).toEqual(['error: A campaign needs at least one chapter.'])
  })
  it('what the loader refuses', () => {
    let d = createCampaign(bare())
    d = addChapter(d, 'Two')
    d = setChapterMap(d, 'chapter_1', '')
    d = setChapterNext(d, 'two', 'nope')
    d = withScene(d, 'chapter_1', 'before', { ...sceneAsMap(d, 'chapter_1', 'before'), events: [{ id: 'x', when: { battle: 'start' }, do: [{ say: ' ' }, { goto: 'two' }, { setSwitch: 'ghost', to: true }] }] })
    const t = texts(d)
    expect(t).toContain('error: Chapter “Chapter 1”: pick a map.')
    expect(t).toContain('error: Chapter “Two”: “nope” is not a chapter of this campaign.')
    expect(t.some((x) => /Before the battle”, command 1: the text to say is empty/.test(x))).toBe(true)
    expect(t.some((x) => /Before the battle”, command 2: “Go to a chapter” only works in an “After the battle” scene/.test(x))).toBe(true)
    expect(t.some((x) => /command 3: “ghost” is not a switch of this game/.test(x))).toBe(true)
  })
  it('unknown map, duplicate chapter id, a gift with no resource', () => {
    let d = createCampaign(bare())
    d = setChapterMap(d, 'chapter_1', 'gone_map')
    d = { ...d, campaign: { ...d.campaign!, chapters: [...d.campaign!.chapters, { id: 'chapter_1', map: d.game!.maps[0].id }] } }
    d = withScene(d, 'chapter_1', 'after', { ...sceneAsMap(d, 'chapter_1', 'after'), events: [{ id: 'x', when: { battle: 'start' }, do: [{ giveResource: {}, side: 'blue' }] }] })
    const t = texts(d)
    expect(t).toContain('error: Chapter “Chapter 1”: “gone_map” is not a map of this game.')
    expect(t.some((x) => /another chapter has the same internal name/.test(x))).toBe(true)
    expect(t.some((x) => /choose a resource to give/.test(x))).toBe(true)
  })
  it('hints: a chapter nothing leads to, a speaker or side no map has; a map used twice is fine', () => {
    let d = createCampaign(bare())
    d = addChapter(d, 'Two')
    d = addChapter(d, 'Three')
    d = setChapterNext(d, 'chapter_1', 'three')
    d = withScene(d, 'chapter_1', 'before', { ...sceneAsMap(d, 'chapter_1', 'before'), events: [{ id: 'x', when: { battle: 'start' }, do: [{ say: 'Hi', speaker: 'nobody_here' }, { giveResource: { gold: 1 }, side: 'ghost_side' }] }] })
    const t = texts(d)
    expect(t).toContain('hint: Chapter “Two”: no chapter leads to it, so it is never played (set another chapter’s “Next chapter” to it, or use “Go to a chapter”).')
    expect(t.some((x) => x.startsWith('hint:') && /nobody_here/.test(x))).toBe(true)
    expect(t.some((x) => x.startsWith('hint:') && /ghost_side/.test(x))).toBe(true)
    // chapters 1 and 3 share the first map: no complaint about that
    expect(t.some((x) => /same map|twice/.test(x))).toBe(false)
  })
})

describe('the flow', () => {
  it('lists where each chapter leads and marks the ones nothing leads to', () => {
    let d = demo()
    d = setChapterNext(d, 'ford', 'pass')
    const rows = flowRows(d.campaign!)
    expect(rows.map((r) => [r.id, r.leadsTo.map((t) => t.id), r.ends, r.unreached])).toEqual([['ford', ['pass'], false, false], ['pass', [], true, false]])
    d = addChapter(d, 'Side story')
    d = setChapterNext(d, 'pass', 'ford')
    const rows2 = flowRows(d.campaign!)
    expect(rows2.find((r) => r.id === 'side_story')!.unreached).toBe(true)
    expect(rows2.find((r) => r.id === 'pass')!.leadsTo.map((t) => t.id)).toEqual(['ford'])
  })
  it('a "go to" is an extra way out', () => {
    let d = demo()
    d = withScene(d, 'ford', 'after', { ...sceneAsMap(d, 'ford', 'after'), events: [{ id: 'x', when: { battle: 'start' }, do: [...sceneOf(d, 'ford', 'after'), { goto: 'ford' }] }] })
    const row = flowRows(d.campaign!)[0]
    expect(row.leadsTo).toEqual([{ id: 'pass', name: 'The North Road', kind: 'next', known: true }, { id: 'ford', name: 'Skirmish at the Ford', kind: 'goto', known: true }])
  })
})

// --- the campaign follows the rest of the game (through the store, so the reconcile step runs) ---
function storeFor(doc: ProjectDoc) {
  const server: ServerProject = { ...doc, id: 'g', revision: 'r0', problems: [] }
  const api = {
    getProject: async () => ({ ok: true, value: server }) as Result<ServerProject>,
    putProject: async () => ({ ok: true, value: { revision: 'r1', problems: [] } }) as Result<PutResult>,
    listArt: async () => ({ ok: true, value: [] }),
    listModels: async () => ({ ok: true, value: [] }),
  } as unknown as Api
  return new DocumentStore(api, 'g', { now: () => 0 })
}
async function open(doc: ProjectDoc) {
  const s = storeFor(doc)
  await s.load()
  return s
}

describe('the campaign follows deletes and renames', () => {
  it('deleting a switch removes the conditions and commands that used it', async () => {
    const s = await open(demo())
    expect(storyItemUsages(s.getState().doc, 'switches', 'sera_joined').some((u) => u.text.startsWith('Campaign, chapter “Skirmish at the Ford”'))).toBe(true)
    s.edit((d) => removeStoryItem(d, 'switches', 'sera_joined'))
    expect(JSON.stringify(s.getState().doc.campaign)).not.toContain('sera_joined')
    // the if had a switch as its only condition: the whole command goes (a condition that cannot stay takes its owner)
    expect(sceneOf(s.getState().doc, 'ford', 'after').map((c) => Object.keys(c)[0])).toEqual(['say', 'giveResource'])
    s.undo()
    expect(JSON.stringify(s.getState().doc.campaign)).toContain('sera_joined')
  })
  it('deleting a resource takes it out of the gifts (a gift of nothing goes)', async () => {
    const s = await open(demo())
    s.edit((d) => removeResource(d, 'gold'))
    expect(sceneOf(s.getState().doc, 'ford', 'after').some((c) => c.giveResource)).toBe(false)
  })
  it('deleting a map clears the chapters on it and takes it off the free battles; one undo brings it all back', async () => {
    const d0 = demo()
    const d = { ...d0, campaign: { ...d0.campaign!, freeBattles: ['bridge', 'north_road'] } }
    expect(mapDeleteUsages(d, 'bridge').map((u) => u.text)).toEqual(expect.arrayContaining([
      'Campaign, chapter “Skirmish at the Ford”: its map is cleared (the scenes are kept; pick another map)',
      'Campaign: the map is taken off the list of free battles',
    ]))
    const s = await open(d)
    s.edit((x) => deleteMap(x, 'bridge'))
    const c = s.getState().doc.campaign!
    expect(chapter(s.getState().doc, 'ford').map).toBe('')
    expect(chapter(s.getState().doc, 'ford').before).toEqual(d.campaign!.chapters[0].before)
    expect(c.freeBattles).toEqual(['north_road'])
    expect(campaignChecks(s.getState().doc).map((x) => x.text)).toContain('Chapter “Skirmish at the Ford”: pick a map.')
    s.undo()
    expect(s.getState().doc.campaign).toEqual(d.campaign)
  })
  it('a unit’s story name renamed follows into the scenes once no map has the old name any more', async () => {
    const s = await open(demo())
    const at = (m: string) => s.getState().doc.maps[m]!.units!.find((u) => u.id === 'eirik')!
    const a = at('bridge')
    s.edit((x) => changeUnitStoryId(x, 'bridge', a.x, a.y, 'erik').doc)
    // north_road still has an "eirik": the roster member is the same, the scenes are left alone
    expect(JSON.stringify(s.getState().doc.campaign)).toContain('"speaker":"eirik"')
    const b = at('north_road')
    s.edit((x) => changeUnitStoryId(x, 'north_road', b.x, b.y, 'erik').doc)
    const text = JSON.stringify(s.getState().doc.campaign)
    expect(text).toContain('"speaker":"erik"')
    expect(text).not.toContain('"eirik"')
    s.undo()
    expect(JSON.stringify(s.getState().doc.campaign)).toContain('"speaker":"eirik"')
  })
  it('deleting the last unit with a story name removes the lines it speaks; a map that keeps the name keeps them', async () => {
    let d = demo()
    // "zed" lives only on the bridge; kell is on both maps
    d = { ...d, maps: { ...d.maps, bridge: { ...d.maps.bridge!, units: [...d.maps.bridge!.units!, { type: 'lord', side: 'blue', x: 0, y: 0, id: 'zed' }] } } }
    d = withScene(d, 'ford', 'before', { ...sceneAsMap(d, 'ford', 'before'), events: [{ id: 'x', when: { battle: 'start' }, do: [{ say: 'Hi', speaker: 'zed' }, { say: 'Yo', speaker: 'kell' }] }] })
    const s = await open(d)
    const at = (id: string) => s.getState().doc.maps.bridge!.units!.find((u) => u.id === id)!
    s.edit((x) => removeUnitAndStory(x, 'bridge', at('kell').x, at('kell').y))
    expect(sceneOf(s.getState().doc, 'ford', 'before').map((c) => c.speaker)).toEqual(['zed', 'kell'])
    s.edit((x) => removeUnitAndStory(x, 'bridge', at('zed').x, at('zed').y))
    expect(sceneOf(s.getState().doc, 'ford', 'before').map((c) => c.speaker)).toEqual(['kell'])
  })
  it('deleting a side only matters when no other map has it', () => {
    const d = demo()
    const usedBy = campaignUsages(d, [{ kind: 'side', id: 'blue' }])
    expect(usedBy.length).toBeGreaterThan(0)
    // another map keeps "blue": nothing is lost there
    expect(campaignUsages(d, [{ kind: 'side', id: 'blue' }], 'bridge')).toEqual(d.maps.north_road!.sides!.some((s) => s.id === 'blue') ? [] : usedBy)
  })
  it('removing a side from the only map that has it takes the gift away', async () => {
    let d = demo()
    // make "blue" exist only on the bridge
    d = { ...d, maps: { ...d.maps, bridge: { ...d.maps.bridge!, sides: [...d.maps.bridge!.sides!, { id: 'green', name: 'Green', controller: 'ai' }] }, north_road: { ...d.maps.north_road!, sides: d.maps.north_road!.sides!.map((s) => (s.id === 'blue' ? { ...s, id: 'blue2' } : s)), units: (d.maps.north_road!.units ?? []).map((u) => (u.side === 'blue' ? { ...u, side: 'blue2' } : u)) } } }
    const s = await open(d)
    s.edit((x) => removeSideAndStory(x, 'bridge', 'blue'))
    expect(sceneOf(s.getState().doc, 'ford', 'after').some((c) => c.giveResource)).toBe(false)
  })
  it('names that were already dangling are left alone', () => {
    const d = demo()
    const d1 = { ...d, campaign: { ...d.campaign!, chapters: [{ ...d.campaign!.chapters[0], next: 'nowhere' }, d.campaign!.chapters[1]] } }
    const d2 = { ...d1, game: { ...d1.game!, switches: [] } }
    expect(reconcileCampaign(d1, d2).campaign!.chapters[0].next).toBe('nowhere')
  })
})
