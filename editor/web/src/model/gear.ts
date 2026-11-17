// "Gear" = what is wielded by group, rank and (optionally) character: a weapon of weapons.json and a staff (an item of kind
// "staff", docs/SRPG.md section 8). The engine answers for both with the same rank and lock code, so the editor does too: the rank
// and lock edits, checks and clean-ups work on the weapons AND the staves through the helpers here, never on one list only.
import type { Item, NumMap, ProjectDoc, Section, Weapon } from './types'
import { patchObj } from './util'

export type GearOwner = 'weapon' | 'item'
export interface GearKeys { group?: string; rank?: string; rankXp?: number; wieldedBy?: string[]; wieldedByTypes?: string[] }
export interface GearRef { owner: GearOwner; id: string; name: string; section: Section; keys: GearKeys & { props?: NumMap } }

export const isStaff = (i: Item): boolean => i.kind === 'staff'
export const staffItems = (doc: ProjectDoc): Item[] => (doc.items ?? []).filter(isStaff)
export const gearSection = (owner: GearOwner): Section => (owner === 'weapon' ? 'weapons' : 'items')

/** Every weapon, then every staff (the order the engine lists groups in). */
export function allGear(doc: ProjectDoc): GearRef[] {
  const w = (doc.weapons ?? []).map((x): GearRef => ({ owner: 'weapon', id: x.id, name: x.name, section: 'weapons', keys: x }))
  const s = staffItems(doc).map((x): GearRef => ({ owner: 'item', id: x.id, name: x.name, section: 'items', keys: x }))
  return [...w, ...s]
}

export const gearOf = (doc: ProjectDoc, owner: GearOwner, id: string): GearRef | undefined => allGear(doc).find((g) => g.owner === owner && g.id === id)

/** Writes keys on one weapon or staff (an undefined value removes the key). */
export function patchGear(doc: ProjectDoc, owner: GearOwner, id: string, patch: Partial<GearKeys>): ProjectDoc {
  if (owner === 'weapon') return { ...doc, weapons: doc.weapons && doc.weapons.map((w): Weapon => (w.id === id ? patchObj(w, patch) : w)) }
  return { ...doc, items: doc.items && doc.items.map((i): Item => (i.id === id && isStaff(i) ? patchObj(i, patch) : i)) }
}

/** Same list when nothing changed (so an untouched part keeps its identity and is not saved again). */
function mapKeep<T>(list: T[] | null | undefined, fn: (t: T) => T): T[] | null {
  if (!list) return null
  const next = list.map(fn)
  return next.some((x, i) => x !== list[i]) ? next : list
}

/** Runs `fn` over every weapon and every staff. */
export function editGear(doc: ProjectDoc, fn: <T extends GearKeys>(g: T) => T): ProjectDoc {
  return { ...doc, weapons: mapKeep(doc.weapons, fn), items: mapKeep(doc.items, (i) => (isStaff(i) ? fn(i) : i)) }
}
