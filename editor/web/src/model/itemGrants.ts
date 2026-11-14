// Lists of items on a unit type, a placed unit, a side's storage: each entry is an id or { item, uses?, drop? }.
// An entry is written as a plain id whenever it says nothing more, and extra keys the editor does not know stay.
import { moveItem } from './util'
import type { ItemEntry } from './types'

export const entryId = (e: ItemEntry): string => (typeof e === 'string' ? e : e.item)
export const entryUses = (e: ItemEntry): number | undefined => (typeof e === 'string' ? undefined : e.uses)
export const entryDrops = (e: ItemEntry): boolean => typeof e !== 'string' && e.drop === true

type EntryPatch = { uses?: number | undefined; drop?: boolean }

/** Sets or clears `uses` / `drop` (undefined or false clears) and normalises the entry. */
export function patchEntry(e: ItemEntry, patch: EntryPatch): ItemEntry {
  const o: Record<string, unknown> = typeof e === 'string' ? { item: e } : { ...e }
  if ('uses' in patch) { if (patch.uses === undefined) delete o.uses; else o.uses = patch.uses }
  if ('drop' in patch) { if (patch.drop) o.drop = true; else delete o.drop }
  return Object.keys(o).length === 1 ? (o.item as string) : (o as ItemEntry)
}

export const addEntry = (list: ItemEntry[] | undefined, item: string): ItemEntry[] => [...(list ?? []), item]

/** A different item has its own number of uses, so what was left of the old one no longer applies. */
export const setEntryItem = (list: ItemEntry[], i: number, item: string): ItemEntry[] =>
  list.map((e, j) => (j === i ? patchEntry(typeof e === 'string' ? item : { ...e, item }, { uses: undefined }) : e))

export const patchEntryAt = (list: ItemEntry[], i: number, patch: EntryPatch): ItemEntry[] =>
  list.map((e, j) => (j === i ? patchEntry(e, patch) : e))

export const removeEntryAt = (list: ItemEntry[], i: number): ItemEntry[] => list.filter((_, j) => j !== i)
export const moveEntry = (list: ItemEntry[], from: number, to: number): ItemEntry[] => moveItem(list, from, to)

/** Entries without the item; the other entries are the same objects. */
export const withoutItem = (list: ItemEntry[], id: string): ItemEntry[] => list.filter((e) => entryId(e) !== id)
