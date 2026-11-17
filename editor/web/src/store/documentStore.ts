// The one place that holds the open project: the document, its undo
// history, the revision on disk and the save status. No React in here; the
// context module wraps it for components.
import type { Api } from '../api/client'
import { reconcileAudio } from '../model/audioRefs'
import { reconcileCampaign } from '../model/campaignRefs'
import { reconcileEconomy } from '../model/researchRefs'
import type { Problem, ProjectDoc } from '../model/types'
import { ArtLibrary } from './art'
import { AudioLibrary } from './audioFiles'
import { ModelLibrary } from './models'
import { Saver, type SaveStatus, type SaverOptions } from './autosave'
import { canRedo, canUndo, initHistory, pushEdit, redo, undo, type History } from './history'

export type Phase = 'loading' | 'ready' | 'failed'

export interface StoreState {
  phase: Phase
  loadError: string | null
  id: string
  doc: ProjectDoc
  revision: string
  problems: Problem[]
  save: SaveStatus
  canUndo: boolean
  canRedo: boolean
  /** A short message the user should see (dismissible), e.g. what a delete changed. */
  notice: string | null
}

const EMPTY: ProjectDoc = { game: null, terrain: null, structures: null, items: null, research: null, weapons: null, units: null, maps: {}, scripts: {}, graphics: null, campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null }

export class DocumentStore {
  private state: StoreState
  private history: History<ProjectDoc> = initHistory(EMPTY)
  private saver: Saver | null = null
  private listeners = new Set<() => void>()
  /** The image files of this game (not part of the undoable document). */
  readonly art: ArtLibrary
  /** The 3D model files of this game (not part of the undoable document). */
  readonly models: ModelLibrary
  /** The sound files of this game (not part of the undoable document). */
  readonly audioFiles: AudioLibrary

  constructor(private api: Api, readonly id: string, private opts: SaverOptions & { now?: () => number } = {}) {
    this.state = {
      phase: 'loading', loadError: null, id, doc: EMPTY, revision: '', problems: [],
      save: { state: 'saved' }, canUndo: false, canRedo: false, notice: null,
    }
    this.art = new ArtLibrary(api, id, (op) => (this.saver ? this.saver.withDiskChange(op) : op()))
    this.models = new ModelLibrary(api, id, (op) => (this.saver ? this.saver.withDiskChange(op) : op()))
    this.audioFiles = new AudioLibrary(api, id, (op) => (this.saver ? this.saver.withDiskChange(op) : op()))
  }

  // --- subscription (useSyncExternalStore) ---
  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => this.listeners.delete(l) }
  getState = (): StoreState => this.state
  private patch(p: Partial<StoreState>) { this.state = { ...this.state, ...p }; this.listeners.forEach((l) => l()) }
  private syncHistory() {
    this.patch({ doc: this.history.present, canUndo: canUndo(this.history), canRedo: canRedo(this.history) })
  }

  // --- loading ---
  async load(): Promise<void> {
    const res = await this.api.getProject(this.id)
    if (!res.ok) { this.patch({ phase: 'failed', loadError: res.error }); return }
    const { id: _id, revision, problems, ...doc } = res.value
    this.history = initHistory(doc)
    this.saver?.dispose()
    this.saver = new Saver({
      api: this.api, id: this.id,
      getDoc: () => this.history.present,
      onStatus: (save) => this.patch({ save }),
      onSaved: (rev, probs) => this.patch({ revision: rev, problems: probs }),
      onRevision: (rev) => this.patch({ revision: rev }),
    }, { doc, revision }, this.opts)
    this.patch({ phase: 'ready', loadError: null, revision, problems, save: { state: 'saved' } })
    this.syncHistory()
    void this.art.refresh()
    void this.models.refresh()
    void this.audioFiles.refresh()
  }

  // --- editing ---
  /** Apply a pure edit. Edits with the same `key` in quick succession
   *  (typing in one field) become one undo step. */
  edit = (fn: (doc: ProjectDoc) => ProjectDoc, key?: string): void => {
    if (this.state.phase !== 'ready') return
    const prev = this.history.present
    // The campaign follows the rest of the game: a switch, side, map or story name that this edit removed is taken out
    // of the scenes in the same undo step (model/campaignRefs.ts).
    // Research, upgrades and repairs lists follow a deleted technology, unit type, structure kind, resource or stat the
    // same way (model/researchRefs.ts); the campaign then follows whatever that took out of the maps.
    // A track or sound that this edit deleted is taken out of the standard set, the overrides, the maps' music, the scenes
    // and the story commands first (model/audioRefs.ts), also in the same undo step.
    const edited = reconcileEconomy(prev, reconcileAudio(prev, fn(prev)))
    const next = reconcileCampaign(prev, edited)
    const h = pushEdit(this.history, next, key, (this.opts.now ?? Date.now)())
    if (h === this.history) return
    this.history = h
    this.syncHistory()
    this.saver?.changed()
  }

  undo(): void { this.step(undo) }
  redo(): void { this.step(redo) }
  private step(f: (h: History<ProjectDoc>) => History<ProjectDoc>) {
    const h = f(this.history)
    if (h === this.history) return
    this.history = h
    this.syncHistory()
    this.saver?.changed()
  }

  setNotice(notice: string | null): void { this.patch({ notice }) }

  // --- saving ---
  /** Save everything now (Play and leaving the page use this). Resolves true when disk matches memory. */
  async flush(): Promise<boolean> {
    await this.saver?.flush()
    return this.state.save.state === 'saved'
  }

  /** The page is hiding or closing: fire the save with keepalive. */
  flushOnHide(): void { void this.saver?.flush({ keepalive: true }) }

  /** Conflict: drop local changes and take what is on disk. */
  async reload(): Promise<void> {
    await this.saver?.diskIdle()
    await Promise.resolve() // let the caller of the finished upload apply its edit before the document is replaced
    const res = await this.api.getProject(this.id)
    if (!res.ok) { this.patch({ save: { state: 'conflict', message: res.error } }); return }
    const { id: _id, revision, problems, ...doc } = res.value
    this.history = initHistory(doc)
    this.saver?.reset(doc, revision)
    this.patch({ revision, problems, notice: null })
    this.syncHistory()
    void this.art.refresh()
    void this.models.refresh()
    void this.audioFiles.refresh()
  }

  /** Conflict: keep local changes and overwrite what is on disk. */
  async keepMine(): Promise<void> { await this.saver?.keepMine() }

  /** Listen for the page going away. Returns the cleanup function. */
  attachPageEvents(): () => void {
    const onHide = () => { if (document.visibilityState === 'hidden') this.flushOnHide() }
    const onPageHide = () => this.flushOnHide()
    document.addEventListener('visibilitychange', onHide)
    window.addEventListener('pagehide', onPageHide)
    return () => {
      document.removeEventListener('visibilitychange', onHide)
      window.removeEventListener('pagehide', onPageHide)
    }
  }

  /** Leaving the project inside the app: write what is pending, then stop. */
  dispose(): void {
    const saver = this.saver
    void saver?.flush().finally(() => saver.dispose())
  }
}
