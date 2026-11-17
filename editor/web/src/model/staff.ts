// The settings of a staff (an item of kind "staff", docs/SRPG.md section 8), as the Items form edits them: reach, who it works on, area,
// a hit chance, experience. (The effects are staffEffects.ts.) Pure functions on the document. Every edit leaves what the form does not
// show (an effect of a later stage, any key a hand-edited file has) exactly as it is.
import { groupsOf } from './ranks'
import { dropGroupRanks, renameGroupRefs } from './rankRefs'
import { MOVE_KINDS, hasFx, isLightStaff, isSquareStaff } from './staffFx'
import type { Item, ItemTarget, ProjectDoc, StaffArea } from './types'
import { withItems } from './doc'
import { patchObj, setNum, updateById } from './util'

/** (Not items.ts's updateItem: that file imports this one.) */
const updateItem = (doc: ProjectDoc, id: string, patch: Partial<Omit<Item, 'id'>>): ProjectDoc => withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, patch)))
export const patchItem = updateItem

// Limits of the loader (engine/core/data/staff.h).
export const STAFF_RANGE_MAX = 20
export const STAFF_RADIUS_MAX = 10
export const STAFF_XP_MAX = 1000
export const STAFF_HEAL_MAX = 1000000
export const STAFF_EFFECTS_MAX = 4

/** Keys only a staff has; leaving the kind removes them. (`target` is shared with usable items and handled apart.) */
export const STAFF_KEYS = ['group', 'rank', 'rankXp', 'wieldedBy', 'wieldedByTypes', 'props', 'range', 'area', 'effects', 'hit', 'xp', 'if']

/** What a new staff starts with: a group (one that exists already, else "staff") and a small heal, so the file is valid straight away. */
export const newStaffDefaults = (doc: ProjectDoc): Partial<Item> => ({
  group: (doc.items ?? []).find((i) => i.kind === 'staff' && i.group)?.group ?? 'staff',
  range: 2,
  effects: [{ heal: 10 }],
})

export type ReachMode = 'upto' | 'between' | 'formula'
export type AreaMode = 'single' | 'radius' | 'all'

const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))
const statIds = (doc: ProjectDoc): string[] => (doc.game?.stats ?? []).map((s) => s.id)

/** The first of the game's stats that has one of these usual names (else the first stat, else the first name): examples read as the game's own. */
const pickStat = (doc: ProjectDoc, names: string[]): string => names.find((n) => statIds(doc).includes(n)) ?? statIds(doc)[0] ?? names[0]
/** Example formulas shown under the boxes, and what a mode starts with: a = the user, b = the unit hit, w = the staff. */
export const reachExample = (doc: ProjectDoc): string => `1 + a.${pickStat(doc, ['mag', 'int', 'res', 'skl'])} / 3`
export const healExample = (doc: ProjectDoc): string => `w.power + a.${pickStat(doc, ['mag', 'int', 'res', 'str'])} / 2`
/** Reads both units, in the game's own stat names: the user's magic against the target's resistance, and the distance (a = the user, b = the unit, dist = squares). */
export const hitExample = (doc: ProjectDoc): string => `70 + a.${pickStat(doc, ['mag', 'int', 'skill', 'skl'])} * 5 - b.${pickStat(doc, ['res', 'def'])} * 5 - dist * 2`
export const xpExample = 'a.level + 5'

// --- the group ----------------------------------------------------------------------------------------------------------

/** The staff's group (blank = none, which the loader refuses). When it was the only gear in its group and the new name is not a group
 *  yet, it is a rename: the classes that wield the group and every starting rank in it follow (as for a weapon). */
export function setStaffGroup(doc: ProjectDoc, id: string, text: string): ProjectDoc {
  const old = doc.items?.find((x) => x.id === id)?.group
  const group = text.trim() === '' ? undefined : text
  if (old === group) return doc
  const out = updateItem(doc, id, { group })
  return old && group && !groupsOf(out).includes(old) && !groupsOf(doc).includes(group) ? renameGroupRefs(out, old, group) : out
}

/** After a staff goes (deleted, or no longer a staff): starting ranks in a group nothing has any more go too (the loader refuses them). */
export const dropGroupIfGone = (before: ProjectDoc, after: ProjectDoc): ProjectDoc => {
  let out = after
  for (const g of groupsOf(before)) if (!groupsOf(after).includes(g)) out = dropGroupRanks(out, g)
  return out
}

// --- reach ----------------------------------------------------------------------------------------------------------------

export interface Reach { mode: ReachMode; min: number; max: number; text: string }
/** Reads a reach value as the staff's `range` and a warp's `range` have it: a number, [min, max] or a formula. */
export function reachOfValue(r: unknown): Reach {
  if (typeof r === 'string') return { mode: 'formula', min: 1, max: 1, text: r }
  if (Array.isArray(r) && r.length === 2) return { mode: 'between', min: Number(r[0]), max: Number(r[1]), text: '' }
  return { mode: 'upto', min: 1, max: typeof r === 'number' ? r : 1, text: '' }
}
export const reachOf = (item: Item): Reach => reachOfValue(item.range)

type ReachValue = number | [number, number] | string
/** The reach value after a change of mode / farthest / nearest square; the same rules for the staff's reach and a warp's. */
export function reachWithMode(doc: ProjectDoc, cur: Reach, mode: ReachMode): ReachValue {
  const max = clampInt(cur.mode === 'formula' ? 3 : cur.max, 1, STAFF_RANGE_MAX)
  if (mode === 'upto') return max
  if (mode === 'between') return [Math.min(cur.min, max), Math.max(2, max)]
  return reachExample(doc)
}
export function reachWithMax(cur: Reach, n: number): ReachValue {
  const max = clampInt(n, 1, STAFF_RANGE_MAX)
  return cur.mode === 'between' ? [Math.min(cur.min, max), max] : max
}
export function reachWithMin(cur: Reach, n: number): ReachValue {
  const min = clampInt(n, 1, STAFF_RANGE_MAX)
  return [min, Math.max(min, cur.max)]
}

export function setReachMode(doc: ProjectDoc, id: string, mode: ReachMode): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item || reachOf(item).mode === mode) return doc
  return updateItem(doc, id, { range: reachWithMode(doc, reachOf(item), mode) })
}
export function setReachMax(doc: ProjectDoc, id: string, n: number): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  return item ? updateItem(doc, id, { range: reachWithMax(reachOf(item), n) }) : doc
}
export function setReachMin(doc: ProjectDoc, id: string, n: number): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  return item ? updateItem(doc, id, { range: reachWithMin(reachOf(item), n) }) : doc
}
export const setReachFormula = (doc: ProjectDoc, id: string, text: string): ProjectDoc => updateItem(doc, id, { range: text })

// --- who it works on, the area ----------------------------------------------------------------------------------------------

/** "Yourself" takes no reach and no area (the loader refuses both), so choosing it drops them in the same step. Two choices the loader
 *  refuses are not made at all: "yourself" for a staff that pulls or swaps (it needs another unit), "an enemy" for one that heals. */
export function setStaffTarget(doc: ProjectDoc, id: string, target: ItemTarget): ProjectDoc {
  const cur = doc.items?.find((x) => x.id === id)
  if (isSquareStaff(cur)) return doc // a light or an unlock picks a square: it has no target
  if (target === 'self' && hasFx(cur, ['pull', 'swap'])) return doc
  if (target === 'foe' && hasFx(cur, ['heal'])) return doc
  return updateItem(doc, id, target === 'self' ? { target, range: undefined, area: undefined } : { target: target === 'ally' ? undefined : target })
}

export function areaModeOf(item: Item): AreaMode {
  const a = item.area
  return a === 'all' ? 'all' : a && typeof a === 'object' ? 'radius' : 'single'
}
export const radiusOf = (item: Item): number => (item.area && typeof item.area === 'object' ? Number(item.area.radius) : 1)

/** A warp, pull, swap or repair works on the one unit picked: the loader refuses any other area. */
export const needsSingleArea = (item: Item | undefined): boolean => hasFx(item, [...MOVE_KINDS, 'repair'])

export function setAreaMode(doc: ProjectDoc, id: string, mode: AreaMode): ProjectDoc {
  const cur = doc.items?.find((x) => x.id === id)
  if (mode !== 'single' && (needsSingleArea(cur) || isSquareStaff(cur))) return doc
  const area: StaffArea | undefined = mode === 'single' ? undefined : mode === 'all' ? 'all' : { radius: 1 }
  return areaModeOf(doc.items?.find((x) => x.id === id) ?? ({} as Item)) === mode ? doc : updateItem(doc, id, { area })
}
export const setRadius = (doc: ProjectDoc, id: string, n: number): ProjectDoc => updateItem(doc, id, { area: { radius: clampInt(n, 1, STAFF_RADIUS_MAX) } })

// --- hit chance, experience, numbers ------------------------------------------------------------------------------------------

/** A formula box also takes a plain number; a whole number the loader accepts as a percent is stored as a number. */
export function setHit(doc: ProjectDoc, id: string, text: string | undefined): ProjectDoc {
  if (text === undefined) return updateItem(doc, id, { hit: undefined })
  if (isLightStaff(doc.items?.find((x) => x.id === id))) return doc // a light has no unit to roll for
  return updateItem(doc, id, { hit: /^\d+$/.test(text.trim()) && Number(text) <= 100 ? Number(text) : text })
}
export function setStaffXp(doc: ProjectDoc, id: string, text: string): ProjectDoc {
  const t = text.trim()
  if (t === '') return updateItem(doc, id, { xp: undefined })
  return updateItem(doc, id, { xp: /^\d+$/.test(t) && Number(t) <= STAFF_XP_MAX ? Number(t) : text })
}
export const xpText = (item: Item): string => (item.xp === undefined ? '' : String(item.xp))
export const hitText = (item: Item): string => (item.hit === undefined ? '' : String(item.hit))

export function setStaffProp(doc: ProjectDoc, id: string, key: string, value: number | undefined): ProjectDoc {
  return withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, { props: setNum(x.props, key, value) })))
}
