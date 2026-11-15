// File-level helpers for sound files: names on disk, the first-bytes check, limits. Pure (no browser APIs).
import { slugify, uniqueId } from './ids'
import { filesOfSound } from './audio'
import type { AudioFile } from './audioTypes'
import type { ProjectDoc } from './types'

export const MAX_AUDIO_BYTES = 8 * 1024 * 1024

export type AudioExt = 'wav' | 'ogg'

/** "audio/hit.wav" -> "hit.wav". */
export const audioName = (file: string): string => file.replace(/^audio\//, '')
export const audioPath = (name: string): string => `audio/${name}`
export const audioExt = (file: string): AudioExt | null => (/\.wav$/i.test(file) ? 'wav' : /\.ogg$/i.test(file) ? 'ogg' : null)

const stemOf = (originalName: string): string => originalName.replace(/^.*[\\/]/, '').replace(/\.[A-Za-z0-9]{1,5}$/, '')

/** The name people see for a new track or sound: the original file name without its extension. */
export const displayStem = (originalName: string): string => stemOf(originalName).trim()

/** Name on disk for an uploaded file ("hit.wav"): slug of the original name, unique among `existing` file paths. */
export function audioNameFor(originalName: string, ext: AudioExt, existing: Iterable<string>): string {
  const taken = [...existing].map((f) => audioName(f).replace(/\.(wav|ogg)$/, ''))
  // Two files with the same stem and different extensions are allowed on disk, but one name per stem keeps things clear.
  return `${uniqueId(slugify(stemOf(originalName), { sep: '_', fallback: 'sound', max: 40 }), taken, '_')}.${ext}`
}

/** What the first bytes say a file is (the helper checks the same thing): a WAV starts "RIFF....WAVE", an Ogg file "OggS". */
export function sniffAudio(bytes: Uint8Array): AudioExt | null {
  const ascii = (at: number, s: string) => bytes.length >= at + s.length && [...s].every((c, i) => bytes[at + i] === c.charCodeAt(0))
  if (ascii(0, 'RIFF') && ascii(8, 'WAVE')) return 'wav'
  if (ascii(0, 'OggS')) return 'ogg'
  return null
}

/** Files in audio/ that no track or sound of the document uses. */
export function unusedAudioFiles(doc: ProjectDoc, files: Array<Pick<AudioFile, 'file'>>): string[] {
  const used = new Set<string>()
  for (const t of Object.values(doc.audio?.music ?? {})) if (t && typeof t.file === 'string') used.add(t.file)
  for (const s of Object.values(doc.audio?.sounds ?? {})) if (s && typeof s === 'object') for (const f of filesOfSound(s)) used.add(f)
  return files.map((f) => f.file).filter((f) => !used.has(f))
}

/** Which tracks and sounds name `file` (for the confirm before a file is deleted). */
export function audioFileUsers(doc: ProjectDoc, file: string): Array<{ kind: 'music' | 'sound'; id: string }> {
  const out: Array<{ kind: 'music' | 'sound'; id: string }> = []
  for (const [id, t] of Object.entries(doc.audio?.music ?? {})) if (t?.file === file) out.push({ kind: 'music', id })
  for (const [id, s] of Object.entries(doc.audio?.sounds ?? {})) if (s && typeof s === 'object' && filesOfSound(s).includes(file)) out.push({ kind: 'sound', id })
  return out
}

/** "2.5 s", "1:05", or "" when the length is unknown. */
export function lengthText(seconds: number | undefined): string {
  if (seconds === undefined || !Number.isFinite(seconds)) return ''
  if (seconds < 60) return `${Math.round(seconds * 10) / 10} s`
  const m = Math.floor(seconds / 60)
  const s = Math.round(seconds - m * 60)
  return `${m}:${String(s).padStart(2, '0')}`
}
