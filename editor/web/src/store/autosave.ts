// Autosave: debounce, exactly one PUT in flight, retry with backoff, and a
// conflict state that stops everything until the user decides.
import type { Api, PutBody, Result } from '../api/client'
import { dropEmptyOwners } from '../model/structuresConvert'
import { supportsEmpty } from '../model/srpgFeatures'
import type { Problem, ProjectDoc } from '../model/types'

export type SaveState = 'saved' | 'dirty' | 'saving' | 'error' | 'conflict'
export interface SaveStatus { state: SaveState; message?: string }

export interface SaverOptions {
  debounceMs?: number
  /** Retry delays in ms; the last one repeats. */
  backoffMs?: number[]
}

interface Deps {
  api: Pick<Api, 'putProject' | 'getProject'>
  id: string
  getDoc: () => ProjectDoc
  onStatus: (s: SaveStatus) => void
  onSaved: (revision: string, problems: Problem[]) => void
  /** The revision changed because of an art upload/delete (not a save). */
  onRevision?: (revision: string) => void
}

/** Browsers cap keepalive request bodies at 64 KB. */
const KEEPALIVE_LIMIT = 60_000

const SECTIONS = ['game', 'terrain', 'structures', 'items', 'research', 'weapons', 'units', 'graphics', 'campaign', 'audio', 'characters', 'skills', 'abilities', 'supports'] as const

/** Only what changed since the last save goes over the wire; sections that
 *  failed to load (null) are never sent. `all` sends every loadable part. */
export function buildBody(doc: ProjectDoc, saved: ProjectDoc, baseRevision: string, all: boolean): PutBody | null {
  const body: PutBody = { baseRevision }
  let any = false
  for (const k of SECTIONS) {
    // Art is the one section that can legitimately go back to "none" (undo past the first image): that is written as an
    // empty section, because sending nothing would leave the old images on disk while the screen shows none.
    // Structures can go back to "none" the same way (undo past the first structure or past Convert): an empty list.
    // For research an empty list removes data/research.json (the loader refuses an empty one).
    const value = k === 'graphics' && doc.graphics === null && saved.graphics !== null ? { sheets: [] }
      : (k === 'structures' || k === 'items' || k === 'research' || k === 'characters' || k === 'skills' || k === 'abilities') && doc[k] === null && saved[k] !== null ? []
        // The campaign likewise (Remove the campaign, or undo past Create): `false` deletes the file.
        : k === 'campaign' && doc.campaign === null && saved.campaign !== null ? false
          // Removing all sound (or undoing past the first track) likewise deletes data/audio.json.
          : k === 'audio' && doc.audio === null && saved.audio !== null ? false
            // Supports: removed (`false`) when gone or when nothing is in it (model/srpgFeatures.ts).
            : k === 'supports' ? (!doc.supports ? (saved.supports ? false : null) : supportsEmpty(doc.supports) ? false : doc.supports)
              : doc[k]
    if (value !== null && (all || doc[k] !== saved[k])) { (body as unknown as Record<string, unknown>)[k] = value; any = true }
  }
  const maps: Record<string, NonNullable<ProjectDoc['maps'][string]>> = {}
  for (const [id, m] of Object.entries(doc.maps)) {
    if (m !== null && (all || m !== saved.maps[id])) { maps[id] = dropEmptyOwners(m); any = true }
  }
  if (Object.keys(maps).length) body.maps = maps
  const scripts: Record<string, string> = {}
  for (const [p, text] of Object.entries(doc.scripts)) {
    if (all || text !== saved.scripts[p]) { scripts[p] = text; any = true }
  }
  if (Object.keys(scripts).length) body.scripts = scripts
  return any || all ? body : null
}

export class Saver {
  private revision: string
  private saved: ProjectDoc
  private status: SaveStatus = { state: 'saved' }
  private timer: ReturnType<typeof setTimeout> | null = null
  private inFlight: Promise<void> | null = null
  /** An art upload/delete is running: no save may start until it is done (see withDiskChange). */
  private diskOp: Promise<unknown> | null = null
  private rerun = false
  private forceAll = false
  private attempts = 0
  private readonly debounceMs: number
  private readonly backoffMs: number[]

  constructor(private deps: Deps, initial: { doc: ProjectDoc; revision: string }, opts: SaverOptions = {}) {
    this.saved = initial.doc
    this.revision = initial.revision
    this.debounceMs = opts.debounceMs ?? 800
    this.backoffMs = opts.backoffMs ?? [1000, 2000, 4000, 8000, 15000, 30000]
  }

  get state(): SaveState { return this.status.state }
  /** Conflict or failed save: stop looping. (A method so TS does not narrow `status` across awaits.) */
  private halted(): boolean { return this.status.state === 'conflict' || this.status.state === 'error' }
  get currentRevision(): string { return this.revision }

  private set(state: SaveState, message?: string) {
    this.status = { state, message }
    this.deps.onStatus(this.status)
  }

  private clearTimer() {
    if (this.timer) clearTimeout(this.timer)
    this.timer = null
  }

  /** The document changed: save after the user pauses. */
  changed(): void {
    if (this.status.state === 'conflict') return
    if (this.deps.getDoc() === this.saved) { if (!this.inFlight) this.set('saved'); return }
    if (this.status.state !== 'saving' && this.status.state !== 'error') this.set('dirty')
    // While a retry is pending the backoff timer already covers new edits.
    if (this.status.state === 'error') return
    this.clearTimer()
    this.timer = setTimeout(() => { this.timer = null; void this.run(false) }, this.debounceMs)
  }

  private run(keepalive: boolean): Promise<void> {
    if (this.inFlight) { this.rerun = true; return this.inFlight }
    if (this.diskOp) { this.rerun = true; return this.diskOp.then(() => undefined) }
    if (this.status.state === 'conflict') return Promise.resolve()
    this.clearTimer()
    const doc = this.deps.getDoc()
    const body = buildBody(doc, this.saved, this.revision, this.forceAll)
    if (!body) { this.set('saved'); return Promise.resolve() }
    this.set('saving')
    const useKeepalive = keepalive && JSON.stringify(body).length < KEEPALIVE_LIMIT
    this.inFlight = this.send(doc, body, useKeepalive).finally(() => { this.inFlight = null })
    return this.inFlight
  }

  private async send(doc: ProjectDoc, body: PutBody, keepalive: boolean): Promise<void> {
    const res = await this.deps.api.putProject(this.deps.id, body, { keepalive })
    if (res.ok) {
      this.revision = res.value.revision
      this.saved = doc
      this.forceAll = false
      this.attempts = 0
      this.deps.onSaved(res.value.revision, res.value.problems)
      const edited = this.deps.getDoc() !== doc
      this.inFlight = null
      if (this.rerun || (edited && !this.timer)) { this.rerun = false; await this.run(false) }
      else this.set(edited ? 'dirty' : 'saved')
      return
    }
    this.rerun = false
    if (res.status === 409) { this.set('conflict', res.error); return }
    this.set('error', res.error)
    const delay = this.backoffMs[Math.min(this.attempts, this.backoffMs.length - 1)]
    this.attempts++
    this.clearTimer()
    this.timer = setTimeout(() => { this.timer = null; this.set('dirty'); void this.run(false) }, delay)
  }

  /** Save right now and wait until everything is written (or a save failed).
   *  With `keepalive` (page is going away) it only fires the request. */
  async flush(opts: { keepalive?: boolean } = {}): Promise<void> {
    this.clearTimer()
    if (this.status.state === 'conflict') return
    if (opts.keepalive) {
      if (!this.inFlight && this.deps.getDoc() !== this.saved) await this.run(true)
      return
    }
    for (let i = 0; i < 20; i++) {
      if (this.diskOp) await this.diskOp
      if (this.inFlight) {
        await this.inFlight
        if (this.halted()) return
      }
      if (this.deps.getDoc() === this.saved) { this.set('saved'); return }
      this.attempts = 0
      await this.run(false)
      if (this.halted()) return
    }
  }

  /** Runs a call that changes the project on disk outside a save (uploading or deleting an image) without ever
   *  causing a false "changed on disk" conflict:
   *  1. it waits for a save in flight, and no save starts while it runs (they wait, then go on);
   *  2. a cheap revision check (a PUT with no sections) tells a real outside change from our own;
   *  3. the revision the call returns is adopted, so the next save carries the right baseRevision.
   *  In the conflict state the call still runs but its revision is not adopted (the user decides first). */
  async withDiskChange<T extends { revision: string }>(op: () => Promise<Result<T>>): Promise<Result<T>> {
    while (this.diskOp || this.inFlight) await (this.diskOp ?? this.inFlight)
    const running = this.runDiskOp(op)
    this.diskOp = running
    try { return await running } finally {
      this.diskOp = null
      if (this.rerun || (this.status.state !== 'conflict' && this.deps.getDoc() !== this.saved)) { this.rerun = false; void this.run(false) }
    }
  }

  private async runDiskOp<T extends { revision: string }>(op: () => Promise<Result<T>>): Promise<Result<T>> {
    let adopt = this.status.state !== 'conflict'
    if (adopt) {
      const check = await this.deps.api.putProject(this.deps.id, { baseRevision: this.revision })
      if (!check.ok && check.status === 409) { this.set('conflict', check.error); adopt = false }
      else if (!check.ok) return check
    }
    const res = await op()
    if (res.ok && adopt) { this.revision = res.value.revision; this.deps.onRevision?.(res.value.revision) }
    return res
  }

  /** Resolves when no art upload/delete is running. Reload waits for this: the conflict banner shows before the
   *  upload itself finishes, and the image it adds afterwards must not land in the freshly reloaded document. */
  async diskIdle(): Promise<void> { while (this.diskOp) await this.diskOp.catch(() => undefined) }

  /** After Reload: the loaded state is now what is on disk. */
  reset(doc: ProjectDoc, revision: string): void {
    this.clearTimer()
    this.saved = doc
    this.revision = revision
    this.forceAll = false
    this.attempts = 0
    this.rerun = false
    this.set('saved')
  }

  /** "Keep mine": take the disk's current revision, then overwrite with everything we have. */
  async keepMine(): Promise<boolean> {
    const res = await this.deps.api.getProject(this.deps.id)
    if (!res.ok) { this.set('conflict', res.error); return false }
    this.revision = res.value.revision
    this.forceAll = true
    this.set('dirty')
    await this.run(false)
    return this.status.state === 'saved'
  }

  dispose(): void { this.clearTimer() }
}
