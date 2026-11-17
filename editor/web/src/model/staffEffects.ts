// What a staff does (docs/SRPG.md section 8, stage one and two), as the "What it does" card edits it: heal, move a unit (warp / pull /
// swap), repair an item, or light an area. The loader's rules about what can be put together (engine/core/data/loader_staves.cpp
// checkCombination) are kept here as refusals: an edit that would make a file the loader refuses simply is not made, and the form greys
// the same choices out (so the model and the screen agree). The order of the effects in a file never matters: each part is found by its
// kind, replaced where it stands, and an effect this file does not know stays exactly as it is.
import { MOVE_KINDS, STAFF_EFFECT_KINDS, fxIndex, fxKind, fxList, hasFx, isLightStaff, isSquareStaff, type MoveKind } from './staffFx'
import { STAFF_EFFECTS_MAX, STAFF_HEAL_MAX, STAFF_RADIUS_MAX, healExample, patchItem, reachOfValue, reachWithMax, reachWithMin, reachWithMode, type Reach, type ReachMode } from './staff'
import type { Extra, Item, ProjectDoc } from './types'

export const STAFF_REPAIR_MAX = 999
export const STAFF_LIGHT_TURNS_MAX = 10
export const WARP_DEFAULT_RANGE = 3

export type HealKind = 'number' | 'percent' | 'full' | 'formula'
export type RepairKind = 'full' | 'uses'

const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))
const itemOf = (doc: ProjectDoc, id: string): Item | undefined => doc.items?.find((x) => x.id === id)
export const room = (item: Item | undefined): boolean => fxList(item).length < STAFF_EFFECTS_MAX

/** Replaces the first effect of one of these kinds where it stands, appends the effect when there is none, removes it when `fx` is
 *  undefined. Everything else in the list is left as it is. */
export function putFx(doc: ProjectDoc, id: string, kinds: readonly string[], fx: Extra | undefined): ProjectDoc {
  const list = [...fxList(itemOf(doc, id))]
  const at = fxIndex(itemOf(doc, id), kinds)
  if (at >= 0 && fx) list[at] = fx
  else if (at >= 0) list.splice(at, 1)
  else if (fx) list.push(fx)
  return patchItem(doc, id, { effects: list })
}

/** The unit-picked area the loader wants for a warp, pull, swap or repair: "single" (the same as no area key). */
const singleArea = (item: Item | undefined): Partial<Item> => (item?.area === undefined || item.area === 'single' ? {} : { area: undefined })

/** A part can be turned off only while another effect is left (a staff needs at least one). */
export const canDropEffect = (item: Item | undefined): boolean => fxList(item).length > 1

// --- healing ------------------------------------------------------------------------------------------------------------------

export interface Heal { on: boolean; kind: HealKind; value: number; text: string; /** The heal's value is one the form can edit. */ editable: boolean }
export function healOf(item: Item): Heal {
  const at = fxIndex(item, ['heal'])
  if (at < 0) return { on: false, kind: 'number', value: 10, text: '', editable: true }
  const h = (fxList(item)[at] as Extra).heal
  if (h === 'full') return { on: true, kind: 'full', value: 100, text: '', editable: true }
  if (typeof h === 'string') return { on: true, kind: 'formula', value: 0, text: h, editable: true }
  if (typeof h === 'number') return { on: true, kind: 'number', value: h, text: '', editable: true }
  if (h && typeof h === 'object' && typeof (h as Extra).percent === 'number') return { on: true, kind: 'percent', value: (h as { percent: number }).percent, text: '', editable: true }
  return { on: true, kind: 'number', value: 10, text: '', editable: false }
}

/** Why healing cannot be switched on now, or '' when it can. */
export function healBlock(item: Item): string {
  if (isSquareStaff(item)) return 'light'
  if (item.target === 'foe') return 'foe'
  return room(item) ? '' : 'full'
}

export function setHealOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || healOf(item).on === on) return doc
  if (on) return healBlock(item) ? doc : putFx(doc, id, ['heal'], { heal: 10 })
  return canDropEffect(item) ? putFx(doc, id, ['heal'], undefined) : doc
}
const setHealValue = (doc: ProjectDoc, id: string, heal: unknown): ProjectDoc => (itemOf(doc, id) && hasFx(itemOf(doc, id), ['heal']) ? putFx(doc, id, ['heal'], { heal }) : doc)
export function setHealKind(doc: ProjectDoc, id: string, kind: HealKind): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || healOf(item).kind === kind) return doc
  return setHealValue(doc, id, kind === 'number' ? 10 : kind === 'percent' ? { percent: 25 } : kind === 'full' ? 'full' : healExample(doc))
}
export const setHealNumber = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setHealValue(doc, id, clampInt(n, 1, STAFF_HEAL_MAX))
export const setHealPercent = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setHealValue(doc, id, { percent: clampInt(n, 1, 100) })
export const setHealFormula = (doc: ProjectDoc, id: string, text: string): ProjectDoc => setHealValue(doc, id, text)

// --- moving a unit ------------------------------------------------------------------------------------------------------------

export interface Move { kind: 'none' | MoveKind; /** The warp's reach (how far from the user the unit may land); only read when kind is "warp". */ warp: Reach }
export function moveOf(item: Item): Move {
  const at = fxIndex(item, MOVE_KINDS)
  if (at < 0) return { kind: 'none', warp: reachOfValue(WARP_DEFAULT_RANGE) }
  const kind = fxKind(fxList(item)[at]) as MoveKind
  const w = (fxList(item)[at] as Extra).warp as { range?: unknown } | undefined
  return { kind, warp: reachOfValue(kind === 'warp' && w && typeof w === 'object' ? w.range : WARP_DEFAULT_RANGE) }
}

/** What stops this kind of move now: 'repair' (a repair is there), 'self' (pull and swap need another unit), 'light', 'full' (4 effects
 *  already), or '' when it can be chosen. */
export function moveBlock(item: Item, kind: MoveKind): string {
  if (isSquareStaff(item)) return 'light'
  if (hasFx(item, ['repair'])) return 'repair'
  if ((kind === 'pull' || kind === 'swap') && item.target === 'self') return 'self'
  return hasFx(item, MOVE_KINDS) || room(item) ? '' : 'full'
}

const moveFx = (kind: MoveKind, range: unknown): Extra => (kind === 'warp' ? { warp: { range } } : { [kind]: true })

export function setMoveKind(doc: ProjectDoc, id: string, kind: 'none' | MoveKind): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || moveOf(item).kind === kind) return doc
  if (kind === 'none') return canDropEffect(item) ? putFx(doc, id, MOVE_KINDS, undefined) : doc
  if (moveBlock(item, kind)) return doc
  // A move works on the one unit picked, so a wider area goes in the same step.
  return patchItem(putFx(doc, id, MOVE_KINDS, moveFx(kind, WARP_DEFAULT_RANGE)), id, singleArea(item))
}

const setWarpRange = (doc: ProjectDoc, id: string, range: unknown): ProjectDoc => (moveOf(itemOf(doc, id) ?? ({} as Item)).kind === 'warp' ? putFx(doc, id, MOVE_KINDS, moveFx('warp', range)) : doc)
export function setWarpMode(doc: ProjectDoc, id: string, mode: ReachMode): ProjectDoc {
  const item = itemOf(doc, id)
  return item && moveOf(item).warp.mode !== mode ? setWarpRange(doc, id, reachWithMode(doc, moveOf(item).warp, mode)) : doc
}
export const setWarpMax = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setWarpRange(doc, id, reachWithMax(moveOf(itemOf(doc, id) ?? ({} as Item)).warp, n))
export const setWarpMin = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setWarpRange(doc, id, reachWithMin(moveOf(itemOf(doc, id) ?? ({} as Item)).warp, n))
export const setWarpFormula = (doc: ProjectDoc, id: string, text: string): ProjectDoc => setWarpRange(doc, id, text)

// --- repairing an item --------------------------------------------------------------------------------------------------------

export interface Repair { on: boolean; kind: RepairKind; uses: number }
export function repairOf(item: Item): Repair {
  const at = fxIndex(item, ['repair'])
  if (at < 0) return { on: false, kind: 'full', uses: 3 }
  const v = (fxList(item)[at] as Extra).repair
  return typeof v === 'number' ? { on: true, kind: 'uses', uses: v } : { on: true, kind: 'full', uses: 3 }
}

/** What stops repairing from being switched on: 'move', 'light', 'full', or ''. */
export function repairBlock(item: Item): string {
  if (isSquareStaff(item)) return 'light'
  if (hasFx(item, MOVE_KINDS)) return 'move'
  return room(item) ? '' : 'full'
}

export function setRepairOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || repairOf(item).on === on) return doc
  if (!on) return canDropEffect(item) ? putFx(doc, id, ['repair'], undefined) : doc
  if (repairBlock(item)) return doc
  return patchItem(putFx(doc, id, ['repair'], { repair: 'full' }), id, singleArea(item))
}
const setRepairValue = (doc: ProjectDoc, id: string, v: 'full' | number): ProjectDoc => (hasFx(itemOf(doc, id), ['repair']) ? putFx(doc, id, ['repair'], { repair: v }) : doc)
export function setRepairKind(doc: ProjectDoc, id: string, kind: RepairKind): ProjectDoc {
  const item = itemOf(doc, id)
  return item && repairOf(item).kind !== kind ? setRepairValue(doc, id, kind === 'full' ? 'full' : 3) : doc
}
export const setRepairUses = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setRepairValue(doc, id, clampInt(n, 1, STAFF_REPAIR_MAX))

// --- lighting an area (a light staff is exactly one light effect: no target, area, chance of missing or conditions) ------------------

export interface Light { on: boolean; radius: number; turns: number; shrink?: number }
export function lightOf(item: Item): Light {
  const at = fxIndex(item, ['light'])
  if (at < 0) return { on: false, radius: 2, turns: 1 }
  const l = (fxList(item)[at] as Extra).light as { radius?: unknown; turns?: unknown; shrink?: unknown } | undefined
  const out: Light = { on: true, radius: typeof l?.radius === 'number' ? l.radius : 2, turns: typeof l?.turns === 'number' ? l.turns : 1 }
  if (typeof l?.shrink === 'number') out.shrink = l.shrink
  return out
}

/** Effects of a kind the form does not know (an unknown kind, a malformed entry). A light staff has no room for them, and they are never
 *  dropped, so switching light on is refused while there are some. */
export const unknownEffectCount = (item: Item | undefined): number => fxList(item).filter((e) => { const k = fxKind(e); return !k || !STAFF_EFFECT_KINDS.includes(k) }).length

/** Switching light on replaces the other effects and removes target, area, chance of missing and conditions in the same step (one Undo
 *  brings them back), as "only the user" removes reach and area. Switching it off leaves a small heal, so the staff stays valid. */
export function setLightOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || isLightStaff(item) === on) return doc
  if (!on) return patchItem(doc, id, { effects: [{ heal: 10 }] })
  if (unknownEffectCount(item) > 0) return doc
  return patchItem(doc, id, { effects: [{ light: { radius: 2, turns: 1 } }], target: undefined, area: undefined, hit: undefined, if: undefined, range: item.range ?? 2 })
}
function setLightPart(doc: ProjectDoc, id: string, patch: Record<string, number>): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || !isLightStaff(item)) return doc
  const cur = (fxList(item)[fxIndex(item, ['light'])] as Extra).light
  return putFx(doc, id, ['light'], { light: { ...(cur && typeof cur === 'object' ? cur : {}), ...patch } })
}
/** A torch: the radius goes down by this much each turn (docs/SRPG.md "Skill blocks, third set" item 9). Empty / 0 removes it; it must stay below the radius. */
export function setLightShrink(doc: ProjectDoc, id: string, n: number | undefined): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || !isLightStaff(item)) return doc
  const cur = { ...((fxList(item)[fxIndex(item, ['light'])] as Extra).light as Record<string, unknown>) }
  if (n === undefined || n <= 0) delete cur.shrink
  else cur.shrink = clampInt(n, 1, Math.max(1, lightOf(item).radius - 1))
  return putFx(doc, id, ['light'], { light: cur })
}
export const setLightRadius = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setLightPart(doc, id, { radius: clampInt(n, 1, STAFF_RADIUS_MAX) })
export const setLightTurns = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setLightPart(doc, id, { turns: clampInt(n, 1, STAFF_LIGHT_TURNS_MAX) })

// --- an item that wears out: can a repair staff mend it? ---------------------------------------------------------------------

export const hasRepairStaff = (doc: ProjectDoc): boolean => (doc.items ?? []).some((i) => i.kind === 'staff' && hasFx(i, ['repair']))
/** Items the repair effect can ever offer: limited uses and not marked `"repairable": false`. */
export const repairableItems = (doc: ProjectDoc): Item[] => (doc.items ?? []).filter((i) => i.uses !== undefined && i.repairable !== false)

/** The switch shows for an item with limited uses once the game has a repair staff, or when the item already carries the key. */
export const repairableShown = (doc: ProjectDoc, item: Item): boolean => item.uses !== undefined && (hasRepairStaff(doc) || item.repairable !== undefined)

/** On = the key is absent (the default); off writes `"repairable": false`. */
export const setRepairable = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => patchItem(doc, id, { repairable: on ? undefined : false })
