// The settings of a staff (an item of kind "staff", docs/SRPG.md section 8), as the Items form edits them: reach, who it works on, area,
// healing, a hit chance, experience. Pure functions on the document. Every edit leaves what the form does not show (a stage-2 or
// stage-3 effect, any key a hand-edited file has) exactly as it is.
import { groupsOf } from './ranks'
import { dropGroupRanks, renameGroupRefs } from './rankRefs'
import type { Extra, Item, ItemTarget, ProjectDoc, StaffArea } from './types'
import { withItems } from './doc'
import { patchObj, setNum, updateById } from './util'

/** (Not items.ts's updateItem: that file imports this one.) */
const updateItem = (doc: ProjectDoc, id: string, patch: Partial<Omit<Item, 'id'>>): ProjectDoc => withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, patch)))

// Limits of the loader (engine/core/data/staff.h).
export const STAFF_RANGE_MAX = 20
export const STAFF_RADIUS_MAX = 10
export const STAFF_XP_MAX = 1000
export const STAFF_HEAL_MAX = 1000000
export const STAFF_EFFECTS_MAX = 4

/** Keys only a staff has; leaving the kind removes them. (`target` is shared with usable items and handled apart.) */
export const STAFF_KEYS = ['group', 'rank', 'rankXp', 'wieldedBy', 'props', 'range', 'area', 'effects', 'hit', 'xp', 'if']

/** What a new staff starts with: a group (one that exists already, else "staff") and a small heal, so the file is valid straight away. */
export const newStaffDefaults = (doc: ProjectDoc): Partial<Item> => ({
  group: (doc.items ?? []).find((i) => i.kind === 'staff' && i.group)?.group ?? 'staff',
  range: 2,
  effects: [{ heal: 10 }],
})

export type ReachMode = 'upto' | 'between' | 'formula'
export type AreaMode = 'single' | 'radius' | 'all'
export type HealKind = 'number' | 'percent' | 'full' | 'formula'

const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))
const statIds = (doc: ProjectDoc): string[] => (doc.game?.stats ?? []).map((s) => s.id)

/** The first of the game's stats that has one of these usual names (else the first stat, else the first name): examples read as the game's own. */
const pickStat = (doc: ProjectDoc, names: string[]): string => names.find((n) => statIds(doc).includes(n)) ?? statIds(doc)[0] ?? names[0]
/** Example formulas shown under the boxes, and what a mode starts with: a = the user, b = the unit hit, w = the staff. */
export const reachExample = (doc: ProjectDoc): string => `1 + a.${pickStat(doc, ['mag', 'int', 'res', 'skl'])} / 3`
export const healExample = (doc: ProjectDoc): string => `w.power + a.${pickStat(doc, ['mag', 'int', 'res', 'str'])} / 2`
export const hitExample = (doc: ProjectDoc): string => `80 + a.${pickStat(doc, ['skill', 'skl', 'mag'])} - b.${pickStat(doc, ['res', 'def'])}`
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
export function reachOf(item: Item): Reach {
  const r = item.range
  if (typeof r === 'string') return { mode: 'formula', min: 1, max: 1, text: r }
  if (Array.isArray(r) && r.length === 2) return { mode: 'between', min: Number(r[0]), max: Number(r[1]), text: '' }
  return { mode: 'upto', min: 1, max: typeof r === 'number' ? r : 1, text: '' }
}

export function setReachMode(doc: ProjectDoc, id: string, mode: ReachMode): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return doc
  const cur = reachOf(item)
  if (cur.mode === mode) return doc
  const max = clampInt(cur.mode === 'formula' ? 3 : cur.max, 1, STAFF_RANGE_MAX)
  if (mode === 'upto') return updateItem(doc, id, { range: max })
  if (mode === 'between') return updateItem(doc, id, { range: [Math.min(cur.min, max), Math.max(2, max)] })
  return updateItem(doc, id, { range: reachExample(doc) })
}
export function setReachMax(doc: ProjectDoc, id: string, n: number): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return doc
  const cur = reachOf(item)
  const max = clampInt(n, 1, STAFF_RANGE_MAX)
  return updateItem(doc, id, { range: cur.mode === 'between' ? [Math.min(cur.min, max), max] : max })
}
export function setReachMin(doc: ProjectDoc, id: string, n: number): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return doc
  const cur = reachOf(item)
  const min = clampInt(n, 1, STAFF_RANGE_MAX)
  return updateItem(doc, id, { range: [min, Math.max(min, cur.max)] })
}
export const setReachFormula = (doc: ProjectDoc, id: string, text: string): ProjectDoc => updateItem(doc, id, { range: text })

// --- who it works on, the area ----------------------------------------------------------------------------------------------

/** "Yourself" takes no reach and no area (the loader refuses both), so choosing it drops them in the same step. */
export function setStaffTarget(doc: ProjectDoc, id: string, target: ItemTarget): ProjectDoc {
  return updateItem(doc, id, target === 'self' ? { target, range: undefined, area: undefined } : { target: target === 'ally' ? undefined : target })
}

export function areaModeOf(item: Item): AreaMode {
  const a = item.area
  return a === 'all' ? 'all' : a && typeof a === 'object' ? 'radius' : 'single'
}
export const radiusOf = (item: Item): number => (item.area && typeof item.area === 'object' ? Number(item.area.radius) : 1)

export function setAreaMode(doc: ProjectDoc, id: string, mode: AreaMode): ProjectDoc {
  const area: StaffArea | undefined = mode === 'single' ? undefined : mode === 'all' ? 'all' : { radius: 1 }
  return areaModeOf(doc.items?.find((x) => x.id === id) ?? ({} as Item)) === mode ? doc : updateItem(doc, id, { area })
}
export const setRadius = (doc: ProjectDoc, id: string, n: number): ProjectDoc => updateItem(doc, id, { area: { radius: clampInt(n, 1, STAFF_RADIUS_MAX) } })

// --- healing (the first effect; further effects and later-stage effects are kept) ---------------------------------------------

export interface Heal { kind: HealKind; value: number; text: string; /** The first effect is a heal the form can edit. */ editable: boolean; /** More effects follow. */ more: number }
export function healOf(item: Item): Heal {
  const fx = item.effects ?? []
  const first = fx[0] as Extra | undefined
  const more = Math.max(0, fx.length - 1)
  if (!first || !('heal' in first)) return { kind: 'number', value: 10, text: '', editable: !first, more }
  const h = first.heal
  if (h === 'full') return { kind: 'full', value: 100, text: '', editable: true, more }
  if (typeof h === 'string') return { kind: 'formula', value: 0, text: h, editable: true, more }
  if (typeof h === 'number') return { kind: 'number', value: h, text: '', editable: true, more }
  if (h && typeof h === 'object' && typeof (h as Extra).percent === 'number') return { kind: 'percent', value: (h as { percent: number }).percent, text: '', editable: true, more }
  return { kind: 'number', value: 10, text: '', editable: false, more }
}

/** Replaces the first effect by a heal (the others stay behind it). */
function setHealValue(doc: ProjectDoc, id: string, heal: unknown): ProjectDoc {
  return updateItem(doc, id, { effects: [{ heal }, ...(doc.items?.find((x) => x.id === id)?.effects ?? []).slice(1)] })
}
export function setHealKind(doc: ProjectDoc, id: string, kind: HealKind): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item || healOf(item).kind === kind) return doc
  return setHealValue(doc, id, kind === 'number' ? 10 : kind === 'percent' ? { percent: 25 } : kind === 'full' ? 'full' : healExample(doc))
}
export const setHealNumber = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setHealValue(doc, id, clampInt(n, 1, STAFF_HEAL_MAX))
export const setHealPercent = (doc: ProjectDoc, id: string, n: number): ProjectDoc => setHealValue(doc, id, { percent: clampInt(n, 1, 100) })
export const setHealFormula = (doc: ProjectDoc, id: string, text: string): ProjectDoc => setHealValue(doc, id, text)

// --- hit chance, experience, numbers ------------------------------------------------------------------------------------------

/** A formula box also takes a plain number; a whole number the loader accepts as a percent is stored as a number. */
export function setHit(doc: ProjectDoc, id: string, text: string | undefined): ProjectDoc {
  if (text === undefined) return updateItem(doc, id, { hit: undefined })
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
