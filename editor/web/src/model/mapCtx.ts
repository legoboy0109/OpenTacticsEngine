// What the map edits need to know about the rest of the game.
import type { MapFile, ProjectDoc, Terrain, Unit } from './types'

export interface MapCtx {
  terrain: Terrain[]
  unitTypes: Unit[]
  resources: string[]
}

export function ctxOf(doc: ProjectDoc): MapCtx | null {
  if (!doc.terrain || !doc.units) return null
  return { terrain: doc.terrain, unitTypes: doc.units, resources: (doc.game?.resources ?? []).map((r) => r.id) }
}

/** Applies a map edit to doc.maps[id]; a no-op when the map or the game data did not load. */
export function editMap(doc: ProjectDoc, id: string, fn: (m: MapFile, c: MapCtx) => MapFile): ProjectDoc {
  const m = doc.maps[id]
  const c = ctxOf(doc)
  if (!m || !c) return doc
  const next = fn(m, c)
  return next === m ? doc : { ...doc, maps: { ...doc.maps, [id]: next } }
}
