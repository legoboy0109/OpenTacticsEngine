// The story commands that change the map and the fog (docs/EVENTS.md "Changing the map"): their sentences and the checks that mirror the
// loader (engine/core/data/loader_story_mapchange.cpp). Kept out of storySummary.ts / storyChecks.ts, which only call these.
import { changesOf } from './mapChanges'
import { rectOf, type Command } from './storyTypes'
import type { MapFile, ProjectDoc } from './types'

const q = (s: string) => `“${s}”`
const areaText = (r: { x: number; y: number; w: number; h: number }): string =>
  r.w === 1 && r.h === 1 ? `square ${r.x + 1}, ${r.y + 1}` : `area from ${r.x + 1}, ${r.y + 1}, ${r.w} × ${r.h}`

/** "Turns 4 squares into Bridge" / "Applies the change “bridge”" ... or null for a command of another kind. */
export function mapChangeText(doc: ProjectDoc, c: Command): string | null {
  const terrainName = (id: string) => doc.terrain?.find((t) => t.id === id)?.name ?? id
  if (c.setTerrain !== undefined) {
    const what = Array.isArray(c.cells) ? `${c.cells.length} ${c.cells.length === 1 ? 'square' : 'squares'}` : `the ${areaText(rectOf(c.area ?? { x: 0, y: 0, w: 1, h: 1 }))}`
    return `Turns ${what} into ${terrainName(c.setTerrain)}`
  }
  if (c.applyChange !== undefined) return `Applies the map change ${q(c.applyChange)}`
  if (c.revertChange !== undefined) return `Takes back the map change ${q(c.revertChange)}`
  if (c.setFog !== undefined) return c.setFog ? 'Fog of war comes down' : 'The fog of war lifts'
  if (c.setVision !== undefined) return `Sets the vision of every unit to ${c.setVision >= 0 ? '+' : ''}${c.setVision} from its normal sight`
  return null
}

/** What the loader refuses about these five commands (the names are checked by the story walk). */
export function mapChangeChecks(map: MapFile, c: Command, where: string, say: (w: string, t: string) => void): void {
  if (c.setTerrain !== undefined) {
    const hasArea = c.area !== undefined, hasCells = c.cells !== undefined
    if (hasArea === hasCells) say(where, 'give the squares as an area or as a list of squares (exactly one of them).')
    if (hasArea) {
      const a = rectOf(c.area!)
      if (a.w < 1 || a.h < 1 || a.x < 0 || a.y < 0 || a.x + a.w > map.grid.width || a.y + a.h > map.grid.height) say(where, 'the area does not fit inside the map.')
    }
    if (hasCells) {
      if (!Array.isArray(c.cells) || c.cells.length === 0 || c.cells.length > 4096) say(where, 'the squares must be a list of 1 to 4096.')
      else if (c.cells.some((p) => !Array.isArray(p) || p.length !== 2 || !Number.isInteger(p[0]) || !Number.isInteger(p[1]) || p[0] < 0 || p[1] < 0 || p[0] >= map.grid.width || p[1] >= map.grid.height)) say(where, 'a square is outside the map.')
    }
  }
  if (c.applyChange !== undefined || c.revertChange !== undefined) {
    const id = c.applyChange ?? c.revertChange ?? ''
    if (Object.keys(changesOf(map)).length === 0 && id) say(where, 'this map has no changes. Make one in the “Changes” tab first.')
  }
  if (c.setVision !== undefined && (!Number.isInteger(c.setVision) || c.setVision < -10 || c.setVision > 10)) say(where, 'the vision must be a whole number from -10 to 10.')
  if (c.setFog !== undefined && typeof c.setFog !== 'boolean') say(where, 'fog is on or off.')
}
