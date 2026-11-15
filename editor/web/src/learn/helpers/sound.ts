// Done-test helpers of the "Sound" lesson (lessons/sound.ts). They read data/audio.json, the first map's music and the weapons'
// own sounds. The lesson needs no file of the maker's own: it only chooses among the sounds the template already has.
import type { ProjectDoc } from '../../model/types'
import { mapOf } from '../helpers'

/** Volume of the music tracks the blank template begins with (a missing volume is 100). */
export const START_VOLUME: Record<string, number> = { title: 100, battle: 80, enemy: 100, victory: 100, defeat: 100, levelup: 100 }

/** Standard events that play a different sound than the template's choice, and still a sound the game has. */
export function eventSoundsReplaced(doc: ProjectDoc, start: Record<string, string>): number {
  const have = doc.audio?.sounds ?? {}
  return Object.entries(doc.audio?.use?.sounds ?? {}).filter(([key, id]) => typeof id === 'string' && id !== '' && id in have && id !== start[key]).length
}

/** Music tracks played quieter than the template has them. */
export function tracksQuieter(doc: ProjectDoc): number {
  return Object.entries(doc.audio?.music ?? {}).filter(([id, t]) => (t.volume ?? 100) < (START_VOLUME[id] ?? 100)).length
}

/** True when the map names a track of the game as its own music. */
export function mapHasOwnMusic(doc: ProjectDoc, mapId?: string): boolean {
  const m = mapOf(doc, mapId)?.music
  return typeof m === 'string' && m !== '' && m in (doc.audio?.music ?? {})
}

/** Weapons that play one of the game's sounds instead of the standard one for something they do. */
export function weaponsWithOwnSound(doc: ProjectDoc): number {
  const have = doc.audio?.sounds ?? {}
  return (doc.weapons ?? []).filter((w) => Object.values(w.sounds ?? {}).some((id) => id !== '' && id in have)).length
}
