// The map a showMap section's commands act on, shaped like a map of its own so the editor's pickers and sentences can read it: the shown
// map with, as its only events, the units the section adds (they count as units of the shown map, never of the event's own map).
import { namedUnits, teamsOf, type NamedUnit } from './storyIds'
import type { Section } from './storyShowMap'
import type { MapFile, ProjectDoc } from './types'

export interface ShownScope { map: MapFile; units: NamedUnit[]; teams: Array<{ id: string; label: string }> }

export function shownScope(doc: ProjectDoc, section: Section): ShownScope | null {
  if (!section.shown) return null
  const adds = section.cmds.filter((c) => c.addUnit && typeof c.addUnit === 'object')
  const map: MapFile = { ...section.shown, events: adds.length ? [{ id: '_section', when: {}, do: adds }] : [] }
  return { map, units: namedUnits(doc, map), teams: teamsOf(map) }
}
