// What the map edits need to know about the rest of the game.
import { dropEmptyOwners } from './structuresConvert'
import type { MapFile, ProjectDoc, Structure, Terrain, Unit } from './types'

export interface MapCtx {
  terrain: Terrain[]
  /** The game's structure kinds (empty when it has none). */
  structureTypes: Structure[]
  unitTypes: Unit[]
  resources: string[]
}

/** A context from loose parts (the canvas has them as props, not as a document). */
export const ctxOfParts = (terrain: Terrain[], structureTypes: Structure[], unitTypes: Unit[]): MapCtx => ({ terrain, structureTypes, unitTypes, resources: [] })

export function ctxOf(doc: ProjectDoc): MapCtx | null {
  if (!doc.terrain || !doc.units) return null
  return { terrain: doc.terrain, structureTypes: doc.structures ?? [], unitTypes: doc.units, resources: (doc.game?.resources ?? []).map((r) => r.id) }
}

/** Applies a map edit to doc.maps[id]; a no-op when the map or the game data did not load. */
export function editMap(doc: ProjectDoc, id: string, fn: (m: MapFile, c: MapCtx) => MapFile): ProjectDoc {
  const m = doc.maps[id]
  const c = ctxOf(doc)
  if (!m || !c) return doc
  // A map's empty `owners` list (older editors wrote one) is left out as soon as the map is edited.
  const next = dropEmptyOwners(fn(m, c))
  return next === m ? doc : { ...doc, maps: { ...doc.maps, [id]: next } }
}
