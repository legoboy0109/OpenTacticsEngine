// Done-test helpers of the "Items" lesson (lessons/items.ts). They read data/items.json and the unit types' starting items.
import type { Item, ProjectDoc } from '../../model/types'

const hasItem = (doc: ProjectDoc, id: string): boolean => (doc.items ?? []).some((i) => i.id === id)

/** A new item starts as "heals 10 HP"; true once its heal is something else (another number, or all HP). */
export const healChanged = (item: Item): boolean =>
  item.kind === 'usable' && (item.heal === 'full' || (typeof item.heal === 'number' && item.heal !== 10))

/** Items of the game that heal differently from a brand-new item. */
export const itemsWithChangedHeal = (doc: ProjectDoc): number => (doc.items ?? []).filter(healChanged).length

/** Unit types that start with at least one item of the game. */
export const unitTypesWithItem = (doc: ProjectDoc): number =>
  (doc.units ?? []).filter((u) => (u.items ?? []).some((e) => hasItem(doc, typeof e === 'string' ? e : e.item))).length

/** Unit types that hand an item of the game to whoever defeats them. */
export const unitTypesDropping = (doc: ProjectDoc): number =>
  (doc.units ?? []).filter((u) => (u.items ?? []).some((e) => typeof e !== 'string' && e.drop === true && hasItem(doc, e.item))).length
