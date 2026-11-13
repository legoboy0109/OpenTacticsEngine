// Unit types (classes): what the player picks to place or build.
import { clone, ids, withUnits } from './doc'
import { makeId } from './ids'
import { cleanUnit } from './references'
import { moveItem, patchObj, setNum, updateById } from './util'
import type { NumMap, ProjectDoc, Unit } from './types'

export const newUnitId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.units), { fallback: 'unit' })

/** First letter or digit of the name, upper-cased: the letter drawn on the map. */
export function glyphFor(name: string): string {
  const m = name.normalize('NFKD').match(/[A-Za-z0-9]/)
  return m ? m[0].toUpperCase() : 'U'
}

export function addUnit(doc: ProjectDoc, name: string, id: string = newUnitId(doc, name)): ProjectDoc {
  const g = doc.game
  if (!g || !doc.units) return doc
  const stats: NumMap = {}
  if (g.roles.hp) stats[g.roles.hp] = 10
  if (g.roles.move) stats[g.roles.move] = 4
  const unit: Unit = { id, name, glyph: glyphFor(name), moveType: g.moveTypes[0]?.id ?? '', stats, weapons: [] }
  return { ...doc, units: [...doc.units, unit] }
}

export function duplicateUnit(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withUnits(doc, (list) => {
    const i = list.findIndex((u) => u.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    const copy: Unit = { ...clone(list[i]), id: newId ?? makeId(name, ids(list), { fallback: 'unit' }), name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
}

export function unitRemovalBlock(doc: ProjectDoc): string | null {
  return (doc.units?.length ?? 0) <= 1 ? 'A game needs at least one unit type.' : null
}

/** Removes the unit and every reference to it (build lists, effectiveness rows, placed units). */
export function deleteUnit(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.units || unitRemovalBlock(doc)) return doc
  const cleaned = cleanUnit(doc, id)
  return { ...cleaned, units: doc.units.filter((u) => u.id !== id) }
}

export function updateUnit(doc: ProjectDoc, id: string, patch: Partial<Omit<Unit, 'id'>>): ProjectDoc {
  return withUnits(doc, (list) => updateById(list, id, (u) => patchObj(u, patch)))
}

export function moveUnit(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withUnits(doc, (list) => moveItem(list, from, to))
}

/** One number in the stats / growths / cost table; undefined removes it (0 stays as 0). */
export function setUnitNumber(doc: ProjectDoc, id: string, table: 'stats' | 'growths' | 'cost', key: string, value: number | undefined): ProjectDoc {
  return withUnits(doc, (list) => updateById(list, id, (u) => patchObj(u, { [table]: setNum(u[table], key, value) })))
}

export function setUnitWeapons(doc: ProjectDoc, id: string, weapons: string[]): ProjectDoc {
  return updateUnit(doc, id, { weapons })
}

export function setUnitTags(doc: ProjectDoc, id: string, tags: string[]): ProjectDoc {
  return updateUnit(doc, id, { tags: tags.length ? tags : undefined })
}

/** Every label (tag) some unit has: what weapon effectiveness rows can point at. */
export function allTags(doc: ProjectDoc): string[] {
  return [...new Set((doc.units ?? []).flatMap((u) => u.tags ?? []))].sort()
}
