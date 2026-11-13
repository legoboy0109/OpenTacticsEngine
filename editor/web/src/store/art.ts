// The game's image files (art/*.png): which exist, how big they are, and the decoded pictures the canvases draw.
// Files are NOT part of the document (undo never touches them). Uploads and deletes go through the saver's
// withDiskChange so they never cause a false "changed on disk" conflict.
import { projectFileUrl, type Api, type ArtFile, type Result, type UploadResult } from '../api/client'
import { artBase, artPath } from '../model/artFiles'

export type DiskChange = <T extends { revision: string }>(op: () => Promise<Result<T>>) => Promise<Result<T>>

export interface ArtState {
  files: ArtFile[]
  loaded: boolean
  error: string | null
  /** Bumped whenever a list or picture changed, so canvases know to redraw. */
  version: number
}

export interface LoadedImage { status: 'loading' | 'ready' | 'error'; img: CanvasImageSource | null; width: number; height: number }
const LOADING: LoadedImage = { status: 'loading', img: null, width: 0, height: 0 }

export class ArtLibrary {
  private state: ArtState = { files: [], loaded: false, error: null, version: 0 }
  private images = new Map<string, LoadedImage>()
  /** Per file: changes when its bytes may have changed, so the browser's cache is not reused. */
  private stamps = new Map<string, number>()
  private listeners = new Set<() => void>()

  constructor(private api: Pick<Api, 'listArt' | 'uploadArt' | 'deleteArt'>, private projectId: string, private diskChange: DiskChange) {}

  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => this.listeners.delete(l) }
  getState = (): ArtState => this.state
  private patch(p: Partial<ArtState>) { this.state = { ...this.state, ...p, version: this.state.version + 1 }; this.listeners.forEach((l) => l()) }

  /** (Re)read the folder listing. Cached pictures are dropped too, since files may have changed on disk. */
  async refresh(): Promise<void> {
    const res = await this.api.listArt(this.projectId)
    if (!res.ok) { this.patch({ error: res.error, loaded: true }); return }
    for (const f of res.value) this.stamps.set(f.file, (this.stamps.get(f.file) ?? 0) + 1)
    this.images.clear()
    this.patch({ files: res.value, loaded: true, error: null })
  }

  fileInfo(file: string): ArtFile | undefined { return this.state.files.find((f) => f.file === file) }

  /** The decoded picture of a file (loads it on first use; the version bumps when it is ready). */
  image(file: string): LoadedImage {
    const have = this.images.get(file)
    if (have) return have
    // A unique object per load: a result is dropped when the file was replaced or the cache cleared meanwhile.
    const token: LoadedImage = { ...LOADING }
    this.images.set(file, token)
    if (typeof Image === 'undefined') { this.images.set(file, { status: 'error', img: null, width: 0, height: 0 }); return this.images.get(file)! }
    const img = new Image()
    const done = (status: 'ready' | 'error') => {
      if (this.images.get(file) !== token) return
      this.images.set(file, { status, img: status === 'ready' ? img : null, width: img.naturalWidth, height: img.naturalHeight })
      this.patch({})
    }
    img.onload = () => done('ready')
    img.onerror = () => done('error')
    img.src = this.url(file)
    return token
  }

  /** Where the browser can show a file (for plain <img> thumbnails). */
  url(file: string): string { return `${projectFileUrl(this.projectId, file)}?v=${this.stamps.get(file) ?? 0}` }

  /** Uploads PNG bytes as art/<base>.png. `overwrite` replaces an existing file (Replace file). */
  async upload(base: string, png: Blob, overwrite: boolean): Promise<Result<UploadResult>> {
    const res = await this.diskChange(() => this.api.uploadArt(this.projectId, base, png, overwrite))
    if (res.ok) {
      const file = res.value.file || artPath(base)
      this.stamps.set(file, (this.stamps.get(file) ?? 0) + 1)
      this.images.delete(file)
      const entry: ArtFile = { file, width: res.value.width, height: res.value.height, size: res.value.size }
      const files = [...this.state.files.filter((f) => f.file !== file), entry].sort((a, b) => (a.file < b.file ? -1 : 1))
      this.patch({ files })
    }
    return res
  }

  /** Deletes art/<base>.png from disk for good. 409 (still used by the saved game) comes back as an error with `usedBy`. */
  async remove(file: string): Promise<Result<{ revision: string }>> {
    const res = await this.diskChange(() => this.api.deleteArt(this.projectId, artBase(file)))
    if (res.ok) {
      this.images.delete(file)
      this.patch({ files: this.state.files.filter((f) => f.file !== file) })
    }
    return res
  }
}
