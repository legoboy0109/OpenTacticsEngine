// Structure kinds (data/structures.json): the list and each kind's settings. Placing them on maps is mapStructures.ts;
// clean-up when a kind is deleted is structuresRefs.ts; converting old projects is structuresConvert.ts.
import { clone, ids, withStructures } from './doc'
import { copyArtOf } from './graphicsClean'
import { makeId } from './ids'
import { cleanStructure } from './structuresRefs'
import { moveItem, patchObj, setNum, updateById } from './util'
import type { ProjectDoc, Structure } from './types'

export const SIZE_MIN = 1
export const SIZE_MAX = 8

const PALETTE = ['#b7b7b7', '#9a9a9a', '#d4af37', '#a8784a', '#7d6fa8', '#4a7fc1', '#8fbf6a', '#c9b458']

export const newStructureId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.structures), { fallback: 'structure' })

export const sizeOf = (s: Structure | undefined): { w: number; h: number } => ({ w: clampSize(s?.width), h: clampSize(s?.height) })
/** Hexagon games: `rings` 1 = a hexagon and the six around it, anything else (missing = 0) = one hexagon. */
export const ringsOf = (s: Structure | undefined): 0 | 1 => (s?.rings === 1 ? 1 : 0)

/** "Covers" in plain words for a hexagon game. */
export const hexCoverText = (s: Structure | undefined): string => (ringsOf(s) === 1 ? '7 hexagons' : '1 hexagon')

/** Hexagon games: the size control. 1 hexagon removes the key (0 is the default and is not written). */
export function setRings(doc: ProjectDoc, id: string, rings: 0 | 1): ProjectDoc {
  return updateStructure(doc, id, { rings: rings === 1 ? 1 : undefined })
}

function clampSize(n: number | undefined): number {
  return typeof n === 'number' && Number.isInteger(n) ? Math.min(SIZE_MAX, Math.max(SIZE_MIN, n)) : 1
}

export function addStructure(doc: ProjectDoc, name: string, id: string = newStructureId(doc, name)): ProjectDoc {
  const glyph = (name.normalize('NFKD').match(/[A-Za-z0-9]/)?.[0] ?? 's').toUpperCase()
  const s: Structure = { id, name, color: PALETTE[(doc.structures?.length ?? 0) % PALETTE.length], glyph }
  return withStructures(doc, (list) => [...list, s])
}

export function duplicateStructure(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  const i = (doc.structures ?? []).findIndex((s) => s.id === id)
  if (i < 0) return doc
  const name = `${doc.structures![i].name} copy`
  const copyId = newId ?? makeId(name, ids(doc.structures), { fallback: 'structure' })
  const added = withStructures(doc, (list) => {
    const copy: Structure = { ...clone(list[i]), id: copyId, name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
  return copyArtOf(added, 'structures', id, copyId)
}

/** Unlike terrain, a game may have no structures at all. Removes the kind, the ones placed on maps, its pictures and style colours. */
export function deleteStructure(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.structures?.some((s) => s.id === id)) return doc
  return { ...cleanStructure(doc, id), structures: doc.structures.filter((s) => s.id !== id) }
}

export function updateStructure(doc: ProjectDoc, id: string, patch: Partial<Omit<Structure, 'id'>>): ProjectDoc {
  return withStructures(doc, (list) => updateById(list, id, (s) => patchObj(s, patch)))
}

export function moveStructureKind(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withStructures(doc, (list) => moveItem(list, from, to))
}

/** Footprint in whole squares, 1 to 8 each way; 1 is the default and is not written. Squares already placed on maps
 *  keep their top-left corner (the map checks flag any that no longer fit or now overlap). */
export function setSize(doc: ProjectDoc, id: string, width: number, height: number): ProjectDoc {
  const w = clampSize(width), h = clampSize(height)
  return updateStructure(doc, id, { width: w > 1 ? w : undefined, height: h > 1 ? h : undefined })
}

export type MoveMode = 'ground' | 'cost' | 'blocked'
export const moveModeOf = (s: Structure, moveType: string): MoveMode => {
  const c = s.moveCost?.[moveType]
  return c === undefined || c < 0 ? 'ground' : c === 0 ? 'blocked' : 'cost'
}

/** Movement through the structure for one movement type: the ground's cost (key removed), an own cost, or cannot enter (0). */
export function setMoveRule(doc: ProjectDoc, id: string, moveType: string, mode: MoveMode, cost = 1): ProjectDoc {
  const value = mode === 'ground' ? undefined : mode === 'blocked' ? 0 : Math.max(1, cost)
  return withStructures(doc, (list) => updateById(list, id, (s) => patchObj(s, { moveCost: setNum(s.moveCost, moveType, value) })))
}

/** One of the structure's own numbers; undefined = same as the ground (key removed). */
export function setStructureNumber(doc: ProjectDoc, id: string, key: string, value: number | undefined): ProjectDoc {
  return withStructures(doc, (list) => updateById(list, id, (s) => patchObj(s, { props: setNum(s.props, key, value) })))
}

export function setStructureIncome(doc: ProjectDoc, id: string, resource: string, value: number | undefined): ProjectDoc {
  return withStructures(doc, (list) => updateById(list, id, (s) => patchObj(s, { income: setNum(s.income, resource, value) })))
}

/** Capture points; 0 or empty = cannot be captured (key removed). */
export function setCapturePoints(doc: ProjectDoc, id: string, points: number | undefined): ProjectDoc {
  return updateStructure(doc, id, { capturePoints: points ? points : undefined })
}

export function setProduces(doc: ProjectDoc, id: string, unitIds: string[]): ProjectDoc {
  return updateStructure(doc, id, { produces: unitIds.length ? unitIds : undefined })
}

/** Items a structure sells (docs/ITEMS.md "Shops"); empty removes the key. */
export function setSells(doc: ProjectDoc, id: string, itemIds: string[]): ProjectDoc {
  return updateStructure(doc, id, { sells: itemIds.length ? itemIds : undefined })
}

export function setStructureTags(doc: ProjectDoc, id: string, tags: string[]): ProjectDoc {
  return updateStructure(doc, id, { tags: tags.length ? tags : undefined })
}

export const isHeadquarters = (s: Structure | undefined): boolean => s?.tags?.includes('hq') === true

/** The "hq" tag marks a headquarters; other tags are left alone. */
export function setHeadquarters(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return withStructures(doc, (list) => updateById(list, id, (s) => {
    const tags = (s.tags ?? []).filter((x) => x !== 'hq')
    if (on) tags.push('hq')
    return patchObj(s, { tags: tags.length ? tags : undefined })
  }))
}

/** True when owning this kind changes anything in a battle (capturable, earns money, builds units, or is a headquarters). */
export const ownershipMatters = (s: Structure | undefined): boolean =>
  !!s && (!!s.capturePoints || !!s.produces?.length || !!(s.income && Object.values(s.income).some((v) => v)) || isHeadquarters(s))
