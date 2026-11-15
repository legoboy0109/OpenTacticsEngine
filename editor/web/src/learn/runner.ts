// The lesson runner: pure functions that move a lesson's progress along (docs/TUTORIAL.md "How a lesson works").
// Steps are done in order; only the current step is tested; undoing later does not untick anything.
import type { SignalName } from './signals'
import type { LessonProgress } from './state'
import type { Lesson, ScreenName, Step } from './types'
import type { ProjectDoc } from '../model/types'

export const currentStep = (lesson: Lesson, p: LessonProgress): Step | null => lesson.steps[p.step] ?? null
export const isAtEnd = (lesson: Lesson, p: LessonProgress): boolean => p.step >= lesson.steps.length

export interface Advance {
  progress: LessonProgress
  /** Index of a step whose test is true but must hold for its `settleMs` first (the caller sets a timer), else null. */
  waiting: number | null
}

/** Ticks every step from the current one on whose test is already true. A step with `settleMs` waits once:
 *  pass its index as `settled` when its timer ran out and its test is still true. */
export function advance(lesson: Lesson, p: LessonProgress, doc: ProjectDoc, screen: ScreenName, settled = -1): Advance {
  const signals = new Set(p.signals) as ReadonlySet<SignalName>
  let progress = p
  const ok = settled
  while (progress.step < lesson.steps.length) {
    const step = lesson.steps[progress.step]
    if (!step.done(doc, screen, signals)) break
    if (step.settleMs && ok !== progress.step) return { progress, waiting: progress.step }
    progress = { ...progress, step: progress.step + 1 }
  }
  return { progress, waiting: null }
}

/** The maker gave up on the current step. */
export function skip(lesson: Lesson, p: LessonProgress): LessonProgress {
  const step = currentStep(lesson, p)
  return step ? { ...p, step: p.step + 1, skipped: p.skipped.includes(step.id) ? p.skipped : [...p.skipped, step.id] } : p
}

/** Records an editor signal (once). */
export const withSignal = (p: LessonProgress, name: SignalName): LessonProgress => (p.signals.includes(name) ? p : { ...p, signals: [...p.signals, name] })

export type StepMark = 'done' | 'skipped' | 'current' | 'todo'
/** What the panel shows for every step. */
export function marks(lesson: Lesson, p: LessonProgress): StepMark[] {
  return lesson.steps.map((s, i) => (i < p.step ? (p.skipped.includes(s.id) ? 'skipped' : 'done') : i === p.step ? 'current' : 'todo'))
}
