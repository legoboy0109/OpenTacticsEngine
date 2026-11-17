// Campaign parties in the editor's model (docs/EVENTS.md "Parties"): the list (add, rename, order, delete with follow-through), the two scene
// commands (defaults, sentences, the checks that mirror the loader, where they are offered), the chapter's own `party`, the timeline's
// "party" sign, and the hints (a party switched to and never merged or ended on, a merge of a party never used, a map naming characters only
// another party has).
import { describe, expect, it } from 'vitest'
import { addChapter, createCampaign, patchChapter } from '../../src/model/campaign'
import { campaignChecks } from '../../src/model/campaignChecks'
import { campaignUsagesOf } from '../../src/model/campaignRefs'
import { addParty, deleteParty, moveParty, newPartyId, partiesOf, partyHints, renameParty } from '../../src/model/parties'
import { newCommand } from '../../src/model/storyDefaults'
import { commandText } from '../../src/model/storySummary'
import { COMMAND_KINDS, SCENE_FX_KINDS, commandKind, type Command } from '../../src/model/storyTypes'
import { buildTimeline } from '../../src/model/timelineGraph'
import { addMarkers, countMarkers, noMarkers } from '../../src/model/timelineMarkers'
import { sceneMap } from '../../src/model/campaignRoster'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => createCampaign({ ...loadDemo('demo-srpg'), campaign: null })
const ctx = (d: ProjectDoc) => ({ doc: d, map: sceneMap(d) })
/** The campaign with parties main + north, three chapters, and the given scene commands. Chapter 1 is on the game's first map; the other two are on a
 *  copy of it whose named units have other names (the north party's own map). */
function story(before1: Command[] = [], before2: Command[] = [], before3: Command[] = []): ProjectDoc {
  let d = base()
  const first = d.game!.maps[0]
  const orig = d.maps[first.id]!
  const copy = { ...orig, units: (orig.units ?? []).map((u) => (u.id ? { ...u, id: `n_${u.id}` } : u)) }
  d = { ...d, game: { ...d.game!, maps: [...d.game!.maps, { id: 'northmap', name: 'North', file: 'maps/northmap.json' }] }, maps: { ...d.maps, northmap: copy } }
  d = addParty(addParty(d, 'The company', 'main'), 'The northern road', 'north')
  d = addChapter(addChapter(d, 'Two', 'two'), 'Three', 'three')
  const set = (doc: ProjectDoc, id: string, list: Command[], map: string) => patchChapter(patchChapter(doc, id, { before: list.length ? list : undefined }), id, { map })
  d = set(d, 'chapter_1', before1, first.id)
  d = set(d, 'two', before2, 'northmap')
  d = set(d, 'three', before3, 'northmap')
  return d
}
const SWITCH: Command = { switchParty: 'north' }
const BACK: Command = { switchParty: 'main' }
const MERGE: Command = { mergeParties: ['north'], into: 'main' }
const texts = (d: ProjectDoc) => campaignChecks(d).map((c) => `${c.level}: ${c.text}`)

describe('the list of parties', () => {
  it('writes the list only while it has a party, the first being the starting one', () => {
    let d = base()
    expect(d.campaign!.parties).toBeUndefined()
    d = addParty(d, 'The company')
    d = addParty(d, 'The northern road')
    expect(d.campaign!.parties).toEqual([{ id: 'the_company', name: 'The company' }, { id: 'the_northern_road', name: 'The northern road' }])
    expect(newPartyId(d, 'The company')).not.toBe('the_company')  // never an id in use
    d = renameParty(d, 'the_company', 'Eirik’s band')
    expect(partiesOf(d.campaign)[0]).toEqual({ id: 'the_company', name: 'Eirik’s band' })  // the id stays
    d = renameParty(d, 'the_company', '')
    expect(partiesOf(d.campaign)[0]).toEqual({ id: 'the_company' })  // an emptied name is written as no key
    d = moveParty(d, 1, 0)
    expect(partiesOf(d.campaign).map((p) => p.id)).toEqual(['the_northern_road', 'the_company'])
    expect(moveParty(d, 0, -1)).toBe(d)
    d = deleteParty(deleteParty(d, 'the_company'), 'the_northern_road')
    expect('parties' in d.campaign!).toBe(false)  // an emptied list leaves its key out
  })
  it('deleting a party cleans up every command and chapter that named it, and says so first', () => {
    let d = story([], [SWITCH, { say: 'x' }], [MERGE, BACK])
    d = patchChapter(d, 'two', { party: 'north' })
    const lines = campaignUsagesOf(d, [{ kind: 'party', id: 'north' }]).map((u) => u.text)
    expect(lines.length).toBeGreaterThanOrEqual(3)
    expect(lines.join('\n')).toContain('Switches to the party “The northern road”')
    expect(lines.join('\n')).toContain('Merges “The northern road” into “The company”')
    const after = deleteParty(d, 'north')
    const ch = (id: string) => after.campaign!.chapters.find((c) => c.id === id)!
    expect(ch('two').before).toEqual([{ say: 'x' }])  // the switch went, the rest stays
    expect(ch('two').party).toBeUndefined()
    expect(ch('three').before).toEqual([BACK])  // the merge had nothing left to merge
    expect(partiesOf(after.campaign).map((p) => p.id)).toEqual(['main'])
    expect(deleteParty(after, 'nothing')).toBe(after)
  })
  it('a merge that loses one of several parties keeps the others', () => {
    let d = addParty(story([], [{ mergeParties: ['north', 'west'], into: 'main' }]), 'West', 'west')
    d = deleteParty(d, 'north')
    expect(d.campaign!.chapters.find((c) => c.id === 'two')!.before).toEqual([{ mergeParties: ['west'], into: 'main' }])
    d = deleteParty(d, 'main')  // the party it merges into: the command goes
    expect(d.campaign!.chapters.find((c) => c.id === 'two')!.before ?? []).toEqual([])
  })
})

describe('the two commands', () => {
  it('are kinds the editor knows, found by their key, before the scene-presentation block', () => {
    expect(commandKind(SWITCH)).toBe('switchParty')
    expect(commandKind(MERGE)).toBe('mergeParties')
    const first = COMMAND_KINDS.indexOf(SCENE_FX_KINDS[0])
    expect(COMMAND_KINDS.indexOf('switchParty')).toBeLessThan(first)
    expect(COMMAND_KINDS.indexOf('mergeParties')).toBeLessThan(first)
    expect(COMMAND_KINDS.slice(-SCENE_FX_KINDS.length)).toEqual(SCENE_FX_KINDS)
  })
  it('start on the campaign’s parties and read as plain sentences', () => {
    const d = story()
    expect(newCommand(ctx(d), 'switchParty')).toEqual({ switchParty: 'north' })
    expect(newCommand(ctx(d), 'mergeParties')).toEqual({ mergeParties: ['north'], into: 'main' })
    expect(commandText(ctx(d), SWITCH)).toBe('Switches to the party “The northern road”')
    expect(commandText(ctx(d), MERGE)).toBe('Merges “The northern road” into “The company”')
    expect(commandText(ctx(d), { mergeParties: ['north', 'main'], into: 'x' })).toBe('Merges “The northern road” and “The company” into “x”')
  })
  it('mirror the loader: unknown parties, a merge into itself, a list twice, nothing to merge', () => {
    expect(texts(story([SWITCH, MERGE, BACK]))).toEqual([])
    expect(texts(story([{ switchParty: 'south' }])).join('\n')).toContain('“south” is not a party of this campaign')
    expect(texts(story([{ switchParty: '' }])).join('\n')).toContain('pick a party')
    expect(texts(story([{ mergeParties: ['north'], into: 'south' }])).join('\n')).toContain('“south” is not a party')
    expect(texts(story([{ mergeParties: ['main'], into: 'main' }])).join('\n')).toContain('cannot be merged into itself')
    expect(texts(story([{ mergeParties: ['north', 'north'], into: 'main' }])).join('\n')).toContain('listed twice')
    expect(texts(story([{ mergeParties: [], into: 'main' }])).join('\n')).toContain('choose the parties to merge')
    // A campaign without parties says where to add them.
    const none = { ...story(), campaign: { ...story().campaign!, parties: undefined } }
    expect(texts({ ...none, campaign: { ...none.campaign!, chapters: none.campaign!.chapters.map((c, i) => (i === 0 ? { ...c, before: [SWITCH] } : c)) } }).join('\n')).toContain('has no parties yet')
    // A chapter naming a party that is not there.
    expect(texts(patchChapter(story(), 'two', { party: 'south' })).join('\n')).toContain('“south” is not a party')
    // Two parties with the same internal name, or a bad one.
    const dup = { ...story(), campaign: { ...story().campaign!, parties: [{ id: 'main' }, { id: 'main' }] } }
    expect(texts(dup).join('\n')).toContain('another party has the same internal name')
  })
})

describe('hints', () => {
  it('say nothing about the owner’s own flow: switch in part 3, merge in part 4', () => {
    expect(partyHints(story([], [SWITCH], [MERGE]))).toEqual([])
    expect(partyHints(story())).toEqual([])
  })
  it('notice a party switched to and never merged or ended on', () => {
    const hints = partyHints(story([], [SWITCH], [BACK]))
    expect(hints.join('\n')).toContain('Party “The northern road” is switched to but never merged')
    expect(partyHints(story([], [], [SWITCH])).join('\n')).not.toContain('never merged')  // the story ends with it: fine
    expect(partyHints(story([], [SWITCH, MERGE], [])).join('\n')).not.toContain('never merged')
  })
  it('notice a merge of a party nothing switched to, and a switch to a merged party', () => {
    expect(partyHints(story([], [MERGE])).join('\n')).toContain('nothing has switched to it before this, so it is empty')
    expect(partyHints(story([], [SWITCH, MERGE], [SWITCH])).join('\n')).toContain('was merged into another party earlier')
  })
  it('a chapter’s own party counts as a switch', () => {
    let d = story([], [], [MERGE])
    d = patchChapter(d, 'two', { party: 'north' })
    expect(partyHints(d)).toEqual([])
  })
  it('notice a map that names characters only another party has', () => {
    // Chapter 2 is north's on its own map; chapter 3 goes back to main on that same map: main never had those characters, north did.
    const hints = partyHints(story([], [SWITCH], [BACK])).join('\n')
    expect(hints).toContain('puts n_')
    expect(hints).toContain('only “The northern road” has them, not “The company”')
    // The first map's own characters in a chapter north plays: main had them.
    let d = story([], [SWITCH], [])
    d = patchChapter(d, 'three', { map: d.game!.maps[0].id })
    expect(partyHints(d).join('\n')).toContain('only “The company” has them, not “The northern road”')
  })
  it('show up with the other checks as hints, not errors', () => {
    const list = campaignChecks(story([], [SWITCH], [BACK]))
    expect(list.filter((c) => c.level === 'error')).toEqual([])
    expect(list.some((c) => c.level === 'hint' && c.text.includes('switched to but never merged'))).toBe(true)
  })
})

describe('the timeline', () => {
  it('counts a switch or merge as a party sign, and the file without them keeps its signs as they were', () => {
    expect(countMarkers([SWITCH, MERGE, { say: 'x' }])).toMatchObject({ party: 2 })
    expect(countMarkers([{ say: 'x' }])).toEqual({ join: 0, leave: 0, items: 0, shop: 0, flags: 0, questions: 0 })
    expect(addMarkers({ ...noMarkers(), party: 1 }, { ...noMarkers(), party: 2 }).party).toBe(3)
    expect(addMarkers(noMarkers(), noMarkers())).toEqual(noMarkers())
  })
  it('puts the sign on the before scene, and on the level when the chapter only has a party key', () => {
    let d = story([SWITCH])
    d = patchChapter(d, 'three', { party: 'main' })
    const g = buildTimeline(d)!
    expect(g.nodes.find((n) => n.id === 'chapter_1:before')!.markers.party).toBe(1)
    expect(g.nodes.find((n) => n.id === 'three:level')!.markers.party).toBe(1)
    expect(g.nodes.find((n) => n.id === 'two:level')!.markers.party).toBeUndefined()
  })
})
