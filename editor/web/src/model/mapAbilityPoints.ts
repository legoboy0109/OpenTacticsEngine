// A map's own "abilityPoints": { "perBattle": n } (loader: data/maps/*.json): the AP a won battle on this map gives, replacing the game's
// rules.abilityPoints.perBattle. Missing = the game's number. Only "perBattle" is allowed, and nobody earns AP unless an item teaches.
import { anyTeaches } from './teaches'
import { AP_MAX } from './abilityPoints'
import type { MapFile, ProjectDoc } from './types'

const isWhole = (n: unknown): boolean => typeof n === 'number' && Number.isInteger(n) && n >= 0 && n <= AP_MAX

/** The map's number, or undefined when it has none (the game's value applies). */
export function mapApPerBattle(map: MapFile): number | undefined {
  const b = map.abilityPoints
  return b && typeof b === 'object' && !Array.isArray(b) && typeof b.perBattle === 'number' ? b.perBattle : undefined
}

/** Sets the number (clamped to 0..9999 and whole); undefined (an empty box) removes the key. Other keys of the block stay (a check names them). */
export function setMapApPerBattle(doc: ProjectDoc, mapId: string, n: number | undefined): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m) return doc
  const next: MapFile = { ...m }
  const rest = m.abilityPoints && typeof m.abilityPoints === 'object' && !Array.isArray(m.abilityPoints) ? { ...m.abilityPoints } : {}
  if (n === undefined) delete rest.perBattle
  else rest.perBattle = Math.min(AP_MAX, Math.max(0, Math.round(n)))
  if (Object.keys(rest).length) next.abilityPoints = rest
  else delete next.abilityPoints
  return { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

/** What the loader would refuse about the map's "abilityPoints", in plain words. */
export function mapApChecks(doc: ProjectDoc, map: MapFile): string[] {
  const b = map.abilityPoints
  if (b === undefined) return []
  if (!b || typeof b !== 'object' || Array.isArray(b)) return ['The map’s ability points must be a block with “perBattle” in it.']
  const out: string[] = []
  for (const k of Object.keys(b)) if (k !== 'perBattle') out.push(`The map’s ability points have an unknown setting “${k}”. Only “perBattle” (the points for a won battle) can be set here.`)
  if (b.perBattle === undefined) out.push('The map’s ability points need “perBattle”, the points a won battle on this map gives. Fill it in or empty the box.')
  else if (!isWhole(b.perBattle)) out.push(`The map’s ability points for a won battle must be a whole number from 0 to ${AP_MAX}.`)
  if (!anyTeaches(doc)) out.push('The map’s ability points do nothing yet: no item teaches anything, so nobody earns points. Add a “Teaches” entry to a weapon or accessory (Items screen), or empty the box.')
  return out
}
