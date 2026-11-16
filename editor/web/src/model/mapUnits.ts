// Units on a map. The placement rules mirror the game loader: one unit per square,
// and a unit may not start on terrain its movement type cannot enter.
import type { MapCtx } from './mapCtx'
import { inBounds, terrainAt } from './mapGrid'
import { typeIdOf } from './characterUnits'
import { structureAt } from './mapStructures'
import { canEnter } from './structuresEffective'
import { patchObj } from './util'
import type { ItemEntry, MapFile, MapUnit } from './types'

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

const typeOf = (c: MapCtx, id: string) => c.unitTypes.find((u) => u.id === id)
const terrainDef = (c: MapCtx, map: MapFile, x: number, y: number) => c.terrain.find((t) => t.id === terrainAt(map, x, y))
const structureDef = (c: MapCtx, map: MapFile, x: number, y: number) => {
  const s = structureAt(map, c, x, y)
  return s ? c.structureTypes.find((d) => d.id === s.type) : undefined
}
const refuse = (map: MapFile, error: string, existing?: { x: number; y: number }): UnitEdit => ({ map, error, existing })

/** Judged the way the loader does: the structure on the square (if any) replaces the terrain's cost where it sets one. */
function blockedReason(c: MapCtx, map: MapFile, type: string, x: number, y: number): string | null {
  const t = terrainDef(c, map, x, y)
  const sd = structureDef(c, map, x, y)
  const u = typeOf(c, type)
  if (canEnter(t, sd, u)) return null
  return `${u?.name ?? 'This unit'} cannot stand on ${sd ? `a ${sd.name}` : t?.name ?? 'that terrain'}: its movement type cannot enter it.`
}

/** `type` places a plain unit; `character` places that character instead (`{ character, side, x, y }`: the type comes
 *  from the character, and a character can stand on a map once). */
export function placeUnit(map: MapFile, c: MapCtx, spec: { type?: string; character?: string; side: string; x: number; y: number }): UnitEdit {
  const { character, side, x, y } = spec
  const person = character === undefined ? undefined : c.characters.find((p) => p.id === character)
  const type = person ? person.type : spec.type ?? ''
  if (!inBounds(map, x, y)) return refuse(map, 'That square is outside the map.')
  if (character !== undefined && !person) return refuse(map, 'Pick a character that is not on this map yet.')
  if (character === undefined && !typeOf(c, type)) return refuse(map, 'Pick a unit type first.')
  if (!map.sides?.some((s) => s.id === side)) return refuse(map, 'Pick a side first.')
  if (character !== undefined && map.units?.some((u) => u.character === character)) return refuse(map, 'This character is already on the map.')
  if (unitAt(map, x, y)) return refuse(map, 'There is already a unit here.', { x, y })
  const why = blockedReason(c, map, type, x, y)
  if (why) return refuse(map, why)
  const unit: MapUnit = character !== undefined ? { character, side, x, y } : { type, side, x, y }
  return { map: { ...map, units: [...(map.units ?? []), unit] }, error: null }
}

export function moveMapUnit(map: MapFile, c: MapCtx, from: { x: number; y: number }, to: { x: number; y: number }): UnitEdit {
  const i = unitIndexAt(map, from.x, from.y)
  if (i < 0 || (from.x === to.x && from.y === to.y)) return { map, error: null }
  if (!inBounds(map, to.x, to.y)) return refuse(map, 'That square is outside the map.')
  if (unitAt(map, to.x, to.y)) return refuse(map, 'There is already a unit there.')
  const u = map.units![i]
  const why = blockedReason(c, map, typeIdOf(c.characters, u), to.x, to.y)
  if (why) return refuse(map, why)
  const units = map.units!.slice()
  units[i] = { ...u, x: to.x, y: to.y }
  return { map: { ...map, units }, error: null }
}

export function removeMapUnit(map: MapFile, x: number, y: number): MapFile {
  const i = unitIndexAt(map, x, y)
  return i < 0 ? map : { ...map, units: map.units!.filter((_, j) => j !== i) }
}

/** `items`: a list (even an empty one) replaces the unit type's starting items; undefined removes the key = uses the type's. */
type UnitPatch = { side?: string; name?: string; level?: number; leader?: boolean; items?: ItemEntry[] }

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
  if (map.units![i].character !== undefined && type === typeIdOf(c.characters, { character: map.units![i].character })) {
    // Back to the character's own class: the map says nothing about it again.
    const u = map.units![i]
    const units = map.units!.slice()
    units[i] = patchObj(u, { type: undefined, stats: minimiseStats(u.stats, startStats(c, u, type)) })
    return { map: { ...map, units }, error: null }
  }
  const why = blockedReason(c, map, type, x, y)
  if (why) return refuse(map, why)
  const u = map.units![i]
  const stats = minimiseStats(u.stats, startStats(c, u, type))
  const units = map.units!.slice()
  units[i] = patchObj(u, { type, stats })
  return { map: { ...map, units }, error: null }
}

function minimiseStats(stats: Record<string, number> | undefined, base: Record<string, number>): Record<string, number> | undefined {
  if (!stats) return undefined
  const kept = Object.entries(stats).filter(([k, v]) => v !== (base[k] ?? 0))
  return kept.length ? Object.fromEntries(kept) : undefined
}

/** What a placed unit starts with when the map says nothing: its type's stats, and for a character plus the
 *  character's own (never below 0), as the loader works them out. `type` = the type the unit will have. */
export function startStats(c: MapCtx, u: Pick<MapUnit, 'character'>, type: string): Record<string, number> {
  const base = { ...(typeOf(c, type)?.stats ?? {}) }
  const person = u.character === undefined ? undefined : c.characters.find((p) => p.id === u.character)
  for (const [k, v] of Object.entries(person?.stats ?? {})) base[k] = Math.max(0, (base[k] ?? 0) + v)
  return base
}

/** Sets one stat override; a value equal to the unit type's own (or undefined) removes the key. */
export function setUnitStat(map: MapFile, c: MapCtx, x: number, y: number, stat: string, value: number | undefined): MapFile {
  const i = unitIndexAt(map, x, y)
  if (i < 0) return map
  const u = map.units![i]
  const type = typeIdOf(c.characters, u)
  const next = { ...(u.stats ?? {}) }
  if (value === undefined) delete next[stat]; else next[stat] = value
  const stats = typeOf(c, type) ? minimiseStats(next, startStats(c, u, type)) : (Object.keys(next).length ? next : undefined)
  const units = map.units!.slice()
  units[i] = patchObj(u, { stats })
  return { ...map, units }
}
