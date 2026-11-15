// The lesson runner (order, auto-tick, skip, settle) and the remembered state (parse, edits, practice names).
import { describe, expect, it } from 'vitest'
import { advance, currentStep, isAtEnd, marks, skip, withSignal } from '../../src/learn/runner'
import {
  emptyState, isFirstRun, lessonStatus, parseState, practiceName, runningLessonId, withLesson, withOfferAnswered, withResumed, withStarted, withStopped,
  type LessonProgress,
} from '../../src/learn/state'
import type { Lesson } from '../../src/learn/types'
import type { ProjectDoc } from '../../src/model/types'

// A tiny lesson over a fake document: each step looks at one number.
type Doc = { a: number; b: number; c: number }
const asDoc = (d: Doc) => d as unknown as ProjectDoc
const lesson: Lesson = {
  id: 'toy', title: 'Toy', description: 'd', minutes: 1, template: 'blank', playable: false, summary: [],
  steps: [
    { id: 'one', title: 'One', text: 't', done: (d) => (d as unknown as Doc).a >= 1 },
    { id: 'two', title: 'Two', text: 't', done: (d) => (d as unknown as Doc).b >= 1, settleMs: 100 },
    { id: 'three', title: 'Three', text: 't', done: (_d, _s, sig) => sig.has('played') },
    { id: 'four', title: 'Four', text: 't', done: (d, screen) => (d as unknown as Doc).c >= 1 && screen === 'units' },
  ],
}
const start: LessonProgress = { state: 'running', step: 0, skipped: [], signals: [], game: 'g' }

describe('advance', () => {
  it('stops at the first step that is not done', () => {
    const r = advance(lesson, start, asDoc({ a: 0, b: 1, c: 0 }), 'game')
    expect(r.progress.step).toBe(0)
    expect(r.waiting).toBeNull()
  })
  it('only the current step is checked: a later step that is true waits its turn', () => {
    expect(advance(lesson, start, asDoc({ a: 0, b: 5, c: 5 }), 'units').progress.step).toBe(0)
  })
  it('ticks a step that is already true when it is reached, and moves on (several at once)', () => {
    const p = { ...start, signals: ['played'] }
    const r = advance(lesson, p, asDoc({ a: 1, b: 1, c: 0 }), 'game', 1)
    expect(r.progress.step).toBe(3) // one, two (settled), three (signal); four is not done
  })
  it('a step with settleMs asks the caller to wait, once', () => {
    const d = asDoc({ a: 1, b: 1, c: 0 })
    const r = advance(lesson, start, d, 'game')
    expect(r.progress.step).toBe(1)
    expect(r.waiting).toBe(1)
    expect(advance(lesson, r.progress, d, 'game', 1).progress.step).toBe(2) // two ticks; three needs the "played" signal
  })
  it('a settled step whose work was undone meanwhile does not tick', () => {
    const r = advance(lesson, { ...start, step: 1 }, asDoc({ a: 1, b: 0, c: 0 }), 'game', 1)
    expect(r.progress.step).toBe(1)
    expect(r.waiting).toBeNull()
  })
  it('does not untick: undoing work after a step was ticked leaves the progress alone', () => {
    const p = advance(lesson, start, asDoc({ a: 1, b: 0, c: 0 }), 'game').progress
    expect(p.step).toBe(1)
    const again = advance(lesson, p, asDoc({ a: 0, b: 0, c: 0 }), 'game')
    expect(again.progress).toBe(p)
  })
  it('the screen is an input of the test', () => {
    const p = { ...start, step: 3 }
    expect(advance(lesson, p, asDoc({ a: 0, b: 0, c: 1 }), 'game').progress.step).toBe(3)
    expect(advance(lesson, p, asDoc({ a: 0, b: 0, c: 1 }), 'units').progress.step).toBe(4)
  })
  it('returns the same object when nothing changed', () => {
    expect(advance(lesson, start, asDoc({ a: 0, b: 0, c: 0 }), 'game').progress).toBe(start)
  })
  it('at the end there is nothing to do', () => {
    const end = { ...start, step: 4 }
    expect(advance(lesson, end, asDoc({ a: 0, b: 0, c: 0 }), 'game').progress).toBe(end)
    expect(isAtEnd(lesson, end)).toBe(true)
    expect(currentStep(lesson, end)).toBeNull()
  })
})

describe('skip and marks', () => {
  it('skip remembers the step and moves on; skipping at the end does nothing', () => {
    const p = skip(lesson, start)
    expect(p.step).toBe(1)
    expect(p.skipped).toEqual(['one'])
    expect(marks(lesson, p)).toEqual(['skipped', 'current', 'todo', 'todo'])
    const end = { ...start, step: 4 }
    expect(skip(lesson, end)).toBe(end)
  })
  it('a finished lesson with skipped steps shows them as skipped, the rest as done', () => {
    const p = { ...start, step: 4, skipped: ['two'] }
    expect(marks(lesson, p)).toEqual(['done', 'skipped', 'done', 'done'])
    expect(lessonStatus(lesson, p)).toEqual({ kind: 'finished' })
  })
  it('signals are recorded once', () => {
    const p = withSignal(start, 'played')
    expect(p.signals).toEqual(['played'])
    expect(withSignal(p, 'played')).toBe(p)
  })
})

describe('state', () => {
  it('parseState: nothing or rubbish = empty state; unknown keys survive', () => {
    expect(parseState(null)).toEqual(emptyState())
    expect(parseState([1])).toEqual(emptyState())
    expect(parseState('x')).toEqual(emptyState())
    expect(parseState({ future: { a: 1 } }).future).toEqual({ a: 1 })
  })
  it('parseState repairs what it can and drops what it cannot', () => {
    const s = parseState({
      offerAnswered: 'yes',
      lessons: {
        ok: { state: 'running', step: 3, skipped: ['a', 5], signals: ['played'], game: 'g' },
        noGame: { state: 'running', step: 1 },
        weird: { state: 'flying', step: -2, game: 'h' },
        notObject: 4,
      },
    })
    expect(s.offerAnswered).toBe(false)
    expect(Object.keys(s.lessons)).toEqual(['ok', 'weird'])
    expect(s.lessons.ok).toEqual({ state: 'running', step: 3, skipped: ['a'], signals: ['played'], game: 'g' })
    expect(s.lessons.weird).toEqual({ state: 'stopped', step: 0, skipped: [], signals: [], game: 'h' })
  })
  it('round trip through JSON', () => {
    const s = withStarted(emptyState(), 'toy', 'practice')
    expect(parseState(JSON.parse(JSON.stringify(s)))).toEqual(s)
  })
  it('first run = nothing saved at all', () => {
    expect(isFirstRun(emptyState())).toBe(true)
    expect(isFirstRun(withOfferAnswered(emptyState()))).toBe(false)
    expect(isFirstRun(withStarted(emptyState(), 'toy', 'g'))).toBe(false)
  })
  it('starting a lesson stops the running one but keeps its progress; the offer counts as answered', () => {
    let s = withStarted(emptyState(), 'toy', 'g1')
    s = withLesson(s, 'toy', { step: 2, skipped: ['one'] })
    s = withStarted(s, 'other', 'g2')
    expect(s.offerAnswered).toBe(true)
    expect(runningLessonId(s)).toBe('other')
    expect(s.lessons.toy).toEqual({ state: 'stopped', step: 2, skipped: ['one'], signals: [], game: 'g1' })
    s = withResumed(s, 'toy')
    expect(runningLessonId(s)).toBe('toy')
    expect(s.lessons.other.state).toBe('stopped')
    expect(withStopped(s).lessons.toy.state).toBe('stopped')
    expect(runningLessonId(withStopped(s))).toBeNull()
  })
  it('starting again replaces the progress', () => {
    let s = withStarted(emptyState(), 'toy', 'g1')
    s = withLesson(s, 'toy', { step: 3, skipped: ['one'], signals: ['played'] })
    s = withStarted(s, 'toy', 'g2')
    expect(s.lessons.toy).toEqual({ state: 'running', step: 0, skipped: [], signals: [], game: 'g2' })
  })
  it('lessonStatus', () => {
    expect(lessonStatus(lesson, undefined)).toEqual({ kind: 'new' })
    expect(lessonStatus(lesson, { ...start, step: 2, state: 'stopped' })).toEqual({ kind: 'progress', step: 3, total: 4 })
    expect(lessonStatus(lesson, { ...start, state: 'finished' })).toEqual({ kind: 'finished' })
  })
})

describe('practiceName', () => {
  it('names the game after the lesson and numbers it while the folder name is taken', () => {
    expect(practiceName('Structures', [])).toBe('Practice: Structures')
    expect(practiceName('Structures', ['practice-structures'])).toBe('Practice: Structures 2')
    expect(practiceName('Structures', ['practice-structures', 'practice-structures-2'])).toBe('Practice: Structures 3')
    expect(practiceName('Structures', ['practice-structures-2'])).toBe('Practice: Structures')
  })
})
