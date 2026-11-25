// The `showMap` / `returnMap` commands (docs/EVENTS.md "Showing another map"): a scene played on another map of the game. Between the two
// ("a section") the commands act on the SHOWN map. Checks mirror the loader (engine/core/data/loader_story_show_map.cpp,
// loader_story_commands.cpp `kNotOnShownMap`, loader_story_refs.cpp); the walk leaves a section's ids alone (storyWalk.ts).
import { storyIdOf } from './characterUnits'
import { rawSections, type RawSection } from './storySections'
import type { Command, CommandKind } from './storyTypes'
import { usesActorFilter } from './storyExtras'
import { mapCommands, type Ref, type RefKind } from './storyWalk'
import type { MapFile, ProjectDoc } from './types'

/** A section with the shown map's data: its units' story names and the names the section's own `addUnit` commands add. */
export interface Section extends RawSection {
  mapId: string
  /** The shown map (missing when game.json does not list it or its file is missing). */
  shown: MapFile | undefined
  /** Story names of the shown map's units and of the units this section adds. */
  unitIds: string[]
}

export interface Sections {
  list: Section[]
  /** The section a command acts in (nested commands too); the showMap itself maps to its own section as well. */
  of: Map<Command, Section>
  /** returnMaps with no showMap before them. */
  stray: Set<Command>
}

/** The sections of an event's command list, with the shown maps looked up in the document. */
export function sectionsOf(doc: ProjectDoc | null, list: Command[] | undefined): Sections {
  const { sections, stray } = rawSections(list)
  const out: Sections = { list: [], of: new Map(), stray: new Set(stray) }
  for (const r of sections) {
    const mapId = typeof r.show.showMap === 'string' ? r.show.showMap : ''
    const shown = doc && mapId ? doc.maps[mapId] ?? undefined : undefined
    const unitIds: string[] = []
    for (const u of shown?.units ?? []) { const id = storyIdOf(u); if (id !== undefined && !unitIds.includes(id)) unitIds.push(id) }
    for (const c of r.cmds) { const id = c.addUnit?.id; if (typeof id === 'string' && id !== '' && !unitIds.includes(id)) unitIds.push(id) }
    const s: Section = { ...r, mapId, shown, unitIds }
    out.list.push(s)
    out.of.set(r.show, s)
    for (const c of r.cmds) out.of.set(c, s)
  }
  return out
}

/** Calls `fn` for the commands of an event's list that act on the event's own map: the section's commands are left out. */
export function ownCommands(list: Command[] | undefined, fn: (c: Command) => void): void {
  const members = sectionsOf(null, list).of
  const walk = (l: Command[] | undefined): void => {
    for (const c of Array.isArray(l) ? l : []) {
      if (!members.has(c)) fn(c)
      walk(c.then); walk(c.else); walk(c.loop)
      for (const a of c.answers ?? []) walk(a.do)
    }
  }
  walk(list)
}

/** Commands that act on the battle itself and are refused while another map is shown (the loader's `kNotOnShownMap`). */
export const NOT_ON_SHOWN_MAP: CommandKind[] = ['win', 'lose', 'endTurn', 'choice', 'objective', 'giveItem', 'takeItem', 'storeItems', 'giveResource',
  'grantResearch', 'promote', 'teachSkill', 'forgetSkill', 'addSupport', 'setBehaviour', 'wakeGroup', 'setFog', 'setVision', 'showUnit']

const q = (s: string) => `“${s}”`

/** What the loader refuses about a showMap or returnMap command itself. `top` = it is in the event's own list. */
export function showMapProblems(doc: ProjectDoc, ownMapId: string, c: Command, kind: 'showMap' | 'returnMap', top: boolean, s: Sections): string[] {
  const out: string[] = []
  if (kind === 'returnMap') {
    if (c.returnMap !== true) out.push('“Back to the battle’s map” must be switched on (true).')
    if (!top) out.push('“Back to the battle’s map” goes in the event’s own list of commands, like the “Show another map” it ends (not inside an “If” or an answer).')
    else if (s.stray.has(c)) out.push('nothing to come back from: no “Show another map” comes before it in this event.')
    return out
  }
  if (!top) out.push('“Show another map” goes in the event’s own list of commands, not inside an “If” or an answer (the map stays until the “Back”, so both must be at the top of the event).')
  const own = s.of.get(c)
  if (!own || own.show !== c) {
    const open = [...s.of.values()].find((x) => x.again.includes(c))
    out.push(`${open ? q(open.mapId) : 'a map'} is shown already: use “Back to the battle’s map” before showing another one.`)
    return out
  }
  const id = c.showMap
  if (typeof id !== 'string' || id === '') out.push('choose the map to show.')
  else if (!doc.game?.maps.some((m) => m.id === id)) out.push(`${q(id)} is not a map of this game.`)
  else if (id === ownMapId) out.push('this is the event’s own map. Show another map of the game (a flashback, a place that is never played).')
  else if (!own.shown) out.push(`map ${q(id)} cannot be shown: its file is missing.`)
  const f = c.focus
  if (f !== undefined && own.shown) {
    if (typeof f === 'string') { if (!own.unitIds.includes(f)) out.push(`the camera start ${q(f)} is not a unit of map ${q(own.mapId)} (its units with a story name and the ones this section adds).`) }
    else if (f === null || typeof f !== 'object' || !Number.isInteger(f.x) || !Number.isInteger(f.y)) out.push('the camera start must be a unit of the shown map or a square.')
    else if (f.x < 0 || f.y < 0 || f.x >= own.shown.grid.width || f.y >= own.shown.grid.height) out.push(`the camera start (${f.x + 1}, ${f.y + 1}) is outside map ${q(own.mapId)}.`)
  }
  return out
}

/** What a command inside a section may not do (the rest of its checks run against the shown map). */
export function sectionRefusals(c: Command, kind: CommandKind | null): string[] {
  const out: string[] = []
  if (kind && NOT_ON_SHOWN_MAP.includes(kind)) out.push('this acts on the battle itself and cannot run while another map is shown: put it after “Back to the battle’s map”.')
  if (kind === 'removeUnit' && c.hide === true) out.push('“Only hide it” keeps a unit of the battle for later; the shown map’s units are thrown away at the return.')
  if (kind !== 'if' && usesActorFilter(c)) out.push('“The unit that set the event off” is a unit of the battle, not of the shown map.')
  return out
}

/** The commands a section still reads against the battle's own map: an `if`'s conditions, a variable that reads a unit, setPhaseMusic. */
export const BATTLE_READERS: CommandKind[] = ['if', 'setVariable', 'addVariable', 'setPhaseMusic']
const MAP_BOUND: RefKind[] = ['unit', 'side', 'team', 'teamOrSide', 'change']

/** The names one command of a section points at that belong to a map (units, sides, teams, changes): `battle` ones are read against the
 *  event's own map, `shown` ones against the shown map. Nested commands are not looked into (the caller visits each). */
export function sectionRefs(c: Command, kind: CommandKind | null): { battle: Ref[]; shown: Ref[] } {
  const found: Ref[] = []
  const flat: Command = { ...c, then: undefined, else: undefined, answers: undefined }
  mapCommands([flat], { ref: (r) => { if (MAP_BOUND.includes(r.kind)) found.push(r); return r.id } }, '')
  return kind && BATTLE_READERS.includes(kind) ? { battle: found, shown: [] } : { battle: [], shown: found }
}
