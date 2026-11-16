// What a campaign scene can point at. A scene knows no map: its speakers are the named units (story names) of the
// human-controlled sides of ANY of the game's maps (the "roster", docs/EVENTS.md "Carry-over"), and "give resources"
// names a side id that the maps use. To let the story command forms and pickers work unchanged, a scene is edited
// against a SCENE MAP: a made-up one-square MapFile whose sides and placed units are those, so `namedUnits`, `sideName`,
// `unitText` and the defaults all answer for the roster. It is never saved.
import { forEachCommand } from './storyWalk'
import { storyIdOf } from './characterUnits'
import { namedUnits } from './storyIds'
import type { Command, StoryEvent } from './storyTypes'
import type { MapFile, MapSide, MapUnit, ProjectDoc } from './types'

/** A side the map does not give to the computer (the loader's rule for "human"). */
export const isHumanSide = (s: Pick<MapSide, 'controller'>): boolean => s.controller !== 'ai'

const mapList = (doc: ProjectDoc): MapFile[] => Object.values(doc.maps).filter((m): m is MapFile => m !== null)

/** Units the maps name (placed ones, then `addUnit` ones) with their side id, in map order. */
function namedPlacements(m: MapFile): Array<{ unit: MapUnit; side: string }> {
  const out: Array<{ unit: MapUnit; side: string }> = []
  for (const u of m.units ?? []) if (storyIdOf(u) !== undefined) out.push({ unit: u, side: u.side })
  for (const e of m.events ?? []) {
    forEachCommand(e.do, '', (c: Command) => {
      const u = c.addUnit
      if (u && typeof u === 'object' && storyIdOf(u) !== undefined) out.push({ unit: u, side: u.side })
    })
  }
  return out
}

/** Every story name any map defines, on any side. */
export function definedStoryIds(doc: ProjectDoc): Set<string> {
  const out = new Set<string>()
  for (const m of mapList(doc)) for (const p of namedPlacements(m)) out.add(storyIdOf(p.unit)!)
  return out
}

/** Every side id any map has. */
export function allSideIds(doc: ProjectDoc): Set<string> {
  const out = new Set<string>()
  for (const m of mapList(doc)) for (const s of m.sides ?? []) out.add(s.id)
  return out
}

/** The sides a scene can give resources to: human-controlled ones, each id once (the first map's name wins). */
export function humanSides(doc: ProjectDoc): MapSide[] {
  const out: MapSide[] = []
  for (const m of mapList(doc)) for (const s of m.sides ?? []) if (isHumanSide(s) && !out.some((o) => o.id === s.id)) out.push(s)
  return out
}

/** The roster's units: named units on human sides, each id once (the first map's unit wins). */
export function rosterUnits(doc: ProjectDoc): MapUnit[] {
  const out: MapUnit[] = []
  for (const m of mapList(doc)) {
    const human = new Set((m.sides ?? []).filter(isHumanSide).map((s) => s.id))
    for (const p of namedPlacements(m)) if (human.has(p.side) && !out.some((o) => storyIdOf(o) === storyIdOf(p.unit))) out.push(p.unit)
  }
  return out
}

/** The made-up map a scene is edited against (see the top of the file). */
export function sceneMap(doc: ProjectDoc): MapFile {
  return {
    name: 'Story', grid: { type: 'square', width: 1, height: 1 }, legend: {}, tiles: [],
    sides: humanSides(doc),
    units: rosterUnits(doc).map((u) => ({ ...(u.type ? { type: u.type } : {}), side: u.side, x: 0, y: 0, ...(u.character ? { character: u.character } : { id: u.id }), ...(u.name ? { name: u.name } : {}) })),
  }
}

/** The label a roster unit gets in pickers ("Eirik (eirik)"). */
export const rosterLabel = (doc: ProjectDoc, id: string): string => {
  const u = namedUnits(doc, sceneMap(doc)).find((x) => x.id === id)
  return u ? u.label : id
}

/** The scene's commands, wrapped as the one event of the scene map, so every command operation of the map editor
 *  (`addCommand`, `patchCommand`, ...) can run on it by event id. */
export const sceneEvent = (eventId: string, commands: Command[]): StoryEvent => ({ id: eventId, when: { battle: 'start' }, do: commands })

export const sceneEventId = (chapterId: string, which: 'before' | 'after'): string => `scene:${chapterId}:${which}`
