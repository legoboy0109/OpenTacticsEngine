// What a side's team sees at the start of a battle on a map with fog, worked out in the browser for the map editor's
// "Show what a side sees" aid. It follows engine/core/battle/sight.cpp (computeTeamSight) and Grid::within: grid distance
// on the square grid is |dx| + |dy|; a unit sees `vision + terrain bonus + structure bonus` squares around it; every
// structure the team owns sees one square around each square of its footprint; and a unit next to a square that hides
// is always able to see that square. Nothing blocks sight (no line of sight yet). Pure.
import { bonusFor, visionOf } from './fog'
import { terrainAt } from './mapGrid'
import { placedOf, rectOf, structureAt } from './mapStructures'
import type { MapFile, ProjectDoc } from './types'

/** The team a side plays for: its `team`, or the side itself when it has none (as the loader does). */
export const teamOfSide = (map: MapFile, sideId: string): string => {
  const s = map.sides?.find((x) => x.id === sideId)
  return s?.team?.trim() ? s.team : sideId
}

/** One entry per square (row by row): 1 = the side's team sees it at the start, 0 = it does not. */
export function seenAtStart(doc: ProjectDoc, map: MapFile, sideId: string): Uint8Array {
  const { width, height } = map.grid
  const seen = new Uint8Array(width * height)
  const team = teamOfSide(map, sideId)
  const mine = new Set((map.sides ?? []).filter((s) => teamOfSide(map, s.id) === team).map((s) => s.id))
  const structureTypes = doc.structures ?? []
  const terrainById = new Map((doc.terrain ?? []).map((t) => [t.id, t]))
  const typeById = new Map((doc.units ?? []).map((u) => [u.id, u]))
  const structById = new Map(structureTypes.map((s) => [s.id, s]))
  const ctx = { structureTypes }

  const inMap = (x: number, y: number): boolean => x >= 0 && y >= 0 && x < width && y < height
  const mark = (cx: number, cy: number, radius: number): void => {
    for (let y = Math.max(0, cy - radius); y <= Math.min(height - 1, cy + radius); y++) {
      const span = radius - Math.abs(y - cy)
      for (let x = Math.max(0, cx - span); x <= Math.min(width - 1, cx + span); x++) seen[y * width + x] = 1
    }
  }
  const hides = (x: number, y: number): boolean => {
    if (terrainById.get(terrainAt(map, x, y) ?? '')?.hides === true) return true
    const st = structureAt(map, ctx, x, y)
    return !!st && structById.get(st.type)?.hides === true
  }

  for (const u of map.units ?? []) {
    if (!mine.has(u.side) || !inMap(u.x, u.y)) continue
    const type = typeById.get(u.type)
    if (!type) continue
    const st = structureAt(map, ctx, u.x, u.y)
    // Each square's bonus may be limited to some unit types (by id or label), as in Battle::visionAt.
    const ground = terrainById.get(terrainAt(map, u.x, u.y) ?? '')
    const stDef = st ? structById.get(st.type) : undefined
    const bonus = (ground ? bonusFor(ground, type) : 0) + (stDef ? bonusFor(stDef, type) : 0)
    mark(u.x, u.y, visionOf(type) + bonus)
  }
  for (const s of placedOf(map)) {
    if (!s.side || !mine.has(s.side)) continue
    const r = rectOf(structById.get(s.type), s.x, s.y)
    for (let y = r.y; y < r.y + r.h; y++) for (let x = r.x; x < r.x + r.w; x++) mark(x, y, 1)
  }
  for (const u of map.units ?? []) {
    if (!mine.has(u.side) || !inMap(u.x, u.y)) continue
    for (const [dx, dy] of [[0, -1], [1, 0], [0, 1], [-1, 0]]) {
      const x = u.x + dx, y = u.y + dy
      if (inMap(x, y) && hides(x, y)) seen[y * width + x] = 1
    }
  }
  return seen
}

/** Sides to offer in the picker: one per side of the map (the team is shown beside a side that shares one). */
export const sideChoices = (map: MapFile): Array<{ value: string; label: string }> =>
  (map.sides ?? []).map((s) => ({ value: s.id, label: s.name || s.id }))
