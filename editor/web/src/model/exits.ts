// A map's exit squares (docs/AI.md "Looting and escaping"): `exits: [{ x, y }]`. A computer unit in mode "escape" (or a looter with nothing left to
// take) walks to the nearest one and leaves the map. Missing = none. The checks mirror engine/core/data/loader_exits.cpp.
import type { MapFile, ProjectDoc } from './types'

export const MAX_EXITS = 64
export interface ExitSquare { x: number; y: number }

export const exitsOf = (map: MapFile): ExitSquare[] => (Array.isArray(map.exits) ? map.exits : [])

/** Writes the list; an empty one removes the key (the loader refuses an empty list). */
export function setExits(doc: ProjectDoc, mapId: string, list: ExitSquare[]): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m) return doc
  const next = { ...m }
  if (list.length) next.exits = list
  else delete next.exits
  return { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

/** A square for a new exit: the first one (row by row) that is not an exit yet; null when every square is. */
export function freeExitSquare(map: MapFile): ExitSquare | null {
  const taken = new Set(exitsOf(map).map((e) => `${e.x},${e.y}`))
  for (let y = 0; y < map.grid.height; y++) for (let x = 0; x < map.grid.width; x++) if (!taken.has(`${x},${y}`)) return { x, y }
  return null
}

const isInt = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)

/** What the loader would refuse about the map's exits, in plain words. */
export function exitProblems(map: MapFile): string[] {
  if (map.exits === undefined) return []
  const out: string[] = []
  if (!Array.isArray(map.exits) || map.exits.length === 0 || map.exits.length > MAX_EXITS) return [`The exits must be a list of 1 to ${MAX_EXITS} squares.`]
  const seen = new Set<string>()
  map.exits.forEach((e, i) => {
    if (!isInt(e?.x) || !isInt(e?.y) || e.x < 0 || e.y < 0) { out.push(`Exit ${i + 1} needs a column and a row.`); return }
    if (e.x >= map.grid.width || e.y >= map.grid.height) out.push(`Exit ${i + 1} (${e.x + 1}, ${e.y + 1}) is outside the map.`)
    const k = `${e.x},${e.y}`
    if (seen.has(k)) out.push(`The square ${e.x + 1}, ${e.y + 1} is an exit twice.`)
    seen.add(k)
  })
  return out
}
