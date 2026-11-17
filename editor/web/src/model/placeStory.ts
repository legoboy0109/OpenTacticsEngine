// The doors, chests and places of a map as the story names them (docs/SRPG.md section 10, docs/EVENTS.md): a placed structure with an
// `id`. A `place` trigger and the commands openDoor / closeDoor / lockDoor / unlockDoor / openChest each fit only some kinds; the loader's
// rules (engine/core/data/loader_places_story.cpp) are kept here so the pickers list only what fits and the checks say why not.
import { placeBlockOf, placeKindOf, type PlaceKind } from './places'
import type { CommandKind, PlaceEvent } from './storyTypes'
import type { MapFile, ProjectDoc } from './types'

export interface PlaceRef {
  id: string
  kindId: string
  kindName: string
  kind: PlaceKind | null
  x: number
  y: number
  /** A place whose kind has a `ruin` can be destroyed. */
  canBeDestroyed: boolean
}

/** Every structure of the map that has a story name. */
export function placesOf(doc: ProjectDoc, map: MapFile): PlaceRef[] {
  const out: PlaceRef[] = []
  for (const s of map.structures ?? []) {
    if (typeof s.id !== 'string' || s.id === '') continue
    const def = doc.structures?.find((k) => k.id === s.type)
    const kind = placeKindOf(def)
    out.push({ id: s.id, kindId: s.type, kindName: def?.name ?? s.type, kind, x: s.x, y: s.y, canBeDestroyed: kind === 'visit' && typeof (placeBlockOf(def) as { ruin?: unknown })?.ruin === 'string' })
  }
  return out
}

export const placeLabel = (p: PlaceRef): string => `${p.id} (${p.kindName})`

/** What each command works on. */
export const COMMAND_PLACES: Partial<Record<CommandKind, PlaceKind[]>> = {
  openDoor: ['door'], closeDoor: ['door'], lockDoor: ['door'], unlockDoor: ['door', 'chest'], openChest: ['chest'],
}
/** What each `place` trigger waits for. */
export const EVENT_PLACES: Record<PlaceEvent, PlaceKind[]> = { opened: ['door', 'chest'], visited: ['visit'], destroyed: ['visit'] }

export function placesForCommand(doc: ProjectDoc, map: MapFile, cmd: CommandKind): PlaceRef[] {
  const kinds = COMMAND_PLACES[cmd] ?? []
  return placesOf(doc, map).filter((p) => p.kind !== null && kinds.includes(p.kind))
}
export function placesForEvent(doc: ProjectDoc, map: MapFile, is: PlaceEvent): PlaceRef[] {
  return placesOf(doc, map).filter((p) => p.kind !== null && EVENT_PLACES[is].includes(p.kind) && (is !== 'destroyed' || p.canBeDestroyed))
}

const NOUN: Record<string, string> = { door: 'a door', chest: 'a chest', visit: 'a place that can be visited' }
/** Why `id` does not fit the command / trigger, or null when it does. */
export function placeProblem(doc: ProjectDoc, map: MapFile, id: string, accepts: { command: CommandKind } | { is: PlaceEvent }): string | null {
  if (id === '') return 'pick a door, chest or place of this map (give it a story name in the map’s structure panel first).'
  const exists = (map.structures ?? []).some((s) => s.id === id)
  if (!exists) return `“${id}” is not the story name of a door, chest or place of this map (it may have been removed or renamed in the map).`
  const p = placesOf(doc, map).find((x) => x.id === id)
  const kinds = 'command' in accepts ? COMMAND_PLACES[accepts.command] ?? [] : EVENT_PLACES[accepts.is]
  if (!p || p.kind === null || !kinds.includes(p.kind)) return `“${id}” is not ${kinds.map((k) => NOUN[k]).join(' or ')}.`
  if ('is' in accepts && accepts.is === 'destroyed' && !p.canBeDestroyed) return `“${id}” cannot be destroyed (its kind needs a “ruin” in its Visit part).`
  return null
}

/** The first place that fits, so a new trigger or command starts valid where it can be. */
export const firstPlaceFor = (doc: ProjectDoc, map: MapFile, cmd: CommandKind): string => placesForCommand(doc, map, cmd)[0]?.id ?? ''
export const firstPlaceForEvent = (doc: ProjectDoc, map: MapFile, is: PlaceEvent): string => placesForEvent(doc, map, is)[0]?.id ?? ''
