// What other parts of a game hold about structures, and the clean-up when something they point at is deleted. Every
// function is pure and returns a new document, so the delete and its clean-up are one undo step.
import { dropArtOf } from './graphicsClean'
import { placeKindUsageLines } from './placeRefs'
import { mapWeaponKindUsageLines } from './mapWeaponRefs'
import { economyUsageLines } from './researchRefs'
import { dropKey, without } from './util'
import type { Usage } from './references'
import type { MapFile, ProjectDoc, Structure } from './types'

const mapEntries = (doc: ProjectDoc): Array<[string, MapFile]> =>
  Object.entries(doc.maps).filter((e): e is [string, MapFile] => e[1] !== null)

const mapMembers = (doc: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc['maps'] =>
  Object.fromEntries(Object.entries(doc.maps).map(([id, m]) => [id, m === null ? null : fn(m)]))

const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id

/** Object copy where an emptied optional map loses its key instead of keeping `{}`. */
function strip<T extends object>(o: T, key: string, value: unknown): T {
  const c: Record<string, unknown> = { ...(o as Record<string, unknown>) }
  if (value === undefined) delete c[key]; else c[key] = value
  return c as T
}

function omit<T>(m: Record<string, T>, key: string): Record<string, T> | undefined {
  const rest = { ...m }
  delete rest[key]
  return Object.keys(rest).length ? rest : undefined
}

const mapStructures = (doc: ProjectDoc, fn: (s: Structure) => Structure): ProjectDoc['structures'] =>
  doc.structures && doc.structures.map(fn)

/** How many of a kind are placed on each map that has some (maps in game order). */
export function placedCounts(doc: ProjectDoc, id: string): Array<{ mapId: string; name: string; count: number }> {
  const out: Array<{ mapId: string; name: string; count: number }> = []
  for (const [mapId, m] of mapEntries(doc)) {
    const count = (m.structures ?? []).filter((s) => s.type === id).length
    if (count) out.push({ mapId, name: mapName(doc, mapId), count })
  }
  return out
}

/** Lines for the delete confirmation: placed ones, pictures, style colours. */
export function structureUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const p of placedCounts(doc, id)) out.push({ section: 'maps', text: `Map “${p.name}” has ${p.count} of them placed (they are removed)` })
  for (const s of doc.game?.styles ?? []) if (s.structureColors && id in s.structureColors) out.push({ section: 'game', text: `Style “${s.name}” recolours it` })
  out.push(...economyUsageLines(doc, 'structure', id))
  out.push(...placeKindUsageLines(doc, id))
  out.push(...mapWeaponKindUsageLines(doc, id))
  const g = doc.graphics
  if (g && (id in (g.structures ?? {}) || Object.values(g.styles ?? {}).some((st) => id in (st.structures ?? {})))) {
    out.push({ section: 'art', text: 'Its picture is cleared (the image file stays, under Art)' })
  }
  return out
}

/** Removes the kind's placed structures from every map, its pictures and its style colours. */
export function cleanStructure(doc: ProjectDoc, id: string): ProjectDoc {
  const styles = doc.game?.styles
  const game = doc.game && styles?.some((s) => s.structureColors && id in s.structureColors)
    ? { ...doc.game, styles: styles.map((s) => (s.structureColors && id in s.structureColors ? strip(s, 'structureColors', omit(s.structureColors, id)) : s)) }
    : doc.game
  return dropArtOf({
    ...doc,
    game,
    maps: mapMembers(doc, (m) => (m.structures?.some((s) => s.type === id) ? { ...m, structures: m.structures.filter((s) => s.type !== id) } : m)),
  }, 'structures', id)
}

/** A deleted unit can no longer be built by any structure. */
export function cleanUnitInStructures(doc: ProjectDoc, unitId: string): ProjectDoc['structures'] {
  return mapStructures(doc, (s) => (s.produces?.includes(unitId) ? strip(s, 'produces', without(s.produces, unitId)) : s))
}

/** A deleted movement type leaves every structure's movement overrides. */
export function cleanMoveTypeInStructures(doc: ProjectDoc, moveTypeId: string): ProjectDoc['structures'] {
  return mapStructures(doc, (s) => (s.moveCost && moveTypeId in s.moveCost ? strip(s, 'moveCost', dropKey(s.moveCost, moveTypeId)) : s))
}

/** A deleted resource leaves every structure's income. */
export function cleanResourceInStructures(doc: ProjectDoc, resourceId: string): ProjectDoc['structures'] {
  return mapStructures(doc, (s) => (s.income && resourceId in s.income ? strip(s, 'income', dropKey(s.income, resourceId)) : s))
}
