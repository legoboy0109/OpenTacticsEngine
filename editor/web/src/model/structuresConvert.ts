// Old projects (made before structures existed) keep capturing, income, "units can be built here" and headquarters on
// terrain, and who owns what in a map's `owners` list. The game now refuses those (docs/STRUCTURES.md). This module
// finds them and converts them in ONE edit (so one Undo brings the old project back exactly), following docs/EDITOR_UI.md:
//   - every terrain kind with such a setting gets a structure kind with the same name, colour, letter and those settings;
//   - every square of that terrain on every map gets one 1 x 1 structure of it (owner taken from `owners`);
//   - the terrain keeps its ground rules (movement, numbers, tags other than "hq") and loses only the moved settings;
//   - a style's colour for that terrain is copied to the structure (`structureColors`), and its picture (a single piece or
//     an animation) is copied to the structure too, so the building looks the same afterwards.
// Nothing is deleted that had a meaning: the terrain stays under the structure, the map squares stay as they were.
import { withStructures } from './doc'
import { readGrid } from './mapGrid'
import { placedOf, rectHas, rectOf } from './mapStructures'
import { makeId } from './ids'
import { patchObj } from './util'
import type { Graphics, Look, MapFile, MapStructure, ProjectDoc, Structure, StyleArt, Terrain } from './types'

/** Keys that moved from terrain to structures (`capture` is the key older games used for capture points). */
export const MOVED_KEYS = ['capture', 'capturePoints', 'income', 'produces'] as const

export const isLegacyTerrain = (t: Terrain): boolean => MOVED_KEYS.some((k) => k in t) || t.tags?.includes('hq') === true
const hasOwners = (m: MapFile): boolean => Array.isArray(m.owners) && m.owners.length > 0

/** A map's empty `owners` list means nothing and older editors wrote one into every map: it is left out when saving. */
export function dropEmptyOwners(m: MapFile): MapFile {
  if (!Array.isArray(m.owners) || m.owners.length > 0) return m
  const { owners: _gone, ...rest } = m
  return rest as MapFile
}

export interface KindPlan { terrainId: string; name: string; structureId: string; squares: number; settings: string[] }
export interface ConvertPlan {
  kinds: KindPlan[]
  /** Structures that will be placed (one per square of those terrain kinds). */
  placed: number
  /** Owner marks that become the owner of a placed structure. */
  ownersKept: number
  /** Owner marks on squares that are not one of those terrain kinds: they never did anything and are left out. */
  ownersDropped: number
  /** Maps whose old `owners` list is replaced. */
  mapsWithOwners: number
}

function structureIdFor(doc: ProjectDoc, terrainId: string, taken: string[]): string {
  return makeId(terrainId, [...(doc.structures ?? []).map((s) => s.id), ...taken], { fallback: 'structure' })
}

function settingsOf(t: Terrain): string[] {
  const out: string[] = []
  if ((t.capturePoints ?? t.capture ?? 0) > 0) out.push('can be captured')
  if (t.income && Object.values(t.income).some((v) => v)) out.push('earns income')
  if (t.produces?.length) out.push('builds units')
  if (t.tags?.includes('hq')) out.push('headquarters')
  return out
}

/** The terrain kinds that would become structures, in terrain order, with the ids they would get. */
function kindsOf(doc: ProjectDoc): Array<{ t: Terrain; structureId: string }> {
  const taken: string[] = []
  const out: Array<{ t: Terrain; structureId: string }> = []
  for (const t of doc.terrain ?? []) {
    if (!isLegacyTerrain(t)) continue
    const structureId = structureIdFor(doc, t.id, taken)
    taken.push(structureId)
    out.push({ t, structureId })
  }
  return out
}

/** Squares of one map that get a structure: squares of a moved terrain not already under a placed structure. */
function squaresFor(doc: ProjectDoc, map: MapFile, kinds: Map<string, string>): Array<{ type: string; x: number; y: number }> {
  const out: Array<{ type: string; x: number; y: number }> = []
  const g = readGrid(map, doc.terrain ?? [])
  const existing = placedOf(map).map((s) => rectOf((doc.structures ?? []).find((d) => d.id === s.type), s.x, s.y))
  for (let y = 0; y < g.height; y++) {
    for (let x = 0; x < g.width; x++) {
      const type = kinds.get(g.cells[y * g.width + x])
      if (type && !existing.some((r) => rectHas(r, x, y))) out.push({ type, x, y })
    }
  }
  return out
}

/** What converting would do, or null when the project has nothing old in it. */
export function convertPlan(doc: ProjectDoc): ConvertPlan | null {
  if (!doc.terrain) return null
  const kinds = kindsOf(doc)
  const maps = Object.values(doc.maps).filter((m): m is MapFile => m !== null)
  const mapsWithOwners = maps.filter(hasOwners).length
  if (kinds.length === 0 && mapsWithOwners === 0) return null
  const typeOf = new Map(kinds.map((k) => [k.t.id, k.structureId]))
  const counts = new Map<string, number>()
  let placed = 0, ownersKept = 0, ownersDropped = 0
  for (const m of maps) {
    const squares = squaresFor(doc, m, typeOf)
    for (const s of squares) counts.set(s.type, (counts.get(s.type) ?? 0) + 1)
    placed += squares.length
    const at = new Set(squares.map((s) => `${s.x},${s.y}`))
    for (const o of m.owners ?? []) {
      if (at.has(`${o.x},${o.y}`) && m.sides?.some((s) => s.id === o.side)) ownersKept++
      else ownersDropped++
    }
  }
  return {
    kinds: kinds.map((k) => ({ terrainId: k.t.id, name: k.t.name, structureId: k.structureId, squares: counts.get(k.structureId) ?? 0, settings: settingsOf(k.t) })),
    placed, ownersKept, ownersDropped, mapsWithOwners,
  }
}

function newKind(t: Terrain, id: string): Structure {
  const points = (t.capturePoints ?? 0) > 0 ? t.capturePoints! : t.capture ?? 0
  const s: Structure = { id, name: t.name }
  if (t.color !== undefined) s.color = t.color
  if (t.glyph !== undefined) s.glyph = t.glyph
  if (points > 0) s.capturePoints = points
  if (t.income && Object.keys(t.income).length) s.income = { ...t.income }
  if (t.produces?.length) s.produces = [...t.produces]
  if (t.tags?.includes('hq')) s.tags = ['hq']
  return s
}

const withoutMoved = (t: Terrain): Terrain => {
  const rest = patchObj(t, { capture: undefined, capturePoints: undefined, income: undefined, produces: undefined })
  const tags = (t.tags ?? []).filter((x) => x !== 'hq')
  return patchObj(rest, { tags: tags.length ? tags : undefined })
}

/** The structure's picture from the terrain's: a still or an animation is copied as it is, a random mix uses its first piece, a
 *  joining set has no structure equivalent and is not copied (the structure then shows as a coloured block). */
function structureLook(look: Look | null): Look | null | undefined {
  if (look === null) return null
  if (look.cell || look.frames) { const { join: _j, joinWith: _w, joinExcept: _x, joinEdge: _e, variants: _v, ...rest } = look; return rest }
  if (look.variants?.length) return { sheet: look.sheet, cell: look.variants[0] }
  return undefined
}

function copyPictures(g: Graphics, pairs: Map<string, string>): Graphics {
  const take = (m: Record<string, Look | null> | undefined): Record<string, Look | null> => {
    const out: Record<string, Look | null> = {}
    for (const [tid, sid] of pairs) {
      if (!m || !Object.prototype.hasOwnProperty.call(m, tid)) continue
      const l = structureLook(m[tid])
      if (l !== undefined) out[sid] = JSON.parse(JSON.stringify(l)) as Look | null
    }
    return out
  }
  const baseNew = take(g.terrain)
  const styles: Record<string, StyleArt> = {}
  for (const [k, st] of Object.entries(g.styles ?? {})) {
    const add = take(st.terrain)
    styles[k] = Object.keys(add).length ? { ...st, structures: { ...(st.structures ?? {}), ...add } } : st
  }
  const next = patchObj(g, {
    structures: Object.keys(baseNew).length ? ({ ...(g.structures ?? {}), ...baseNew } as Record<string, Look>) : g.structures,
    styles: g.styles ? styles : undefined,
  })
  return next
}

/** Converts the old parts of the project (see the top of this file). Returns the document unchanged when there is nothing to convert. */
export function convertOldProject(doc: ProjectDoc): ProjectDoc {
  const plan = convertPlan(doc)
  if (!plan || !doc.terrain) return doc
  const kinds = kindsOf(doc)
  const typeOf = new Map(kinds.map((k) => [k.t.id, k.structureId]))

  let next: ProjectDoc = { ...doc, terrain: doc.terrain.map((t) => (isLegacyTerrain(t) ? withoutMoved(t) : t)) }
  if (kinds.length) next = withStructures(next, (list) => [...list, ...kinds.map((k) => newKind(k.t, k.structureId))])

  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(doc.maps)) {
    if (m === null) { maps[id] = null; continue }
    const added: MapStructure[] = squaresFor(doc, m, typeOf).map((s) => {
      const owner = (m.owners ?? []).find((o) => o.x === s.x && o.y === s.y)?.side
      return owner && m.sides?.some((x) => x.id === owner) ? { type: s.type, x: s.x, y: s.y, side: owner } : { type: s.type, x: s.x, y: s.y }
    })
    const { owners: _gone, ...rest } = m
    maps[id] = added.length || m.structures ? { ...rest, structures: [...placedOf(m), ...added] } : rest
  }
  next = { ...next, maps }

  if (doc.game?.styles?.some((s) => s.terrainColors && kinds.some((k) => k.t.id in s.terrainColors!))) {
    next = {
      ...next,
      game: {
        ...doc.game,
        styles: doc.game.styles.map((s) => {
          const colors = { ...(s.structureColors ?? {}) }
          let any = false
          for (const k of kinds) { const c = s.terrainColors?.[k.t.id]; if (c !== undefined) { colors[k.structureId] = c; any = true } }
          return any ? { ...s, structureColors: colors } : s
        }),
      },
    }
  }
  if (next.graphics) next = { ...next, graphics: copyPictures(next.graphics, typeOf) }
  return next
}
