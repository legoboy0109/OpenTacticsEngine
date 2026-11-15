// Editing data/audio.json and the sound keys that sit in other files: pure functions on the document (docs/SOUND.md).
// Defaults are written as NO key (a track that repeats, a volume of 100, a loop start of 0), like every other setting.
// Deleting or renaming a track or sound is `audioRefs.ts`: the document store cleans every place that names it.
import { makeId } from './ids'
import type { Audio, AudioUse, MusicTrack, SoundEffect } from './audioTypes'
import { OVERRIDE_KEYS, type MapMusicKey, type MusicSlot, type OverrideOwner } from './audioLabels'
import { patchObj } from './util'
import type { ProjectDoc } from './types'

export const MAX_VARIANTS = 16
export const SOUND_EFFECT_SECONDS = 10

const CANON = ['music', 'sounds', 'use'] as const
const NO_MUSIC: Record<string, MusicTrack> = {}
const NO_SOUNDS: Record<string, SoundEffect> = {}

export const musicOf = (doc: ProjectDoc): Record<string, MusicTrack> => doc.audio?.music ?? NO_MUSIC
export const soundsOf = (doc: ProjectDoc): Record<string, SoundEffect> => doc.audio?.sounds ?? NO_SOUNDS
export const musicIds = (doc: ProjectDoc): string[] => Object.keys(musicOf(doc))
export const soundIds = (doc: ProjectDoc): string[] => Object.keys(soundsOf(doc))
/** The pickers elsewhere (weapons, units, items, maps, scenes) are shown only when there is something to pick. */
export const hasSounds = (doc: ProjectDoc): boolean => soundIds(doc).length > 0
export const hasMusic = (doc: ProjectDoc): boolean => musicIds(doc).length > 0

/** The files a sound effect plays: `files` (variants) or the one `file`. */
export const filesOfSound = (s: SoundEffect): string[] => (Array.isArray(s.files) ? s.files.filter((f): f is string => typeof f === 'string') : typeof s.file === 'string' ? [s.file] : [])

export const newTrackId = (doc: ProjectDoc, name: string): string => makeId(name, musicIds(doc), { fallback: 'track' })
export const newSoundId = (doc: ProjectDoc, name: string): string => makeId(name, soundIds(doc), { fallback: 'sound' })

/** `a` with section `key` set. A section that is new goes where the file format lists it (music, sounds, use). */
function withSection<K extends keyof Audio>(a: Audio, key: K, value: Audio[K]): Audio {
  if (key in a) return { ...a, [key]: value }
  const out: Record<string, unknown> = {}
  for (const k of CANON) {
    if (k === key) out[k] = value
    else if (k in a) out[k] = a[k]
  }
  for (const [k, v] of Object.entries(a)) if (!(k in out)) out[k] = v
  return out as Audio
}

/** Applies `fn` to the audio (an empty one when the game has none: the first edit creates data/audio.json). */
export function withAudio(doc: ProjectDoc, fn: (a: Audio) => Audio): ProjectDoc {
  const next = fn(doc.audio ?? {})
  return next === doc.audio ? doc : { ...doc, audio: next }
}

// --- tracks ---

export function addTrack(doc: ProjectDoc, id: string, file: string): ProjectDoc {
  return withAudio(doc, (a) => withSection(a, 'music', { ...(a.music ?? {}), [id]: { file } }))
}

/** `undefined` removes a key. */
export function patchTrack(doc: ProjectDoc, id: string, patch: Record<string, unknown>): ProjectDoc {
  return withAudio(doc, (a) => (a.music?.[id] ? { ...a, music: { ...a.music, [id]: patchObj(a.music[id], patch) } } : a))
}

export const setTrackFile = (doc: ProjectDoc, id: string, file: string): ProjectDoc => patchTrack(doc, id, { file })
export const setTrackVolume = (doc: ProjectDoc, id: string, volume: number): ProjectDoc => patchTrack(doc, id, { volume: volume === 100 ? undefined : volume })
export const setTrackLoopStart = (doc: ProjectDoc, id: string, seconds: number): ProjectDoc => patchTrack(doc, id, { loopStart: seconds === 0 ? undefined : seconds })
/** "Repeats" on is what a missing key means; only a jingle says `false`. */
export const setTrackRepeats = (doc: ProjectDoc, id: string, repeats: boolean): ProjectDoc => patchTrack(doc, id, { loop: repeats ? undefined : false })
export const tracksRepeats = (t: MusicTrack): boolean => t.loop !== false

/** Removes the track from audio.json. (The places that name it are cleaned by the document store: audioRefs.ts.) */
export function deleteTrack(doc: ProjectDoc, id: string): ProjectDoc {
  return withAudio(doc, (a) => {
    if (!a.music || !(id in a.music)) return a
    const { [id]: _gone, ...rest } = a.music
    return { ...a, music: rest }
  })
}

// --- sound effects ---

export function addSound(doc: ProjectDoc, id: string, file: string): ProjectDoc {
  return withAudio(doc, (a) => withSection(a, 'sounds', { ...(a.sounds ?? {}), [id]: { file } }))
}

export function patchSound(doc: ProjectDoc, id: string, patch: Record<string, unknown>): ProjectDoc {
  return withAudio(doc, (a) => (a.sounds?.[id] ? { ...a, sounds: { ...a.sounds, [id]: patchObj(a.sounds[id], patch) } } : a))
}

export const setSoundVolume = (doc: ProjectDoc, id: string, volume: number): ProjectDoc => patchSound(doc, id, { volume: volume === 100 ? undefined : volume })

/** Writes the variants: one file is `file`, several are `files` (the file format never allows both). */
export function setSoundFiles(doc: ProjectDoc, id: string, files: string[]): ProjectDoc {
  const list = files.slice(0, MAX_VARIANTS)
  if (list.length === 0) return doc
  return withAudio(doc, (a) => {
    const cur = a.sounds?.[id]
    if (!cur) return a
    // Key order: the file key stays where it was (first, when the entry had none).
    const out: Record<string, unknown> = {}
    const put = () => { out[list.length === 1 ? 'file' : 'files'] = list.length === 1 ? list[0] : list }
    if (!('file' in cur) && !('files' in cur)) put()
    for (const [k, v] of Object.entries(cur)) {
      if (k === 'file' || k === 'files') { if (!(('file' in out) || ('files' in out))) put() }
      else out[k] = v
    }
    return { ...a, sounds: { ...a.sounds, [id]: out as SoundEffect } }
  })
}

export function deleteSound(doc: ProjectDoc, id: string): ProjectDoc {
  return withAudio(doc, (a) => {
    if (!a.sounds || !(id in a.sounds)) return a
    const { [id]: _gone, ...rest } = a.sounds
    return { ...a, sounds: rest }
  })
}

// --- the game's standard set (audio.use) ---

/** `use` with `patch` applied; an emptied `use` is left out of the file. */
function patchUse(a: Audio, patch: Record<string, unknown>): Audio {
  const use = patchObj((a.use ?? {}) as AudioUse, patch)
  if (Object.keys(use).length === 0) {
    if (!a.use) return a
    const { use: _gone, ...rest } = a
    return rest
  }
  return withSection(a, 'use', use)
}

/** A music slot: a track id, or undefined for "Nothing" (the key is left out). */
export const setUseMusic = (doc: ProjectDoc, slot: MusicSlot, id: string | undefined): ProjectDoc =>
  withAudio(doc, (a) => (a.use?.[slot] === id || (id === undefined && a.use?.[slot] === undefined) ? a : patchUse(a, { [slot]: id })))

/** An event's standard sound: a sound id, or undefined for "Nothing". */
export function setUseSound(doc: ProjectDoc, key: string, id: string | undefined): ProjectDoc {
  return withAudio(doc, (a) => {
    const block = { ...(a.use?.sounds ?? {}) }
    if (block[key] === id || (id === undefined && !(key in block))) return a
    if (id === undefined) delete block[key]
    else block[key] = id
    return patchUse(a, { sounds: Object.keys(block).length ? block : undefined })
  })
}

// --- overrides in weapons, units and items, and music keys on maps ---

/** One event of an entry's `sounds` block: a sound id, "" (silence on purpose) or undefined ("same as the game"). An emptied block is left out. */
export function setOverride(doc: ProjectDoc, owner: OverrideOwner, id: string, key: string, value: string | undefined): ProjectDoc {
  const list = doc[owner] as Array<{ id: string; sounds?: Record<string, string> }> | null
  const entry = list?.find((x) => x.id === id)
  if (!list || !entry || !OVERRIDE_KEYS[owner].includes(key)) return doc
  const block = { ...(entry.sounds ?? {}) }
  if (block[key] === value || (value === undefined && !(key in block))) return doc
  if (value === undefined) delete block[key]
  else block[key] = value
  const next = patchObj(entry, { sounds: Object.keys(block).length ? block : undefined })
  return { ...doc, [owner]: list.map((x) => (x === entry ? next : x)) }
}

export const overrideOf = (doc: ProjectDoc, owner: OverrideOwner, id: string, key: string): string | undefined =>
  (doc[owner] as Array<{ id: string; sounds?: Record<string, string> }> | null)?.find((x) => x.id === id)?.sounds?.[key]

/** A map's `music`, `playerTurnMusic` or `enemyTurnMusic`: a track id, "" (no music on this map) or undefined ("same as the game"). */
export function setMapMusic(doc: ProjectDoc, mapId: string, key: MapMusicKey, value: string | undefined): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m || m[key] === value) return doc
  return { ...doc, maps: { ...doc.maps, [mapId]: patchObj(m, { [key]: value }) } }
}

/** A sound file is going: the tracks that play it are deleted and the sounds that have it as a variant lose that variant (a
 *  sound whose only file it is is deleted). The places that name a deleted track or sound are cleaned by the store. */
export function dropAudioFile(doc: ProjectDoc, file: string): ProjectDoc {
  let out = doc
  for (const [id, t] of Object.entries(musicOf(doc))) if (t?.file === file) out = deleteTrack(out, id)
  for (const [id, s] of Object.entries(soundsOf(doc))) {
    const files = s && typeof s === 'object' ? filesOfSound(s) : []
    if (!files.includes(file)) continue
    const rest = files.filter((f) => f !== file)
    out = rest.length ? setSoundFiles(out, id, rest) : deleteSound(out, id)
  }
  return out
}

/** Takes all sound out of the game: audio.json goes (the files in audio/ stay). Everything that named a track or sound is cleaned by the store. */
export const removeAllAudio = (doc: ProjectDoc): ProjectDoc => (doc.audio ? { ...doc, audio: null } : doc)
