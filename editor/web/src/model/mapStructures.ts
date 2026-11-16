// Structures on a map. The placement rules mirror the game loader (engine/core/data/map_loader.cpp): the whole
// footprint must lie inside the map, structures may not overlap, and a unit may not start on a square it cannot
// enter once the structure's movement costs apply. `x`, `y` of a placed structure is its top-left square
// (hexagon games: its middle hexagon; it covers that hexagon and, with `rings`, the six around it).
// Placing never touches the terrain underneath.
import { typeIdOf } from './characterUnits'
import type { MapCtx } from './mapCtx'
import { inBounds, terrainAt, type Cell } from './mapGrid'
import { isHex, layoutOf, SQUARE, type GridSpec } from './gridSpec'
import { neighbours } from './hex'
import { ringsOf, sizeOf } from './structures'
import { canEnter } from './structuresEffective'
import { patchObj } from './util'
import type { MapFile, MapStructure, Structure } from './types'

export interface StructureEdit {
  map: MapFile
  /** Plain-language reason the edit was refused (map is then the unchanged input). */
  error: string | null
}

export interface Rect { x: number; y: number; w: number; h: number }
export interface Square { x: number; y: number }

/** What the structure functions need of the context: the kinds, and the grid (squares when missing). */
export type StructCtx = Pick<MapCtx, 'structureTypes'> & { grid?: GridSpec }

const defOf = (c: Pick<MapCtx, 'structureTypes'>, type: string): Structure | undefined => c.structureTypes.find((s) => s.id === type)
const refuse = (map: MapFile, error: string): StructureEdit => ({ map, error })
const typeName = (c: Pick<MapCtx, 'structureTypes'>, type: string): string => defOf(c, type)?.name ?? type

export const placedOf = (map: MapFile): MapStructure[] => map.structures ?? []

/** The squares a structure of `def` covers when its top-left corner is at (x, y). */
export function rectOf(def: Structure | undefined, x: number, y: number): Rect {
  const { w, h } = sizeOf(def)
  return { x, y, w, h }
}
export const rectHas = (r: Rect, x: number, y: number): boolean => x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h

/** Every cell a structure of `def` covers when placed at (x, y): squares row by row from the top-left corner; hexagons the
 *  middle first, then its neighbours in direction order (docs/HEX.md). Cells may lie outside the map. */
export function footprint(grid: GridSpec | undefined, def: Structure | undefined, x: number, y: number): Cell[] {
  const g = grid ?? SQUARE
  if (isHex(g)) return ringsOf(def) === 1 ? [{ x, y }, ...neighbours(layoutOf(g), x, y)] : [{ x, y }]
  const { w, h } = sizeOf(def)
  const out: Cell[] = []
  for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) out.push({ x: x + i, y: y + j })
  return out
}
const keyOf = (c: Cell): string => `${c.x},${c.y}`
const coversCell = (cells: Cell[], x: number, y: number): boolean => cells.some((c) => c.x === x && c.y === y)
const unitWord = (c: StructCtx): string => (isHex(c.grid ?? SQUARE) ? 'hexagon' : 'square')

/** Index (in the map's list) of the structure covering square (x, y); -1 when none. */
export function structureIndexAt(map: MapFile, c: StructCtx, x: number, y: number): number {
  return placedOf(map).findIndex((s) => coversCell(footprint(c.grid, defOf(c, s.type), s.x, s.y), x, y))
}

export const structureAt = (map: MapFile, c: StructCtx, x: number, y: number): MapStructure | undefined => placedOf(map)[structureIndexAt(map, c, x, y)]

/** Index of the structure whose top-left corner is (x, y); -1 when none. */
export const indexByOrigin = (map: MapFile, x: number, y: number): number => placedOf(map).findIndex((s) => s.x === x && s.y === y)

/** Why a structure of `type` cannot have its top-left corner (hexagons: its middle) at (x, y) (null = it can). `ignore` = the one being moved. */
export function fitProblem(map: MapFile, c: MapCtx, type: string, x: number, y: number, ignore = -1): string | null {
  const def = defOf(c, type)
  if (!def) return 'Pick a structure first.'
  const cells = footprint(c.grid, def, x, y)
  if (!cells.every((q) => inBounds(map, q.x, q.y))) {
    if (isHex(c.grid)) return cells.length === 1 ? 'That hexagon is outside the map.' : `A ${def.name} covers a hexagon and the six around it, and would stick out of the map there.`
    const { w, h } = sizeOf(def)
    return w === 1 && h === 1 ? 'That square is outside the map.' : `A ${def.name} needs ${w} × ${h} squares and would stick out of the map there.`
  }
  const taken = new Set(cells.map(keyOf))
  const list = placedOf(map)
  for (let i = 0; i < list.length; i++) {
    if (i === ignore) continue
    if (footprint(c.grid, defOf(c, list[i].type), list[i].x, list[i].y).some((q) => taken.has(keyOf(q)))) return `A ${def.name} would overlap the ${typeName(c, list[i].type)} that is already there.`
  }
  for (const u of map.units ?? []) {
    if (!taken.has(`${u.x},${u.y}`)) continue
    const unit = c.unitTypes.find((t) => t.id === typeIdOf(c.characters, u))
    if (!canEnter(c.terrain.find((t) => t.id === terrainAt(map, u.x, u.y)), def, unit)) {
      return `${unit?.name ?? 'A unit'} stands on a ${unitWord(c)} of it and could not enter a ${def.name}. Move the unit first.`
    }
  }
  return null
}

export function placeStructure(map: MapFile, c: MapCtx, spec: { type: string; x: number; y: number; side?: string }): StructureEdit {
  const { type, x, y } = spec
  const side = spec.side || undefined
  if (side !== undefined && !map.sides?.some((s) => s.id === side)) return refuse(map, 'Pick an owner first.')
  const why = fitProblem(map, c, type, x, y)
  if (why) return refuse(map, why)
  const s: MapStructure = side ? { type, x, y, side } : { type, x, y }
  return { map: { ...map, structures: [...placedOf(map), s] }, error: null }
}

/** Moves the structure whose top-left corner is `from` by (dx, dy). */
export function moveStructureBy(map: MapFile, c: MapCtx, from: Square, dx: number, dy: number): StructureEdit {
  const i = indexByOrigin(map, from.x, from.y)
  if (i < 0 || (dx === 0 && dy === 0)) return { map, error: null }
  const s = placedOf(map)[i]
  const why = fitProblem(map, c, s.type, s.x + dx, s.y + dy, i)
  if (why) return refuse(map, why)
  const list = placedOf(map).slice()
  list[i] = { ...s, x: s.x + dx, y: s.y + dy }
  return { map: { ...map, structures: list }, error: null }
}

/** Removes the structure covering square (x, y). */
export function removeStructureAt(map: MapFile, c: StructCtx, x: number, y: number): MapFile {
  const i = structureIndexAt(map, c, x, y)
  return i < 0 ? map : { ...map, structures: placedOf(map).filter((_, j) => j !== i) }
}

/** Gives the structure (top-left corner at `at`) to a side, or to nobody with null. */
export function setStructureOwner(map: MapFile, at: Square, side: string | null): MapFile {
  const i = indexByOrigin(map, at.x, at.y)
  if (i < 0) return map
  if (side !== null && !map.sides?.some((s) => s.id === side)) return map
  const cur = placedOf(map)[i]
  if ((cur.side ?? null) === side) return map
  const list = placedOf(map).slice()
  list[i] = patchObj(cur, { side: side ?? undefined })
  return { ...map, structures: list }
}

/** Switches the kind of the structure at `at`; refused when the new kind does not fit there. */
export function changeStructureKind(map: MapFile, c: MapCtx, at: Square, type: string): StructureEdit {
  const i = indexByOrigin(map, at.x, at.y)
  if (i < 0 || placedOf(map)[i].type === type) return { map, error: null }
  const why = fitProblem(map, c, type, at.x, at.y, i)
  if (why) return refuse(map, why)
  const list = placedOf(map).slice()
  list[i] = { ...list[i], type }
  return { map: { ...map, structures: list }, error: null }
}

/** Placed structures that break the loader's rules now (a kind was made bigger, or a map shrunk some other way). */
export function misfits(map: MapFile, c: MapCtx): number[] {
  const out: number[] = []
  const seen = new Set<string>()
  placedOf(map).forEach((s, i) => {
    const cells = footprint(c.grid, defOf(c, s.type), s.x, s.y)
    const outside = !cells.every((q) => inBounds(map, q.x, q.y))
    const overlaps = cells.some((q) => seen.has(keyOf(q)))
    cells.forEach((q) => seen.add(keyOf(q)))
    if (outside || overlaps) out.push(i)
  })
  return out
}
