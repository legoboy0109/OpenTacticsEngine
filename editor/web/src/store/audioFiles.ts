// The game's sound files (audio/*.wav, *.ogg): which exist and what the helper knows about each (length, rate, channels).
// Like the art library, files are NOT part of the document (undo never touches them) and uploads / deletes go through the
// saver's withDiskChange so they never cause a false "changed on disk" conflict.
import { projectFileUrl, type Api, type AudioUploadResult, type Result } from '../api/client'
import { audioName, audioPath } from '../model/audioFiles'
import type { AudioFile } from '../model/audioTypes'
import type { DiskChange } from './art'

export interface AudioFilesState {
  files: AudioFile[]
  loaded: boolean
  error: string | null
  /** Bumped whenever the list changed. */
  version: number
}

export class AudioLibrary {
  private state: AudioFilesState = { files: [], loaded: false, error: null, version: 0 }
  /** Per file: changes when its bytes may have changed, so the browser's cache is not reused. */
  private stamps = new Map<string, number>()
  private listeners = new Set<() => void>()

  constructor(private api: Pick<Api, 'listAudio' | 'uploadAudio' | 'deleteAudio'>, private projectId: string, private diskChange: DiskChange) {}

  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => this.listeners.delete(l) }
  getState = (): AudioFilesState => this.state
  private patch(p: Partial<AudioFilesState>) { this.state = { ...this.state, ...p, version: this.state.version + 1 }; this.listeners.forEach((l) => l()) }

  /** (Re)read the folder listing. */
  async refresh(): Promise<void> {
    const res = await this.api.listAudio(this.projectId)
    if (!res.ok) { this.patch({ error: res.error, loaded: true }); return }
    for (const f of res.value) this.stamps.set(f.file, (this.stamps.get(f.file) ?? 0) + 1)
    this.patch({ files: res.value, loaded: true, error: null })
  }

  fileInfo(file: string): AudioFile | undefined { return this.state.files.find((f) => f.file === file) }

  /** Where the browser can play a file from. */
  url(file: string): string { return `${projectFileUrl(this.projectId, file)}?v=${this.stamps.get(file) ?? 0}` }

  /** Uploads sound bytes as audio/<name> ("hit.wav"). `overwrite` replaces an existing file. */
  async upload(name: string, bytes: Blob, overwrite: boolean): Promise<Result<AudioUploadResult>> {
    const res = await this.diskChange(() => this.api.uploadAudio(this.projectId, name, bytes, overwrite))
    if (res.ok) {
      const file = res.value.file || audioPath(name)
      this.stamps.set(file, (this.stamps.get(file) ?? 0) + 1)
      const { revision: _revision, ...info } = res.value
      const files = [...this.state.files.filter((f) => f.file !== file), { ...info, file }].sort((a, b) => (a.file < b.file ? -1 : 1))
      this.patch({ files })
    }
    return res
  }

  /** Deletes audio/<name> from disk for good. 409 (still used by the saved sound settings) comes back as an error with `uses`. */
  async remove(file: string): Promise<Result<{ revision: string }>> {
    const res = await this.diskChange(() => this.api.deleteAudio(this.projectId, audioName(file)))
    if (res.ok) this.patch({ files: this.state.files.filter((f) => f.file !== file) })
    return res
  }
}
