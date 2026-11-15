// Every done test of the lesson "Structures" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { advance } from '../../src/learn/runner'
import { structures } from '../../src/learn/lessons/structures'
import type { LessonProgress } from '../../src/learn/state'
import type { ProjectDoc } from '../../src/model/types'
import { doneOf, fresh, lessonBasics } from './learnTopicKit'

const f = () => fresh(structures)
const done = doneOf(structures)
const step = (id: string) => structures.steps.find((s) => s.id === id)!
const addMine = (d: ProjectDoc) => d.structures!.push({ id: 'mine', name: 'Mine', color: '#aa8844', glyph: 'M' })
const mine = (d: ProjectDoc) => d.structures!.find((s) => s.id === 'mine')!

describe('lesson data', () => {
  lessonBasics(structures, 5)
  it('starts from economy-tactics as the steps assume', () => {
    const d = f()
    expect(structures.template).toBe('economy-tactics')
    expect(d.structures!.map((s) => s.id)).toEqual(['city', 'factory', 'hq'])
    expect(d.game!.maps[0].id).toBe('crossroads')
    expect(d.game!.rules!.capture!.enabled).toBe(true)
    expect(d.game!.rules!.economy!.enabled).toBe(true)
    expect(d.game!.resources!.map((r) => r.id)).toEqual(['funds'])
    expect(d.maps.crossroads!.sides!.map((s) => s.id)).toEqual(['blue', 'red'])
  })
})

describe('steps: new-kind, capture, income', () => {
  it('a new kind ticks new-kind; changing City does not', () => {
    const d = f()
    d.structures![0].name = 'Town'
    expect(done('new-kind', d)).toBe(false)
    addMine(d)
    expect(done('new-kind', d)).toBe(true)
  })
  it('capture needs a new kind that can be captured (the City already can be)', () => {
    const d = f()
    expect(done('capture', d)).toBe(false)
    addMine(d)
    expect(done('capture', d)).toBe(false)
    mine(d).capturePoints = 0
    expect(done('capture', d)).toBe(false)
    mine(d).capturePoints = 20
    expect(done('capture', d)).toBe(true)
  })
  it('income needs funds on the new kind, not on the City', () => {
    const d = f()
    d.structures![0].income = { funds: 5000 }
    expect(done('income', d)).toBe(false)
    addMine(d)
    expect(done('income', d)).toBe(false)
    mine(d).income = { funds: 0 }
    expect(done('income', d)).toBe(false)
    mine(d).income = { funds: 500 }
    expect(done('income', d)).toBe(true)
  })
  it('income settles before ticking, so typing is not cut off', () => {
    expect(step('income').settleMs).toBeGreaterThan(0)
  })
  it('"Take me there" opens the new kind, not the City', () => {
    const d = f()
    expect(step('capture').where!.item!(d)).toBeUndefined()
    addMine(d)
    expect(step('capture').where!.item!(d)).toBe('mine')
    expect(step('income').where!.item!(d)).toBe('mine')
  })
})

describe('steps: place, give', () => {
  it('only the new kind on the first map counts, whoever owns it', () => {
    const d = f()
    addMine(d)
    d.maps.crossroads!.structures!.push({ type: 'city', x: 0, y: 7 })
    expect(done('place', d)).toBe(false)
    d.maps.crossroads!.structures!.push({ type: 'mine', x: 0, y: 6 })
    expect(done('place', d)).toBe(true)
  })
  it('give needs the new kind owned by Blue: a neutral one or another kind of Blue does not count', () => {
    const d = f()
    addMine(d)
    const m = d.maps.crossroads!
    m.structures!.push({ type: 'mine', x: 0, y: 6 })
    expect(done('give', d)).toBe(false)
    m.structures!.push({ type: 'city', x: 0, y: 7, side: 'blue' })
    expect(done('give', d)).toBe(false)
    m.structures!.push({ type: 'mine', x: 1, y: 6, side: 'red' })
    expect(done('give', d)).toBe(false)
    m.structures!.push({ type: 'mine', x: 2, y: 6, side: 'blue' })
    expect(done('give', d)).toBe(true)
  })
  it('only the first map counts', () => {
    const d = f()
    addMine(d)
    d.game!.maps.push({ id: 'second', name: 'Second', file: 'maps/second.json' })
    d.maps.second = { ...structuredClone(d.maps.crossroads!), structures: [{ type: 'mine', x: 0, y: 0, side: 'blue' }] }
    expect(done('place', d)).toBe(false)
    expect(done('give', d)).toBe(false)
  })
})

describe('step: play', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', f(), ['saved', 'problemsChecked'])).toBe(false)
    expect(done('play', f(), ['played'])).toBe(true)
  })
})

describe('the lesson through the runner', () => {
  const start: LessonProgress = { state: 'running', step: 0, skipped: [], signals: [], game: 'practice' }
  it('ticks in order, never ahead', () => {
    const d = f()
    addMine(d)
    mine(d).capturePoints = 20
    mine(d).income = { funds: 500 }
    // The kind is made, captured and paying: the first three steps tick (income after its pause), place is next.
    const waiting = advance(structures, start, d, 'game')
    expect(waiting).toMatchObject({ progress: { step: 2 }, waiting: 2 }) // income is typed: it waits for its pause
    const first = advance(structures, start, d, 'game', 2).progress
    expect(first.step).toBe(3)
    d.maps.crossroads!.structures!.push({ type: 'mine', x: 0, y: 6, side: 'blue' })
    const second = advance(structures, first, d, 'game').progress
    expect(second.step).toBe(5) // placed and owned by Blue: both steps tick, Play waits
  })
})
