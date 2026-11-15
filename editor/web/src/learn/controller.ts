// The Learn controller: one object that holds the remembered state, runs the current lesson against the open game,
// makes practice games and talks to the helper. No React and no DOM in here (the web wiring is in LearnHost.tsx), so
// the whole flow is unit tested with a fake api (tests/unit/learnController.test.ts).
import type { ProjectInfo, Result } from '../api/client'
import { projectSlug } from '../model/ids'
import type { ProjectDoc, Section } from '../model/types'
import { LESSONS } from './lessons'
import { advance, currentStep, isAtEnd, skip, withSignal } from './runner'
import type { SignalBus, SignalName } from './signals'
import {
  emptyState, parseState, practiceName, runningLessonId, withLesson, withOfferAnswered, withResumed, withStarted, withStopped,
  type LearnState,
} from './state'
import { T } from './text'
import type { Lesson } from './types'

export interface LearnDeps {
  lessons?: Lesson[]
  api: {
    getState(): Promise<Result<unknown>>
    putState(state: LearnState): Promise<Result<unknown>>
    listProjects(): Promise<Result<ProjectInfo[]>>
    createProject(body: { id: string; name: string; template: string }): Promise<Result<{ id: string }>>
  }
  bus: SignalBus
  /** Opens a screen of a game (the router's navigate). */
  goTo(game: string, section: Section, item?: string): void
}

/** What the open game tells the controller (ProjectShell, through useLearnContext). */
export interface ProjectContext {
  projectId: string
  section: Section
  /** The loaded document; null while the game loads or when it could not be opened. */
  doc: ProjectDoc | null
  /** The game could not be opened (for example it was deleted). */
  failed: boolean
  /** Presses Play. */
  play: () => void
}

export interface LearnSnapshot {
  /** The helper answered GET /api/learn (well or badly). */
  loaded: boolean
  /** The helper's state was read: only then may the first-run offer be shown. */
  available: boolean
  state: LearnState
  listOpen: boolean
  busy: boolean
  error: string | null
  /** Lesson whose practice game was found missing, or null. */
  gone: string | null
  /** The game that is open (null on the game list). */
  projectId: string | null
  section: Section | null
  canPlay: boolean
}

export class LearnController {
  private snap: LearnSnapshot = {
    loaded: false, available: false, state: emptyState(), listOpen: false, busy: false, error: null, gone: null,
    projectId: null, section: null, canPlay: false,
  }
  private listeners = new Set<() => void>()
  private ctx: ProjectContext | null = null
  /** The last loaded document of the practice game, so "Take me there" can pick an item after the game was closed. */
  private lastDoc: { game: string; doc: ProjectDoc } | null = null
  private settle: { step: number; timer: ReturnType<typeof setTimeout> } | null = null
  private dirty = false
  private flushing: Promise<void> | null = null
  private readonly lessons: Lesson[]

  constructor(private deps: LearnDeps) {
    this.lessons = deps.lessons ?? LESSONS
    deps.bus.subscribe((name) => this.onSignal(name))
  }

  // --- subscription (useSyncExternalStore) ---
  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => { this.listeners.delete(l) } }
  getSnapshot = (): LearnSnapshot => this.snap
  private patch(p: Partial<LearnSnapshot>): void { this.snap = { ...this.snap, ...p }; this.listeners.forEach((l) => l()) }

  lesson(id: string): Lesson | undefined { return this.lessons.find((l) => l.id === id) }
  /** The lesson whose panel is open, with its progress. */
  running(): { lesson: Lesson; id: string } | null {
    const id = runningLessonId(this.snap.state)
    const lesson = id ? this.lesson(id) : undefined
    return id && lesson ? { lesson, id } : null
  }

  // --- the remembered state ---
  async load(): Promise<void> {
    const res = await this.deps.api.getState()
    if (res.ok) this.patch({ loaded: true, available: true, state: parseState(res.value) })
    else { console.warn('learn: could not read the remembered state:', res.error); this.patch({ loaded: true, available: false }) }
    this.evaluate()
  }

  private commit(state: LearnState): void {
    this.patch({ state })
    this.dirty = true
    if (!this.flushing) this.flushing = this.flush()
  }
  private async flush(): Promise<void> {
    try {
      while (this.dirty) {
        this.dirty = false
        const res = await this.deps.api.putState(this.snap.state)
        if (!res.ok) console.warn('learn: could not save the remembered state:', res.error)
      }
    } finally { this.flushing = null }
  }
  /** Resolves when every change has been sent to the helper (tests wait on this). */
  async idle(): Promise<void> { while (this.flushing) await this.flushing }

  // --- what the open game says ---
  setContext(ctx: ProjectContext): void {
    const was = this.ctx
    this.ctx = ctx
    const run = this.running()
    if (ctx.doc && run && ctx.projectId === this.snap.state.lessons[run.id].game) this.lastDoc = { game: ctx.projectId, doc: ctx.doc }
    if (!was || was.projectId !== ctx.projectId || was.section !== ctx.section || (was.doc !== null) !== (ctx.doc !== null)) {
      this.patch({ projectId: ctx.projectId, section: ctx.section, canPlay: ctx.doc !== null })
    }
    if (ctx.failed && run && ctx.projectId === this.snap.state.lessons[run.id].game && this.snap.gone !== run.id) this.patch({ gone: run.id })
    this.evaluate()
  }
  clearContext(projectId: string): void {
    if (this.ctx?.projectId !== projectId) return
    this.ctx = null
    this.patch({ projectId: null, section: null, canPlay: false })
  }

  private onSignal(name: SignalName): void {
    const run = this.running()
    if (!run) return
    const p = this.snap.state.lessons[run.id]
    const next = withSignal(p, name)
    if (next !== p) this.commit(withLesson(this.snap.state, run.id, next))
    this.evaluate()
  }

  /** Ticks what is done. Runs after every change of the document, the screen, the signals or the lesson. */
  evaluate(): void {
    const run = this.running()
    const p = run ? this.snap.state.lessons[run.id] : null
    const ctx = this.ctx
    if (!run || !p || !ctx || !ctx.doc || ctx.projectId !== p.game) { this.clearSettle(); return }
    const res = advance(run.lesson, p, ctx.doc, ctx.section)
    if (res.progress !== p) this.commit(withLesson(this.snap.state, run.id, res.progress))
    if (res.waiting === null) { this.clearSettle(); return }
    if (this.settle?.step === res.waiting) return
    this.clearSettle()
    const ms = run.lesson.steps[res.waiting].settleMs ?? 0
    const lessonId = run.id
    this.settle = {
      step: res.waiting,
      timer: setTimeout(() => {
        this.settle = null
        const now = this.snap.state.lessons[lessonId]
        const c = this.ctx
        if (!now || now.state !== 'running' || !c?.doc || c.projectId !== now.game) return
        const done = advance(this.lesson(lessonId)!, now, c.doc, c.section, res.waiting!)
        if (done.progress !== now) this.commit(withLesson(this.snap.state, lessonId, done.progress))
        this.evaluate()
      }, ms),
    }
  }
  private clearSettle(): void { if (this.settle) { clearTimeout(this.settle.timer); this.settle = null } }

  // --- the Learn list and the first-run offer ---
  openList(): void { this.patch({ listOpen: true, error: null }); void this.checkPractice() }
  closeList(): void { this.patch({ listOpen: false, error: null }) }

  /** The first-run card: remembers the answer, and starts the lesson when `start` is true. */
  async answerOffer(lessonId: string, start: boolean): Promise<void> {
    if (start) { await this.start(lessonId); return }
    this.commit(withOfferAnswered(this.snap.state))
  }

  // --- lessons ---
  /** Makes a new practice game from the lesson's template and opens it at the first step. */
  async start(lessonId: string): Promise<void> {
    const lesson = this.lesson(lessonId)
    if (!lesson || this.snap.busy) return
    this.patch({ busy: true, error: null })
    try {
      const listed = await this.deps.api.listProjects()
      if (!listed.ok) { this.patch({ error: T.startFailed(listed.error) }); return }
      const taken = listed.value.map((p) => p.id)
      for (let attempt = 0; attempt < 5; attempt++) {
        const name = practiceName(lesson.title, taken)
        const id = projectSlug(name)
        const made = await this.deps.api.createProject({ id, name, template: lesson.template })
        if (made.ok) {
          this.commit(withStarted(this.snap.state, lessonId, made.value.id))
          this.patch({ gone: null, listOpen: false })
          this.deps.goTo(made.value.id, lesson.steps[0]?.where?.section ?? 'game', undefined)
          return
        }
        if (made.status !== 409) { this.patch({ error: T.startFailed(made.error) }); return }
        taken.push(id) // somebody took the name between the list and now: try the next number
      }
      this.patch({ error: T.startFailed('The names are all taken.') })
    } finally { this.patch({ busy: false }) }
  }

  /** Reopens a stopped lesson's practice game. When that game is gone, says so instead. */
  async resume(lessonId: string): Promise<void> {
    const p = this.snap.state.lessons[lessonId]
    if (!p || this.snap.busy) return
    this.patch({ busy: true, error: null })
    try {
      const listed = await this.deps.api.listProjects()
      if (listed.ok && !listed.value.some((g) => g.id === p.game)) { this.patch({ gone: lessonId }); return }
      this.commit(withResumed(this.snap.state, lessonId))
      this.patch({ gone: null, listOpen: false })
      this.takeMeThere()
    } finally { this.patch({ busy: false }) }
  }

  /** Looks whether the running lesson's practice game still exists (page load, game list, Learn list). */
  async checkPractice(): Promise<void> {
    const run = this.running()
    if (!run) { if (this.snap.gone) this.patch({ gone: null }); return }
    const listed = await this.deps.api.listProjects()
    if (!listed.ok) return
    const missing = !listed.value.some((g) => g.id === this.snap.state.lessons[run.id]?.game)
    if (missing && this.snap.gone !== run.id) this.patch({ gone: run.id })
    else if (!missing && this.snap.gone === run.id) this.patch({ gone: null })
  }

  stop(): void { this.clearSettle(); this.commit(withStopped(this.snap.state)); this.patch({ gone: null }) }

  skipStep(): void {
    const run = this.running()
    if (!run) return
    const next = skip(run.lesson, this.snap.state.lessons[run.id])
    this.commit(withLesson(this.snap.state, run.id, next))
    this.evaluate()
  }

  /** The end screen's Done: the lesson counts as finished and its panel closes. */
  finish(): void {
    const run = this.running()
    if (run) this.commit(withLesson(this.snap.state, run.id, { state: 'finished', step: run.lesson.steps.length }))
  }

  /** Opens the screen the current step happens on (in the practice game). */
  takeMeThere(): void {
    const run = this.running()
    if (!run) return
    const p = this.snap.state.lessons[run.id]
    const step = currentStep(run.lesson, p)
    const where = step?.where
    const doc = this.ctx?.projectId === p.game ? this.ctx.doc : this.lastDoc?.game === p.game ? this.lastDoc.doc : null
    this.deps.goTo(p.game, where?.section ?? 'game', where?.item && doc ? where.item(doc) : undefined)
  }

  /** Presses Play in the open game (the end screen's button). */
  play(): void { this.ctx?.play() }

  atEnd(): boolean {
    const run = this.running()
    return !!run && isAtEnd(run.lesson, this.snap.state.lessons[run.id])
  }
}
