// Character-locked weapons and staves (docs/SRPG.md "Character-locked weapons", section 8): a `wieldedBy` list of character ids. The lock
// only narrows: the class must still be able to use the weapon and the rank still applies. Pure functions on the document; the clean-up
// when a character goes, is renamed or the Characters feature is switched off is here too, so each is one Undo step. A staff is the
// same as a weapon here (model/gear.ts), which is why every list below covers both.
import { allGear, editGear, gearOf, patchGear, type GearKeys, type GearOwner, type GearRef } from './gear'
import { carriedWeapons } from './itemRules'
import type { Usage } from './references'
import type { ProjectDoc, Unit } from './types'

/** The card shows when the game has characters, and for a weapon when weapons are carried too (a staff needs no carried weapons).
 *  Hidden, the key is left exactly as it is. */
export const locksShown = (doc: ProjectDoc, owner: GearOwner = 'weapon'): boolean => (owner === 'item' || carriedWeapons(doc)) && (doc.characters?.length ?? 0) > 0

export const wieldersOf = (w: { wieldedBy?: string[] }): string[] => w.wieldedBy ?? []

/** Writes the list; an empty list removes the key (absent and empty are the same game). */
export const setGearWielders = (doc: ProjectDoc, owner: GearOwner, id: string, ids: string[]): ProjectDoc => patchGear(doc, owner, id, { wieldedBy: ids.length ? ids : undefined })
export const setWielders = (doc: ProjectDoc, weapon: string, ids: string[]): ProjectDoc => setGearWielders(doc, 'weapon', weapon, ids)

/** The weapons and staves locked to a character (shown on the Characters screen). */
export const gearLockedTo = (doc: ProjectDoc, character: string): GearRef[] => allGear(doc).filter((g) => g.keys.wieldedBy?.includes(character))

/** Rewrites every list with `fn`; an emptied list drops the key; untouched gear keeps its identity. */
function editLists(doc: ProjectDoc, fn: (ids: string[]) => string[]): ProjectDoc {
  if (!allGear(doc).some((g) => g.keys.wieldedBy)) return doc
  return editGear(doc, <T extends GearKeys>(w: T): T => {
    if (!w.wieldedBy) return w
    const next = fn(w.wieldedBy)
    if (next.length === w.wieldedBy.length && next.every((x, i) => x === w.wieldedBy![i])) return w
    const c: T = { ...w }
    if (next.length) c.wieldedBy = next; else delete c.wieldedBy
    return c
  })
}

/** A character is deleted: it comes off every list (a list left empty is no lock, so the key goes). */
export const dropCharacterLocks = (doc: ProjectDoc, id: string): ProjectDoc => editLists(doc, (ids) => ids.filter((x) => x !== id))
/** A character's id changes: the lists follow. */
export const renameCharacterLocks = (doc: ProjectDoc, from: string, to: string): ProjectDoc => editLists(doc, (ids) => ids.map((x) => (x === from ? to : x)))
/** The Characters feature is switched off: nothing keeps a list (the loader refuses a list without characters). */
export const cleanAllLocks = (doc: ProjectDoc): ProjectDoc => editLists(doc, () => [])

/** How many weapons and staves have a lock (for the "are you sure" question when characters are switched off). */
export const lockCount = (doc: ProjectDoc): number => allGear(doc).filter((g) => g.keys.wieldedBy?.length).length

/** …of which are staves (the question then says "weapons and staves"). */
export const staffLockCount = (doc: ProjectDoc): number => allGear(doc).filter((g) => g.owner === 'item' && g.keys.wieldedBy?.length).length

/** Which weapons and staves list this character (for the delete question). */
export const lockUsageLines = (doc: ProjectDoc, id: string): Usage[] =>
  gearLockedTo(doc, id).map((g) => ({ section: g.section, text: `${g.owner === 'weapon' ? 'Weapon' : 'Staff'} “${g.name}” is locked to named characters, and this one is on its list (it comes off the list)` }))

/** The class can use it: a weapon is on the class's own list, or its group is one the class wields (docs/ITEMS.md); a staff only by its group. */
export function classCanUse(u: Unit | undefined, doc: ProjectDoc, owner: GearOwner, id: string): boolean {
  const g = gearOf(doc, owner, id)
  return !!u && !!g && ((owner === 'weapon' && !!u.weapons?.includes(id)) || (!!g.keys.group && !!u.wields?.includes(g.keys.group)))
}
