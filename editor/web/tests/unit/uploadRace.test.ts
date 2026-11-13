// Image uploads change the project's revision on disk. The store must adopt it without a false "changed on disk"
// conflict, whatever the autosave is doing at that moment, and must never lose edits.
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { Api, PutBody, PutResult, Result, UploadResult } from '../../src/api/client'
import { addSheet } from '../../src/model/graphics'
import type { ProjectDoc, ServerProject } from '../../src/model/types'
import { updateUnit } from '../../src/model/units'
import { DocumentStore } from '../../src/store/documentStore'
import { loadTemplate } from './fixtures'

const server = (): ServerProject => ({ ...loadTemplate('blank'), id: 'g', revision: 'r0', problems: [] })
const rename = (n: string) => (d: ProjectDoc) => updateUnit(d, d.units![0].id, { name: n })
const png = () => new Blob([new Uint8Array([1, 2, 3])])
const uploaded = (rev: string, file = 'art/a.png'): Result<UploadResult> => ({ ok: true, value: { file, width: 24, height: 24, size: 3, revision: rev } })

/** A fake helper: `disk` is its current revision; a PUT with another baseRevision is a 409 like the real one. */
function fakeHelper() {
  const state = { disk: 'r0', puts: [] as PutBody[], order: [] as string[], externalEdit: false }
  let n = 0
  const gates: { put?: () => Promise<void>; upload?: () => Promise<void> } = {}
  const api = {
    getProject: vi.fn(async () => ({ ok: true, value: { ...server(), revision: state.disk } }) as Result<ServerProject>),
    putProject: vi.fn(async (_id: string, body: PutBody): Promise<Result<PutResult>> => {
      state.puts.push(body)
      const sections = Object.keys(body).filter((k) => k !== 'baseRevision')
      state.order.push(sections.length ? 'put' : 'check')
      if (gates.put && sections.length) await gates.put()
      if (body.baseRevision !== state.disk) return { ok: false, status: 409, error: 'changed', revision: state.disk }
      if (sections.length) state.disk = `p${++n}`
      return { ok: true, value: { revision: state.disk, problems: [] } }
    }),
    listArt: vi.fn(async () => ({ ok: true, value: [] }) as Result<never[]>),
    uploadArt: vi.fn(async (_id: string, name: string, _b: Blob, overwrite: boolean): Promise<Result<UploadResult>> => {
      state.order.push('upload')
      if (gates.upload) await gates.upload()
      if (state.externalEdit) return { ok: false, status: 400, error: 'not a PNG' }
      state.disk = `u${++n}`
      void overwrite
      return uploaded(state.disk, `art/${name}.png`)
    }),
    deleteArt: vi.fn(async () => { state.disk = `d${++n}`; return { ok: true, value: { revision: state.disk } } as Result<{ revision: string }> }),
  } as unknown as Api
  return { api, state, gates }
}

const settle = () => vi.advanceTimersByTimeAsync(0)

describe('art uploads and the autosave', () => {
  beforeEach(() => { vi.useFakeTimers() })
  afterEach(() => { vi.useRealTimers() })

  async function open() {
    const h = fakeHelper()
    const store = new DocumentStore(h.api, 'g', { now: () => Date.now() })
    await store.load()
    return { store, ...h }
  }

  it('adopts the new revision: the next save is not a conflict', async () => {
    const { store, state } = await open()
    const r = await store.art.upload('a', png(), false)
    expect(r.ok).toBe(true)
    expect(store.getState().revision).toBe('u1')
    store.edit(rename('X')); await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save.state).toBe('saved')
    expect(state.puts.at(-1)!.baseRevision).toBe('u1')
  })

  it('an edit waiting for its 800 ms when the upload happens is saved afterwards with the new revision', async () => {
    const { store, state } = await open()
    store.edit(rename('pending')); await vi.advanceTimersByTimeAsync(300)
    expect(store.getState().save.state).toBe('dirty')
    const up = store.art.upload('a', png(), false)
    await up
    await vi.advanceTimersByTimeAsync(2000)
    expect(store.getState().save.state).toBe('saved')
    expect(state.puts.filter((p) => p.units)).toHaveLength(1)
    expect(state.puts.find((p) => p.units)!.baseRevision).toBe('u1')
    expect(store.getState().doc.units![0].name).toBe('pending')
  })

  it('a save in flight finishes first; the upload then runs on top of it and its revision wins', async () => {
    const { store, state, gates } = await open()
    let release!: () => void
    gates.put = () => new Promise<void>((r) => { release = r })
    store.edit(rename('A')); await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save.state).toBe('saving')
    const up = store.art.upload('a', png(), false)
    await settle()
    expect(state.order.filter((o) => o === 'upload')).toHaveLength(0) // waiting for the save
    release(); await up
    await vi.advanceTimersByTimeAsync(2000)
    expect(state.order.indexOf('put')).toBeLessThan(state.order.indexOf('upload'))
    expect(store.getState().save.state).toBe('saved')
    expect(store.getState().revision).toBe(state.disk)
  })

  it('an edit made while the upload runs waits for it, then saves with the right revision', async () => {
    const { store, state, gates } = await open()
    let release!: () => void
    gates.upload = () => new Promise<void>((r) => { release = r })
    const up = store.art.upload('a', png(), false)
    await vi.advanceTimersByTimeAsync(10)
    store.edit(rename('during')); await vi.advanceTimersByTimeAsync(1500)
    expect(state.puts.filter((p) => p.units)).toHaveLength(0) // no save while the upload is running
    release(); await up
    await vi.advanceTimersByTimeAsync(10)
    expect(state.puts.filter((p) => p.units)).toHaveLength(1)
    expect(state.puts.find((p) => p.units)!.baseRevision).toBe('u1')
    expect(store.getState().save.state).toBe('saved')
    expect(store.getState().doc.units![0].name).toBe('during')
  })

  it('flush (Play, leaving) waits for a running upload and then writes the edits', async () => {
    const { store, state, gates } = await open()
    let release!: () => void
    gates.upload = () => new Promise<void>((r) => { release = r })
    const up = store.art.upload('a', png(), false)
    await vi.advanceTimersByTimeAsync(10)
    store.edit(rename('F'))
    const flushed = store.flush()
    await settle()
    release(); await up
    expect(await flushed).toBe(true)
    expect(state.puts.find((p) => p.units)!.baseRevision).toBe('u1')
  })

  it('a real outside change is still a conflict: the file is uploaded but the revision is not adopted', async () => {
    const { store, state } = await open()
    state.disk = 'someone-else'
    const r = await store.art.upload('a', png(), false)
    expect(r.ok).toBe(true) // the upload itself worked
    expect(store.getState().save.state).toBe('conflict')
    expect(store.getState().revision).toBe('r0')
    store.edit(rename('mine')); await vi.advanceTimersByTimeAsync(3000)
    expect(state.puts.filter((p) => p.units)).toHaveLength(0)
  })

  it('a failed upload changes nothing: no revision change, no edit lost, document untouched', async () => {
    const { store, state } = await open()
    state.externalEdit = true // the fake helper now rejects uploads
    const before = store.getState().doc
    const r = await store.art.upload('a', png(), false)
    expect(r.ok).toBe(false)
    expect(store.getState().doc).toBe(before)
    expect(store.getState().revision).toBe('r0')
    expect(store.getState().canUndo).toBe(false)
    expect(store.getState().save.state).toBe('saved')
  })

  it('uploading when the helper cannot be reached reports the error and does not touch the document', async () => {
    const { store, api } = await open()
    ;(api.putProject as any).mockResolvedValueOnce({ ok: false, status: 0, error: 'offline' })
    const r = await store.art.upload('a', png(), false)
    expect(r).toMatchObject({ ok: false, status: 0 })
    expect((api.uploadArt as any).mock.calls).toHaveLength(0)
  })

  it('adding the image to the document is one undo step; undo never deletes the file', async () => {
    const { store, api } = await open()
    await store.art.upload('a', png(), false)
    store.edit((d) => addSheet(d, { file: 'art/a.png', cellWidth: 24, cellHeight: 24 }))
    await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().doc.graphics!.sheets).toHaveLength(1)
    store.undo()
    expect(store.getState().doc.graphics).toBeNull()
    expect((api.deleteArt as any).mock.calls).toHaveLength(0)
    expect(store.art.getState().files.map((f) => f.file)).toEqual(['art/a.png'])
    store.redo()
    expect(store.getState().doc.graphics!.sheets).toHaveLength(1)
    await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save.state).toBe('saved')
  })

  it('deleting a file adopts its revision too, and the list drops it', async () => {
    const { store, state } = await open()
    await store.art.upload('a', png(), false)
    const r = await store.art.remove('art/a.png')
    expect(r.ok).toBe(true)
    expect(store.art.getState().files).toEqual([])
    expect(store.getState().revision).toBe(state.disk)
    store.edit(rename('after')); await vi.advanceTimersByTimeAsync(800)
    expect(store.getState().save.state).toBe('saved')
  })

  it('sends the graphics section with a save only when it changed', async () => {
    const { store, state } = await open()
    store.edit((d) => addSheet(d, { file: 'art/a.png', cellWidth: 24, cellHeight: 24 })); await vi.advanceTimersByTimeAsync(800)
    expect(Object.keys(state.puts.at(-1)!).sort()).toEqual(['baseRevision', 'graphics'])
    store.edit(rename('x')); await vi.advanceTimersByTimeAsync(800)
    expect(Object.keys(state.puts.at(-1)!)).not.toContain('graphics')
  })

  it('undoing back past the first image writes an empty art section instead of leaving the old one on disk', async () => {
    const { store, state } = await open()
    store.edit((d) => addSheet(d, { file: 'art/a.png', cellWidth: 24, cellHeight: 24 })); await vi.advanceTimersByTimeAsync(800)
    store.undo(); await vi.advanceTimersByTimeAsync(800)
    expect(state.puts.at(-1)!.graphics).toEqual({ sheets: [] })
    expect(store.getState().save.state).toBe('saved')
    store.redo(); await vi.advanceTimersByTimeAsync(800)
    expect(state.puts.at(-1)!.graphics!.sheets).toHaveLength(1)
  })
})
