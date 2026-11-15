// The Learn controller with a fake helper: loading and saving the state, starting (practice game and its name), resuming,
// the practice game being gone, skipping, signals, the settle pause, finishing.
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { ProjectInfo, Result } from '../../src/api/client'
import { LearnController, type ProjectContext } from '../../src/learn/controller'
import { SignalBus } from '../../src/learn/signals'
import { emptyState, type LearnState } from '../../src/learn/state'
import type { Lesson } from '../../src/learn/types'
import type { ProjectDoc, Section } from '../../src/model/types'

type Doc = { a: number; b: number }
const asDoc = (d: Doc) => d as unknown as ProjectDoc
const toy: Lesson = {
  id: 'toy', title: 'Toy', description: 'd', minutes: 1, template: 'blank', playable: true, summary: ['made it'],
  steps: [
    { id: 'one', title: 'One', text: 't', where: { section: 'units', item: (d) => `item${(d as unknown as Doc).a}` }, done: (d) => (d as unknown as Doc).a >= 1 },
    { id: 'two', title: 'Two', text: 't', where: { section: 'maps' }, done: (d) => (d as unknown as Doc).b >= 1, settleMs: 100 },
    { id: 'three', title: 'Three', text: 't', done: (_d, _s, sig) => sig.has('played') },
  ],
}
const other: Lesson = { ...toy, id: 'other', title: 'Other' }

const ok = <T,>(value: T): Result<T> => ({ ok: true, value })
const fail = (status: number, error: string): Result<never> => ({ ok: false, status, error })

class Fake {
  stored: unknown = {}
  getFails = false
  puts: LearnState[] = []
  games: string[] = []
  createStatus: number[] = [] // statuses to answer with before succeeding (409 = taken)
  created: Array<{ id: string; name: string; template: string }> = []
  api = {
    getState: async () => (this.getFails ? fail(0, 'offline') : ok(this.stored)),
    putState: async (s: LearnState) => { this.puts.push(structuredClone(s)); this.stored = structuredClone(s); return ok(s) },
    listProjects: async () => ok(this.games.map((id) => ({ id, name: id, description: '', author: '', maps: 1, ok: true, error: '' }) as ProjectInfo)),
    createProject: async (b: { id: string; name: string; template: string }) => {
      const st = this.createStatus.shift()
      if (st) return fail(st, 'taken')
      this.created.push(b); this.games.push(b.id)
      return ok({ id: b.id })
    },
  }
}

let fake: Fake
let bus: SignalBus
let went: Array<[string, Section, string | undefined]>
let c: LearnController
let played = 0

const ctx = (doc: Doc | null, over: Partial<ProjectContext> = {}): ProjectContext =>
  ({ projectId: 'practice-toy', section: 'game', doc: doc ? asDoc(doc) : null, failed: false, play: () => { played++ }, ...over })
const make = (lessons = [toy, other]) => {
  c = new LearnController({ lessons, api: fake.api, bus, goTo: (g, s, i) => went.push([g, s, i]) })
}
const step = () => c.getSnapshot().state.lessons.toy?.step

beforeEach(() => { fake = new Fake(); bus = new SignalBus(); went = []; played = 0; make() })
afterEach(() => { vi.useRealTimers() })

describe('loading and saving', () => {
  it('reads the helper state; a failing helper means no offer and no crash', async () => {
    fake.stored = { version: 1, offerAnswered: true, lessons: {} }
    await c.load()
    expect(c.getSnapshot()).toMatchObject({ loaded: true, available: true })
    expect(c.getSnapshot().state.offerAnswered).toBe(true)
    fake.getFails = true
    make()
    await c.load()
    expect(c.getSnapshot()).toMatchObject({ loaded: true, available: false })
    expect(c.getSnapshot().state).toEqual(emptyState())
  })
  it('every change is sent to the helper, and the last one wins when they come quickly', async () => {
    await c.load()
    await c.answerOffer('toy', false)
    await c.idle()
    expect(fake.puts.at(-1)!.offerAnswered).toBe(true)
  })
})

describe('first-run offer', () => {
  it('"Not now" is remembered and starts nothing', async () => {
    await c.load()
    await c.answerOffer('toy', false)
    await c.idle()
    expect(fake.stored).toMatchObject({ offerAnswered: true, lessons: {} })
    expect(fake.created).toEqual([])
  })
  it('"Start" makes the practice game, opens it, and remembers both', async () => {
    await c.load()
    await c.answerOffer('toy', true)
    await c.idle()
    expect(fake.created).toEqual([{ id: 'practice-toy', name: 'Practice: Toy', template: 'blank' }])
    expect(went).toEqual([['practice-toy', 'units', undefined]])
    expect(fake.stored).toMatchObject({ offerAnswered: true, lessons: { toy: { state: 'running', step: 0, game: 'practice-toy' } } })
  })
})

describe('starting', () => {
  it('numbers the name while the folder is taken, also when somebody takes it meanwhile (409)', async () => {
    await c.load()
    fake.games = ['practice-toy']
    fake.createStatus = [409]
    await c.start('toy')
    expect(fake.created.map((g) => g.name)).toEqual(['Practice: Toy 3'])
    expect(c.getSnapshot().state.lessons.toy.game).toBe('practice-toy-3')
  })
  it('says so when the practice game cannot be made, and changes nothing', async () => {
    await c.load()
    fake.createStatus = [500]
    await c.start('toy')
    expect(c.getSnapshot().error).toMatch(/could not be made/)
    expect(c.getSnapshot().state.lessons.toy).toBeUndefined()
    expect(c.getSnapshot().busy).toBe(false)
  })
  it('starting another lesson stops the first but keeps its progress', async () => {
    await c.load()
    await c.start('toy')
    await c.start('other')
    expect(c.getSnapshot().state.lessons.toy.state).toBe('stopped')
    expect(c.running()!.id).toBe('other')
  })
})

describe('running a lesson', () => {
  beforeEach(async () => { await c.load(); await c.start('toy') })

  it('ticks in order from the open practice game', () => {
    c.setContext(ctx({ a: 0, b: 0 }))
    expect(step()).toBe(0)
    c.setContext(ctx({ a: 1, b: 0 }))
    expect(step()).toBe(1)
  })
  it('does not test a game that is not the practice game', () => {
    c.setContext(ctx({ a: 5, b: 5 }, { projectId: 'some-other-game' }))
    expect(step()).toBe(0)
    c.clearContext('some-other-game')
    expect(step()).toBe(0)
  })
  it('a step that is true when reached ticks at once; a step with a pause waits for it', () => {
    vi.useFakeTimers()
    c.setContext(ctx({ a: 1, b: 1 }))
    expect(step()).toBe(1) // two waits
    vi.advanceTimersByTime(99)
    expect(step()).toBe(1)
    vi.advanceTimersByTime(2)
    expect(step()).toBe(2)
  })
  it('the pause starts over when the work is undone, and a later edit does not tick it early', () => {
    vi.useFakeTimers()
    c.setContext(ctx({ a: 1, b: 1 }))
    vi.advanceTimersByTime(50)
    c.setContext(ctx({ a: 1, b: 0 })) // undone
    vi.advanceTimersByTime(500)
    expect(step()).toBe(1)
    c.setContext(ctx({ a: 1, b: 1 }))
    vi.advanceTimersByTime(99)
    expect(step()).toBe(1)
    vi.advanceTimersByTime(2)
    expect(step()).toBe(2)
  })
  it('signals are remembered with the lesson and can tick a step', async () => {
    vi.useFakeTimers()
    c.setContext(ctx({ a: 1, b: 1 }))
    vi.advanceTimersByTime(200)
    expect(step()).toBe(2)
    bus.emit('saved')
    expect(step()).toBe(2)
    bus.emit('played')
    expect(step()).toBe(3)
    vi.useRealTimers()
    await c.idle()
    expect((fake.stored as LearnState).lessons.toy.signals).toEqual(['saved', 'played'])
    expect(c.atEnd()).toBe(true)
  })
  it('a signal that arrives with no lesson running is ignored', () => {
    c.stop()
    bus.emit('played')
    expect(c.getSnapshot().state.lessons.toy.signals).toEqual([])
  })
  it('skip moves on and remembers it; skipped steps do not block the end', () => {
    c.skipStep()
    c.skipStep()
    expect(c.getSnapshot().state.lessons.toy).toMatchObject({ step: 2, skipped: ['one', 'two'] })
    bus.emit('played') // not testable without the game open: signal is recorded, nothing ticks yet
    c.setContext(ctx({ a: 0, b: 0 }))
    expect(c.atEnd()).toBe(true)
    c.finish()
    expect(c.running()).toBeNull()
    expect(c.getSnapshot().state.lessons.toy.state).toBe('finished')
  })
  it('stop keeps the progress; resume continues at the step and opens its screen', async () => {
    c.setContext(ctx({ a: 1, b: 0 }))
    c.stop()
    expect(c.running()).toBeNull()
    expect(step()).toBe(1)
    went.length = 0
    await c.resume('toy')
    expect(c.running()!.id).toBe('toy')
    expect(step()).toBe(1)
    expect(went).toEqual([['practice-toy', 'maps', undefined]])
  })
  it('take me there opens the step screen, with the item the lesson picks from the game', () => {
    c.setContext(ctx({ a: 0, b: 0 }, { section: 'rules' }))
    went.length = 0
    c.takeMeThere()
    expect(went).toEqual([['practice-toy', 'units', 'item0']])
  })
  it('take me there still picks the item after the game was closed (the last document is kept)', () => {
    c.setContext(ctx({ a: 0, b: 0 }))
    c.clearContext('practice-toy')
    went.length = 0
    c.takeMeThere()
    expect(went).toEqual([['practice-toy', 'units', 'item0']])
  })
  it('the end screen can press Play in the open game', () => {
    c.setContext(ctx({ a: 0, b: 0 }))
    c.play()
    expect(played).toBe(1)
  })
})

describe('the practice game is gone', () => {
  beforeEach(async () => { await c.load(); await c.start('toy') })

  it('resume says so instead of opening anything', async () => {
    c.stop()
    fake.games = []
    went.length = 0
    await c.resume('toy')
    expect(c.getSnapshot().gone).toBe('toy')
    expect(went).toEqual([])
    expect(c.running()).toBeNull()
  })
  it('a running lesson finds out on page load / game list (checkPractice), and when the game cannot be opened', async () => {
    fake.games = []
    await c.checkPractice()
    expect(c.getSnapshot().gone).toBe('toy')
    fake.games = ['practice-toy']
    await c.checkPractice()
    expect(c.getSnapshot().gone).toBeNull()
    c.setContext(ctx(null, { failed: true }))
    expect(c.getSnapshot().gone).toBe('toy')
  })
  it('start again makes a new practice game and clears the message', async () => {
    fake.games = []
    await c.checkPractice()
    await c.start('toy')
    expect(c.getSnapshot().gone).toBeNull()
    expect(c.getSnapshot().state.lessons.toy).toMatchObject({ state: 'running', step: 0, game: 'practice-toy' })
  })
})
