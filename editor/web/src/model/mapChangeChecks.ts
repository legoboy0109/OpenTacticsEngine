// Checks on a map's named changes and the fog options of the map (docs/EVENTS.md "Changing the map", docs/ECONOMY.md "Fog of war"). They mirror
// what the loader refuses (engine/core/data/loader_map_changes.cpp, loader_fog.cpp) and add notes the loader does not give: a change nobody
// applies, and a placed unit that would be left on a square it cannot stand on (the game then moves it to the nearest square it can).
import { cellsOf, changesOf, CHANGE_CELLS_MAX, CHANGE_MAX, isChangeId, swapsOf } from './mapChanges'
import { ctxOf } from './mapCtx'
import { forEachCommand } from './storyWalk'
import { sizeOf, ringsOf } from './structures'
import { canEnter } from './structuresEffective'
import { structureAt } from './mapStructures'
import { typeIdOf } from './characterUnits'
import type { ProjectDoc } from './types'

export interface ChangeCheck { mapId: string; text: string; note?: boolean }

export function changeChecks(doc: ProjectDoc, mapId: string): ChangeCheck[] {
  const map = doc.maps[mapId]
  const c = ctxOf(doc)
  const out: ChangeCheck[] = []
  if (!map || !c) return out
  const say = (text: string, note?: boolean) => out.push({ mapId, text, ...(note ? { note } : {}) })
  const ids = Object.keys(changesOf(map))
  if (ids.length > CHANGE_MAX) say(`The map has ${ids.length} changes; the game takes at most ${CHANGE_MAX}.`)

  // Fog options of the map and of its computer sides.
  const m = map as { fogVision?: unknown; aiSeesAll?: unknown }
  if (m.fogVision !== undefined && (!Number.isInteger(m.fogVision) || (m.fogVision as number) < -10 || (m.fogVision as number) > 10)) say('“Extra vision” must be a whole number from -10 to 10.')
  if ((m.fogVision || m.aiSeesAll) && map.fog !== true) say('“Extra vision” and “The computer sees everything” only work on a map with fog of war. Switch fog on, or clear them.')
  for (const s of map.sides ?? []) {
    if (s.aiSeesAll !== undefined && s.controller !== 'ai') say(`${s.name}: “sees everything” is for sides the computer plays.`)
  }

  const applied = new Set<string>()
  for (const e of map.events ?? []) forEachCommand(e.do, '', (cmd) => { if (typeof cmd.applyChange === 'string') applied.add(cmd.applyChange) })

  for (const id of ids) {
    const ch = changesOf(map)[id]
    const at = `The change “${id}”`
    if (!isChangeId(id)) say(`${at}: its name may only have letters, digits, “_” and “-”.`)
    const cells = cellsOf(ch), swaps = swapsOf(ch)
    if (cells.length === 0 && swaps.length === 0) say(`${at} changes nothing yet: paint some squares into it (the Terrain tool while the change is selected) or swap a structure. The game will not load until it does something or is deleted.`)
    if (cells.length > CHANGE_CELLS_MAX) say(`${at} has ${cells.length} squares; the game takes at most ${CHANGE_CELLS_MAX}.`)
    const seen = new Set<string>()
    for (const k of cells) {
      if (!Number.isInteger(k.x) || !Number.isInteger(k.y) || k.x < 0 || k.y < 0 || k.x >= map.grid.width || k.y >= map.grid.height) { say(`${at}: the square ${k.x + 1}, ${k.y + 1} is outside the map.`); continue }
      if (!c.terrain.some((t) => t.id === k.terrain)) say(`${at}: the square ${k.x + 1}, ${k.y + 1} names terrain “${k.terrain}”, which this game does not have.`)
      const key = `${k.x},${k.y}`
      if (seen.has(key)) say(`${at}: the square ${k.x + 1}, ${k.y + 1} is in it twice.`)
      seen.add(key)
    }
    const swapped = new Set<string>()
    for (const s of swaps) {
      const placed = (map.structures ?? []).find((p) => p.id === s.id && s.id !== '')
      const to = c.structureTypes.find((k) => k.id === s.kind)
      if (!placed) { say(`${at}: no structure of this map has the story name “${s.id}”.`); continue }
      if (!to) { say(`${at}: “${s.kind}” is not a structure kind of this game.`); continue }
      const from = c.structureTypes.find((k) => k.id === placed.type)
      if (from && (sizeOf(from).w !== sizeOf(to).w || sizeOf(from).h !== sizeOf(to).h || ringsOf(from) !== ringsOf(to)))
        say(`${at}: “${to.name}” does not have the size of “${from.name}”, the kind placed there, so it cannot take its place.`)
      if (swapped.has(s.id)) say(`${at}: the structure “${s.id}” is in it twice.`)
      swapped.add(s.id)
    }
    if (!applied.has(id) && (cells.length > 0 || swaps.length > 0)) say(`${at} is never applied: no event of this map has an “Apply a map change” command for it.`, true)
    // A placed unit that the change would leave on a square it cannot stand on (the game moves it to the nearest square it can).
    for (const u of map.units ?? []) {
      const k = cells.find((x) => x.x === u.x && x.y === u.y)
      if (!k) continue
      const type = c.unitTypes.find((t) => t.id === typeIdOf(c.characters, u))
      const t = c.terrain.find((x) => x.id === k.terrain)
      const st = structureAt(map, c, u.x, u.y)
      if (type && t && !canEnter(t, c.structureTypes.find((s) => s.id === st?.type), type))
        say(`${at}: ${type.name} at ${u.x + 1}, ${u.y + 1} would be left on ${t.name}, which it cannot enter. The game moves it to the nearest square it can stand on.`, true)
    }
  }
  return out
}
