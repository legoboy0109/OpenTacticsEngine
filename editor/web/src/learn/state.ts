// The remembered Learn state (kept by the helper: GET/PUT /api/learn, docs/EDITOR_API.md) and the pure edits of it.
// The helper stores any JSON object; this file owns its shape. Unknown keys survive a round trip.
import { projectSlug } from '../model/ids'
import type { Lesson } from './types'

export type LessonRunState = 'running' | 'stopped' | 'finished'

export interface LessonProgress {
  /** At most one lesson is 'running' (its panel is open); starting another one stops it. */
  state: LessonRunState
  /** Index of the current step; `steps.length` = every step is ticked or skipped (the end screen). */
  step: number
  /** Ids of the steps the maker skipped. */
  skipped: string[]
  /** Editor signals seen since the lesson started (SignalName). */
  signals: string[]
  /** Id of the practice game. */
  game: string
}

export interface LearnState {
  version: 1
  /** The first-run offer was answered (Start or Not now). */
  offerAnswered: boolean
  lessons: Record<string, LessonProgress>
  [unknown: string]: unknown
}

export const emptyState = (): LearnState => ({ version: 1, offerAnswered: false, lessons: {} })

const isObj = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v)
const strings = (v: unknown): string[] => (Array.isArray(v) ? v.filter((x): x is string => typeof x === 'string') : [])

/** Reads whatever the helper sent; anything that does not fit is dropped, never an error. */
export function parseState(raw: unknown): LearnState {
  if (!isObj(raw)) return emptyState()
  const lessons: Record<string, LessonProgress> = {}
  if (isObj(raw.lessons)) {
    for (const [id, p] of Object.entries(raw.lessons)) {
      if (!isObj(p) || typeof p.game !== 'string') continue
      const state: LessonRunState = p.state === 'running' || p.state === 'finished' ? p.state : 'stopped'
      const step = typeof p.step === 'number' && Number.isInteger(p.step) && p.step >= 0 ? p.step : 0
      lessons[id] = { state, step, skipped: strings(p.skipped), signals: strings(p.signals), game: p.game }
    }
  }
  return { ...raw, version: 1, offerAnswered: raw.offerAnswered === true, lessons }
}

/** True when nothing has ever been saved: the game list then offers the first lesson. */
export const isFirstRun = (s: LearnState): boolean => !s.offerAnswered && Object.keys(s.lessons).length === 0

export const runningLessonId = (s: LearnState): string | null => Object.keys(s.lessons).find((id) => s.lessons[id].state === 'running') ?? null

export type LessonStatus =
  | { kind: 'new' }
  | { kind: 'progress'; step: number; total: number }
  | { kind: 'finished' }

/** What the Learn list says about a lesson. `step` is 1-based for display. */
export function lessonStatus(lesson: Lesson, p: LessonProgress | undefined): LessonStatus {
  if (!p) return { kind: 'new' }
  if (p.state === 'finished' || p.step >= lesson.steps.length) return { kind: 'finished' }
  return { kind: 'progress', step: p.step + 1, total: lesson.steps.length }
}

/** Starts (or restarts) a lesson on a practice game: it runs, any other running lesson is stopped (its progress stays). */
export function withStarted(s: LearnState, lessonId: string, game: string): LearnState {
  const base = stopped(s)
  return { ...base, offerAnswered: true, lessons: { ...base.lessons, [lessonId]: { state: 'running', step: 0, skipped: [], signals: [], game } } }
}

/** Continues a stopped lesson where it was; any other running lesson is stopped. */
export function withResumed(s: LearnState, lessonId: string): LearnState {
  const base = stopped(s)
  const p = base.lessons[lessonId]
  if (!p) return s
  return { ...base, lessons: { ...base.lessons, [lessonId]: { ...p, state: 'running' } } }
}

export const withLesson = (s: LearnState, lessonId: string, patch: Partial<LessonProgress>): LearnState => {
  const p = s.lessons[lessonId]
  return p ? { ...s, lessons: { ...s.lessons, [lessonId]: { ...p, ...patch } } } : s
}

export const withStopped = (s: LearnState): LearnState => stopped(s)
function stopped(s: LearnState): LearnState {
  const lessons: Record<string, LessonProgress> = {}
  for (const [id, p] of Object.entries(s.lessons)) lessons[id] = p.state === 'running' ? { ...p, state: 'stopped' } : p
  return { ...s, lessons }
}

export const withOfferAnswered = (s: LearnState): LearnState => (s.offerAnswered ? s : { ...s, offerAnswered: true })

/** "Practice: Your first game", or "... 2", "... 3" while the folder name is taken. `taken` = existing game ids. */
export function practiceName(title: string, taken: Iterable<string>): string {
  const ids = new Set(taken)
  const base = `Practice: ${title}`
  for (let n = 1; ; n++) {
    const name = n === 1 ? base : `${base} ${n}`
    if (!ids.has(projectSlug(name))) return name
  }
}
