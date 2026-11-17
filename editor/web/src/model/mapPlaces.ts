// Doors, chests and places as they stand on a map (docs/SRPG.md section 10): the placed structure's story name (`id`), a lock override (`locked`), a chest's
// `contents`, and who may visit (`visitSides`, `visitUnits`). Plus the computer unit's `goesFor`. Pure functions on a MapFile, or on the document where
// the story must follow (a story name is pointed at by events). The checks are placeChecks.ts.
import { renameBehaviourPlace } from './behaviour'
import { indexByOrigin, placedOf } from './mapStructures'
import { placeKindOf, type PlaceKind } from './places'
import { idProblem } from './storyIds'
import { walkStory } from './storyWalk'
import { patchObj } from './util'
import type { ChestLoot, MapFile, MapStructure, MapUnit, ProjectDoc } from './types'

export const MAX_CHEST_LOOT = 8
export const GOES_FOR: Array<NonNullable<MapUnit['goesFor']>[number]> = ['chests', 'villages', 'doors', 'steal', 'walls', 'weapons']
type At = { x: number; y: number }

const kindOfPlaced = (doc: ProjectDoc, s: MapStructure): PlaceKind | null => placeKindOf(doc.structures?.find((k) => k.id === s.type))

/** Changes keys of the structure whose top-left corner is `at`; undefined removes a key. */
function patchPlaced(map: MapFile, at: At, patch: Record<string, unknown>): MapFile {
  const i = indexByOrigin(map, at.x, at.y)
  if (i < 0) return map
  const list = placedOf(map).slice()
  list[i] = patchObj(list[i], patch)
  return { ...map, structures: list }
}

// --- the story name -----------------------------------------------------------------------------------------------------------------

export const takenPlaceIds = (map: MapFile, except?: MapStructure): string[] => placedOf(map).filter((s) => s !== except && typeof s.id === 'string').map((s) => s.id!)

/** Why `text` cannot be this structure's story name (null = fine). */
export const placeIdProblem = (map: MapFile, s: MapStructure, text: string): string | null => (text === '' ? null : idProblem(text, takenPlaceIds(map, s)))

/** The first free "<kind>_<n>" (a door "gate" gets gate_1, gate_2...). */
export function suggestPlaceId(map: MapFile, kindId: string): string {
  const taken = new Set(takenPlaceIds(map))
  for (let n = 1; ; n++) if (!taken.has(`${kindId}_${n}`)) return `${kindId}_${n}`
}

/** Every place in the map's events that names `from` names `to` instead. */
export const renamePlaceRefs = (map: MapFile, from: string, to: string): MapFile =>
  from === to ? map : renameBehaviourPlace(!map.events && !map.objectives ? map : walkStory(map, { ref: (r) => (r.kind === 'place' && r.id === from ? to : r.id) }), from, to) // and the units that head for it (docs/AI.md)

/** Does any event of the map point at this story name? */
export function placeIsNamedByEvents(map: MapFile, id: string): boolean {
  let found = false
  walkStory(map, { ref: (r) => { if (r.kind === 'place' && r.id === id) found = true; return r.id } })
  return found
}

/** Gives the placed structure at `at` a story name (or none with ''); events that named the old one follow in the same edit. A name that is not
 *  valid or is taken is refused (`error`, the document is unchanged). */
export function changePlaceId(doc: ProjectDoc, mapId: string, at: At, to: string): { doc: ProjectDoc; error: string | null } {
  const map = doc.maps[mapId]
  const i = map ? indexByOrigin(map, at.x, at.y) : -1
  if (!map || i < 0) return { doc, error: null }
  const s = placedOf(map)[i]
  if ((s.id ?? '') === to) return { doc, error: null }
  if (to === '' && s.id && placeIsNamedByEvents(map, s.id)) return { doc, error: 'Events point at this name. Remove or change them first, or pick another name.' }
  const error = placeIdProblem(map, s, to)
  if (error) return { doc, error }
  const next = patchPlaced(map, at, { id: to === '' ? undefined : to })
  return { doc: { ...doc, maps: { ...doc.maps, [mapId]: s.id && to ? renamePlaceRefs(next, s.id, to) : next } }, error: null }
}

// --- lock, chest contents, visitors -------------------------------------------------------------------------------------------------

/** `undefined` = whatever the kind says (the key is not written). */
export const setPlacedLocked = (map: MapFile, at: At, locked: boolean | undefined): MapFile => patchPlaced(map, at, { locked })

export const setContents = (map: MapFile, at: At, list: ChestLoot[]): MapFile => patchPlaced(map, at, { contents: list.length ? list.slice(0, MAX_CHEST_LOOT) : undefined })

/** What a new chest holds so it loads: the first item of the game, else 10 of its first resource, else nothing (the check then says a chest needs contents). */
export function starterContents(doc: ProjectDoc): ChestLoot[] {
  const item = doc.items?.[0]
  if (item) return [item.id]
  const res = doc.game?.resources?.[0]
  return res ? [{ resource: res.id, amount: 10 }] : []
}

export const setVisitSides = (map: MapFile, at: At, sides: string[]): MapFile => patchPlaced(map, at, { visitSides: sides.length ? sides : undefined })
export const setVisitUnits = (map: MapFile, at: At, units: string[]): MapFile => patchPlaced(map, at, { visitUnits: units.length ? units : undefined })

/** A newly placed or re-kinded structure: a chest gets starter contents; keys of another kind than it now is are dropped (they would stop the game loading). */
export function fitPlaceKeys(doc: ProjectDoc, map: MapFile, at: At): MapFile {
  const i = indexByOrigin(map, at.x, at.y)
  if (i < 0) return map
  const s = placedOf(map)[i]
  const k = kindOfPlaced(doc, s)
  const patch: Record<string, unknown> = {}
  if (k !== 'chest' && s.contents !== undefined) patch.contents = undefined
  if (k === 'chest' && s.contents === undefined) { const c = starterContents(doc); if (c.length) patch.contents = c }
  if (k !== 'door' && k !== 'chest' && s.locked !== undefined) patch.locked = undefined
  if (k !== 'visit') { if (s.visitSides !== undefined) patch.visitSides = undefined; if (s.visitUnits !== undefined) patch.visitUnits = undefined }
  return Object.keys(patch).length ? patchPlaced(map, at, patch) : map
}

/** A unit's story name changed: the places that list it as a visitor follow. */
export function renameVisitUnit(map: MapFile, from: string, to: string): MapFile {
  if (!placedOf(map).some((s) => s.visitUnits?.includes(from))) return map
  return { ...map, structures: placedOf(map).map((s) => (s.visitUnits?.includes(from) ? { ...s, visitUnits: s.visitUnits.map((u) => (u === from ? to : u)) } : s)) }
}

// --- the computer's goals ---------------------------------------------------------------------------------------------------------

export function setGoesFor(map: MapFile, at: At, goals: NonNullable<MapUnit['goesFor']>): MapFile {
  const units = (map.units ?? []).map((u) => (u.x === at.x && u.y === at.y ? patchObj(u, { goesFor: goals.length ? GOES_FOR.filter((g) => goals.includes(g)) : undefined }) : u))
  return { ...map, units }
}
