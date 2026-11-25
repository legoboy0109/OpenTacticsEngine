// Speech bubbles, notices, names, faces, loops and scene backdrops in the editor's model (docs/EVENTS.md "Speech bubbles, notices,
// names, faces and loops"): the model round trip (what is written, what is left out) and every check that mirrors a loader refusal.
import { describe, expect, it } from 'vitest'
import { campaignChecks } from '../../src/model/campaignChecks'
import { campaignBackdrop, backdropProblems, setCampaignBackdrop, setSceneBackdrop } from '../../src/model/campaignBackdrop'
import { sceneBackdrop, sceneCommands } from '../../src/model/campaignScene'
import { bubbleNames, bubblePortrait, dialogueProblems as styleProblems, dialogueStyle, setBubbleNames, setBubblePortrait, setDialogueStyle } from '../../src/model/dialogueStyle'
import { newCommand } from '../../src/model/storyDefaults'
import { dialogueProblems, commandsInLoops, maxExpressions, nameTokenProblem } from '../../src/model/storyDialogue'
import { addCommand, getList, type ListPath } from '../../src/model/storyEdit'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText } from '../../src/model/storySummary'
import { commandKind, type Command } from '../../src/model/storyTypes'
import { forEachCommand } from '../../src/model/storyWalk'
import { parseFrames, setPortraitFrames } from '../../src/model/portraits'
import { artChecks } from '../../src/model/artChecks'
import { renameBackground, removeBackground, reconcileBackgrounds } from '../../src/model/backgrounds'
import type { MapFile, ProjectDoc } from '../../src/model/types'

const map = (events: Array<{ id: string; do: Command[] }> = []): MapFile => ({
  name: 'M', grid: { width: 4, height: 3 }, legend: { a: 'grass' }, tiles: ['aaaa', 'aaaa', 'aaaa'],
  sides: [{ id: 'red', name: 'Red' }], units: [{ type: 'sol', side: 'red', x: 0, y: 0, id: 'hero' }],
  events: events.map((e) => ({ id: e.id, when: { battle: 'start' as const }, do: e.do })),
}) as unknown as MapFile

const doc = (over: Record<string, unknown> = {}, events: Array<{ id: string; do: Command[] }> = []): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [{ id: 'm', name: 'M', file: 'maps/m.json' }] },
  terrain: [], structures: null, weapons: null, items: null, research: null, units: [{ id: 'sol', name: 'Soldier' }], maps: { m: map(events) }, scripts: {},
  graphics: {
    sheets: [{ id: 'faces', file: 'art/faces.png', cellWidth: 16, cellHeight: 16 }],
    portraits: { hero: { sheet: 'faces', cell: [0, 0], expressions: [[1, 0], [2, 0]] } },
    backgrounds: { town: { sheet: 'faces' }, classroom: { sheet: 'faces' } },
  },
  campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
  ...over,
} as unknown as ProjectDoc)

const withCampaign = (before: unknown, extra: Record<string, unknown> = {}): ProjectDoc =>
  doc({ campaign: { chapters: [{ id: 'ch1', map: 'm', before }], ...extra } })

const checks = (d: ProjectDoc) => storyChecks(d, 'm').map((c) => c.text)
const sceneChecks = (d: ProjectDoc) => campaignChecks(d).map((c) => c.text)
const ev = (do_: Command[]) => doc({}, [{ id: 'e', do: do_ }])
/** The sentences of one command read on its own (map event: scene false; campaign scene: scene true). */
const one = (c: Command, scene = false, inLoop = false) => dialogueProblems(doc(), c, scene, inLoop)

describe('game.json "dialogue" (the Dialogue style card)', () => {
  it('reads as the box with no key, and a missing or bad word as the default', () => {
    expect(dialogueStyle(doc())).toBe('box')
    expect(bubblePortrait(doc())).toBe('beside')
    expect(bubbleNames(doc())).toBe(true)
    expect(dialogueStyle(doc({ game: { dialogue: { style: 'fancy' } } } as never))).toBe('box')
  })
  it('writes the bubble and the non-default words only; the defaults leave their keys out', () => {
    let d = setDialogueStyle(doc(), 'bubble')
    expect(d.game!.dialogue).toEqual({ style: 'bubble' })
    d = setBubblePortrait(d, 'inside')
    d = setBubbleNames(d, false)
    expect(d.game!.dialogue).toEqual({ style: 'bubble', portrait: 'inside', names: false })
    d = setBubblePortrait(d, 'beside')
    d = setBubbleNames(d, true)
    expect(d.game!.dialogue).toEqual({ style: 'bubble' })
    d = setDialogueStyle(d, 'box')
    expect('dialogue' in d.game!).toBe(false)
    expect(styleProblems(d)).toEqual([])
  })
  it('going back to the box takes portrait and names with it (the loader refuses them there)', () => {
    let d = setDialogueStyle(doc(), 'bubble')
    d = setBubblePortrait(setBubbleNames(d, false), 'none')
    expect(styleProblems(d)).toEqual([])
    d = setDialogueStyle(d, 'box')
    expect('dialogue' in d.game!).toBe(false)
  })
  it('does nothing while the box is chosen for the bubble-only settings, and keeps keys it does not know', () => {
    const same = doc()
    expect(setBubblePortrait(same, 'none')).toBe(same)
    expect(setBubbleNames(same, false)).toBe(same)
    const d = setDialogueStyle(doc({ game: { name: 'G', dialogue: { extra: 1 } } } as never), 'bubble')
    expect((d.game!.dialogue as Record<string, unknown>).extra).toBe(1)
  })
  const bad = (dialogue: unknown) => styleProblems(doc({ game: { name: 'G', dialogue } } as never))
  it('says each refusal: not an object, unknown key, bad words, bubble-only keys with the box', () => {
    expect(bad(5)).toHaveLength(1)
    expect(bad([])).toHaveLength(1)
    expect(bad({ style: 'bubble', colour: 'red' })[0]).toMatch(/colour/)
    expect(bad({ style: 'fancy' })[0]).toMatch(/style/)
    expect(bad({ style: 'bubble', portrait: 'above' })[0]).toMatch(/portrait/)
    expect(bad({ style: 'bubble', names: 'yes' })[0]).toMatch(/name/)
    expect(bad({ portrait: 'inside' })[0]).toMatch(/speech bubbles/)
    expect(bad({ style: 'box', names: false })[0]).toMatch(/speech bubbles/)
    expect(bad({ style: 'bubble', portrait: 'none', names: false })).toEqual([])
    expect(bad(undefined)).toEqual([])
  })
})

describe('the new commands', () => {
  const ctx = { doc: doc(), map: map() }
  it('start valid and are told apart from the others (a notice question stays a question)', () => {
    expect(newCommand(ctx, 'notice')).toEqual({ notice: '' })
    expect(newCommand(ctx, 'loop')).toEqual({ loop: [] })
    expect(newCommand(ctx, 'break')).toEqual({ break: true })
    expect(commandKind({ notice: 'x' })).toBe('notice')
    expect(commandKind({ loop: [] })).toBe('loop')
    expect(commandKind({ break: true })).toBe('break')
    expect(commandKind({ askName: 'hero' })).toBe('askName')
    expect(commandKind({ choice: 'Q', notice: true, answers: [] })).toBe('choice')
  })
  it('are described in a plain sentence', () => {
    const c = { doc: doc(), map: map() }
    expect(commandText(c, { notice: 'Clan grew.' })).toBe('Shows the notice “Clan grew.”')
    expect(commandText(c, { loop: [{ say: 'a' }, { say: 'b' }] })).toBe('Repeats 2 commands (at most 10 times)')
    expect(commandText(c, { loop: [{ say: 'a' }], max: 3 })).toBe('Repeats 1 command (at most 3 times)')
    expect(commandText(c, { break: true })).toBe('Leaves the repeat')
    expect(commandText(c, { askName: 'hero' })).toBe('Asks for Soldier’s name')
  })
  it('a loop is a list of its own in the edit path: add inside it and read it back', () => {
    const d = ev([{ loop: [{ say: 'one' }] }])
    const path: ListPath = [[0, 'loop']]
    const m = addCommand(d.maps.m!, 'e', path, { break: true })
    expect(getList(m.events![0].do, path)).toEqual([{ say: 'one' }, { break: true }])
    expect(m.events![0].do[0].loop).toHaveLength(2)
  })
  it('the story walk reaches the commands inside a loop (checks, renames), also through if and answers', () => {
    const list: Command[] = [{ loop: [{ say: 'a' }, { if: [{ switch: 's' }], then: [{ break: true }] }, { choice: 'Q', answers: [{ text: 'A', do: [{ say: 'deep' }] }, { text: 'B' }] }] }]
    const seen: string[] = []
    forEachCommand(list, 'x', (c) => seen.push(commandKind(c) ?? '?'))
    expect(seen).toEqual(['loop', 'say', 'if', 'break', 'choice', 'say'])
    const inLoop = commandsInLoops(list)
    expect(inLoop.has(list[0])).toBe(false)
    expect(inLoop.has(list[0].loop![1].then![0])).toBe(true)
    expect(inLoop.has(list[0].loop![2].answers![0].do![0])).toBe(true)
  })
})

describe('checks: faces', () => {
  it('a say or a question may name a face from 1 to the longest expressions list (2 here)', () => {
    expect(one({ say: 'Hi', face: 2 })).toEqual([])
    expect(one({ say: 'Hi', face: 3 })[0]).toMatch(/face 3 needs a portrait with at least 3 expressions \(the most any portrait has is 2\)/)
    expect(one({ say: 'Hi', face: 0 })[0]).toMatch(/1 to 15/)
    expect(one({ say: 'Hi', face: 16 })[0]).toMatch(/1 to 15/)
    expect(one({ say: 'Hi', face: 1.5 })[0]).toMatch(/whole number/)
    expect(one({ choice: 'Q', answers: [{ text: 'A' }, { text: 'B' }], face: 5 })[0]).toMatch(/face 5/)
  })
  it('a game with no expressions at all says so', () => {
    const d = doc({ graphics: { sheets: [], portraits: { hero: { sheet: 'x', cell: [0, 0] } } } })
    expect(maxExpressions(d)).toBe(0)
    expect(dialogueProblems(d, { say: 'Hi', face: 1 }, false, false)[0]).toMatch(/no portrait has any yet/)
  })
  it('is checked in map events and in campaign scenes', () => {
    expect(checks(ev([{ say: 'Hi', face: 9 }])).join('\n')).toMatch(/face 9/)
    expect(sceneChecks(withCampaign([{ say: 'Hi', face: 9 }])).join('\n')).toMatch(/face 9/)
    expect(checks(ev([{ say: 'Hi', face: 2 }]))).toEqual([])
  })
})

describe('checks: notices and the question window', () => {
  it('a notice needs text, takes only notice and auto, and auto is 1 to 60000', () => {
    expect(one({ notice: 'Hello' })).toEqual([])
    expect(one({ notice: '' })[0]).toMatch(/empty/)
    expect(one({ notice: 5 as never })[0]).toMatch(/empty/)
    expect(one({ notice: 'x', speaker: 'hero' })[0]).toMatch(/cannot have “speaker”/)
    expect(one({ notice: 'x', auto: 0 })[0]).toMatch(/1 to 60000/)
    expect(one({ notice: 'x', auto: 60001 })[0]).toMatch(/1 to 60000/)
    expect(one({ notice: 'x', auto: 1500 })).toEqual([])
  })
  it('a question’s notice is on or off; a notice question has no speaker, name, portrait or face', () => {
    const q = (extra: Record<string, unknown>): Command => ({ choice: 'Q', answers: [{ text: 'A' }, { text: 'B' }], ...extra })
    expect(one(q({ notice: true }))).toEqual([])
    expect(one(q({ notice: 'yes' }))[0]).toMatch(/on or off/)
    for (const k of ['speaker', 'speakerName', 'portrait', 'face'] as const)
      expect(one(q({ notice: true, [k]: k === 'face' ? 1 : 'hero' })).join('\n')).toMatch(/makes no sense/)
    expect(one(q({ notice: false, speaker: 'hero' }))).toEqual([])
  })
  it('cursor is a whole number from 1 to the number of answers', () => {
    const q = (cursor: unknown): Command => ({ choice: 'Q', answers: [{ text: 'A' }, { text: 'B' }, { text: 'C' }], cursor: cursor as number })
    expect(one(q(1))).toEqual([])
    expect(one(q(3))).toEqual([])
    expect(one(q(4))[0]).toMatch(/1 to the number of answers \(3\)/)
    expect(one(q(0))[0]).toMatch(/1 to the number of answers/)
    expect(one(q(1.5))[0]).toMatch(/whole number/)
    expect(one(q('2'))[0]).toMatch(/whole number/)
  })
  it('shows in the map event checks and the campaign scene checks', () => {
    expect(checks(ev([{ notice: '' }])).join('\n')).toMatch(/notice text is empty/)
    expect(sceneChecks(withCampaign([{ notice: '' }])).join('\n')).toMatch(/notice text is empty/)
    expect(checks(ev([{ notice: 'ok' }]))).toEqual([])
    expect(sceneChecks(withCampaign([{ notice: 'ok' }]))).toEqual([])
  })
})

describe('checks: loops and break', () => {
  it('a loop needs commands and a max from 1 to 100', () => {
    expect(one({ loop: [{ say: 'x' }] })).toEqual([])
    expect(one({ loop: [] })[0]).toMatch(/at least one command/)
    expect(one({ loop: 'x' as never })[0]).toMatch(/at least one command/)
    expect(one({ loop: [{ say: 'x' }], max: 0 })[0]).toMatch(/1 to 100/)
    expect(one({ loop: [{ say: 'x' }], max: 101 })[0]).toMatch(/1 to 100/)
    expect(one({ loop: [{ say: 'x' }], max: 2.5 })[0]).toMatch(/whole number/)
    expect(one({ loop: [{ say: 'x' }], max: 100 })).toEqual([])
    expect(one({ loop: [{ say: 'x' }], colour: 1 })[0]).toMatch(/cannot have “colour”/)
  })
  it('break must be true and only works inside a loop (any depth)', () => {
    expect(one({ break: true }, false, true)).toEqual([])
    expect(one({ break: true })[0]).toMatch(/only works inside a loop/)
    expect(one({ break: false as never }, false, true)[0]).toMatch(/break": true/)
    expect(one({ break: true, x: 1 }, false, true)[0]).toMatch(/cannot have/)
  })
  it('the whole-event check finds a break outside any loop and accepts one inside a loop, an if and an answer', () => {
    expect(checks(ev([{ break: true }])).join('\n')).toMatch(/only works inside a loop/)
    const ok = ev([{ loop: [{ choice: 'Again?', notice: true, cursor: 2, answers: [{ text: 'Yes' }, { text: 'No', do: [{ break: true }] }] }, { if: [{ switch: 's' }], then: [{ break: true }] }] }])
    expect(checks(ok).filter((t) => /loop/.test(t))).toEqual([])
    // a break after the loop (not inside it) is refused
    expect(checks(ev([{ loop: [{ say: 'a' }] }, { break: true }])).join('\n')).toMatch(/only works inside a loop/)
    expect(sceneChecks(withCampaign([{ break: true }])).join('\n')).toMatch(/only works inside a loop/)
    expect(sceneChecks(withCampaign([{ loop: [{ say: 'a' }, { break: true }] }]))).toEqual([])
  })
  it('an empty loop in a scene or event is reported', () => {
    expect(checks(ev([{ loop: [] }])).join('\n')).toMatch(/at least one command/)
    expect(sceneChecks(withCampaign([{ loop: [] }])).join('\n')).toMatch(/at least one command/)
  })
})

describe('checks: asking for a name', () => {
  it('is for campaign scenes only', () => {
    expect(one({ askName: 'hero' }, false)[0]).toMatch(/campaign scenes only/)
    expect(one({ askName: 'hero' }, true)).toEqual([])
    expect(checks(ev([{ askName: 'hero' }])).join('\n')).toMatch(/campaign scenes only/)
    expect(sceneChecks(withCampaign([{ askName: 'hero' }])).join('\n')).not.toMatch(/campaign scenes only/)
  })
  it('the id is a story id; max 1 to 24; default 1..max letters with no braces; prompt is text', () => {
    expect(one({ askName: '' }, true)[0]).toMatch(/pick the character/)
    expect(one({ askName: 'bad id' }, true)[0]).toMatch(/pick the character/)
    expect(one({ askName: 'hero', max: 0 }, true)[0]).toMatch(/1 to 24/)
    expect(one({ askName: 'hero', max: 25 }, true)[0]).toMatch(/1 to 24/)
    expect(one({ askName: 'hero', max: 24 }, true)).toEqual([])
    expect(one({ askName: 'hero', default: '' }, true)[0]).toMatch(/1 to 24 letters/)
    expect(one({ askName: 'hero', default: 'a{b' }, true)[0]).toMatch(/no \{ or \}/)
    expect(one({ askName: 'hero', default: 'a}' }, true)[0]).toMatch(/no \{ or \}/)
    expect(one({ askName: 'hero', default: 'x'.repeat(25), max: 24 }, true)[0]).toMatch(/1 to 24 letters/)
    expect(one({ askName: 'hero', default: 'Marche', max: 5 }, true)[0]).toMatch(/6 letters, more than the most allowed \(5\)/)
    expect(one({ askName: 'hero', default: 'Marche', max: 6 }, true)).toEqual([])
    expect(one({ askName: 'hero', default: 'Marche' }, true)).toEqual([]) // max 12 by default
    expect(one({ askName: 'hero', default: 'Thirteenletter' }, true)[0]).toMatch(/more than the most allowed \(12\)/)
    expect(one({ askName: 'hero', default: 'Zoë' , max: 3 }, true)).toEqual([]) // letters, not bytes
    expect(one({ askName: 'hero', prompt: '' }, true)[0]).toMatch(/empty/)
    expect(one({ askName: 'hero', prompt: 'Name?' }, true)).toEqual([])
    expect(one({ askName: 'hero', colour: 1 }, true)[0]).toMatch(/cannot have “colour”/)
  })
})

describe('checks: {name:id} tokens', () => {
  it('a "{name:" must be followed by an id and "}"', () => {
    expect(nameTokenProblem('Hello {name:hero}!')).toBeNull()
    expect(nameTokenProblem('{name:a}{name:b-2}')).toBeNull()
    expect(nameTokenProblem('no token {name}')).toBeNull()
    expect(nameTokenProblem('Hello {name:hero')).toMatch(/\{name:/)
    expect(nameTokenProblem('Hello {name:}')).toMatch(/\{name:/)
    expect(nameTokenProblem('Hello {name:bad id}')).toMatch(/\{name:/)
    expect(nameTokenProblem(undefined)).toBeNull()
  })
  it('is read in say, narrate, notice, a question, its answers and the name prompt', () => {
    expect(one({ say: 'x {name:' })).toHaveLength(1)
    expect(one({ narrate: 'x {name:' })).toHaveLength(1)
    expect(one({ notice: 'x {name:' })).toHaveLength(1)
    expect(one({ choice: 'x {name:', answers: [{ text: 'A' }, { text: 'B' }] })[0]).toMatch(/question/)
    expect(one({ choice: 'Q', answers: [{ text: 'A' }, { text: 'B {name:' }] })[0]).toMatch(/answer 2/)
    expect(one({ askName: 'hero', prompt: 'x {name:' }, true)[0]).toMatch(/words above/)
    expect(one({ say: 'Welcome, {name:hero}!' })).toEqual([])
  })
})

describe('portraits: expressions', () => {
  const d0 = doc()
  it('are set from a typed list and left out when emptied', () => {
    const parsed = parseFrames('1,1  2,1  3,2', 15)
    expect('cells' in parsed && parsed.cells).toEqual([[0, 0], [1, 0], [2, 1]])
    const d = setPortraitFrames(d0, 'hero', 'expressions', [[0, 0], [1, 1], [2, 1]])
    expect(d.graphics!.portraits!.hero.expressions).toEqual([[0, 0], [1, 1], [2, 1]])
    expect(maxExpressions(d)).toBe(3)
    const cleared = setPortraitFrames(d, 'hero', 'expressions', [])
    expect('expressions' in cleared.graphics!.portraits!.hero).toBe(false)
    expect(maxExpressions(cleared)).toBe(0)
  })
  it('allows up to 15, not 8 as the talk and blink lists do', () => {
    const many = Array.from({ length: 15 }, (_, i) => `${i + 1},1`).join(' ')
    expect('cells' in parseFrames(many, 15)).toBe(true)
    expect('error' in parseFrames(many)).toBe(true)
    expect('error' in parseFrames(`${many} 1,2`, 15)).toBe(true)
  })
  it('checks the list: 1 to 15 whole cells', () => {
    const art = (expressions: unknown) => artChecks(doc({ graphics: { sheets: [], portraits: { hero: { sheet: 'x', cell: [0, 0], expressions } } } })).map((c) => c.text)
    expect(art([[1, 0]])).toEqual([])
    expect(art([])[0]).toMatch(/1 to 15 expressions/)
    expect(art('x')[0]).toMatch(/1 to 15 expressions/)
    expect(art(Array.from({ length: 16 }, () => [0, 0]))[0]).toMatch(/1 to 15 expressions/)
    expect(art([[0, -1]])[0]).toMatch(/not a piece/)
    expect(art([[0]])[0]).toMatch(/not a piece/)
    expect(art([[0.5, 0]])[0]).toMatch(/not a piece/)
  })
})

describe('scene backdrops', () => {
  const list: Command[] = [{ say: 'Hi' }]
  it('the campaign backdrop is set and cleared (no key when none)', () => {
    let d = withCampaign(list)
    d = setCampaignBackdrop(d, 'town')
    expect(campaignBackdrop(d)).toBe('town')
    expect(d.campaign!.backdrop).toBe('town')
    d = setCampaignBackdrop(d, undefined)
    expect('backdrop' in d.campaign!).toBe(false)
  })
  it('a scene’s backdrop moves a plain list into the object form and back again', () => {
    let d = withCampaign(list)
    d = setSceneBackdrop(d, 'ch1', 'before', 'classroom')
    expect(d.campaign!.chapters[0].before).toEqual({ backdrop: 'classroom', commands: list })
    d = setSceneBackdrop(d, 'ch1', 'before', null)
    expect(d.campaign!.chapters[0].before).toEqual({ backdrop: null, commands: list })
    expect(sceneBackdrop(d.campaign!.chapters[0].before)).toBeNull()
    expect(sceneCommands(d.campaign!.chapters[0].before)).toEqual(list)
    d = setSceneBackdrop(d, 'ch1', 'before', undefined)
    expect(d.campaign!.chapters[0].before).toEqual(list)
  })
  it('keeps the music and drops an empty object; an empty scene can hold only a backdrop', () => {
    let d = withCampaign({ music: 'song', commands: list })
    d = setSceneBackdrop(d, 'ch1', 'before', 'town')
    expect(d.campaign!.chapters[0].before).toEqual({ music: 'song', commands: list, backdrop: 'town' })
    d = setSceneBackdrop(d, 'ch1', 'before', undefined)
    expect(d.campaign!.chapters[0].before).toEqual({ music: 'song', commands: list })
    let e = withCampaign(undefined)
    e = setSceneBackdrop(e, 'ch1', 'before', 'town')
    expect(e.campaign!.chapters[0].before).toEqual({ backdrop: 'town' })
    e = setSceneBackdrop(e, 'ch1', 'before', undefined)
    expect(e.campaign!.chapters[0].before).toBeUndefined()
  })
  it('says an unknown background of the campaign or of a scene; null and known ones are fine', () => {
    expect(backdropProblems(withCampaign(list, { backdrop: 'town' }))).toEqual([])
    expect(backdropProblems(withCampaign({ backdrop: null, commands: list }))).toEqual([])
    expect(backdropProblems(withCampaign({ backdrop: 'classroom', commands: list }))).toEqual([])
    expect(sceneChecks(withCampaign(list, { backdrop: 'nowhere' })).join('\n')).toMatch(/campaign’s backdrop: the background “nowhere”/)
    expect(sceneChecks(withCampaign(list, { backdrop: 7 })).join('\n')).toMatch(/campaign’s backdrop/)
    expect(sceneChecks(withCampaign({ backdrop: 'nowhere', commands: list })).join('\n')).toMatch(/Before the battle.*“nowhere”/)
    expect(sceneChecks(withCampaign({ backdrop: 5, commands: list })).join('\n')).toMatch(/Before the battle/)
    expect(sceneChecks(withCampaign({ backdrop: 'classroom', commands: list }, { backdrop: 'town' }))).toEqual([])
  })
  it('follows a renamed background and is taken off when the background is deleted', () => {
    let d = withCampaign({ backdrop: 'classroom', commands: list }, { backdrop: 'town' })
    const renamed = renameBackground(d, 'town', 'village')
    expect(renamed.campaign!.backdrop).toBe('village')
    expect(renamed.graphics!.backgrounds!.village).toBeDefined()
    const renamed2 = renameBackground(d, 'classroom', 'school')
    expect(sceneBackdrop(renamed2.campaign!.chapters[0].before)).toBe('school')
    const removed = reconcileBackgrounds(d, removeBackground(removeBackground(d, 'town'), 'classroom'))
    expect('backdrop' in removed.campaign!).toBe(false)
    expect(removed.campaign!.chapters[0].before).toEqual(list)
  })
})
