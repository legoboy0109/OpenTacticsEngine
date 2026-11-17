// Characters (docs/SRPG.md section 1): the edits behind the Characters screen. Pure functions on the document, like
// `units.ts`. Deleting lives in `srpgRefs.ts` (`deleteCharacter`), which also takes the character off the maps.
import { clone, withCharacters } from './doc'
import { newCharacterId } from './srpgRefs'
import type { Character } from './srpgTypes'
import type { ItemEntry, ProjectDoc } from './types'
import { moveItem, patchObj, setNum, updateById } from './util'

/** The deepest level the game allows (the loader refuses a character above it); 20 when the game does not say. */
export const maxLevel = (doc: ProjectDoc): number => doc.game?.rules?.progression?.maxLevel ?? 20

/** "class" once a game has at least one character, "unit type" before that (docs/SRPG.md open question 1, default). */
export const classWord = (doc: ProjectDoc, plural = false): string => {
  const has = (doc.characters?.length ?? 0) > 0
  return has ? (plural ? 'classes' : 'class') : (plural ? 'unit types' : 'unit type')
}

/** A new character: the first unit type, nothing else (the person fills it in). */
export function addCharacter(doc: ProjectDoc, name: string, id: string = newCharacterId(doc, name)): ProjectDoc {
  const type = doc.units?.[0]?.id
  if (doc.characters === null || !type) return doc
  const c: Character = { id, name, type }
  return withCharacters(doc, (list) => [...list, c])
}

/** A copy right after the original; it is not placed on any map (a character stands on a map once). */
export function duplicateCharacter(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  const i = (doc.characters ?? []).findIndex((c) => c.id === id)
  if (i < 0) return doc
  const name = `${doc.characters![i].name} copy`
  const copyId = newId ?? newCharacterId(doc, name)
  return withCharacters(doc, (list) => [...list.slice(0, i + 1), { ...clone(list[i]), id: copyId, name }, ...list.slice(i + 1)])
}

export const moveCharacter = (doc: ProjectDoc, from: number, to: number): ProjectDoc => withCharacters(doc, (list) => moveItem(list, from, to))

/** A character's own extra weapon groups; an empty list removes the key. */
export const setCharacterWields = (doc: ProjectDoc, id: string, groups: string[]): ProjectDoc => updateCharacter(doc, id, { wields: groups.length ? groups : undefined })

export const updateCharacter = (doc: ProjectDoc, id: string, patch: Partial<Omit<Character, 'id'>>): ProjectDoc =>
  withCharacters(doc, (list) => updateById(list, id, (c) => patchObj(c, patch)))

/** One personal bonus (stats or growths; may be negative); empty removes the key, and an empty table removes the table. */
export const setCharacterNumber = (doc: ProjectDoc, id: string, table: 'stats' | 'growths', key: string, value: number | undefined): ProjectDoc =>
  withCharacters(doc, (list) => updateById(list, id, (c) => patchObj(c, { [table]: setNum(c[table], key, value) })))

/** Starting items: a list (even an empty one) replaces the class's; undefined removes the key = the class's start. */
export const setCharacterItems = (doc: ProjectDoc, id: string, items: ItemEntry[] | undefined): ProjectDoc =>
  updateCharacter(doc, id, { items })

/** The level the character joins at; 1 (or empty) is the default, so the key goes. */
export const setCharacterLevel = (doc: ProjectDoc, id: string, level: number | undefined): ProjectDoc =>
  updateCharacter(doc, id, { level: level && level > 1 ? Math.min(level, maxLevel(doc)) : undefined })

/** `unit type value + personal bonus`, never below 0 (the loader floors a unit's stat the same way). */
export const totalStat = (base: number | undefined, bonus: number | undefined): number => Math.max(0, (base ?? 0) + (bonus ?? 0))

/** Characters whose class is this unit type. */
export const charactersOfType = (doc: ProjectDoc, type: string): Character[] => (doc.characters ?? []).filter((c) => c.type === type)
