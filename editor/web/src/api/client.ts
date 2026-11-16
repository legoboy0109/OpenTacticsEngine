// The only place that calls fetch. Every endpoint of docs/EDITOR_API.md is
// one typed function returning a Result (never throws).
import type { AudioFile } from '../model/audioTypes'
import type { ProjectDoc, Problem, ServerProject } from '../model/types'

export interface ApiError {
  ok: false
  /** HTTP status, or 0 when the helper could not be reached. */
  status: number
  error: string
  /** Present on 409: the revision now on disk. */
  revision?: string
  /** Present on a 409 when deleting an image that the saved game still uses: the image entries (sheet ids). */
  usedBy?: string[]
  /** Present on a 409 when deleting a sound file that the saved sound settings still use: the tracks and sounds. */
  uses?: Array<{ kind: string; id: string }>
}
export type Result<T> = { ok: true; value: T } | ApiError

export interface ProjectInfo { id: string; name: string; description: string; author: string; maps: number; ok: boolean; error: string; /** A demo that comes with the editor: it cannot be deleted. */ protected?: boolean }
/** `grid` = the template's grid (game.json): { type: 'square' } or { type: 'hex', layout: 'pointy' | 'flat' }. */
export interface TemplateInfo { id: string; name: string; description: string; grid?: { type: string; layout?: string } }
export interface FontInfo { id: string; name: string; size: number; smooth: boolean }
/** `hexViews` = the kinds of view a hexagon game may use with this build (absent on older helpers: top-down only). */
export interface ApiInfo { engine: string; apiVersion: number; formatVersion: number; hexViews?: string[] }
export interface FileInfo { path: string; size: number }
/** `campaign: false` removes data/campaign.json, `audio: false` removes data/audio.json (null / absent leave them alone). */
export interface PutBody extends Partial<Omit<ProjectDoc, 'campaign' | 'audio' | 'supports' | 'ranks' | 'caps'>> {
  baseRevision: string
  campaign?: ProjectDoc['campaign'] | false
  audio?: ProjectDoc['audio'] | false
  /** `false` removes data/supports.json; an empty list of characters / skills / abilities removes its file. */
  supports?: ProjectDoc['supports'] | false
  /** Weapon ranks and stat caps: the file's object (even an empty ladder: the switch stays on), `false` removes the file. */
  ranks?: ProjectDoc['ranks'] | false
  caps?: ProjectDoc['caps'] | false
}
export interface PutResult { revision: string; problems: Problem[] }
/** One PNG in the game's art/ folder (width/height 0 + error when it cannot be read). */
export interface ArtFile { file: string; width: number; height: number; size: number; error?: string }
/** One model file in the game's models/ folder; `usedBy` = the saved graphics entries that name it. */
export interface ModelFile { file: string; size: number; usedBy: Array<{ style: string; kind: string; id: string }> }
/** The helper's answer to a sound upload: the file's facts and the new revision. */
export type AudioUploadResult = AudioFile & { revision: string }
export interface UploadResult { file: string; width: number; height: number; size: number; revision: string }

async function request<T>(method: string, path: string, body?: unknown, keepalive = false): Promise<Result<T>> {
  let res: Response
  try {
    const raw = body instanceof Blob
    res = await fetch(path, {
      method,
      headers: body === undefined || raw ? undefined : { 'Content-Type': 'application/json' },
      body: body === undefined ? undefined : raw ? body : JSON.stringify(body),
      keepalive,
    })
  } catch (e) {
    return { ok: false, status: 0, error: `Could not reach the editor helper (${e instanceof Error ? e.message : String(e)}).` }
  }
  let data: unknown = null
  try { data = await res.json() } catch { /* non-JSON error page */ }
  if (res.ok) return { ok: true, value: data as T }
  const obj = (data ?? {}) as { error?: string; revision?: string; usedBy?: unknown[] }
  // Images name the sheets that use them (strings); sound files name tracks and sounds ({ kind, id }).
  const names = Array.isArray(obj.usedBy) && obj.usedBy.every((u) => typeof u === 'string') ? (obj.usedBy as string[]) : undefined
  const uses = Array.isArray(obj.usedBy) && obj.usedBy.every((u) => typeof u === 'object' && u !== null) ? (obj.usedBy as Array<{ kind: string; id: string }>) : undefined
  return { ok: false, status: res.status, error: obj.error ?? `Request failed (${res.status}).`, revision: obj.revision, usedBy: names, uses }
}

const enc = encodeURIComponent
const unwrap = async <T, K extends string>(p: Promise<Result<Record<K, T>>>, key: K): Promise<Result<T>> => {
  const r = await p
  return r.ok ? { ok: true, value: r.value[key] } : r
}

export interface Api {
  info(): Promise<Result<ApiInfo>>
  listFiles(id: string): Promise<Result<FileInfo[]>>
  listProjects(): Promise<Result<ProjectInfo[]>>
  listTemplates(): Promise<Result<TemplateInfo[]>>
  listFonts(): Promise<Result<FontInfo[]>>
  createProject(body: { id: string; name: string; template: string }): Promise<Result<{ id: string }>>
  /** Removes the game's folder for good. The helper refuses the demos and a request that does not repeat the id. */
  deleteProject(id: string): Promise<Result<{ deleted: string }>>
  getProject(id: string): Promise<Result<ServerProject>>
  putProject(id: string, body: PutBody, opts?: { keepalive?: boolean }): Promise<Result<PutResult>>
  getProblems(id: string): Promise<Result<Problem[]>>
  listArt(id: string): Promise<Result<ArtFile[]>>
  /** `name` is the file name without folder and extension ("tiles" -> art/tiles.png). */
  uploadArt(id: string, name: string, png: Blob, overwrite: boolean): Promise<Result<UploadResult>>
  deleteArt(id: string, name: string): Promise<Result<{ revision: string }>>
  listAudio(id: string): Promise<Result<AudioFile[]>>
  /** `name` is the file name with its extension ("hit.wav" -> audio/hit.wav). */
  uploadAudio(id: string, name: string, bytes: Blob, overwrite: boolean): Promise<Result<AudioUploadResult>>
  deleteAudio(id: string, name: string): Promise<Result<{ revision: string }>>
  listModels(id: string): Promise<Result<ModelFile[]>>
  /** `name` is the file name without folder and extension ("knight" -> models/knight.glb). */
  uploadModel(id: string, name: string, glb: Blob, overwrite: boolean): Promise<Result<{ file: string; size: number; revision: string }>>
}

/** URL of one raw project file (the Play panel feeds these to the embedded player). */
export const projectFileUrl = (id: string, path: string): string => `/api/projects/${enc(id)}/file/${path.split('/').map(enc).join('/')}`

/** The tutorial's remembered state (GET/PUT /api/learn); the shape is owned by learn/state.ts. */
export const learnApi = {
  get: (): Promise<Result<unknown>> => request('GET', '/api/learn'),
  put: (state: unknown): Promise<Result<unknown>> => request('PUT', '/api/learn', state),
}

export const api: Api = {
  info: () => request('GET', '/api/info'),
  listFiles: (id) => unwrap(request<{ files: FileInfo[] }>('GET', `/api/projects/${enc(id)}/files`), 'files'),
  listProjects: () => unwrap(request<{ projects: ProjectInfo[] }>('GET', '/api/projects'), 'projects'),
  listTemplates: () => unwrap(request<{ templates: TemplateInfo[] }>('GET', '/api/templates'), 'templates'),
  listFonts: () => unwrap(request<{ fonts: FontInfo[] }>('GET', '/api/fonts'), 'fonts'),
  createProject: (body) => request('POST', '/api/projects', body),
  deleteProject: (id) => request('DELETE', `/api/projects/${enc(id)}?confirm=${enc(id)}`),
  getProject: (id) => request('GET', `/api/projects/${enc(id)}`),
  putProject: (id, body, opts) => request('PUT', `/api/projects/${enc(id)}`, body, opts?.keepalive),
  getProblems: (id) => unwrap(request<{ problems: Problem[] }>('GET', `/api/projects/${enc(id)}/problems`), 'problems'),
  listArt: (id) => unwrap(request<{ art: ArtFile[] }>('GET', `/api/projects/${enc(id)}/art`), 'art'),
  uploadArt: (id, name, png, overwrite) => request('PUT', `/api/projects/${enc(id)}/art/${enc(name)}.png${overwrite ? '?overwrite=1' : ''}`, png),
  deleteArt: (id, name) => request('DELETE', `/api/projects/${enc(id)}/art/${enc(name)}.png`),
  listAudio: (id) => unwrap(request<{ audio: AudioFile[] }>('GET', `/api/projects/${enc(id)}/audio`), 'audio'),
  uploadAudio: (id, name, bytes, overwrite) => request('PUT', `/api/projects/${enc(id)}/audio/${enc(name)}${overwrite ? '?overwrite=1' : ''}`, bytes),
  deleteAudio: (id, name) => request('DELETE', `/api/projects/${enc(id)}/audio/${enc(name)}`),
  listModels: (id) => unwrap(request<{ models: ModelFile[] }>('GET', `/api/projects/${enc(id)}/models`), 'models'),
  uploadModel: (id, name, glb, overwrite) => request('PUT', `/api/projects/${enc(id)}/models/${enc(name)}.glb${overwrite ? '?overwrite=1' : ''}`, glb),
}
