// Document-level edits where a map's units or sides meet its events: a unit's story id, deleting a unit, deleting a
// side. Each is ONE edit, so a single Undo restores the units and everything the clean-up removed.
import { dropBehaviourUnit, renameBehaviourUnit } from './behaviour'
import { renameVisitUnit } from './mapPlaces'
import { removeMapUnit, unitAt } from './mapUnits'
import { removeSide } from './mapSides'
import { followUnitRename } from './campaignRefs'
import { idProblem, takenUnitIds } from './storyIds'
import { setUnitStoryId } from './storyEdit'
import { scrubMap, storyIdUsages, storyUsages } from './storyRefs'
import type { Ref } from './storyWalk'
import type { Usage } from './references'
import type { MapFile, ProjectDoc } from './types'

const withMap = (doc: ProjectDoc, mapId: string, map: MapFile): ProjectDoc => ({ ...doc, maps: { ...doc.maps, [mapId]: map } })

/** A story name for the placed unit at (x, y). `error` says why it was refused (the document is then unchanged);
 *  '' clears the name, which is refused while events or objectives still point at it. */
export function changeUnitStoryId(doc: ProjectDoc, mapId: string, x: number, y: number, to: string, v = 0): { doc: ProjectDoc; error: string | null } {
  const map = doc.maps[mapId]
  const unit = map ? unitAt(map, x, y, v) : undefined
  if (!map || !unit) return { doc, error: null }
  if ((unit.id ?? '') === to) return { doc, error: null }
  // Another variant of this unit (same name, for other difficulties) keeps the old name: events still find it, so nothing else follows.
  const siblings = unit.id !== undefined && (map.units ?? []).some((u) => u !== unit && u.id === unit.id)
  if (siblings) {
    const error = to === '' ? null : idProblem(to, takenUnitIds(map, unit))
    if (error) return { doc, error }
    return { doc: withMap(doc, mapId, { ...map, units: (map.units ?? []).map((u) => (u === unit ? { ...u, id: to === '' ? undefined : to } : u)) }), error: null }
  }
  if (to === '') {
    if (unit.id && storyIdUsages(doc, mapId, unit.id).length) return { doc, error: 'Events or objectives point at this name. Remove them first, or pick another name.' }
  } else {
    const error = idProblem(to, takenUnitIds(map, unit))
    if (error) return { doc, error }
  }
  const renamed = setUnitStoryId(map, x, y, to, v)
  const next = withMap(doc, mapId, unit.id && to ? renameBehaviourUnit(renameVisitUnit(renamed, unit.id, to), unit.id, to) : unit.id ? dropBehaviourUnit(renamed, unit.id) : renamed) // a place that lists it as a visitor follows too
  // The campaign's scenes that named the old story name follow it (unless another map still has that name).
  return { doc: unit.id ? followUnitRename(doc, next, unit.id, to) : next, error: null }
}

/** What deleting the unit at (x, y) would remove from the events and objectives. */
export function unitRemovalUsages(doc: ProjectDoc, mapId: string, x: number, y: number, v = 0): Usage[] {
  const map = doc.maps[mapId]
  const unit = map ? unitAt(map, x, y, v) : undefined
  if (!map || !unit?.id || (map.units ?? []).some((u) => u !== unit && u.id === unit.id)) return []  // another variant keeps the name
  return storyIdUsages(doc, mapId, unit.id)
}

/** Deletes the unit and everything in the events and objectives that pointed at its story name. */
export function removeUnitAndStory(doc: ProjectDoc, mapId: string, x: number, y: number, v = 0): ProjectDoc {
  const map = doc.maps[mapId]
  const unit = map ? unitAt(map, x, y, v) : undefined
  if (!map || !unit) return doc
  const removed = removeMapUnit(map, x, y, v)
  const siblings = unit.id !== undefined && (map.units ?? []).some((u) => u !== unit && u.id === unit.id)  // another variant keeps the name
  return withMap(doc, mapId, unit.id && !siblings ? scrubMap(doc, dropBehaviourUnit(removed, unit.id), [{ kind: 'unit', id: unit.id }]) : removed)
}

/** The names that go when side `sideId` goes: the side, its team when no other side shares it, and its units' story names. */
export function sideGoneRefs(map: MapFile, sideId: string): Ref[] {
  const side = map.sides?.find((s) => s.id === sideId)
  if (!side) return []
  const refs: Ref[] = [{ kind: 'side', id: sideId }]
  const team = side.team?.trim() ? side.team : side.id
  const others = (map.sides ?? []).filter((s) => s.id !== sideId)
  if (!others.some((s) => (s.team?.trim() ? s.team : s.id) === team)) refs.push({ kind: 'team', id: team })
  for (const u of map.units ?? []) if (u.side === sideId && u.id) refs.push({ kind: 'unit', id: u.id })
  return refs
}

export const sideRemovalUsages = (doc: ProjectDoc, mapId: string, sideId: string): Usage[] => {
  const map = doc.maps[mapId]
  return map ? storyUsages(doc, sideGoneRefs(map, sideId), mapId) : []
}

/** Deletes the side (and its units, as before) and cleans the events and objectives that named it or its units. */
export function removeSideAndStory(doc: ProjectDoc, mapId: string, sideId: string): ProjectDoc {
  const map = doc.maps[mapId]
  if (!map) return doc
  const gone = sideGoneRefs(map, sideId)
  const removed = removeSide(map, sideId)
  return removed === map ? doc : withMap(doc, mapId, scrubMap(doc, removed, gone))
}
