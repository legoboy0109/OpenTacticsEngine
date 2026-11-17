// The places besides staves and skills that use status conditions (docs/SRPG.md section 9): a usable item cures conditions
// (`"cure": ["poison"]` or `"all"`) and may put one on the unit it is used on (`"inflict": { "condition", "turns" }`); a weapon may put one on the unit
// it strikes (`"inflict": { "condition", "chance", "turns" }`). Pure edits on the document; each part is removed with its key.
import { withItems, withWeapons } from './doc'
import { CONDITION_TURNS_MAX, conditionList } from './conditions'
import { countsTurns } from './staffConditions'
import type { Item, ProjectDoc, Weapon } from './types'
import { patchObj, updateById } from './util'

const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))
const patchItem = (doc: ProjectDoc, id: string, patch: Partial<Omit<Item, 'id'>>): ProjectDoc => withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, patch)))
const patchWeapon = (doc: ProjectDoc, id: string, patch: Partial<Omit<Weapon, 'id'>>): ProjectDoc => withWeapons(doc, (list) => updateById(list, id, (x) => patchObj(x, patch)))
const itemOf = (doc: ProjectDoc, id: string) => doc.items?.find((x) => x.id === id)

// --- a usable item: cures ------------------------------------------------------------------------------------------------------

export type CureMode = 'none' | 'list' | 'all'
export interface ItemCure { mode: CureMode; ids: string[] }
export function itemCureOf(item: Item): ItemCure {
  if (item.cure === 'all') return { mode: 'all', ids: [] }
  if (Array.isArray(item.cure)) return { mode: 'list', ids: item.cure.filter((x): x is string => typeof x === 'string') }
  return { mode: 'none', ids: [] }
}

/** "Cures": nothing / every condition. ("The conditions I pick" is only written with the first pick, `setItemCureIds`: the loader refuses an
 *  empty list, so the form keeps that choice unsaved until something is ticked.) */
export function setItemCureMode(doc: ProjectDoc, id: string, mode: 'none' | 'all'): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || itemCureOf(item).mode === mode) return doc
  return patchItem(doc, id, { cure: mode === 'all' ? 'all' : undefined })
}
/** The listed conditions in the game's order (from any mode: the first pick starts the list); unticking the last one switches curing off. */
export function setItemCureIds(doc: ProjectDoc, id: string, ids: string[]): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item) return doc
  if (itemCureOf(item).mode !== 'list' && ids.length === 0) return doc
  return patchItem(doc, id, { cure: ids.length ? ids : undefined })
}

// --- a usable item: inflicts ---------------------------------------------------------------------------------------------------

export function itemInflictOf(item: Item): { on: boolean; condition: string; turns: number | undefined } {
  const v = item.inflict
  if (!v || typeof v !== 'object') return { on: false, condition: '', turns: undefined }
  return { on: true, condition: typeof v.condition === 'string' ? v.condition : '', turns: typeof v.turns === 'number' ? v.turns : undefined }
}
const inflictValue = (condition: string, turns: number | undefined, extra?: object) => ({ ...extra, condition, ...(turns !== undefined ? { turns } : {}) })

export function setItemInflictOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || itemInflictOf(item).on === on) return doc
  if (!on) return patchItem(doc, id, { inflict: undefined })
  const first = conditionList(doc)[0]
  return first ? patchItem(doc, id, { inflict: { condition: first.id } }) : doc
}
export function setItemInflictCondition(doc: ProjectDoc, id: string, condition: string): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || !item.inflict) return doc
  return patchItem(doc, id, { inflict: inflictValue(condition, countsTurns(doc, condition) ? itemInflictOf(item).turns : undefined) })
}
export function setItemInflictTurns(doc: ProjectDoc, id: string, turns: number | undefined): ProjectDoc {
  const item = itemOf(doc, id)
  if (!item || !item.inflict) return doc
  return patchItem(doc, id, { inflict: inflictValue(itemInflictOf(item).condition, turns === undefined ? undefined : clampInt(turns, 1, CONDITION_TURNS_MAX)) })
}

// --- a weapon: may inflict on a hit --------------------------------------------------------------------------------------------

export type ChanceKind = 'always' | 'percent' | 'formula'
export interface WeaponInflict { on: boolean; condition: string; chanceKind: ChanceKind; percent: number; formula: string; turns: number | undefined }
export function weaponInflictOf(w: Weapon): WeaponInflict {
  const v = w.inflict
  if (!v || typeof v !== 'object') return { on: false, condition: '', chanceKind: 'always', percent: 30, formula: '', turns: undefined }
  const ch = v.chance
  return {
    on: true,
    condition: typeof v.condition === 'string' ? v.condition : '',
    chanceKind: typeof ch === 'string' ? 'formula' : typeof ch === 'number' && ch < 100 ? 'percent' : 'always',
    percent: typeof ch === 'number' ? ch : 30,
    formula: typeof ch === 'string' ? ch : '',
    turns: typeof v.turns === 'number' ? v.turns : undefined,
  }
}
const weaponValue = (s: Pick<WeaponInflict, 'condition' | 'chanceKind' | 'percent' | 'formula' | 'turns'>) => ({
  condition: s.condition,
  ...(s.chanceKind === 'percent' ? { chance: s.percent } : s.chanceKind === 'formula' ? { chance: s.formula } : {}),
  ...(s.turns !== undefined ? { turns: s.turns } : {}),
})
function putWeapon(doc: ProjectDoc, id: string, fn: (s: WeaponInflict) => WeaponInflict): ProjectDoc {
  const w = doc.weapons?.find((x) => x.id === id)
  if (!w || !w.inflict) return doc
  return patchWeapon(doc, id, { inflict: weaponValue(fn(weaponInflictOf(w))) })
}

export function setWeaponInflictOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const w = doc.weapons?.find((x) => x.id === id)
  if (!w || weaponInflictOf(w).on === on) return doc
  if (!on) return patchWeapon(doc, id, { inflict: undefined })
  const first = conditionList(doc)[0]
  return first ? patchWeapon(doc, id, { inflict: { condition: first.id, chance: 30 } }) : doc
}
export const setWeaponInflictCondition = (doc: ProjectDoc, id: string, condition: string): ProjectDoc =>
  putWeapon(doc, id, (s) => ({ ...s, condition, turns: countsTurns(doc, condition) ? s.turns : undefined }))
export const setWeaponInflictTurns = (doc: ProjectDoc, id: string, turns: number | undefined): ProjectDoc =>
  putWeapon(doc, id, (s) => ({ ...s, turns: turns === undefined ? undefined : clampInt(turns, 1, CONDITION_TURNS_MAX) }))
/** Every hit / a percent / a formula that reads the wielder (a) and the unit struck (b). */
export function setWeaponChanceKind(doc: ProjectDoc, id: string, kind: ChanceKind): ProjectDoc {
  return putWeapon(doc, id, (s) => (s.chanceKind === kind ? s : { ...s, chanceKind: kind, ...(kind === 'formula' && s.formula === '' ? { formula: weaponChanceExample(doc) } : {}) }))
}
export const setWeaponChancePercent = (doc: ProjectDoc, id: string, n: number): ProjectDoc => putWeapon(doc, id, (s) => ({ ...s, chanceKind: 'percent', percent: clampInt(n, 1, 99) }))
export const setWeaponChanceFormula = (doc: ProjectDoc, id: string, text: string): ProjectDoc => putWeapon(doc, id, (s) => ({ ...s, chanceKind: 'formula', formula: text }))

/** A chance formula that reads both fighters in the game's own stat names (a = the wielder, b = the unit struck). */
export function weaponChanceExample(doc: ProjectDoc): string {
  const ids = (doc.game?.stats ?? []).map((s) => s.id)
  const pick = (names: string[]) => names.find((n) => ids.includes(n)) ?? ids[0] ?? names[0]
  return `60 + a.${pick(['skl', 'skill', 'mag'])} - b.${pick(['res', 'def'])} * 5`
}
