// A key is a usable item with an `opens` list (docs/SRPG.md section 10): "door", "chest" and/or ids of structure kinds. The Items form shows it as an
// "It opens" part. Which doors and chests exist is places.ts; the checks are placeChecks.ts.
import { updateItem } from './items'
import { placeKindOf, isKeyItem } from './places'
import type { Item, ProjectDoc } from './types'

export { isKeyItem }

/** What a new key opens: any door. */
export const DEFAULT_OPENS = ['door']

/** A key does nothing else by default: a fresh usable item heals 10, and keeping that would make the key a potion too, so switching "It opens" on clears an
 *  untouched default heal (a maker's own numbers stay). Switching it off removes the list only. */
export function setKeyOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item || item.kind !== 'usable' || isKeyItem(item) === on) return doc
  if (!on) return updateItem(doc, id, { opens: undefined })
  const plain = item.heal === 10 && !item.raise && !item.promotes && item.cure === undefined && item.inflict === undefined
  return updateItem(doc, id, { opens: [...DEFAULT_OPENS], ...(plain ? { heal: undefined } : {}) })
}

/** The list of things the key opens; an empty list is not written (the loader refuses it), so it is ignored while the part is on. */
export function setKeyOpens(doc: ProjectDoc, id: string, opens: string[]): ProjectDoc {
  return opens.length === 0 ? doc : updateItem(doc, id, { opens })
}

export interface OpensChoice { value: string; label: string; group: 'any' | 'kind' }
/** What can be ticked: any door, any chest, then each door and chest kind of the game. */
export function opensChoices(doc: ProjectDoc): OpensChoice[] {
  const kinds = (doc.structures ?? []).flatMap((s): OpensChoice[] => {
    const k = placeKindOf(s)
    return k === 'door' || k === 'chest' ? [{ value: s.id, label: `${s.name} (${k})`, group: 'kind' }] : []
  })
  return [{ value: 'door', label: 'Any door', group: 'any' }, { value: 'chest', label: 'Any chest', group: 'any' }, ...kinds]
}

/** True when the game has door or chest kinds (so the "It opens" part is worth offering on every usable item). */
export const gameHasLocks = (doc: ProjectDoc): boolean => (doc.structures ?? []).some((s) => { const k = placeKindOf(s); return k === 'door' || k === 'chest' })

export const keyItems = (doc: ProjectDoc): Item[] => (doc.items ?? []).filter(isKeyItem)
