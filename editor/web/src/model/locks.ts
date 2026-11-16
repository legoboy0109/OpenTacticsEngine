// Character-locked weapons (docs/SRPG.md "Character-locked weapons"): a weapon's `wieldedBy` list of character ids. The lock only
// narrows: the class must still be able to use the weapon and the rank still applies. Pure functions on the document; the clean-up
// when a character goes, is renamed or the Characters feature is switched off is here too, so each is one Undo step.
import { carriedWeapons } from './itemRules'
import type { Usage } from './references'
import type { ProjectDoc, Unit, Weapon } from './types'
import { patchObj } from './util'

/** The card shows when weapons are carried and the game has characters; hidden, the key is left exactly as it is. */
export const locksShown = (doc: ProjectDoc): boolean => carriedWeapons(doc) && (doc.characters?.length ?? 0) > 0

export const wieldersOf = (w: Weapon): string[] => w.wieldedBy ?? []

/** Writes the list; an empty list removes the key (absent and empty are the same game). */
export function setWielders(doc: ProjectDoc, weapon: string, ids: string[]): ProjectDoc {
  return { ...doc, weapons: doc.weapons && doc.weapons.map((w) => (w.id === weapon ? patchObj(w, { wieldedBy: ids.length ? ids : undefined }) : w)) }
}

/** The weapons locked to a character (shown on the Characters screen). */
export const weaponsLockedTo = (doc: ProjectDoc, character: string): Weapon[] => (doc.weapons ?? []).filter((w) => w.wieldedBy?.includes(character))

/** Rewrites every list with `fn`; an emptied list drops the key; untouched weapons keep their identity. */
function editLists(doc: ProjectDoc, fn: (ids: string[]) => string[]): ProjectDoc {
  if (!doc.weapons?.some((w) => w.wieldedBy)) return doc
  const weapons = doc.weapons.map((w) => {
    if (!w.wieldedBy) return w
    const next = fn(w.wieldedBy)
    if (next.length === w.wieldedBy.length && next.every((x, i) => x === w.wieldedBy![i])) return w
    return patchObj(w, { wieldedBy: next.length ? next : undefined })
  })
  return { ...doc, weapons }
}

/** A character is deleted: it comes off every list (a list left empty is no lock, so the key goes). */
export const dropCharacterLocks = (doc: ProjectDoc, id: string): ProjectDoc => editLists(doc, (ids) => ids.filter((x) => x !== id))
/** A character's id changes: the lists follow. */
export const renameCharacterLocks = (doc: ProjectDoc, from: string, to: string): ProjectDoc => editLists(doc, (ids) => ids.map((x) => (x === from ? to : x)))
/** The Characters feature is switched off: no weapon keeps a list (the loader refuses a list without characters). */
export const cleanAllLocks = (doc: ProjectDoc): ProjectDoc => editLists(doc, () => [])

/** How many weapons have a lock (for the "are you sure" question when characters are switched off). */
export const lockCount = (doc: ProjectDoc): number => (doc.weapons ?? []).filter((w) => w.wieldedBy?.length).length

/** Which weapons list this character (for the delete question). */
export const lockUsageLines = (doc: ProjectDoc, id: string): Usage[] =>
  weaponsLockedTo(doc, id).map((w) => ({ section: 'weapons' as const, text: `Weapon “${w.name}” is locked to named characters, and this one is on its list (it comes off the list)` }))

/** The class can use the weapon: it is on the class's own list, or its group is one the class wields (docs/ITEMS.md). */
export const classCanUse = (u: Unit | undefined, w: Weapon): boolean => !!u && (!!u.weapons?.includes(w.id) || (!!w.group && !!u.wields?.includes(w.group)))
