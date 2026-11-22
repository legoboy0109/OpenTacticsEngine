// Weapons / attacks.
import { clone, ids, withWeapons } from './doc'
import { suggestedWeaponProps } from './formulas'
import { makeId } from './ids'
import { groupsOf } from './ranks'
import { dropGroupRanks, renameGroupRefs } from './rankRefs'
import { cleanWeapon } from './references'
import { dropKey, moveItem, patchObj, setNum, updateById } from './util'
import type { NumMap, ProjectDoc, Weapon } from './types'

export const newWeaponId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.weapons), { fallback: 'weapon' })

export function addWeapon(doc: ProjectDoc, name: string, id: string = newWeaponId(doc, name)): ProjectDoc {
  if (!doc.weapons) return doc
  const props: NumMap = {}
  if (doc.game) for (const p of suggestedWeaponProps(doc.game)) props[p.id] = p.default
  return { ...doc, weapons: [...doc.weapons, { id, name, props }] }
}

export function duplicateWeapon(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withWeapons(doc, (list) => {
    const i = list.findIndex((w) => w.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    const copy: Weapon = { ...clone(list[i]), id: newId ?? makeId(name, ids(list), { fallback: 'weapon' }), name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
}

/** The groups still in use by a weapon or a staff (a group is only gone when neither has it). */
const groupsLeft = (doc: ProjectDoc): Set<string> => new Set(groupsOf(doc))

/** Removes the weapon and takes it off every unit that carried it. When it was the last weapon of its group, starting ranks in
 *  that group go too (the loader refuses a rank in a group no weapon has). */
export function deleteWeapon(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.weapons) return doc
  const group = doc.weapons.find((w) => w.id === id)?.group
  const out = { ...cleanWeapon(doc, id), weapons: doc.weapons.filter((w) => w.id !== id) }
  return group && !groupsLeft(out).has(group) ? dropGroupRanks(out, group) : out
}

/** The weapon's group (blank = none). When this was the only weapon of its group and the new name is not a group yet, it is a
 *  rename: the classes that wield the group and every starting rank in it follow. Anything else (a group that disappears, one that
 *  merges into another) leaves them alone: the rank checks name what no longer fits, and nothing is lost while someone is typing. */
export function setWeaponGroup(doc: ProjectDoc, id: string, text: string): ProjectDoc {
  const old = doc.weapons?.find((w) => w.id === id)?.group
  const group = text.trim() === '' ? undefined : text
  if (old === group) return doc
  const out = updateWeapon(doc, id, { group })
  return old && group && !groupsLeft(out).has(old) && !groupsLeft(doc).has(group) ? renameGroupRefs(out, old, group) : out
}

/** The weapon's own hit formula (blank = the game's); the key is removed when empty. */
export const setWeaponHit = (doc: ProjectDoc, id: string, text: string): ProjectDoc => updateWeapon(doc, id, { hit: text.trim() === '' ? undefined : text })

/** The loader refuses a "hit" that is not formula text. */
export const weaponHitChecks = (doc: ProjectDoc): string[] =>
  (doc.weapons ?? []).filter((w) => w.hit !== undefined && (typeof w.hit !== 'string' || w.hit.trim() === '')).map((w) => `${w.name}: the hit formula must be text, or left out.`)

export function updateWeapon(doc: ProjectDoc, id: string, patch: Partial<Omit<Weapon, 'id'>>): ProjectDoc {
  return withWeapons(doc, (list) => updateById(list, id, (w) => patchObj(w, patch)))
}

export function moveWeapon(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withWeapons(doc, (list) => moveItem(list, from, to))
}

/** Range is stored only when it differs from the default 1-1. */
export function setWeaponRange(doc: ProjectDoc, id: string, min: number, max: number): ProjectDoc {
  const hi = Math.max(min, max)
  return updateWeapon(doc, id, { range: min === 1 && hi === 1 ? undefined : [min, hi] })
}

export function setWeaponProp(doc: ProjectDoc, id: string, key: string, value: number | undefined): ProjectDoc {
  return withWeapons(doc, (list) => updateById(list, id, (w) => patchObj(w, { props: setNum(w.props, key, value) })))
}

/** An effectiveness row for a unit id or tag. The first row on a weapon that
 *  could hit anything keeps that: `elseValue` becomes "everything else". */
export function setVersus(doc: ProjectDoc, id: string, key: string, value: number, elseValue: number): ProjectDoc {
  return withWeapons(doc, (list) => updateById(list, id, (w) => {
    const first = !w.versus || Object.keys(w.versus).length === 0
    const patch: Partial<Weapon> = { versus: { ...(w.versus ?? {}), [key]: value } }
    if (first && w.versusDefault === undefined) patch.versusDefault = elseValue
    return patchObj(w, patch)
  }))
}

export function removeVersus(doc: ProjectDoc, id: string, key: string): ProjectDoc {
  return withWeapons(doc, (list) => updateById(list, id, (w) => patchObj(w, { versus: dropKey(w.versus, key) })))
}

/** undefined = targets not in the table cannot be attacked. */
export function setVersusDefault(doc: ProjectDoc, id: string, value: number | undefined): ProjectDoc {
  return updateWeapon(doc, id, { versusDefault: value })
}

/** Point an effectiveness row at a different unit/label, keeping its value and its place in the list. */
export function changeVersusKey(doc: ProjectDoc, id: string, from: string, to: string): ProjectDoc {
  if (from === to) return doc
  return withWeapons(doc, (list) => updateById(list, id, (w) => {
    if (!w.versus || !(from in w.versus) || to in w.versus) return w
    const entries = Object.entries(w.versus).map(([k, v]) => [k === from ? to : k, v] as const)
    return { ...w, versus: Object.fromEntries(entries) }
  }))
}
