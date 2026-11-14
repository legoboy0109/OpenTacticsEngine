// Structures on a map. The placement rules mirror the game loader (engine/core/data/map_loader.cpp): the whole
// footprint must lie inside the map, structures may not overlap, and a unit may not start on a square it cannot
// enter once the structure's movement costs apply. `x`, `y` of a placed structure is its top-left square.
// Placing never touches the terrain underneath.
import type { MapCtx } from './mapCtx'
import { inBounds, terrainAt } from './mapGrid'
import { sizeOf } from './structures'
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

/** Index (in the map's list) of the structure covering square (x, y); -1 when none. */
export function structureIndexAt(map: MapFile, c: Pick<MapCtx, 'structureTypes'>, x: number, y: number): number {
  return placedOf(map).findIndex((s) => rectHas(rectOf(defOf(c, s.type), s.x, s.y), x, y))
}

export const structureAt = (map: MapFile, c: Pick<MapCtx, 'structureTypes'>, x: number, y: number): MapStructure | undefined => placedOf(map)[structureIndexAt(map, c, x, y)]

/** Index of the structure whose top-left corner is (x, y); -1 when none. */
export const indexByOrigin = (map: MapFile, x: number, y: number): number => placedOf(map).findIndex((s) => s.x === x && s.y === y)

/** Why a structure of `type` cannot have its top-left corner at (x, y) (null = it can). `ignore` = the one being moved. */
export function fitProblem(map: MapFile, c: MapCtx, type: string, x: number, y: number, ignore = -1): string | null {
  const def = defOf(c, type)
  if (!def) return 'Pick a structure first.'
  const r = rectOf(def, x, y)
  if (!inBounds(map, r.x, r.y) || !inBounds(map, r.x + r.w - 1, r.y + r.h - 1)) {
    return r.w === 1 && r.h === 1 ? 'That square is outside the map.' : `A ${def.name} needs ${r.w} × ${r.h} squares and would stick out of the map there.`
  }
  const list = placedOf(map)
  for (let i = 0; i < list.length; i++) {
    if (i === ignore) continue
    const o = rectOf(defOf(c, list[i].type), list[i].x, list[i].y)
    if (r.x < o.x + o.w && o.x < r.x + r.w && r.y < o.y + o.h && o.y < r.y + r.h) return `A ${def.name} would overlap the ${typeName(c, list[i].type)} that is already there.`
  }
  for (const u of map.units ?? []) {
    if (!rectHas(r, u.x, u.y)) continue
    const unit = c.unitTypes.find((t) => t.id === u.type)
    if (!canEnter(c.terrain.find((t) => t.id === terrainAt(map, u.x, u.y)), def, unit)) {
      return `${unit?.name ?? 'A unit'} stands on a square of it and could not enter a ${def.name}. Move the unit first.`
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
export function removeStructureAt(map: MapFile, c: Pick<MapCtx, 'structureTypes'>, x: number, y: number): MapFile {
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
  placedOf(map).forEach((s, i) => {
    const r = rectOf(defOf(c, s.type), s.x, s.y)
    const outside = !inBounds(map, r.x, r.y) || !inBounds(map, r.x + r.w - 1, r.y + r.h - 1)
    const overlaps = placedOf(map).some((o, j) => {
      if (j >= i) return false
      const q = rectOf(defOf(c, o.type), o.x, o.y)
      return r.x < q.x + q.w && q.x < r.x + r.w && r.y < q.y + q.h && q.y < r.y + r.h
    })
    if (outside || overlaps) out.push(i)
  })
  return out
}
