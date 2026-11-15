// Shared by the unit tests of the topic lessons (structures, economy, art, hexagons): the practice game as the editor makes it, and the
// checks every lesson must pass (well formed, nothing true at the start, no forbidden words).
import { expect, it } from 'vitest'
import { LESSONS } from '../../src/learn/lessons'
import type { Lesson } from '../../src/learn/types'
import type { SignalName } from '../../src/learn/signals'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplateById } from './fixtures'

/** The practice game as the editor makes it: the lesson's template, named "Practice: <lesson title>". */
export function fresh(lesson: Lesson): ProjectDoc {
  const d = structuredClone(loadTemplateById(lesson.template))
  d.game!.name = `Practice: ${lesson.title}`
  return d
}

export const NONE: ReadonlySet<SignalName> = new Set()

/** Runs a step's done test: `doc` on `screen` with the given signals. */
export const doneOf = (lesson: Lesson) => (id: string, doc: ProjectDoc, signals: SignalName[] = [], screen: Parameters<Lesson['steps'][number]['done']>[1] = 'game') =>
  lesson.steps.find((s) => s.id === id)!.done(doc, screen, new Set(signals))

/** The checks every lesson must pass: registered, 5 to 10 steps with unique ids, plain words, nothing true at the start. */
export function lessonBasics(lesson: Lesson, minSteps = 5): void {
  it('is registered, has unique step ids and a template that exists', () => {
    expect(LESSONS).toContain(lesson)
    expect(new Set(lesson.steps.map((s) => s.id)).size).toBe(lesson.steps.length)
    expect(lesson.steps.length).toBeGreaterThanOrEqual(minSteps)
    expect(lesson.steps.length).toBeLessThanOrEqual(10)
    expect(() => loadTemplateById(lesson.template)).not.toThrow()
    for (const s of lesson.steps) { expect(s.title.length).toBeGreaterThan(0); expect(s.text.length).toBeGreaterThan(0) }
    expect(lesson.summary.length).toBeGreaterThan(0)
    expect(lesson.description.length).toBeGreaterThan(0)
  })
  it('uses no programmer words in the texts, and never calls the style a "look"', () => {
    const all = [lesson.title, lesson.description, ...lesson.summary, ...lesson.steps.flatMap((s) => [s.title, s.text])].join(' ')
    expect(all).not.toMatch(/\b(tile|tiles|json|id|expression|formula|lua|script|look|looks)\b/i)
  })
  it('has no step that is already true at the start (not even with every signal missing)', () => {
    const d = fresh(lesson)
    for (const s of lesson.steps) expect(s.done(d, 'game', NONE), s.id).toBe(false)
  })
  it('ends on Play, which follows the "played" signal and nothing else', () => {
    const d = fresh(lesson)
    const play = lesson.steps.at(-1)!
    expect(play.id).toBe('play')
    expect(play.done(d, 'game', new Set<SignalName>(['saved', 'problemsChecked', 'pictureChooser']))).toBe(false)
    expect(play.done(d, 'game', new Set<SignalName>(['played']))).toBe(true)
  })
}
