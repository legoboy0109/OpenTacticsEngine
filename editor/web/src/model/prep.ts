// Editing a map's preparation screen (docs/EVENTS.md "Preparation"): switching it on and off, the deployment squares, the
// limit, required / forbidden / fixed characters and the shop. Every function returns a new map (the same object when
// nothing changed); the refusals mirror the game loader's (engine/core/data/loader_prep.cpp).
import { storyIdOf, typeIdOf } from './characterUnits'
import type { MapCtx } from './mapCtx'
import { inBounds, terrainAt } from './mapGrid'
import { structureAt } from './mapStructures'
import { canEnter } from './structuresEffective'
import type { PrepRequired, MapPrep } from './prepTypes'
import type { Overlay } from './storyOverlays'
import type { MapFile, MapSide, MapUnit } from './types'

export const SLOTS_MAX = 64

export const requiredId = (r: PrepRequired): string => (typeof r === 'string' ? r : r.character)
/** The required character's own slot, as a number from 1; undefined = any slot. */
export const requiredSlot = (r: PrepRequired): number | undefined => (typeof r === 'string' ? undefined : r.slot)

const isHuman = (s: Pick<MapSide, 'controller'>) => s.controller !== 'ai'

/** The side the deployed characters join: the one the file names, else the first human side ('' = none). */
export const prepSide = (map: MapFile, prep: MapPrep): string => prep.side && map.sides?.some((s) => s.id === prep.side) ? prep.side : (map.sides ?? []).find(isHuman)?.id ?? ''

/** A placed unit the player chooses about: named, on the preparation's side, not in "fixed". Its own square is ignored. */
export const isChoosable = (map: MapFile, prep: MapPrep, u: MapUnit): boolean => {
  const id = storyIdOf(u)
  return u.side === prepSide(map, prep) && id !== undefined && !(prep.fixed ?? []).includes(id)
}

/** The unit standing at (x, y) that stays where it is (so the square cannot be a slot). */
export function stayingAt(map: MapFile, prep: MapPrep, x: number, y: number): MapUnit | undefined {
  return (map.units ?? []).find((u) => u.x === x && u.y === y && !isChoosable(map, prep, u))
}

/** Can some kind of unit stand on the square? (The loader refuses a slot nobody could use.) */
export function someoneCanStand(map: MapFile, c: MapCtx, x: number, y: number): boolean {
  const t = c.terrain.find((d) => d.id === terrainAt(map, x, y))
  const s = structureAt(map, c, x, y)
  const st = s ? c.structureTypes.find((d) => d.id === s.type) : undefined
  return c.unitTypes.some((u) => canEnter(t, st, u))
}

/** Can a unit of type `typeId` stand on the square? */
export function typeCanStand(map: MapFile, c: MapCtx, typeId: string, x: number, y: number): boolean {
  const t = c.terrain.find((d) => d.id === terrainAt(map, x, y))
  const s = structureAt(map, c, x, y)
  const st = s ? c.structureTypes.find((d) => d.id === s.type) : undefined
  return canEnter(t, st, c.unitTypes.find((u) => u.id === typeId))
}

const withPrep = (map: MapFile, prep: MapPrep): MapFile => ({ ...map, prep })

// ---- on and off

/** Switches the preparation on: the squares of the named human units become the first slots (their own squares stop
 *  mattering once it is on, so nothing is lost), or one free square when there are none. */
export function enablePrep(map: MapFile, c: MapCtx): MapFile {
  if (map.prep) return map
  const probe: MapPrep = { slots: [] }
  const side = prepSide(map, probe)
  const slots = (map.units ?? []).filter((u) => u.side === side && storyIdOf(u) !== undefined).slice(0, SLOTS_MAX).map((u) => ({ x: u.x, y: u.y }))
  if (slots.length === 0) {
    const w = map.grid.width, h = map.grid.height
    for (let y = 0; y < h && slots.length === 0; y++)
      for (let x = 0; x < w && slots.length === 0; x++)
        if (!(map.units ?? []).some((u) => u.x === x && u.y === y) && someoneCanStand(map, c, x, y)) slots.push({ x, y })
  }
  return withPrep(map, { slots, ...(slots.length > 0 ? { limit: slots.length } : {}) })
}

export function disablePrep(map: MapFile): MapFile {
  if (!map.prep) return map
  const { prep: _gone, ...rest } = map
  return rest as MapFile
}

// ---- slots

export const slotIndexAt = (prep: MapPrep, x: number, y: number): number => prep.slots.findIndex((s) => s.x === x && s.y === y)

export interface SlotEdit { map: MapFile; error: string | null; added?: boolean }
const refuse = (map: MapFile, error: string): SlotEdit => ({ map, error })

/** Click a square: a slot is removed, an empty square becomes the next slot. */
export function toggleSlot(map: MapFile, c: MapCtx, x: number, y: number): SlotEdit {
  const prep = map.prep
  if (!prep) return refuse(map, 'Switch the preparation on first (the Preparation tab).')
  if (!inBounds(map, x, y)) return refuse(map, 'That square is outside the map.')
  const at = slotIndexAt(prep, x, y)
  if (at >= 0) return { map: removeSlot(map, at), error: null, added: false }
  if (prep.slots.length >= SLOTS_MAX) return refuse(map, `A map can have at most ${SLOTS_MAX} slots.`)
  const stays = stayingAt(map, prep, x, y)
  if (stays) return refuse(map, `A unit that stays where it is placed stands there (${stays.name ?? stays.id ?? stays.character ?? 'a unit'}). Move it first.`)
  if (!someoneCanStand(map, c, x, y)) return refuse(map, 'No kind of unit can stand on that square.')
  return { map: withPrep(map, { ...prep, slots: [...prep.slots, { x, y }] }), error: null, added: true }
}

/** Removes slot `index`: later slots move up one number, a required character fixed to it is no longer fixed, and the
 *  limit never exceeds what is left. */
export function removeSlot(map: MapFile, index: number): MapFile {
  const prep = map.prep
  if (!prep || index < 0 || index >= prep.slots.length) return map
  const slots = prep.slots.filter((_, i) => i !== index)
  const gone = index + 1
  const required = prep.required?.map((r): PrepRequired => {
    const n = requiredSlot(r)
    if (n === undefined) return r
    if (n === gone) return requiredId(r)
    return n > gone ? { character: requiredId(r), slot: n - 1 } : r
  })
  const next: MapPrep = { ...prep, slots }
  if (required) next.required = required
  if (prep.limit !== undefined && slots.length > 0 && prep.limit > slots.length) next.limit = slots.length
  return withPrep(map, next)
}

export const setLimit = (map: MapFile, n: number | undefined): MapFile => {
  const prep = map.prep
  if (!prep) return map
  const lo = Math.max(1, prep.required?.length ?? 0), hi = Math.max(1, prep.slots.length)
  const limit = n === undefined ? undefined : Math.max(lo, Math.min(hi, Math.round(n)))
  if (limit === prep.limit) return map
  const { limit: _old, ...rest } = prep
  return withPrep(map, limit === undefined ? rest : { ...rest, limit })
}

export const setSide = (map: MapFile, side: string): MapFile => {
  const prep = map.prep
  if (!prep || prep.side === side) return map
  return withPrep(map, { ...prep, side })
}

// ---- who

const without = (list: string[] | undefined, id: string) => (list ?? []).filter((x) => x !== id)
const clean = (prep: MapPrep): MapPrep => {
  const out = { ...prep }
  for (const k of ['required', 'forbidden', 'fixed'] as const) if (out[k] !== undefined && out[k]!.length === 0) delete out[k]
  return out
}

export interface PrepEdit { map: MapFile; error: string | null }

/** A character that is always deployed; leaves the forbidden and fixed lists. Refused beyond the limit (the loader would). */
export function addRequired(map: MapFile, id: string): PrepEdit {
  const prep = map.prep
  if (!prep || id === '') return { map, error: null }
  if (prep.required?.some((r) => requiredId(r) === id)) return { map, error: null }
  const limit = prep.limit ?? prep.slots.length
  if ((prep.required?.length ?? 0) >= limit) return { map, error: `The limit is ${limit}: raise it before requiring another character.` }
  return { map: withPrep(map, clean({ ...prep, required: [...(prep.required ?? []), id], forbidden: without(prep.forbidden, id), fixed: without(prep.fixed, id) })), error: null }
}

export function removeRequired(map: MapFile, id: string): MapFile {
  const prep = map.prep
  if (!prep) return map
  return withPrep(map, clean({ ...prep, required: (prep.required ?? []).filter((r) => requiredId(r) !== id) }))
}

/** Fix a required character to slot `slot` (a number from 1), or `undefined` for any slot. One slot can be fixed for one character. */
export function setRequiredSlot(map: MapFile, id: string, slot: number | undefined): PrepEdit {
  const prep = map.prep
  if (!prep || !prep.required) return { map, error: null }
  if (slot !== undefined) {
    if (slot < 1 || slot > prep.slots.length) return { map, error: 'That slot does not exist.' }
    const other = prep.required.find((r) => requiredSlot(r) === slot && requiredId(r) !== id)
    if (other) return { map, error: `Slot ${slot} is already fixed for ${requiredId(other)}.` }
  }
  const required = prep.required.map((r): PrepRequired => (requiredId(r) === id ? (slot === undefined ? id : { character: id, slot }) : r))
  return { map: withPrep(map, { ...prep, required }), error: null }
}

export function addForbidden(map: MapFile, id: string): MapFile {
  const prep = map.prep
  if (!prep || id === '' || prep.forbidden?.includes(id)) return map
  return withPrep(map, clean({ ...prep, forbidden: [...(prep.forbidden ?? []), id], required: (prep.required ?? []).filter((r) => requiredId(r) !== id), fixed: without(prep.fixed, id) }))
}

export function removeForbidden(map: MapFile, id: string): MapFile {
  const prep = map.prep
  return prep ? withPrep(map, clean({ ...prep, forbidden: without(prep.forbidden, id) })) : map
}

/** A placed unit that stays where the map puts it. */
export function addFixed(map: MapFile, id: string): MapFile {
  const prep = map.prep
  if (!prep || id === '' || prep.fixed?.includes(id)) return map
  return withPrep(map, clean({ ...prep, fixed: [...(prep.fixed ?? []), id], required: (prep.required ?? []).filter((r) => requiredId(r) !== id), forbidden: without(prep.forbidden, id) }))
}

export function removeFixed(map: MapFile, id: string): MapFile {
  const prep = map.prep
  return prep ? withPrep(map, clean({ ...prep, fixed: without(prep.fixed, id) })) : map
}

// ---- the shop

/** `null` removes the shop; a list (even an empty one) keeps it (the loader refuses an empty one: the checks say so). */
export function setShop(map: MapFile, sells: string[] | null): MapFile {
  const prep = map.prep
  if (!prep) return map
  const { shop: _old, ...rest } = prep
  return withPrep(map, sells === null ? rest : { ...rest, shop: { sells } })
}

// ---- what the canvas draws

/** The deployment squares as numbered overlays, in slot order. */
export function slotOverlays(map: MapFile): Overlay[] {
  return (map.prep?.slots ?? []).map((s, i) => ({ x: s.x, y: s.y, w: 1, h: 1, label: String(i + 1), tone: 'slot' as const }))
}

/** The type of a placed unit (a character's own when it has none). */
export const placedTypeId = (c: MapCtx, u: MapUnit): string => typeIdOf(c.characters, u)
