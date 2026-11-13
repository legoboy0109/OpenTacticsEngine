// Terrain types.
import { clone, ids, withTerrain } from './doc'
import { makeId } from './ids'
import { cleanTerrain } from './references'
import { moveItem, patchObj, setNum, updateById } from './util'
import type { ProjectDoc, Terrain } from './types'

const PALETTE = ['#8fbf6a', '#3f7f3a', '#8a7a68', '#4a7fc1', '#b0a58c', '#a8784a', '#c9b458', '#7d6fa8']

export const newTerrainId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.terrain), { fallback: 'terrain' })

export function addTerrain(doc: ProjectDoc, name: string, id: string = newTerrainId(doc, name)): ProjectDoc {
  if (!doc.terrain) return doc
  const glyph = (name.normalize('NFKD').match(/[A-Za-z0-9]/)?.[0] ?? 't').toLowerCase()
  const t: Terrain = { id, name, color: PALETTE[doc.terrain.length % PALETTE.length], glyph, props: { defense: 0, avoid: 0 } }
  return { ...doc, terrain: [...doc.terrain, t] }
}

export function duplicateTerrain(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withTerrain(doc, (list) => {
    const i = list.findIndex((t) => t.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    const copy: Terrain = { ...clone(list[i]), id: newId ?? makeId(name, ids(list), { fallback: 'terrain' }), name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
}

export function terrainRemovalBlock(doc: ProjectDoc): string | null {
  return (doc.terrain?.length ?? 0) <= 1 ? 'A game needs at least one kind of terrain.' : null
}

/** Map tiles of the removed terrain become the first remaining terrain. */
export function deleteTerrain(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.terrain || terrainRemovalBlock(doc)) return doc
  const replacement = doc.terrain.find((t) => t.id !== id)!.id
  return { ...cleanTerrain(doc, id, replacement), terrain: doc.terrain.filter((t) => t.id !== id) }
}

export function updateTerrain(doc: ProjectDoc, id: string, patch: Partial<Omit<Terrain, 'id'>>): ProjectDoc {
  return withTerrain(doc, (list) => updateById(list, id, (t) => patchObj(t, patch)))
}

export function moveTerrain(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withTerrain(doc, (list) => moveItem(list, from, to))
}

/** Movement cost for one movement type. 0 = cannot enter; undefined = back to the default of 1. */
export function setMoveCost(doc: ProjectDoc, id: string, moveType: string, cost: number | undefined): ProjectDoc {
  return withTerrain(doc, (list) => updateById(list, id, (t) => patchObj(t, { moveCost: setNum(t.moveCost, moveType, cost) })))
}

type TerrainTable = 'props' | 'income'
export function setTerrainNumber(doc: ProjectDoc, id: string, table: TerrainTable, key: string, value: number | undefined): ProjectDoc {
  return withTerrain(doc, (list) => updateById(list, id, (t) => patchObj(t, { [table]: setNum(t[table], key, value) })))
}

/** Capture points; 0 or empty = cannot be captured (key removed). */
export function setCapturePoints(doc: ProjectDoc, id: string, points: number | undefined): ProjectDoc {
  return updateTerrain(doc, id, { capture: points ? points : undefined })
}

export function setProduces(doc: ProjectDoc, id: string, unitIds: string[]): ProjectDoc {
  return updateTerrain(doc, id, { produces: unitIds.length ? unitIds : undefined })
}

export function setTerrainTags(doc: ProjectDoc, id: string, tags: string[]): ProjectDoc {
  return updateTerrain(doc, id, { tags: tags.length ? tags : undefined })
}

/** The "hq" tag marks a headquarters; other tags are left alone. */
export function setHeadquarters(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return withTerrain(doc, (list) => updateById(list, id, (t) => {
    const tags = (t.tags ?? []).filter((x) => x !== 'hq')
    if (on) tags.push('hq')
    return patchObj(t, { tags: tags.length ? tags : undefined })
  }))
}
