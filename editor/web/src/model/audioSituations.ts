// Music by situation and steps by movement type (docs/SOUND.md "Music by situation", "Step sounds by movement type"), and the
// sounds a battle animation plays at its frames (docs/BATTLE_SCENE.md "Sounds"): the model's edits and the checks, mirroring what
// the player's loader refuses (engine/player/sound/audio_data.cpp, engine/core/data/loader_battle_art.cpp).
import { ANIM_KEYS, battleOf } from './battleArt'
import { musicOf, soundsOf, withAudio } from './audio'
import { patchObj } from './util'
import type { AudioUse } from './audioTypes'
import type { BattleAnimDef, ProjectDoc } from './types'

export const FEW_COUNT_MAX = 999
export const STEP_INTERVAL_MIN = 0.02
export const STEP_INTERVAL_MAX = 2

const isObject = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v)
const q = (s: string) => `“${s}”`

/** The few-enemies setting: the track and the number of hostile units at or under which it plays. */
export interface FewEnemies { music?: string; count?: number }

// --- the game's standard set (`use`) ---

function patchUse(doc: ProjectDoc, patch: Partial<AudioUse>): ProjectDoc {
  return withAudio(doc, (a) => {
    const use = patchObj({ ...(a.use ?? {}) }, patch as Record<string, unknown>) as AudioUse
    return { ...a, use: Object.keys(use).length ? use : undefined } as typeof a
  })
}

/** A side's phase music in the standard set: a track id, "" (none) or undefined (the key is left out). */
export function setUsePhaseMusic(doc: ProjectDoc, side: string, value: string | undefined): ProjectDoc {
  const block = { ...(doc.audio?.use?.phaseMusic ?? {}) }
  if (block[side] === value || (value === undefined && !(side in block))) return doc
  if (value === undefined) delete block[side]
  else block[side] = value
  return patchUse(doc, { phaseMusic: Object.keys(block).length ? block : undefined })
}

export const setUseFew = (doc: ProjectDoc, few: FewEnemies | undefined): ProjectDoc => patchUse(doc, { fewEnemies: few })

// --- a map's own ---

export function setMapPhaseMusic(doc: ProjectDoc, mapId: string, side: string, value: string | undefined): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m) return doc
  const block = { ...(m.phaseMusic ?? {}) }
  if (block[side] === value || (value === undefined && !(side in block))) return doc
  if (value === undefined) delete block[side]
  else block[side] = value
  return { ...doc, maps: { ...doc.maps, [mapId]: patchObj(m, { phaseMusic: Object.keys(block).length ? block : undefined }) } }
}

export function setMapFew(doc: ProjectDoc, mapId: string, few: FewEnemies | undefined): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m) return doc
  return { ...doc, maps: { ...doc.maps, [mapId]: patchObj(m, { fewEnemies: few }) } }
}

// --- a movement type's step sound and interval ---

/** `sounds.step` of a movement type: a sound id, "" (silent steps) or undefined ("same as the game"). */
export function setMoveTypeStep(doc: ProjectDoc, id: string, value: string | undefined): ProjectDoc {
  return patchMoveSounds(doc, id, { step: value })
}

/** `sounds.stepInterval` of a movement type, in seconds (undefined = the usual 0.1). */
export function setMoveTypeInterval(doc: ProjectDoc, id: string, seconds: number | undefined): ProjectDoc {
  return patchMoveSounds(doc, id, { stepInterval: seconds })
}

function patchMoveSounds(doc: ProjectDoc, id: string, patch: Record<string, unknown>): ProjectDoc {
  const game = doc.game
  const mt = game?.moveTypes.find((m) => m.id === id)
  if (!game || !mt) return doc
  const sounds = patchObj({ ...((mt.sounds as Record<string, unknown> | undefined) ?? {}) }, patch)
  const next = patchObj(mt, { sounds: Object.keys(sounds).length ? sounds : undefined })
  return { ...doc, game: { ...game, moveTypes: game.moveTypes.map((m) => (m === mt ? next : m)) } }
}

export const moveTypeStep = (doc: ProjectDoc, id: string): { step?: string; interval?: number } => {
  const s = doc.game?.moveTypes.find((m) => m.id === id)?.sounds as Record<string, unknown> | undefined
  return { step: typeof s?.step === 'string' ? s.step : undefined, interval: typeof s?.stepInterval === 'number' ? s.stepInterval : undefined }
}

// --- a battle animation's sounds at frames ---

/** The sound played when frame `frame` of an animation begins: a sound id, or undefined to take it away. */
export function setAnimSound(doc: ProjectDoc, setId: string, anim: string, frame: number, value: string | undefined): ProjectDoc {
  const set = battleOf(doc).sets?.[setId]
  const a = set?.animations[anim]
  if (!set || !a) return doc
  const block = { ...(a.sounds ?? {}) }
  const key = String(frame)
  if (block[key] === value || (value === undefined && !(key in block))) return doc
  if (value === undefined) delete block[key]
  else block[key] = value
  const next = patchObj(a, { sounds: Object.keys(block).length ? block : undefined }) as BattleAnimDef
  const b = doc.graphics?.battle
  if (!b || !doc.graphics) return doc
  return { ...doc, graphics: { ...doc.graphics, battle: { ...b, sets: { ...(b.sets ?? {}), [setId]: { ...set, animations: { ...set.animations, [anim]: next } } } } } }
}

/** The frames of an animation that have a sound, in frame order. */
export const animSounds = (a: BattleAnimDef): Array<[number, string]> =>
  Object.entries(a.sounds ?? {}).map(([k, v]): [number, string] => [Number(k), String(v)]).sort((x, y) => x[0] - y[0])

// --- checks ---

type Say = (section: 'sound' | 'maps' | 'art' | 'game' | 'units', text: string, mapId?: string) => void

const music = (doc: ProjectDoc, at: string, v: unknown, say: Say, section: 'sound' | 'maps', mapId?: string, allowEmpty = true): void => {
  if (typeof v !== 'string') { say(section, `${at}: pick a music track${allowEmpty ? ' or nothing' : ''}.`, mapId); return }
  if (v === '') return
  if (!(v in musicOf(doc))) say(section, `${at}: ${q(v)} is not a music track of this game.`, mapId)
}

const fewChecks = (doc: ProjectDoc, at: string, f: unknown, say: Say, section: 'sound' | 'maps', mapId?: string, mapAllowsEmpty = false): void => {
  if (!isObject(f)) { say(section, `${at}: few enemies needs a track and a number.`, mapId); return }
  if (!mapAllowsEmpty && (typeof f.music !== 'string' || f.music === '')) say(section, `${at}: pick the track that plays when few enemies are left.`, mapId)
  else music(doc, at, f.music, say, section, mapId, mapAllowsEmpty)
  if (!Number.isInteger(f.count) || (f.count as number) < 1 || (f.count as number) > FEW_COUNT_MAX) say(section, `${at}: the number of hostile units is a whole number from 1 to ${FEW_COUNT_MAX}.`, mapId)
}

/** Everything wrong with the situation settings (standard set, maps, movement types, battle animations). */
export function situationProblems(doc: ProjectDoc): Array<{ section: 'sound' | 'maps' | 'art' | 'game' | 'units'; text: string; mapId?: string }> {
  const out: Array<{ section: 'sound' | 'maps' | 'art' | 'game' | 'units'; text: string; mapId?: string }> = []
  const say: Say = (section, text, mapId) => out.push({ section, text, mapId })
  const use = doc.audio?.use
  if (isObject(use)) {
    if (use.phaseMusic !== undefined) {
      if (!isObject(use.phaseMusic)) say('sound', 'Standard set, music by side is not a list of settings.')
      else for (const [side, v] of Object.entries(use.phaseMusic)) music(doc, `Standard set, music on ${q(side)}’s turn`, v, say, 'sound')
    }
    if (use.fewEnemies !== undefined) fewChecks(doc, 'Standard set, few enemies left', use.fewEnemies, say, 'sound')
  }
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    const name = doc.game?.maps.find((x) => x.id === mapId)?.name ?? mapId
    if (m.phaseMusic !== undefined) {
      if (!isObject(m.phaseMusic)) say('maps', `Map ${q(name)}: music by side is not a list of settings.`, mapId)
      else {
        for (const [side, v] of Object.entries(m.phaseMusic)) {
          if (!(m.sides ?? []).some((s) => s.id === side)) say('maps', `Map ${q(name)}: music on ${q(side)}’s turn names a side this map does not have.`, mapId)
          music(doc, `Map ${q(name)}, music on ${q(side)}’s turn`, v, say, 'maps', mapId)
        }
      }
    }
    if (m.fewEnemies !== undefined) fewChecks(doc, `Map ${q(name)}, few enemies left`, m.fewEnemies, say, 'maps', mapId, true)
  }
  const sounds = soundsOf(doc)
  for (const mt of doc.game?.moveTypes ?? []) {
    const s = mt.sounds as Record<string, unknown> | undefined
    if (s === undefined) continue
    const at = `Movement type ${q(mt.name || mt.id)}`
    if (!isObject(s)) { say('game', `${at}: its sounds are not a list of settings.`); continue }
    for (const [k, v] of Object.entries(s)) {
      if (k === 'stepInterval') {
        if (typeof v !== 'number' || v < STEP_INTERVAL_MIN || v > STEP_INTERVAL_MAX) say('game', `${at}: the time between steps is seconds, from ${STEP_INTERVAL_MIN} to ${STEP_INTERVAL_MAX}.`)
      } else if (k !== 'step') say('game', `${at}: a movement type can only have a step sound (not ${q(k)}).`)
      else if (typeof v !== 'string') say('game', `${at}: pick a sound, “Same as the game” or “Silent”.`)
      else if (v !== '' && !(v in sounds)) say('game', `${at}: the step sound ${q(v)} is not a sound of this game.`)
    }
  }
  for (const u of doc.units ?? []) {
    const v = (u.sounds as Record<string, unknown> | undefined)?.stepInterval
    if (v !== undefined && (typeof v !== 'number' || v < STEP_INTERVAL_MIN || v > STEP_INTERVAL_MAX)) say('units', `Unit ${q(u.name || u.id)}: the time between steps is seconds, from ${STEP_INTERVAL_MIN} to ${STEP_INTERVAL_MAX}.`)
  }
  for (const [setId, set] of Object.entries(battleOf(doc).sets ?? {})) {
    for (const key of ANIM_KEYS) {
      const a = set.animations[key]
      if (!a || a.sounds === undefined) continue
      const at = `Battle set ${q(setId)}, ${q(key)}`
      if (!isObject(a.sounds)) { say('art', `${at}: its sounds are not a list of settings.`); continue }
      for (const [frame, id] of Object.entries(a.sounds)) {
        const n = Number(frame)
        if (!Number.isInteger(n) || n < 0 || n >= a.frames.length) say('art', `${at}: a sound on frame ${frame}, but the animation has frames 0 to ${a.frames.length - 1}.`)
        if (typeof id !== 'string' || id === '' || !(id in sounds)) say('art', `${at}: frame ${frame} names ${q(String(id))}, which is not a sound of this game${doc.audio ? '' : ' (it has no sounds yet)'}.`)
      }
    }
  }
  return out
}
