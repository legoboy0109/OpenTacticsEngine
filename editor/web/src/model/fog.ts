// Fog of war settings (docs/ECONOMY.md "Fog of war"): the map's `fog`, a unit type's `vision`, and `visionBonus` / `hides`
// on terrain and structures. Pure edits. Defaults are never written: a key is present only when it differs.
import { updateStructure } from './structures'
import { updateTerrain } from './terrain'
import { updateUnit } from './units'
import type { ProjectDoc } from './types'

/** Sight of a unit type that sets none (the loader's default, `UnitTypeDef::vision`). */
export const VISION_DEFAULT = 2
export const VISION_MAX = 20
export const BONUS_MAX = 10
/** The loader refuses fog on a map with more sides (an event's "seen by" has one bit per team). */
export const FOG_MAX_SIDES = 32

export const visionOf = (u: { vision?: number }): number => u.vision ?? VISION_DEFAULT

/** Whether a map has fog on. */
export const mapHasFog = (doc: ProjectDoc, mapId: string): boolean => doc.maps[mapId]?.fog === true


/** Does a chapter of the campaign play on a map with fog (the "play the story" choice). */
export const campaignMapHasFog = (doc: ProjectDoc): boolean => (doc.campaign?.chapters ?? []).some((c) => doc.maps[c.map]?.fog === true)

/** Why fog cannot be switched on for this map, or null. */
export function fogBlock(doc: ProjectDoc, mapId: string): string | null {
  const n = doc.maps[mapId]?.sides?.length ?? 0
  return n > FOG_MAX_SIDES ? `This map has ${n} sides. Fog of war works with at most ${FOG_MAX_SIDES}.` : null
}

/** On writes `true`, off removes the key (the same as `false` for the game). */
export function setFog(doc: ProjectDoc, mapId: string, on: boolean): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m || (m.fog === true) === on) return doc
  const next = { ...m }
  if (on) next.fog = true
  else delete next.fog
  return { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

const whole = (n: number, max: number) => Math.min(max, Math.max(0, Math.round(n)))

/** A unit type's sight; the default (or undefined) removes the key. */
export function setVision(doc: ProjectDoc, id: string, n: number | undefined): ProjectDoc {
  const v = n === undefined ? undefined : whole(n, VISION_MAX)
  return updateUnit(doc, id, { vision: v === undefined || v === VISION_DEFAULT ? undefined : v })
}

export type Ground = 'terrain' | 'structure'

/** Extra sight for a unit standing on a terrain / structure kind; 0 removes the key. */
export function setVisionBonus(doc: ProjectDoc, kind: Ground, id: string, n: number | undefined): ProjectDoc {
  const v = n === undefined ? undefined : whole(n, BONUS_MAX)
  const patch = { visionBonus: v ? v : undefined }
  return kind === 'terrain' ? updateTerrain(doc, id, patch) : updateStructure(doc, id, patch)
}

export function setHides(doc: ProjectDoc, kind: Ground, id: string, on: boolean): ProjectDoc {
  const patch = { hides: on ? true : undefined }
  return kind === 'terrain' ? updateTerrain(doc, id, patch) : updateStructure(doc, id, patch)
}
