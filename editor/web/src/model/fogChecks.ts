// Checks that mirror what the game loader refuses about fog of war (engine/core/data/loader_fog.cpp), so a maker sees them
// in the editor before pressing Play. Bad `sees` conditions are in storyChecks.ts (they share its walk). The wording is
// plain; the loader's own messages still show in the Problems list after a save.
import { BONUS_MAX, FOG_MAX_SIDES, VISION_MAX } from './fog'
import type { ProjectDoc, Section } from './types'

export interface FogCheck { section: Section; text: string; mapId?: string }

const isInt = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)
const bad = (v: unknown, max: number): boolean => v !== undefined && (!isInt(v) || v < 0 || v > max)

function groundChecks(doc: ProjectDoc, out: FogCheck[]): void {
  const lists: Array<[Section, Array<{ name: string; visionBonus?: number; hides?: unknown }>]> = [['terrain', doc.terrain ?? []], ['structures', doc.structures ?? []]]
  for (const [section, list] of lists)
    for (const g of list) {
      const say = (text: string) => out.push({ section, text: `${g.name}: ${text}` })
      if (bad(g.visionBonus, BONUS_MAX)) say(`the extra sight must be a whole number from 0 to ${BONUS_MAX}.`)
      if (g.hides !== undefined && typeof g.hides !== 'boolean') say('“hides” must be on or off.')
    }
}

/** Everything the loader would refuse about fog of war, with the screen each belongs to (maps carry their id). */
export function fogChecks(doc: ProjectDoc): FogCheck[] {
  const out: FogCheck[] = []
  for (const u of doc.units ?? [])
    if (bad(u.vision, VISION_MAX)) out.push({ section: 'units', text: `${u.name}: the sight must be a whole number from 0 to ${VISION_MAX}.` })
  groundChecks(doc, out)
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    if (m.fog !== undefined && typeof m.fog !== 'boolean') out.push({ section: 'maps', mapId, text: 'Fog of war must be on or off.' })
    else if (m.fog === true && (m.sides?.length ?? 0) > FOG_MAX_SIDES)
      out.push({ section: 'maps', mapId, text: `Fog of war works with at most ${FOG_MAX_SIDES} sides; this map has ${m.sides!.length}. Remove sides or switch fog off.` })
  }
  return out
}
