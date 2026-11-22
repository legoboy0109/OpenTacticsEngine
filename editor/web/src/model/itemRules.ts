// rules.items in game.json (docs/ITEMS.md). Defaults are never written, so a game that does not touch these keys keeps
// a game.json without `rules.items`.
import { withGame } from './doc'
import type { ItemRules, ProjectDoc, StorageMode } from './types'

export const INVENTORY_DEFAULT = 5
export const INVENTORY_MIN = 1
export const INVENTORY_MAX = 20

export const carriedWeapons = (doc: ProjectDoc): boolean => doc.game?.rules?.items?.weapons === 'carried'
export const inventorySize = (doc: ProjectDoc): number => doc.game?.rules?.items?.inventorySize ?? INVENTORY_DEFAULT
export type ItemDrops = 'marked' | 'last'
export const itemDrops = (doc: ProjectDoc): ItemDrops => (doc.game?.rules?.items?.drops === 'last' ? 'last' : 'marked')
export const storageMode = (doc: ProjectDoc): StorageMode => doc.game?.rules?.items?.storage ?? 'off'

/** Writes the block with defaults left out; an all-default block removes `rules.items` altogether. */
function withItemRules(doc: ProjectDoc, fn: (r: ItemRules) => ItemRules): ProjectDoc {
  return withGame(doc, (g) => {
    const next: ItemRules = { ...fn(g.rules?.items ?? {}) }
    if (next.weapons === 'fixed') delete next.weapons
    if (next.inventorySize === INVENTORY_DEFAULT) delete next.inventorySize
    if (next.storage === 'off') delete next.storage
    if (next.drops === 'marked') delete next.drops
    const rules = { ...(g.rules ?? {}) }
    if (Object.keys(next).length) rules.items = next
    else delete rules.items
    return { ...g, rules }
  })
}

export const setWeaponsMode = (doc: ProjectDoc, mode: 'fixed' | 'carried'): ProjectDoc =>
  withItemRules(doc, (r) => ({ ...r, weapons: mode }))

export const setInventorySize = (doc: ProjectDoc, n: number): ProjectDoc =>
  withItemRules(doc, (r) => ({ ...r, inventorySize: Math.min(INVENTORY_MAX, Math.max(INVENTORY_MIN, Math.round(n))) }))

export const setStorageMode = (doc: ProjectDoc, mode: StorageMode): ProjectDoc =>
  withItemRules(doc, (r) => ({ ...r, storage: mode }))

/** Weapon items that stop working when weapons go back to "fixed". */
export const weaponItemCount = (doc: ProjectDoc): number => (doc.items ?? []).filter((i) => i.kind === 'weapon').length

/** "marked" (every item marked to drop) is the default and is not written. */
export const setItemDrops = (doc: ProjectDoc, mode: ItemDrops): ProjectDoc =>
  withItemRules(doc, (r) => ({ ...r, drops: mode }))
