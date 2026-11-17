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
  else { delete next.fog; delete next.fogVision; delete next.aiSeesAll }  // the loader refuses them on a map without fog
  return { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

const whole = (n: number, max: number) => Math.min(max, Math.max(0, Math.round(n)))

/** A unit type's sight; the default (or undefined) removes the key. */
export function setVision(doc: ProjectDoc, id: string, n: number | undefined): ProjectDoc {
  const v = n === undefined ? undefined : whole(n, VISION_MAX)
  return updateUnit(doc, id, { vision: v === undefined || v === VISION_DEFAULT ? undefined : v })
}

export type Ground = 'terrain' | 'structure'

// --- extra sight: a plain number (every unit) or { amount, units } (those unit types and labels only) ---
const isLimit = (b: unknown): b is { amount?: unknown; units?: unknown } => typeof b === 'object' && b !== null && !Array.isArray(b)

/** The amount of extra sight a kind gives (0 when it gives none). */
export const bonusAmount = (g: { visionBonus?: unknown }): number => {
  const b = g.visionBonus
  const n = isLimit(b) ? b.amount : b
  return typeof n === 'number' ? n : 0
}
/** The unit type ids / labels the extra sight is limited to; undefined = every unit (the plain form). */
export const bonusUnits = (g: { visionBonus?: unknown }): string[] | undefined => {
  const b = g.visionBonus
  if (!isLimit(b)) return undefined
  return Array.isArray(b.units) ? b.units.filter((x): x is string => typeof x === 'string') : []
}
/** What a unit type gets from the square: the amount, or 0 when the bonus is limited to other unit types. */
export function bonusFor(g: { visionBonus?: unknown }, type: { id: string; tags?: string[] }): number {
  const only = bonusUnits(g)
  if (only === undefined) return bonusAmount(g)
  return only.some((k) => k === type.id || (type.tags ?? []).includes(k)) ? bonusAmount(g) : 0
}

const patchGround = (doc: ProjectDoc, kind: Ground, id: string, patch: { visionBonus: number | { amount: number; units: string[] } | undefined }): ProjectDoc =>
  kind === 'terrain' ? updateTerrain(doc, id, patch) : updateStructure(doc, id, patch)
const groundOf = (doc: ProjectDoc, kind: Ground, id: string) => (kind === 'terrain' ? doc.terrain : doc.structures)?.find((x) => x.id === id)

/** Extra sight for a unit standing on a terrain / structure kind; 0 removes the key. Keeps the unit types it is limited to. */
export function setVisionBonus(doc: ProjectDoc, kind: Ground, id: string, n: number | undefined): ProjectDoc {
  const v = n === undefined ? 0 : whole(n, BONUS_MAX)
  const units = bonusUnits(groundOf(doc, kind, id) ?? {})
  // A limited bonus keeps its list at 0 too (the maker may be about to type another number); a plain 0 removes the key.
  return patchGround(doc, kind, id, { visionBonus: units !== undefined ? { amount: v, units } : v ? v : undefined })
}

/** Limits the extra sight to these unit types and labels; undefined = every unit (writes the plain number again). */
export function setVisionUnits(doc: ProjectDoc, kind: Ground, id: string, units: string[] | undefined): ProjectDoc {
  const amount = whole(bonusAmount(groundOf(doc, kind, id) ?? {}), BONUS_MAX)
  return patchGround(doc, kind, id, { visionBonus: units === undefined ? (amount ? amount : undefined) : { amount, units } })
}

export function setHides(doc: ProjectDoc, kind: Ground, id: string, on: boolean): ProjectDoc {
  const patch = { hides: on ? true : undefined }
  return kind === 'terrain' ? updateTerrain(doc, id, patch) : updateStructure(doc, id, patch)
}

/** The map's extra vision (-10..10, added to every unit); 0 removes the key. */
export function setFogVision(doc: ProjectDoc, mapId: string, n: number | undefined): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m) return doc
  const v = n === undefined ? 0 : Math.min(10, Math.max(-10, Math.round(n)))
  if ((m.fogVision ?? 0) === v) return doc
  const next = { ...m }
  if (v === 0) delete next.fogVision
  else next.fogVision = v
  return { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

/** Computer sides see everything on this map (off removes the key; a side's own setting overrules it). */
export function setAiSeesAll(doc: ProjectDoc, mapId: string, on: boolean): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m || (m.aiSeesAll === true) === on) return doc
  const next = { ...m }
  if (on) next.aiSeesAll = true
  else delete next.aiSeesAll
  return { ...doc, maps: { ...doc.maps, [mapId]: next } }
}
