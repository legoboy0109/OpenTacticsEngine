// The game's 3D model files (models/*.glb): which exist, and what each one contains (animation and material names,
// read in the browser from the file's JSON chunk). Like the art library, files are NOT part of the document; uploads go
// through the saver's withDiskChange so they never cause a false "changed on disk" conflict.
import { projectFileUrl, type Api, type ModelFile, type Result } from '../api/client'
import { readGlbFacts, type ModelFacts } from '../model/models3d'
import type { DiskChange } from './art'

export interface ModelState {
  files: ModelFile[]
  loaded: boolean
  error: string | null
  /** Bumped whenever the list or a file's facts changed. */
  version: number
}

/** undefined = not asked yet (asking starts the read); 'loading'; null = the bytes are not a readable model. */
export type FactsState = ModelFacts | 'loading' | null

export class ModelLibrary {
  private state: ModelState = { files: [], loaded: false, error: null, version: 0 }
  private facts = new Map<string, FactsState>()
  private listeners = new Set<() => void>()

  constructor(private api: Pick<Api, 'listModels' | 'uploadModel'>, private projectId: string, private diskChange: DiskChange) {}

  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => this.listeners.delete(l) }
  getState = (): ModelState => this.state
  private patch(p: Partial<ModelState>) { this.state = { ...this.state, ...p, version: this.state.version + 1 }; this.listeners.forEach((l) => l()) }

  /** (Re)read the folder listing; remembered facts are dropped since files may have changed on disk. */
  async refresh(): Promise<void> {
    const res = await this.api.listModels(this.projectId)
    if (!res.ok) { this.patch({ error: res.error, loaded: true }); return }
    this.facts.clear()
    this.patch({ files: res.value, loaded: true, error: null })
  }

  /** What the file contains. The first ask starts the read; the version bumps when it is ready. */
  factsOf(file: string): FactsState {
    const have = this.facts.get(file)
    if (have !== undefined) return have
    this.facts.set(file, 'loading')
    void this.read(file)
    return 'loading'
  }

  private async read(file: string): Promise<void> {
    let result: FactsState = null
    try {
      const res = await fetch(projectFileUrl(this.projectId, file))
      if (res.ok) result = readGlbFacts(new Uint8Array(await res.arrayBuffer()))
    } catch { /* unreachable helper: shown as unreadable */ }
    if (this.facts.get(file) !== 'loading') return // replaced or cleared meanwhile
    this.facts.set(file, result)
    this.patch({})
  }

  /** Uploads a .glb as models/<base>.glb. `overwrite` replaces an existing file. */
  async upload(base: string, glb: Blob, overwrite: boolean): Promise<Result<{ file: string; size: number; revision: string }>> {
    const res = await this.diskChange(() => this.api.uploadModel(this.projectId, base, glb, overwrite))
    if (res.ok) {
      const file = res.value.file
      this.facts.delete(file)
      const entry: ModelFile = { file, size: res.value.size, usedBy: this.state.files.find((f) => f.file === file)?.usedBy ?? [] }
      this.patch({ files: [...this.state.files.filter((f) => f.file !== file), entry].sort((a, b) => (a.file < b.file ? -1 : 1)) })
    }
    return res
  }

}
