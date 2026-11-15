// Building blocks for done tests: small pure functions that read the game document.
// Every helper that looks at a map takes an optional map id and defaults to the game's first map.
import { terrainAt } from '../model/mapGrid'
import type { MapFile, ProjectDoc } from '../model/types'

const idsOf = (list: Array<{ id: string }> | null | undefined): string[] => (list ?? []).map((x) => x.id)

/** The map with this id, or the game's first map. */
export function mapOf(doc: ProjectDoc, mapId?: string): MapFile | null {
  const id = mapId ?? doc.game?.maps[0]?.id
  return (id ? doc.maps[id] : null) ?? null
}
export const firstMapId = (doc: ProjectDoc): string | undefined => doc.game?.maps[0]?.id

/** Game name: true when it is set and does not start with `notPrefix` (a practice game starts as "Practice: ..."). */
export const nameChanged = (doc: ProjectDoc, notPrefix: string): boolean => {
  const name = doc.game?.name.trim() ?? ''
  return name !== '' && !name.startsWith(notPrefix)
}

/** How many entries of a list (terrain, weapons, unit types...) are not among `start`, the ones the template began with. */
export const addedCount = (list: Array<{ id: string }> | null | undefined, start: string[]): number => idsOf(list).filter((id) => !start.includes(id)).length

/** Squares of the map painted with terrain `terrainId`. */
export function cellsOfTerrain(doc: ProjectDoc, terrainId: string, mapId?: string): number {
  return countCells(doc, mapId, (t) => t === terrainId)
}
/** Squares painted with any terrain that is not among `exclude` (the terrain the template began with). */
export function cellsOfOtherTerrain(doc: ProjectDoc, exclude: string[], mapId?: string): number {
  return countCells(doc, mapId, (t) => !exclude.includes(t))
}
function countCells(doc: ProjectDoc, mapId: string | undefined, want: (terrain: string) => boolean): number {
  const map = mapOf(doc, mapId)
  if (!map) return 0
  let n = 0
  for (let y = 0; y < map.tiles.length; y++) {
    for (let x = 0; x < (map.tiles[y]?.length ?? 0); x++) {
      const t = terrainAt(map, x, y)
      if (t !== undefined && want(t)) n++
    }
  }
  return n
}

export interface UnitFilter {
  /** Side id ("blue"). */
  side?: string
  /** Unit type id. */
  type?: string
  /** Unit types to leave out (the ones the template began with). */
  notTypes?: string[]
}
/** Units standing on the map that match the filter. */
export function unitsOnMap(doc: ProjectDoc, filter: UnitFilter = {}, mapId?: string): number {
  return (mapOf(doc, mapId)?.units ?? []).filter((u) =>
    (filter.side === undefined || u.side === filter.side) &&
    (filter.type === undefined || u.type === filter.type) &&
    !(filter.notTypes ?? []).includes(u.type)).length
}

/** Structures placed on the map, optionally of one kind and/or owned by one side. */
export function structuresOnMap(doc: ProjectDoc, filter: { type?: string; side?: string } = {}, mapId?: string): number {
  return (mapOf(doc, mapId)?.structures ?? []).filter((s) =>
    (filter.type === undefined || s.type === filter.type) && (filter.side === undefined || s.side === filter.side)).length
}

/** Unit types (leaving out `exclude`) that have at least one weapon. */
export function unitTypesWithWeapon(doc: ProjectDoc, exclude: string[] = []): number {
  return (doc.units ?? []).filter((u) => !exclude.includes(u.id) && (u.weapons?.length ?? 0) > 0).length
}
