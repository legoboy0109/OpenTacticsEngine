// Dialogue choices in the editor's model (docs/EVENTS.md "Dialogue choices"): the command and its defaults, editing the answers'
// command lists, renames and clean-up reaching into answers, summaries, and the checks (map events and campaign scenes).
import { describe, expect, it } from 'vitest'
import { campaignChecks } from '../../src/model/campaignChecks'
import { flowRows } from '../../src/model/campaignFlow'
import { sceneAsMap, withScene } from '../../src/model/campaign'
import { addStoryItem, removeStoryItem } from '../../src/model/storyLists'
import { newCommand } from '../../src/model/storyDefaults'
import { addCommand, getList, moveCommand, patchCommand, removeCommand, setTriggerKind, type ListPath } from '../../src/model/storyEdit'
import { renameUnitRefs } from '../../src/model/storyRefs'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText } from '../../src/model/storySummary'
import { commandKind, type Command } from '../../src/model/storyTypes'
import { forEachCommand } from '../../src/model/storyWalk'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const map = (d: ProjectDoc) => d.maps[mapId(d)]!
const withMap = (d: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc => ({ ...d, maps: { ...d.maps, [mapId(d)]: fn(map(d)) } })

function base(): ProjectDoc {
  let d = loadDemo('demo-economy')
  d = withMap(d, (m) => ({ ...m, units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'gorm', name: 'Gorm' } : u)) }))
  d = addStoryItem(d, 'switches', 'Gate opened')
  d = addStoryItem(d, 'variables', 'Gate answer')
  d = addStoryItem(d, 'variables', 'Mood')
  return d
}
const sw = (d: ProjectDoc) => d.game!.switches![0].id
const v1 = (d: ProjectDoc) => d.game!.variables![0].id
const v2 = (d: ProjectDoc) => d.game!.variables![1].id

const choice = (d: ProjectDoc): Command => ({
  choice: 'Open the gate?', speaker: 'gorm', store: v1(d),
  answers: [
    { text: 'Open it', do: [{ say: 'It opens.' }, { setSwitch: sw(d), to: true }] },
    { text: 'Leave it', if: [{ switch: sw(d), is: false }], do: [] },
  ],
})
const withChoice = (d: ProjectDoc, c: Command = choice(d)): ProjectDoc => withMap(d, (m) => ({ ...m, events: [{ id: 'gate', when: { battle: 'start' }, do: [c] }] }))
const texts = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text)

describe('the command', () => {
  it('starts with two answers, Yes and No, and no question yet', () => {
    const c = newCommand({ doc: base(), map: map(base()) }, 'choice')
    expect(commandKind(c)).toBe('choice')
    expect(c.answers!.map((a) => a.text)).toEqual(['Yes', 'No'])
    expect(c.choice).toBe('')
  })
  it('is described in a plain sentence', () => {
    const d = withChoice(base())
    const cmd = map(d).events![0].do[0]
    expect(commandText({ doc: d, map: map(d) }, cmd)).toBe(`Gorm asks “Open the gate?” (2 answers, saved in “Gate answer”)`)
    expect(commandText({ doc: d, map: map(d) }, { choice: 'Q?', answers: [] })).toBe('Asks “Q?” (0 answers)')
  })
})

describe('editing the commands of an answer', () => {
  const path = (k: number): ListPath => [[0, `answer${k}`]]
  it('adds, moves, removes and reads commands by the answer’s path', () => {
    let d = withChoice(base())
    let m = addCommand(map(d), 'gate', path(1), { say: 'Fine.' })
    expect(getList(m.events![0].do, path(1))).toEqual([{ say: 'Fine.' }])
    expect(m.events![0].do[0].answers![1].do).toEqual([{ say: 'Fine.' }])
    m = addCommand(m, 'gate', path(1), { say: 'Second.' })
    m = moveCommand(m, 'gate', path(1), 0, 1)
    expect(getList(m.events![0].do, path(1))!.map((c) => c.say)).toEqual(['Second.', 'Fine.'])
    m = patchCommand(m, 'gate', path(1), 0, { say: 'Changed.' })
    expect(getList(m.events![0].do, path(1))![0].say).toBe('Changed.')
    // The other answer is untouched.
    expect(m.events![0].do[0].answers![0]).toEqual(map(d).events![0].do[0].answers![0])
    m = removeCommand(removeCommand(m, 'gate', path(1), 0), 'gate', path(1), 0)
    expect('do' in m.events![0].do[0].answers![1]).toBe(false) // an emptied answer leaves its list out
    d = withMap(d, () => m)
    expect(texts(d)).toEqual([])
  })
  it('nests: a question inside an answer, an If inside that', () => {
    let m = addCommand(map(withChoice(base())), 'gate', path(0), { choice: 'Sure?', answers: [{ text: 'Yes' }, { text: 'No' }] })
    const deeper: ListPath = [[0, 'answer0'], [2, 'answer1']]
    m = addCommand(m, 'gate', deeper, { if: [], then: [] })
    m = addCommand(m, 'gate', [...deeper, [0, 'then']], { say: 'Deep.' })
    expect(getList(m.events![0].do, [...deeper, [0, 'then']])![0].say).toBe('Deep.')
  })
  it('changing a talk event into another kind strips an endTurn from inside answers', () => {
    const d = base()
    const c: Command = { choice: 'Q?', answers: [{ text: 'a', do: [{ endTurn: true }, { say: 'x' }] }, { text: 'b' }] }
    let m = withChoice(d, c)
    m = withMap(m, (mm) => setTriggerKind(mm, 'gate', 'talk', { doc: m, map: mm }))
    expect(map(m).events![0].do[0].answers![0].do).toHaveLength(2)
    const stripped = setTriggerKind(map(m), 'gate', 'start', { doc: m, map: map(m) })
    expect(stripped.events![0].do[0].answers![0].do).toEqual([{ say: 'x' }])
  })
})

describe('renames and clean-up reach into answers', () => {
  it('renaming a unit changes the speaker, answers’ conditions are walked, nested commands too', () => {
    const d = withChoice(base(), { choice: 'Q?', speaker: 'gorm', answers: [{ text: 'a', do: [{ say: 'Hi', speaker: 'gorm' }] }, { text: 'b', if: [{ alive: 'gorm' }] }] })
    const m = renameUnitRefs(map(d), 'gorm', 'grim')
    const c = m.events![0].do[0]
    expect(c.speaker).toBe('grim')
    expect(c.answers![0].do![0].speaker).toBe('grim')
    expect(c.answers![1].if![0].alive).toBe('grim') // (a plain string where a filter is expected: the unit)
  })
  it('deleting the variable that a question stores its answer in removes that question, and one an answer sets', () => {
    let d = withChoice(base())
    d = withMap(d, (m) => ({ ...m, events: [{ id: 'gate', when: { battle: 'start' }, do: [{ choice: 'Q?', answers: [{ text: 'a', do: [{ setVariable: v1(d), to: 1 }, { say: 'kept' }] }, { text: 'b' }] }, choice(d)] }] }))
    const after = removeStoryItem(d, 'variables', v1(d))
    const cmds = map(after).events![0].do
    expect(cmds).toHaveLength(1) // the second question stored its answer in the deleted variable
    expect(cmds[0].answers![0].do).toEqual([{ say: 'kept' }])
  })
  it('deleting a switch that an answer is hidden by removes that answer; the check then says the question is short', () => {
    const d = withChoice(base())
    const after = removeStoryItem(d, 'switches', sw(d))
    const c = map(after).events![0].do[0]
    expect(c.answers!.map((a) => a.text)).toEqual(['Open it'])
    expect(c.answers![0].do).toEqual([{ say: 'It opens.' }]) // the answer's own switch command was dropped, its say stays
    expect(texts(after).join('\n')).toContain('it has 1 answer; a question needs two to four')
  })
  it('walks every command, answers included', () => {
    const d = withChoice(base())
    const seen: string[] = []
    forEachCommand(map(d).events![0].do, 'event “gate”', (c, w) => seen.push(`${commandKind(c)} @ ${w}`))
    expect(seen).toEqual([
      'choice @ event “gate”, command 1',
      'say @ event “gate”, command 1, answer 1, command 1',
      'setSwitch @ event “gate”, command 1, answer 1, command 2',
    ])
  })
})

describe('checks of a map’s questions', () => {
  it('a good question has nothing to complain about', () => {
    expect(texts(withChoice(base()))).toEqual([])
  })
  it('says what the loader refuses', () => {
    const d = base()
    expect(texts(withChoice(d, { choice: '  ', answers: [{ text: 'a' }, { text: '' }, { text: 'a' }] }))).toEqual([
      expect.stringContaining('the question is empty'),
      expect.stringContaining('answer 2: the words of the answer are empty'),
      expect.stringContaining('answer 3: it says the same as an earlier answer'),
    ])
    expect(texts(withChoice(d, { choice: 'Q?', answers: [{ text: 'only' }] }))[0]).toContain('it has 1 answer; a question needs two to four')
    expect(texts(withChoice(d, { choice: 'Q?', answers: ['a', 'b', 'c', 'd', 'e'].map((text) => ({ text })) }))[0]).toContain('it has 5 answers; a question needs two to four')
  })
  it('points at a missing switch, unit or variable inside an answer, with its place', () => {
    const d = withChoice(base(), { choice: 'Q?', store: 'nope', answers: [{ text: 'a', if: [{ switch: 'ghost' }], do: [{ say: 'x', speaker: 'nobody' }] }, { text: 'b' }] })
    const found = texts(d).join('\n')
    expect(found).toContain('“ghost” is not a switch of this game')
    expect(found).toContain('command 1, answer 1, command 1')
    expect(found).toContain('“nobody” is not a named unit of this map')
    expect(found).toContain('“nope” is not a variable of this game')
  })
  it('refuses two questions of one map that save in the same variable', () => {
    const d = withChoice(base(), { choice: 'A?', store: 'x', answers: [{ text: 'a' }, { text: 'b' }] })
    const two = withMap(withChoice(d, { choice: 'A?', store: v1(d), answers: [{ text: 'a' }, { text: 'b' }] }), (m) => ({
      ...m, events: [{ id: 'gate', when: { battle: 'start' }, do: [{ choice: 'A?', store: v1(d), answers: [{ text: 'a' }, { text: 'b' }] }, { choice: 'B?', store: v1(d), answers: [{ text: 'a' }, { text: 'b' }] }] }],
    }))
    expect(texts(two).filter((t) => t.includes('also saves its answer'))).toHaveLength(2)
    // A different variable, or none: fine.
    const ok = withMap(two, (m) => ({ ...m, events: [{ id: 'gate', when: { battle: 'start' }, do: [{ choice: 'A?', store: v1(d), answers: [{ text: 'a' }, { text: 'b' }] }, { choice: 'B?', store: v2(d), answers: [{ text: 'a' }, { text: 'b' }] }, { choice: 'C?', answers: [{ text: 'a' }, { text: 'b' }] }] }] }))
    expect(texts(ok)).toEqual([])
  })
})

describe('questions in a campaign scene', () => {
  const demo = (): ProjectDoc => loadDemo('demo-srpg')
  const scene = (d: ProjectDoc, which: 'before' | 'after', cmds: Command[]) => withScene(d, 'ford', which, { ...sceneAsMap(d, 'ford', which), events: [{ id: 'x', when: { battle: 'start' }, do: cmds }] })
  const problems = (d: ProjectDoc) => campaignChecks(d).map((c) => `${c.level}: ${c.text}`)

  it('are checked like a map’s, with the scene named', () => {
    const d = scene(demo(), 'before', [{ choice: '', answers: [{ text: 'a' }] }])
    const p = problems(d).join('\n')
    expect(p).toContain('the question is empty')
    expect(p).toContain('it has 1 answer; a question needs two to four')
    expect(p).toContain('Chapter “')
  })
  it('a "before" scene may go to another chapter but not its own; "after" scenes may too', () => {
    const other = demo().campaign!.chapters[1].id
    expect(problems(scene(demo(), 'before', [{ choice: 'Tutorial?', answers: [{ text: 'Yes', do: [{ goto: other }] }, { text: 'No' }] }]))).toEqual([])
    expect(problems(scene(demo(), 'before', [{ goto: 'ford' }])).join('\n')).toContain('can only go to another chapter')
    expect(problems(scene(demo(), 'after', [{ choice: 'Where?', answers: [{ text: 'A', do: [{ goto: other }] }, { text: 'B' }] }]))).toEqual([])
  })
  it('an unknown chapter in an answer is reported; the flow shows a jump from a before scene', () => {
    const bad = scene(demo(), 'after', [{ choice: 'Where?', answers: [{ text: 'A', do: [{ goto: 'nowhere' }] }, { text: 'B' }] }])
    expect(problems(bad).join('\n')).toContain('“nowhere” is not a chapter of this campaign')
    const d = demo()
    const last = d.campaign!.chapters[1].id // the last chapter: its only way on is the jump
    const jump = withScene(d, last, 'before', { ...sceneAsMap(d, last, 'before'), events: [{ id: 'x', when: { battle: 'start' }, do: [{ choice: 'Q?', answers: [{ text: 'A', do: [{ goto: 'ford' }] }, { text: 'B' }] }] }] })
    expect(flowRows(jump.campaign!)[1].leadsTo.some((t) => t.id === 'ford' && t.kind === 'goto')).toBe(true)
  })
  it('refuses two questions of one scene that save in the same variable, but not of two scenes', () => {
    const d = addStoryItem(demo(), 'variables', 'Pick')
    const v = d.game!.variables!.at(-1)!.id
    const q: Command = { choice: 'Q?', store: v, answers: [{ text: 'a' }, { text: 'b' }] }
    expect(problems(scene(d, 'before', [q, { ...q, choice: 'R?' }])).join('\n')).toContain('also saves its answer')
    expect(problems(scene(scene(d, 'before', [q]), 'after', [q]))).toEqual([])
  })
})
