// The two staff parts that use status conditions (docs/SRPG.md section 9): "It inflicts a condition" (`{ "inflict": { "condition": id, "turns": n } }`)
// and "It cures" (`{ "cure": [ids] }` or `{ "cure": "all" }`). Each is found by its kind, replaced where it stands, or added to the end; a staff has at
// most one of each (the loader refuses two) and at most four effects. A light staff is exactly one light effect, so neither can be added to one.
// The loader's other rules about effects (engine/core/data/loader_staves.cpp checkCombination) do not touch these two: an area is allowed, and they
// go with healing, repairing and a move.
import { conditionList } from './conditions'
import { CONDITION_TURNS_MAX } from './conditions'
import { STAFF_EFFECTS_MAX } from './staff'
import { putFx, room } from './staffEffects'
import { fxIndex, fxList, isSquareStaff } from './staffFx'
import type { CureValue, Extra, Item, ProjectDoc } from './types'

const itemOf = (doc: ProjectDoc, id: string): Item | undefined => doc.items?.find((x) => x.id === id)
const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

/** Whether a condition counts turns (an optional `turns` only means something for those). */
export const countsTurns = (doc: ProjectDoc, id: string | undefined): boolean => !!doc.conditions?.find((c) => c.id === id) && doc.conditions.find((c) => c.id === id)!.until === undefined

/** Why the part cannot be switched on now: 'light' (a light staff has no other effect), 'full' (four effects already), 'none' (the game has no conditions), or ''. */
export function conditionPartBlock(doc: ProjectDoc, item: Item, kind: 'inflict' | 'cure'): '' | 'light' | 'full' | 'none' {
  if (isSquareStaff(item)) return 'light'
  if (fxIndex(item, [kind]) >= 0) return ''
  if (conditionList(doc).length === 0) return 'none'
  return room(item) ? '' : 'full'
}

// --- It inflicts a condition ---------------------------------------------------------------------------------------------------

export interface StaffInflict { on: boolean; condition: string; turns: number | undefined; /** The effect is written in a way the form cannot show. */ editable: boolean }
export function inflictOf(item: Item): StaffInflict {
  const at = fxIndex(item, ['inflict'])
  if (at < 0) return { on: false, condition: '', turns: undefined, editable: true }
  const v = (fxList(item)[at] as Extra).inflict as Extra | undefined
  if (!v || typeof v !== 'object' || Array.isArray(v) || typeof v.condition !== 'string') return { on: true, condition: '', turns: undefined, editable: false }
  return { on: true, condition: v.condition, turns: typeof v.turns === 'number' ? v.turns : undefined, editable: true }
}

const inflictFx = (condition: string, turns: number | undefined): Extra => ({ inflict: { condition, ...(turns !== undefined ? { turns } : {}) } })

export function setInflictOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || inflictOf(item).on === on) return doc
  if (!on) return fxList(item).length > 1 ? putFx(doc, id, ['inflict'], undefined) : doc
  if (conditionPartBlock(doc, item, 'inflict')) return doc
  return putFx(doc, id, ['inflict'], inflictFx(conditionList(doc)[0].id, undefined))
}
/** Another condition; the turns go when the new one does not count turns. */
export function setInflictCondition(doc: ProjectDoc, id: string, condition: string): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || !inflictOf(item).on) return doc
  const turns = inflictOf(item).turns
  return putFx(doc, id, ['inflict'], inflictFx(condition, countsTurns(doc, condition) ? turns : undefined))
}
/** The number of turns that replaces the condition's own; undefined = the condition's own. */
export function setInflictTurns(doc: ProjectDoc, id: string, turns: number | undefined): ProjectDoc {
  const item = itemOf(doc, id)
  const cur = item && inflictOf(item)
  if (!cur || !cur.on || !cur.editable) return doc
  return putFx(doc, id, ['inflict'], inflictFx(cur.condition, turns === undefined ? undefined : clampInt(turns, 1, CONDITION_TURNS_MAX)))
}

// --- It cures ---------------------------------------------------------------------------------------------------------------

export interface StaffCure { on: boolean; all: boolean; ids: string[]; editable: boolean; kind?: 'harmful' | 'helpful' }
export function cureOf(item: Item): StaffCure {
  const at = fxIndex(item, ['cure'])
  if (at < 0) return { on: false, all: false, ids: [], editable: true }
  const v = (fxList(item)[at] as Extra).cure
  if (v === 'all') return { on: true, all: true, ids: [], editable: true }
  if (v === 'harmful' || v === 'helpful') return { on: true, all: false, ids: [], editable: true, kind: v }  // every condition of the kind
  if (Array.isArray(v) && v.every((x) => typeof x === 'string')) return { on: true, all: false, ids: v as string[], editable: true }
  return { on: true, all: false, ids: [], editable: false }
}

/** Switches the part off (the last effect of a staff cannot go). */
export function dropCure(doc: ProjectDoc, id: string): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || !cureOf(item).on) return doc
  return fxList(item).length > 1 ? putFx(doc, id, ['cure'], undefined) : doc
}
/** Sets what it cures: `'all'` or at least one condition (the loader refuses an empty list, so an empty pick changes nothing: the form keeps
 *  the part unsaved until one is picked). Adds the part when it is not there yet (refused when a light staff, four effects, or no conditions). */
export function setCure(doc: ProjectDoc, id: string, what: CureValue): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || (Array.isArray(what) && what.length === 0)) return doc
  const cur = cureOf(item)
  if (!cur.on && conditionPartBlock(doc, item, 'cure')) return doc
  if (cur.on && !cur.editable) return doc
  return putFx(doc, id, ['cure'], { cure: what })
}

/** A usable staff's four effects, for the form's "a staff has at most four effects" note. */
export const effectRoom = (item: Item): boolean => fxList(item).length < STAFF_EFFECTS_MAX
