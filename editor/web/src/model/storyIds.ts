// Story names ("ids") of units, events, switches and variables, and which names a map offers to events.
import { makeId } from './ids'
import { forEachCommand } from './storyWalk'
import type { MapFile, MapUnit, ProjectDoc } from './types'

/** Letters, digits, "_" and "-", 1 to 64 characters (the loader's isStoryId). */
export const STORY_ID = /^[A-Za-z0-9_-]{1,64}$/
export const isStoryId = (s: string): boolean => STORY_ID.test(s)

export const ID_RULE = 'Use letters, digits, _ and - only (no spaces).'

/** Spaces become "_" as the person types; everything else is left for the check to explain. */
export const tidyId = (s: string): string => s.replace(/\s+/g, '_')

/** Why `text` cannot be a story name (null when it can). `taken` = names already used by something else. */
export function idProblem(text: string, taken: Iterable<string>): string | null {
  if (text === '') return 'Type a name.'
  if (text.length > 64) return 'At most 64 characters.'
  if (!isStoryId(text)) return ID_RULE
  for (const t of taken) if (t === text) return 'Something else here already has this name.'
  return null
}

const typeName = (doc: ProjectDoc, type: string) => doc.units?.find((u) => u.id === type)?.name ?? type

/** A unit's name as people see it: its own name, else its type's. */
export const unitDisplayName = (doc: ProjectDoc, u: Pick<MapUnit, 'type' | 'name'>): string => (u.name?.trim() ? u.name : typeName(doc, u.type))

export interface NamedUnit { id: string; label: string; source: 'placed' | 'added' }

/** Units events can point at: the map's units that have a story name, then the units its addUnit commands create. */
export function namedUnits(doc: ProjectDoc, map: MapFile): NamedUnit[] {
  const out: NamedUnit[] = []
  const seen = new Set<string>()
  const add = (u: MapUnit, source: NamedUnit['source']) => {
    if (typeof u.id !== 'string' || u.id === '' || seen.has(u.id)) return
    seen.add(u.id)
    out.push({ id: u.id, label: `${unitDisplayName(doc, u)} (${u.id})`, source })
  }
  for (const u of map.units ?? []) add(u, 'placed')
  for (const e of map.events ?? []) forEachCommand(e.do, '', (c) => { if (c.addUnit && typeof c.addUnit === 'object') add(c.addUnit, 'added') })
  return out
}

/** Plain list of the story names a map defines (placed units, then added ones). */
export const definedUnitIds = (doc: ProjectDoc, map: MapFile): string[] => namedUnits(doc, map).map((u) => u.id)

/** The names taken by units other than `except` (a placed unit or an added unit, compared by identity). */
export const takenUnitIds = (map: MapFile, except?: MapUnit): string[] => {
  const out: string[] = []
  for (const u of map.units ?? []) if (u !== except && u.id) out.push(u.id)
  for (const e of map.events ?? []) forEachCommand(e.do, '', (c) => { const u = c.addUnit; if (u && u !== except && typeof u === 'object' && u.id) out.push(u.id) })
  return out
}

/** A story name made from the unit's name (or its type), not used by anything else in the map. */
export function suggestUnitId(doc: ProjectDoc, map: MapFile, u: MapUnit): string {
  return makeId(unitDisplayName(doc, u), takenUnitIds(map, u), { fallback: 'unit' })
}

/** The teams of a map: each side's team, or the side itself when it has none (first appearance order). */
export function teamsOf(map: MapFile): Array<{ id: string; label: string }> {
  const out: Array<{ id: string; label: string }> = []
  for (const s of map.sides ?? []) {
    const id = s.team?.trim() ? s.team : s.id
    if (!out.some((t) => t.id === id)) out.push({ id, label: id === s.id ? s.name || s.id : id })
  }
  return out
}

/** Every tag a unit type carries (for the "with tag" filter's suggestions). */
export const knownTags = (doc: ProjectDoc): string[] => [...new Set((doc.units ?? []).flatMap((u) => u.tags ?? []))]
