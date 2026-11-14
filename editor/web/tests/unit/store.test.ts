import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { Api, PutBody, Result, PutResult } from '../../src/api/client'
import { updateUnit } from '../../src/model/units'
import type { ProjectDoc, ServerProject } from '../../src/model/types'
import { buildBody } from '../../src/store/autosave'
import { DocumentStore } from '../../src/store/documentStore'
import { HISTORY_CAP, initHistory, pushEdit, redo, undo } from '../../src/store/history'
import { loadDemo } from './fixtures'

describe('history', () => {
  it('undo and redo walk the snapshots', () => {
    let h = initHistory(0)
    h = pushEdit(h, 1, undefined, 0)
    h = pushEdit(h, 2, undefined, 1)
    h = undo(h); expect(h.present).toBe(1)
    h = undo(h); expect(h.present).toBe(0)
    expect(undo(h)).toBe(h)
    h = redo(h); expect(h.present).toBe(1)
    h = pushEdit(h, 9, undefined, 2)
    expect(h.future).toEqual([])
  })
  it('edits with one key in quick succession are one step; a pause or another key starts a new one', () => {
    let h = initHistory('')
    for (const [i, c] of ['a', 'ab', 'abc'].entries()) h = pushEdit(h, c, 'name', i * 100)
    expect(h.past).toEqual([''])
    h = pushEdit(h, 'abcd', 'name', 5000)
    expect(h.past).toEqual(['', 'abc'])
    h = pushEdit(h, 'X', 'other', 5100)
    expect(h.past).toEqual(['', 'abc', 'abcd'])
    expect(undo(h).lastKey).toBeNull()
  })
  it('keeps at most 200 steps and ignores no-op edits', () => {
    let h = initHistory(0)
    for (let i = 1; i <= 250; i++) h = pushEdit(h, i, undefined, i)
    expect(h.past).toHaveLength(HISTORY_CAP)
    expect(pushEdit(h, 250, undefined, 1)).toBe(h)
  })
})

describe('buildBody', () => {
  it('sends only what changed and never a null section', () => {
    const a = loadDemo('demo-srpg')
    const b: ProjectDoc = { ...a, units: updateUnit(a, 'lord', { name: 'X' }).units, terrain: null }
    const body = buildBody(b, { ...a, terrain: a.terrain }, 'r1', false)!
    expect(Object.keys(body).sort()).toEqual(['baseRevision', 'units'])
    expect(buildBody(a, a, 'r1', false)).toBeNull()
    const all = buildBody(b, a, 'r1', true)!
    expect(Object.keys(all)).toContain('game')
    expect(Object.keys(all)).not.toContain('terrain')
  })
})

// --- store + autosave against a fake API ---
type PutFn = (body: PutBody) => Promise<Result<PutResult>>

function fakeApi(project: ServerProject, put: PutFn) {
  const calls: PutBody[] = []
  const api = {
    getProject: vi.fn(async () => ({ ok: true, value: project }) as Result<ServerProject>),
    putProject: vi.fn(async (_id: string, body: PutBody) => { calls.push(body); return put(body) }),
    listArt: vi.fn(async () => ({ ok: true, value: [] })),
    listModels: vi.fn(async () => ({ ok: true, value: [] })),
  } as unknown as Api
  return { api, calls }
}
const server = (): ServerProject => ({ ...loadDemo('demo-srpg'), id: 'g', revision: 'r0', problems: [] })
const ok = (rev: string): Result<PutResult> => ({ ok: true, value: { revision: rev, problems: [] } })
const rename = (n: string) => (d: ProjectDoc) => updateUnit(d, 'lord', { name: n })

describe('autosave', () => {
  beforeEach(() => { vi.useFakeTimers() })
  afterEach(() => { vi.useRealTimers() })

  async function open(put: PutFn) {
    const f = fakeApi(server(), put)
    const store = new DocumentStore(f.api, 'g', { now: () => Date.now() })
    await store.load()
    return { store, ...f }
  }

  it('waits 800 ms after the last edit, then saves once', async () => {
    const { store, calls } = await open(async () => ok('r1'))
    store.edit(rename('A'), 'k'); await vi.advanceTimersByTimeAsync(500)
    store.edit(rename('AB'), 'k'); await vi.advanceTimersByTimeAsync(700)
    expect(calls).toHaveLength(0)
    expect(store.getState().save.state).toBe('dirty')
    await vi.advanceTimersByTimeAsync(200)
    expect(calls).toHaveLength(1)
    expect(calls[0].baseRevision).toBe('r0')
    expect(calls[0].units![0].name).toBe('AB')
    expect(store.getState().save.state).toBe('saved')
    expect(store.getState().revision).toBe('r1')
  })

  it('never has two saves in flight; edits made meanwhile are saved afterwards with the new revision', async () => {
    let release!: (r: Result<PutResult>) => void
    let active = 0, maxActive = 0
    const { store, calls } = await open(() => {
      active++; maxActive = Math.max(maxActive, active)
      if (calls.length === 1) return new Promise((res) => { release = (r) => { active--; res(r) } })
      active--; return Promise.resolve(ok('r2'))
    })
    store.edit(rename('first'), 'a'); await vi.advanceTimersByTimeAsync(800)
    expect(calls).toHaveLength(1)
    expect(store.getState().save.state).toBe('saving')
    store.edit(rename('second'), 'b'); await vi.advanceTimersByTimeAsync(2000)
    expect(calls).toHaveLength(1)
    release(ok('r1')); await vi.advanceTimersByTimeAsync(10)
    await vi.advanceTimersByTimeAsync(900)
    expect(calls).toHaveLength(2)
    expect(calls[1].baseRevision).toBe('r1')
    expect(calls[1].units![0].name).toBe('second')
    expect(maxActive).toBe(1)
    expect(store.getState().save.state).toBe('saved')
  })

  it('retries with backoff, keeps the edits, and recovers', async () => {
    let fail = 2
    const { store, calls } = await open(async () => (fail-- > 0 ? { ok: false, status: 0, error: 'offline' } : ok('r1')))
    store.edit(rename('A')); await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save).toMatchObject({ state: 'error', message: 'offline' })
    await vi.advanceTimersByTimeAsync(1000)
    expect(calls).toHaveLength(2)
    await vi.advanceTimersByTimeAsync(2000)
    expect(calls).toHaveLength(3)
    expect(store.getState().save.state).toBe('saved')
    expect(store.getState().doc.units![0].name).toBe('A')
  })

  it('409 stops saving until Reload (drops edits) or Keep mine (sends everything)', async () => {
    let conflict = true
    const { store, calls, api } = await open(async () => (conflict ? { ok: false, status: 409, error: 'changed', revision: 'disk' } : ok('r5')))
    store.edit(rename('mine')); await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save.state).toBe('conflict')
    store.edit(rename('mine2')); await vi.advanceTimersByTimeAsync(5000)
    expect(calls).toHaveLength(1)

    conflict = false
    ;(api.getProject as any).mockResolvedValueOnce({ ok: true, value: { ...server(), revision: 'disk' } })
    await store.keepMine()
    expect(calls).toHaveLength(2)
    expect(calls[1].baseRevision).toBe('disk')
    expect(Object.keys(calls[1])).toEqual(expect.arrayContaining(['game', 'units', 'terrain', 'weapons', 'maps']))
    expect(calls[1].units![0].name).toBe('mine2')
    expect(store.getState().save.state).toBe('saved')

    // Second conflict, this time Reload.
    conflict = true
    store.edit(rename('again')); await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save.state).toBe('conflict')
    ;(api.getProject as any).mockResolvedValueOnce({ ok: true, value: { ...server(), revision: 'disk2', units: server().units!.map((u, i) => (i ? u : { ...u, name: 'From disk' })) } })
    await store.reload()
    expect(store.getState().doc.units![0].name).toBe('From disk')
    expect(store.getState().save.state).toBe('saved')
    expect(store.getState().canUndo).toBe(false)
  })

  it('undo and redo are edits too and get saved', async () => {
    const { store, calls } = await open(async () => ok('r1'))
    store.edit(rename('A')); store.undo()
    expect(store.getState().doc.units![0].name).toBe('Lord')
    expect(store.getState().canRedo).toBe(true)
    await vi.advanceTimersByTimeAsync(800)
    expect(calls).toHaveLength(0) // back to what is on disk: nothing to write
    expect(store.getState().save.state).toBe('saved')
    store.redo(); await vi.advanceTimersByTimeAsync(800)
    expect(calls).toHaveLength(1)
  })

  it('flush saves immediately; null sections are never sent', async () => {
    const p = server()
    p.units = null
    const f = fakeApi(p, async () => ok('r1'))
    const store = new DocumentStore(f.api, 'g')
    await store.load()
    store.edit((d) => ({ ...d, game: { ...d.game!, name: 'N' } }))
    store.edit((d) => updateUnit(d, 'lord', { name: 'ignored' }))
    expect(await store.flush()).toBe(true)
    expect(f.calls).toHaveLength(1)
    expect('units' in f.calls[0]).toBe(false)
    expect(f.calls[0].game!.name).toBe('N')
  })
})
