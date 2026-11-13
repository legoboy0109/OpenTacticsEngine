// Units on a map. The placement rules mirror the game loader: one unit per square,
// and a unit may not start on terrain its movement type cannot enter.
import type { MapCtx } from './mapCtx'
import { inBounds, terrainAt } from './mapGrid'
import { patchObj } from './util'
import type { MapFile, MapUnit, Terrain, Unit } from './types'

export interface UnitEdit {
  map: MapFile
  /** Plain-language reason the edit was refused (map is then the unchanged input). */
  error: string | null
  /** When a placement hit an existing unit: where it is, so the UI can select it instead. */
  existing?: { x: number; y: number }
}

export const unitIndexAt = (map: MapFile, x: number, y: number): number =>
  (map.units ?? []).findIndex((u) => u.x === x && u.y === y)

export const unitAt = (map: MapFile, x: number, y: number): MapUnit | undefined => map.units?.[unitIndexAt(map, x, y)]

/** Movement cost of 0 or less means the unit can never enter (the loader's rule); a missing entry costs 1. */
export const canEnter = (t: Terrain | undefined, u: Unit | undefined): boolean => {
  if (!t || !u) return true
  const cost = t.moveCost?.[u.moveType]
  return cost === undefined || cost > 0
}

const typeOf = (c: MapCtx, id: string) => c.unitTypes.find((u) => u.id === id)
const terrainDef = (c: MapCtx, map: MapFile, x: number, y: number) => c.terrain.find((t) => t.id === terrainAt(map, x, y))
const refuse = (map: MapFile, error: string, existing?: { x: number; y: number }): UnitEdit => ({ map, error, existing })

function blockedReason(c: MapCtx, map: MapFile, type: string, x: number, y: number): string | null {
  const t = terrainDef(c, map, x, y)
  const u = typeOf(c, type)
  if (canEnter(t, u)) return null
  return `${u?.name ?? 'This unit'} cannot stand on ${t?.name ?? 'that terrain'}: its movement type cannot enter it.`
}

export function placeUnit(map: MapFile, c: MapCtx, spec: { type: string; side: string; x: number; y: number }): UnitEdit {
  const { type, side, x, y } = spec
  if (!inBounds(map, x, y)) return refuse(map, 'That square is outside the map.')
  if (!typeOf(c, type)) return refuse(map, 'Pick a unit type first.')
  if (!map.sides?.some((s) => s.id === side)) return refuse(map, 'Pick a side first.')
  if (unitAt(map, x, y)) return refuse(map, 'There is already a unit here.', { x, y })
  const why = blockedReason(c, map, type, x, y)
  if (why) return refuse(map, why)
  const unit: MapUnit = { type, side, x, y }
  return { map: { ...map, units: [...(map.units ?? []), unit] }, error: null }
}

export function moveMapUnit(map: MapFile, c: MapCtx, from: { x: number; y: number }, to: { x: number; y: number }): UnitEdit {
  const i = unitIndexAt(map, from.x, from.y)
  if (i < 0 || (from.x === to.x && from.y === to.y)) return { map, error: null }
  if (!inBounds(map, to.x, to.y)) return refuse(map, 'That square is outside the map.')
  if (unitAt(map, to.x, to.y)) return refuse(map, 'There is already a unit there.')
  const u = map.units![i]
  const why = blockedReason(c, map, u.type, to.x, to.y)
  if (why) return refuse(map, why)
  const units = map.units!.slice()
  units[i] = { ...u, x: to.x, y: to.y }
  return { map: { ...map, units }, error: null }
}

export function removeMapUnit(map: MapFile, x: number, y: number): MapFile {
  const i = unitIndexAt(map, x, y)
  return i < 0 ? map : { ...map, units: map.units!.filter((_, j) => j !== i) }
}

type UnitPatch = { side?: string; name?: string; level?: number; leader?: boolean }

/** Name, level and leader are stored only when they mean something; clearing removes the key. */
export function updateMapUnit(map: MapFile, x: number, y: number, patch: UnitPatch): MapFile {
  const i = unitIndexAt(map, x, y)
  if (i < 0) return map
  const clean: Record<string, unknown> = { ...patch }
  if ('name' in patch && !patch.name?.trim()) clean.name = undefined
  if ('leader' in patch && !patch.leader) clean.leader = undefined
  if ('level' in patch && (!patch.level || patch.level < 1)) clean.level = undefined
  const units = map.units!.slice()
  units[i] = patchObj(units[i], clean)
  return { ...map, units }
}

/** Switches the unit type; refused when the new type cannot stand there. Overrides that now equal the new base are dropped. */
export function changeUnitType(map: MapFile, c: MapCtx, x: number, y: number, type: string): UnitEdit {
  const i = unitIndexAt(map, x, y)
  const next = typeOf(c, type)
  if (i < 0 || !next) return { map, error: null }
  if (map.units![i].type === type) return { map, error: null }
  const why = blockedReason(c, map, type, x, y)
  if (why) return refuse(map, why)
  const u = map.units![i]
  const stats = minimiseStats(u.stats, next)
  const units = map.units!.slice()
  units[i] = patchObj(u, { type, stats })
  return { map: { ...map, units }, error: null }
}

function minimiseStats(stats: Record<string, number> | undefined, base: Unit): Record<string, number> | undefined {
  if (!stats) return undefined
  const kept = Object.entries(stats).filter(([k, v]) => v !== (base.stats?.[k] ?? 0))
  return kept.length ? Object.fromEntries(kept) : undefined
}

/** Sets one stat override; a value equal to the unit type's own (or undefined) removes the key. */
export function setUnitStat(map: MapFile, c: MapCtx, x: number, y: number, stat: string, value: number | undefined): MapFile {
  const i = unitIndexAt(map, x, y)
  if (i < 0) return map
  const u = map.units![i]
  const base = typeOf(c, u.type)
  const next = { ...(u.stats ?? {}) }
  if (value === undefined) delete next[stat]; else next[stat] = value
  const stats = base ? minimiseStats(next, base) : (Object.keys(next).length ? next : undefined)
  const units = map.units!.slice()
  units[i] = patchObj(u, { stats })
  return { ...map, units }
}
