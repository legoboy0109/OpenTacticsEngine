// Items (data/items.json, docs/ITEMS.md): the list and each item's settings. Lists of items on units, structures and
// maps are itemGrants.ts; clean-up when an item or something it points at is deleted is itemRefs.ts.
import { clone, ids, withItems } from './doc'
import { makeId } from './ids'
import { cleanItem } from './itemRefs'
import { carriedWeapons } from './itemRules'
import { unlistPromotionItem } from './promotion'
import { STAFF_KEYS, dropGroupIfGone, newStaffDefaults } from './staff'
import { moveItem, patchObj, setNum, updateById } from './util'
import type { Item, ItemKind, ProjectDoc } from './types'

export const USES_MAX = 999
/** Uses a limited item gets when "Unlimited" is switched off. */
const DEFAULT_USES: Record<ItemKind, number> = { usable: 3, weapon: 40, accessory: 1, staff: 8 }

export const newItemId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.items), { fallback: 'item' })

/** A new item is a small healing item, so the file is valid straight away. */
export function addItem(doc: ProjectDoc, name: string, id: string = newItemId(doc, name)): ProjectDoc {
  const item: Item = { id, name, kind: 'usable', uses: DEFAULT_USES.usable, heal: 10 }
  return withItems(doc, (list) => [...list, item])
}

export function duplicateItem(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withItems(doc, (list) => {
    const i = list.findIndex((x) => x.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    const copy: Item = { ...clone(list[i]), id: newId ?? makeId(name, ids(list), { fallback: 'item' }), name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
}

/** Removes the item and every place that holds it (starting items, shops, placed units, side storage). */
export function deleteItem(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.items?.some((x) => x.id === id)) return doc
  const cleaned = cleanItem(doc, id)
  // The last staff of a group takes the starting ranks in that group with it (the loader refuses a rank in a group nothing has).
  return dropGroupIfGone(doc, { ...cleaned, items: doc.items.filter((x) => x.id !== id) })
}

export function updateItem(doc: ProjectDoc, id: string, patch: Partial<Omit<Item, 'id'>>): ProjectDoc {
  return withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, patch)))
}

export function moveItemInList(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withItems(doc, (list) => moveItem(list, from, to))
}

/** Fields that only make sense for one kind; the loader refuses them on the others. */
const KIND_FIELDS: Record<ItemKind, string[]> = { usable: ['target', 'heal', 'raise', 'promotes'], weapon: ['weapon'], accessory: ['bonus', 'skills'], staff: STAFF_KEYS }

/** Switches the kind and drops what belonged to the old one. The new kind starts with what makes it valid where that
 *  can be guessed (a heal amount, the first weapon); an accessory's bonus is left for the maker to fill in. */
export function setItemKind(doc: ProjectDoc, id: string, kind: ItemKind): ProjectDoc {
  const wasPromotion = doc.items?.find((x) => x.id === id)?.promotes === true
  const next = withItems(doc, (list) => updateById(list, id, (x) => {
    if (x.kind === kind) return x
    const patch: Record<string, unknown> = { kind }
    if (x.kind === 'staff') patch.target = undefined
    for (const [k, fields] of Object.entries(KIND_FIELDS)) if (k !== kind) for (const f of fields) patch[f] = undefined
    if (kind === 'usable') patch.heal = 10
    // A staff's target has more choices than a usable item's: whatever was there does not carry over either way.
    if (kind === 'staff') Object.assign(patch, newStaffDefaults(doc), { target: undefined })
    if (kind === 'weapon') patch.weapon = doc.weapons?.[0]?.id
    // An accessory is worn, never used up.
    if (kind === 'accessory') patch.uses = undefined
    else if (x.uses === undefined && (kind === 'weapon' || kind === 'staff')) patch.uses = DEFAULT_USES[kind]
    return patchObj(x, patch)
  }))
  // Only a usable item can be a promotion item: promotions that listed it let go of it.
  const unlisted = wasPromotion && kind !== 'usable' ? unlistPromotionItem(next, id) : next
  // A staff that stops being one may have been the last of its group: starting ranks in it go too.
  return dropGroupIfGone(doc, unlisted)
}

/** undefined = unlimited (key removed). */
export function setUses(doc: ProjectDoc, id: string, uses: number | undefined): ProjectDoc {
  return updateItem(doc, id, { uses: uses === undefined ? undefined : Math.min(USES_MAX, Math.max(1, Math.round(uses))) })
}

export function setUnlimited(doc: ProjectDoc, id: string, unlimited: boolean): ProjectDoc {
  const kind = doc.items?.find((x) => x.id === id)?.kind ?? 'usable'
  return setUses(doc, id, unlimited ? undefined : DEFAULT_USES[kind])
}

/** Price in one resource; undefined removes it, and an empty price leaves the key out (= not sold). */
export function setPrice(doc: ProjectDoc, id: string, resource: string, value: number | undefined): ProjectDoc {
  return withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, { price: setNum(x.price, resource, value) })))
}

/** 'full' = all HP; a number; undefined = no healing. */
export function setHeal(doc: ProjectDoc, id: string, heal: number | 'full' | undefined): ProjectDoc {
  return updateItem(doc, id, { heal })
}

/** "Itself" is the default and is not written. */
export function setTarget(doc: ProjectDoc, id: string, target: 'self' | 'ally'): ProjectDoc {
  return updateItem(doc, id, { target: target === 'self' ? undefined : target })
}

export function setStatNumber(doc: ProjectDoc, id: string, field: 'raise' | 'bonus', stat: string, value: number | undefined): ProjectDoc {
  return withItems(doc, (list) => updateById(list, id, (x) => patchObj(x, { [field]: setNum(x[field], stat, value) })))
}

/** Weapon items need the game's weapons to be carried. */
export const weaponKindAvailable = (doc: ProjectDoc): boolean => carriedWeapons(doc)

export const hasPrice = (x: Item): boolean => !!x.price && Object.keys(x.price).length > 0
export const itemName = (doc: ProjectDoc, id: string): string => doc.items?.find((x) => x.id === id)?.name ?? id
