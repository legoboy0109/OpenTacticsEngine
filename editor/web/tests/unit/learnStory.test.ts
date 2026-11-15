// Every done test of the lesson "Story events" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { advance } from '../../src/learn/runner'
import { LESSONS } from '../../src/learn/lessons'
import { story } from '../../src/learn/lessons/story'
import type { SignalName } from '../../src/learn/signals'
import type { LessonProgress } from '../../src/learn/state'
import type { StoryEvent } from '../../src/model/storyTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const fresh = (): ProjectDoc => { const d = structuredClone(loadTemplate('blank')); d.game!.name = 'Practice: Story events'; return d }
const NONE: ReadonlySet<SignalName> = new Set()
const step = (id: string) => story.steps.find((s) => s.id === id)!
const done = (id: string, doc: ProjectDoc, signals: SignalName[] = []) => step(id).done(doc, 'game', new Set(signals))

const opening = (say?: string): StoryEvent => ({ id: 'event_1', when: { battle: 'start' }, do: say === undefined ? [] : [{ say }] })
const reach = (r = { x: 0, y: 0, w: 1, h: 1 }, say?: string): StoryEvent => ({ id: 'event_2', when: { reach: r }, do: say === undefined ? [] : [{ say }] })
const withEvents = (...e: StoryEvent[]): ProjectDoc => { const d = fresh(); d.maps.start!.events = e; return d }

describe('lesson data', () => {
  it('is registered, with unique step ids and a template that exists', () => {
    expect(LESSONS).toContain(story)
    expect(new Set(story.steps.map((s) => s.id)).size).toBe(story.steps.length)
    expect(story.steps.length).toBeGreaterThanOrEqual(5)
    expect(story.steps.length).toBeLessThanOrEqual(10)
    expect(story.template).toBe('blank')
    expect(story.playable).toBe(true)
    for (const s of story.steps) { expect(s.title.length).toBeGreaterThan(0); expect(s.text.length).toBeGreaterThan(0) }
  })
  it('uses no programmer words in the texts, and never calls the style a "look"', () => {
    const all = [story.title, story.description, ...story.summary, ...story.steps.flatMap((s) => [s.title, s.text])].join(' ')
    expect(all).not.toMatch(/\b(tile|json|id|expression|formula|lua|script|look|looks)\b/i)
  })
  it('starts from the blank template as the steps assume: a map with two soldiers, no events, no objectives', () => {
    const d = fresh()
    expect(d.game!.maps.map((m) => m.id)).toEqual(['start'])
    expect(d.maps.start!.events).toBeUndefined()
    expect(d.maps.start!.objectives).toBeUndefined()
    expect(d.maps.start!.grid).toMatchObject({ width: 8, height: 6 })
  })
  it('has no step that is already true at the start', () => {
    for (const s of story.steps) expect(s.done(fresh(), 'game', NONE), s.id).toBe(false)
  })
})

describe('steps: opening-event, opening-say', () => {
  it('an event that waits for the battle to start ticks the first step; one that waits for a reach does not', () => {
    expect(done('opening-event', withEvents(reach()))).toBe(false)
    expect(done('opening-event', withEvents(opening()))).toBe(true)
  })
  it('the say step needs a line with text in the opening event, not a blank one, not another event', () => {
    expect(done('opening-say', withEvents(opening()))).toBe(false)
    expect(done('opening-say', withEvents(opening('   ')))).toBe(false)
    expect(done('opening-say', withEvents(opening(), reach(undefined, 'Hello')))).toBe(false)
    expect(done('opening-say', withEvents(opening('Hold the bridge!')))).toBe(true)
  })
  it('a command that is not "say" does not count', () => {
    const d = withEvents({ id: 'event_1', when: { battle: 'start' }, do: [{ endTurn: true }] })
    expect(done('opening-say', d)).toBe(false)
  })
  it('the typed steps settle first', () => {
    for (const id of ['opening-say', 'reach-say', 'objective-text']) expect(step(id).settleMs, id).toBeGreaterThan(0)
  })
})

describe('steps: second-event, reach-when, reach-area, reach-say', () => {
  it('second-event needs two events', () => {
    expect(done('second-event', withEvents(opening()))).toBe(false)
    expect(done('second-event', withEvents(opening(), reach()))).toBe(true)
  })
  it('reach-when needs an event whose When is a unit reaching an area', () => {
    expect(done('reach-when', withEvents(opening(), { id: 'event_2', when: { turn: 1 }, do: [] }))).toBe(false)
    expect(done('reach-when', withEvents(opening(), reach()))).toBe(true)
  })
  it('reach-area needs an area that is not the starting top-left square', () => {
    expect(done('reach-area', withEvents(opening(), reach()))).toBe(false)
    expect(done('reach-area', withEvents(opening(), reach({ x: 7, y: 0, w: 1, h: 6 })))).toBe(true)
    expect(done('reach-area', withEvents(opening(), reach({ x: 0, y: 0, w: 2, h: 1 })))).toBe(true)
  })
  it('reach-say needs text in the reach event, not in the opening one', () => {
    expect(done('reach-say', withEvents(opening('Hi'), reach({ x: 7, y: 0, w: 1, h: 6 })))).toBe(false)
    expect(done('reach-say', withEvents(opening(), reach({ x: 7, y: 0, w: 1, h: 6 }, 'You made it!')))).toBe(true)
  })
})

describe('steps: objective-text, objective-win', () => {
  const withObjectives = (o: object): ProjectDoc => { const d = fresh(); d.maps.start!.objectives = o as never; return d }
  it('the text step needs some text', () => {
    expect(done('objective-text', withObjectives({}))).toBe(false)
    expect(done('objective-text', withObjectives({ text: '  ' }))).toBe(false)
    expect(done('objective-text', withObjectives({ win: [{ survive: 5 }] }))).toBe(false)
    expect(done('objective-text', withObjectives({ text: 'Survive 5 rounds' }))).toBe(true)
  })
  it('the win step needs a way to win, not just a way to lose or a text', () => {
    expect(done('objective-win', withObjectives({ text: 'Survive' }))).toBe(false)
    expect(done('objective-win', withObjectives({ lose: [{ round: 5 }] }))).toBe(false)
    expect(done('objective-win', withObjectives({ win: [{ survive: 5 }] }))).toBe(true)
  })
})

describe('step: play (editor signal)', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', fresh(), ['saved', 'problemsChecked'])).toBe(false)
    expect(done('play', fresh(), ['played'])).toBe(true)
  })
})

describe('the lesson through the runner', () => {
  it('ticks in order, never ahead: a finished opening with nothing else stops at the second event', () => {
    const start: LessonProgress = { state: 'running', step: 0, skipped: [], signals: [], game: 'practice' }
    const d = withEvents(opening('Hold the bridge!'))
    expect(advance(story, start, d, 'game').waiting).toBe(1) // the line waits for its pause
    expect(advance(story, start, d, 'game', 1).progress.step).toBe(2)
  })
})
