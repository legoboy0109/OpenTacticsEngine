// An ability's "inflict": one condition entry ({ condition, turns?, chance? }) or a list of 1 to 8 of them. The editor reads
// both forms as a list, and writes ONE entry back as a plain object (so a file that never had a list stays as it was) and
// two or more as a list. Keys the editor does not know on an entry are kept.
import { amountFromText } from './fidelityAbility'
import { patchObj } from './util'
import type { Ability } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'

export const INFLICT_MAX = 8

/** The entries as written (objects only), whichever form the file uses. */
export function inflictList(a: Ability): Extra[] {
  const v = a.inflict as unknown
  if (Array.isArray(v)) return v.filter((e): e is Extra => !!e && typeof e === 'object' && !Array.isArray(e))
  return v && typeof v === 'object' ? [v as Extra] : []
}

export interface InflictEntry { condition: string; turns?: number; chance?: number | string }
export const inflictEntry = (e: Extra): InflictEntry => ({
  condition: typeof e.condition === 'string' ? e.condition : '',
  turns: typeof e.turns === 'number' ? e.turns : undefined,
  chance: typeof e.chance === 'number' || typeof e.chance === 'string' ? e.chance : undefined,
})
export const inflictEntries = (a: Ability): InflictEntry[] => inflictList(a).map(inflictEntry)

/** No entries = no key; one = an object; two or more = a list. */
export function writeInflicts(a: Ability, entries: Extra[]): Ability {
  return patchObj(a, { inflict: entries.length === 0 ? undefined : entries.length === 1 ? entries[0] : entries })
}

const withEntry = (a: Ability, i: number, fn: (e: Extra) => Extra): Ability => {
  const list = inflictList(a)
  if (i < 0 || i >= list.length) return a
  return writeInflicts(a, list.map((e, j) => (j === i ? fn(e) : e)))
}

/** A new entry for the first condition that is not used yet (the loader refuses one condition twice); nothing when all are used or the list is full. */
export function addInflict(doc: ProjectDoc, a: Ability): Ability {
  const list = inflictList(a)
  if (list.length >= INFLICT_MAX) return a
  const used = new Set(list.map((e) => e.condition))
  const free = (doc.conditions ?? []).find((c) => !used.has(c.id))
  return free ? writeInflicts(a, [...list, { condition: free.id }]) : a
}
export const canAddInflict = (doc: ProjectDoc, a: Ability): boolean => addInflict(doc, a) !== a

export const removeInflict = (a: Ability, i: number): Ability => {
  const list = inflictList(a)
  return i < 0 || i >= list.length ? a : writeInflicts(a, list.filter((_, j) => j !== i))
}

/** Index === the number of entries starts a new one; condition '' removes the entry. The turns and chance of the entry stay. */
export function setInflictCondition(a: Ability, i: number, condition: string): Ability {
  if (condition === '') return removeInflict(a, i)
  if (i === inflictList(a).length) return writeInflicts(a, [...inflictList(a), { condition }])
  return withEntry(a, i, (e) => ({ ...e, condition }))
}
/** undefined = the condition's own duration (key removed). */
export const setInflictTurns = (a: Ability, i: number, turns: number | undefined): Ability => withEntry(a, i, (e) => patchObj(e, { turns }))
/** "60" -> 60, other text = a formula, '' -> removed (always takes hold). */
export const setInflictChance = (a: Ability, i: number, text: string): Ability => withEntry(a, i, (e) => patchObj(e, { chance: amountFromText(text) }))
