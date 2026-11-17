// Checks that mirror what the PLAYER's sound loader refuses (engine/player/sound/audio_data.cpp, sound_bank.cpp), so a
// maker sees every one of them in the editor at once before pressing Play (the loader stops at the first). The wording is
// plain; the loader's own message still shows in the Problems list after a save. Names in story commands and scenes are
// checked with the maps' and the campaign's own checks (`audioRefProblem` is what they use).
// A sound FILE that is missing is not refused by the player (that sound stays silent), so it is a `hint`.
import { JINGLE_SLOTS, MAP_MUSIC_KEYS, MAP_MUSIC_LABEL, MUSIC_SLOT_LABEL, MUSIC_SLOTS, OVERRIDE_KEY_LABEL, OVERRIDE_KEYS, SOUND_KEY_LABEL, STANDARD_SOUND_KEYS, UNIT_KEY_LABEL, type StandardSoundKey } from './audioLabels'
import { MAX_VARIANTS, SOUND_EFFECT_SECONDS, filesOfSound, musicOf, soundsOf } from './audio'
import type { AudioFile } from './audioTypes'
import { situationProblems } from './audioSituations'
import { isStoryId } from './storyIds'
import type { ProjectDoc, Section } from './types'

export interface AudioCheck {
  /** Which screen shows it. */
  section: Section
  level: 'error' | 'hint'
  text: string
  /** For maps: which one. */
  mapId?: string
}

const isObject = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v)
const q = (s: string) => `“${s}”`

/** Inside the game folder, forward slashes, .ogg or .wav (the loader's rule). */
export function goodAudioPath(p: unknown): p is string {
  if (typeof p !== 'string' || p.length < 5 || p.startsWith('/') || p.includes('..') || p.includes('\\') || p.includes(':')) return false
  const low = p.toLowerCase()
  return low.endsWith('.ogg') || low.endsWith('.wav')
}

/** Why a name in a story command or a scene cannot be played (null when it is fine). `kind` says what it should be. */
export function audioRefProblem(doc: ProjectDoc, kind: 'music' | 'sound', id: string): string | null {
  if (id === '') return kind === 'music' ? 'pick a music track.' : 'pick a sound.'
  if (!isStoryId(id)) return 'a name may use only letters, digits, _ and - (up to 64).'
  if (!doc.audio) return `the game has no sound yet, so ${q(id)} would never be heard (add it on the Sound screen).`
  const known = kind === 'music' ? id in musicOf(doc) : id in soundsOf(doc)
  return known ? null : `${q(id)} is not ${kind === 'music' ? 'a music track' : 'a sound'} of this game.`
}

const onlyKeys = (o: Record<string, unknown>, keys: readonly string[]): string[] => Object.keys(o).filter((k) => !keys.includes(k))

/** Everything the player would refuse (or warn about) in the sound settings; empty when all is well or the game has none.
 *  `files` = the game's audio folder as listed by the helper (null while it is not known: file checks are then skipped). */
export function audioChecks(doc: ProjectDoc, files: Map<string, AudioFile> | null = null): AudioCheck[] {
  const out: AudioCheck[] = []
  const say = (section: Section, text: string, level: AudioCheck['level'] = 'error', mapId?: string) => out.push({ section, level, text, mapId })
  const a = doc.audio
  if (a) fileChecks(doc, files, say)
  if (a) structureChecks(doc, say)
  overrideChecks(doc, say)
  mapChecks(doc, say)
  for (const p of situationProblems(doc)) say(p.section, p.text, 'error', p.mapId)  // music by situation, movement-type steps, animation sounds (audioSituations.ts)
  return out
}

type Say = (section: Section, text: string, level?: AudioCheck['level'], mapId?: string) => void

function trackChecks(doc: ProjectDoc, say: Say): void {
  const a = doc.audio!
  if (a.music !== undefined && !isObject(a.music)) { say('sound', 'The music list is not a list of tracks.'); return }
  for (const [id, t] of Object.entries(a.music ?? {})) {
    const at = `Music ${q(id)}`
    if (!isStoryId(id)) say('sound', `${at}: a name may use only letters, digits, _ and - (up to 64).`)
    if (!isObject(t)) { say('sound', `${at}: is not a track.`); continue }
    for (const k of onlyKeys(t, ['file', 'loopStart', 'volume', 'loop'])) say('sound', `${at}: has something this game does not know (${q(k)}).`)
    if (!goodAudioPath(t.file)) say('sound', `${at}: pick a .ogg or .wav file from this game’s audio folder.`)
    if (t.loopStart !== undefined && (typeof t.loopStart !== 'number' || t.loopStart < 0 || t.loopStart > 36000)) say('sound', `${at}: “Repeat from” is a number of seconds, 0 or more.`)
    if (t.volume !== undefined && (!Number.isInteger(t.volume) || (t.volume as number) < 0 || (t.volume as number) > 100)) say('sound', `${at}: the volume is a whole number from 0 to 100.`)
    if (t.loop !== undefined && typeof t.loop !== 'boolean') say('sound', `${at}: “Repeats” must be on or off.`)
  }
}

function effectChecks(doc: ProjectDoc, say: Say): void {
  const a = doc.audio!
  if (a.sounds !== undefined && !isObject(a.sounds)) { say('sound', 'The sound list is not a list of sounds.'); return }
  for (const [id, s] of Object.entries(a.sounds ?? {})) {
    const at = `Sound ${q(id)}`
    if (!isStoryId(id)) say('sound', `${at}: a name may use only letters, digits, _ and - (up to 64).`)
    if (!isObject(s)) { say('sound', `${at}: is not a sound.`); continue }
    for (const k of onlyKeys(s, ['file', 'files', 'volume'])) say('sound', `${at}: has something this game does not know (${q(k)}).`)
    if (('file' in s) === ('files' in s)) say('sound', `${at}: needs exactly one file, or a list of variants.`)
    else if ('files' in s) {
      const list = s.files
      if (!Array.isArray(list) || list.length < 1 || list.length > MAX_VARIANTS) say('sound', `${at}: a sound has from 1 to ${MAX_VARIANTS} variants.`)
      else if (!list.every(goodAudioPath)) say('sound', `${at}: every variant must be a .ogg or .wav file from this game’s audio folder.`)
    } else if (!goodAudioPath(s.file)) say('sound', `${at}: pick a .ogg or .wav file from this game’s audio folder.`)
    if (s.volume !== undefined && (!Number.isInteger(s.volume) || (s.volume as number) < 0 || (s.volume as number) > 100)) say('sound', `${at}: the volume is a whole number from 0 to 100.`)
  }
}

function useChecks(doc: ProjectDoc, say: Say): void {
  const use = doc.audio!.use
  if (use === undefined) return
  if (!isObject(use)) { say('sound', 'The standard set is not a list of settings.'); return }
  for (const k of onlyKeys(use, [...MUSIC_SLOTS, 'sounds', 'phaseMusic', 'fewEnemies'])) say('sound', `The standard set has something this game does not know (${q(k)}).`)
  for (const slot of MUSIC_SLOTS) {
    const v = use[slot]
    if (v === undefined || v === '') continue
    const at = `Standard set, ${q(MUSIC_SLOT_LABEL[slot])}`
    if (typeof v !== 'string') { say('sound', `${at}: pick a music track or nothing.`); continue }
    const track = musicOf(doc)[v]
    if (!track) say('sound', `${at}: ${q(v)} is not a music track of this game.`)
    else if (JINGLE_SLOTS.includes(slot) && track.loop !== false) say('sound', `${at}: ${q(v)} repeats, but this plays once. Switch off “Repeats” on that track, or pick another.`)
  }
  if (use.sounds === undefined) return
  if (!isObject(use.sounds)) { say('sound', 'The standard sounds are not a list of settings.'); return }
  for (const [k, v] of Object.entries(use.sounds)) {
    const known = (STANDARD_SOUND_KEYS as readonly string[]).includes(k)
    const at = `Standard set, ${known ? q(SOUND_KEY_LABEL[k as StandardSoundKey]) : q(k)}`
    if (!known) { say('sound', `The standard set has a sound for ${q(k)}, which is not something that happens in this game.`); continue }
    if (typeof v !== 'string') { say('sound', `${at}: pick a sound or nothing.`); continue }
    if (v !== '' && !(v in soundsOf(doc))) say('sound', `${at}: ${q(v)} is not a sound of this game.`)
  }
}

function structureChecks(doc: ProjectDoc, say: Say): void {
  const a = doc.audio!
  for (const k of onlyKeys(a, ['music', 'sounds', 'use'])) say('sound', `The sound settings have something this game does not know (${q(k)}).`)
  trackChecks(doc, say)
  effectChecks(doc, say)
  useChecks(doc, say)
}

/** Missing files (a hint: the game starts without that sound) and effects the loader refuses because of the file itself. */
function fileChecks(doc: ProjectDoc, files: Map<string, AudioFile> | null, say: Say): void {
  if (!files) return
  const a = doc.audio!
  for (const [id, t] of Object.entries(isObject(a.music) ? a.music : {})) {
    if (isObject(t) && goodAudioPath(t.file) && !files.has(t.file)) say('sound', `Music ${q(id)}: the file ${t.file} is not in the game, so this track stays silent.`, 'hint')
  }
  for (const [id, s] of Object.entries(isObject(a.sounds) ? a.sounds : {})) {
    if (!isObject(s)) continue
    for (const f of filesOfSound(s)) {
      if (!goodAudioPath(f)) continue
      const info = files.get(f)
      if (!info) { say('sound', `Sound ${q(id)}: the file ${f} is not in the game, so this sound stays silent.`, 'hint'); continue }
      if (info.error) continue
      if (info.seconds !== undefined && info.seconds > SOUND_EFFECT_SECONDS) say('sound', `Sound ${q(id)}: ${f} is ${Math.round(info.seconds * 10) / 10} seconds long, but a sound effect can be at most ${SOUND_EFFECT_SECONDS} seconds. Use it as music, or shorten it.`)
      if (info.rate !== undefined && (info.rate < 8000 || info.rate > 48000)) say('sound', `Sound ${q(id)}: ${f} has a rate of ${info.rate} Hz; the game plays 8000 to 48000.`)
      if (info.channels !== undefined && info.channels > 2) say('sound', `Sound ${q(id)}: ${f} has ${info.channels} channels; the game plays mono or stereo.`)
    }
  }
}

function overrideChecks(doc: ProjectDoc, say: Say): void {
  const sounds = soundsOf(doc)
  for (const owner of ['weapons', 'units', 'items'] as const) {
    const noun = owner === 'weapons' ? 'Weapon' : owner === 'units' ? 'Unit' : 'Item'
    for (const e of (doc[owner] ?? []) as Array<{ id: string; name?: string; sounds?: unknown }>) {
      if (e.sounds === undefined) continue
      const at = `${e.name || e.id}`
      if (!isObject(e.sounds)) { say(owner, `${at}: its sounds are not a list of settings.`); continue }
      for (const [k, v] of Object.entries(e.sounds)) {
        if (owner === 'units' && k === 'stepInterval') continue  // seconds between steps: checked in audioSituations.ts
        if (!OVERRIDE_KEYS[owner].includes(k)) { say(owner, `${at}: a ${noun.toLowerCase()} cannot have a sound for ${q(k)}.`); continue }
        const label = (owner === 'units' ? UNIT_KEY_LABEL[k] : OVERRIDE_KEY_LABEL[k]) ?? k
        if (typeof v !== 'string') { say(owner, `${at}: ${q(label)}: pick a sound, “Same as the game” or “Silent”.`); continue }
        if (v === '') continue
        if (!doc.audio) say(owner, `${at}: ${q(label)} names ${q(v)}, but the game has no sounds yet (Sound screen).`)
        else if (!(v in sounds)) say(owner, `${at}: ${q(label)} names ${q(v)}, which is not a sound of this game.`)
      }
    }
  }
}

function mapChecks(doc: ProjectDoc, say: Say): void {
  const music = musicOf(doc)
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    const name = doc.game?.maps.find((x) => x.id === mapId)?.name ?? mapId
    for (const key of MAP_MUSIC_KEYS) {
      const v = m[key]
      if (v === undefined) continue
      const at = `Map ${q(name)}, ${q(MAP_MUSIC_LABEL[key])}`
      if (typeof v !== 'string') { say('maps', `${at}: pick a track, “Same as the game” or “No music”.`, 'error', mapId); continue }
      if (v === '') continue
      if (!doc.audio) say('maps', `${at} names ${q(v)}, but the game has no music yet (Sound screen).`, 'error', mapId)
      else if (!(v in music)) say('maps', `${at} names ${q(v)}, which is not a music track of this game.`, 'error', mapId)
    }
  }
}
