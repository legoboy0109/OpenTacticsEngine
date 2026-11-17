// What the loader refuses about the "read a unit's number" form of setVariable / addVariable that is not about a missing
// name (the names are checked with every other reference): engine/core/data/loader_unit_reading.cpp. Shared by the map
// checks (storyChecks.ts) and the campaign checks (campaignChecks.ts).
import { commandKind, type Command } from './storyTypes'

const has = (c: Command, k: string) => Object.prototype.hasOwnProperty.call(c, k)

/** Plain sentences, none when the command is fine. `scene`: a campaign scene (where a unit has no hit points between battles). */
export function readingProblems(c: Command, scene: boolean): string[] {
  const kind = commandKind(c)
  if (kind !== 'setVariable' && kind !== 'addVariable') return []
  const reads = (['level', 'hp', 'maxHp', 'stat'] as const).filter((k) => has(c, k))
  const number = kind === 'setVariable' ? 'to' : 'by'
  const out: string[] = []
  if (reads.length === 0) {
    if (has(c, 'of')) out.push('“Of” belongs to a stat reading; choose “A stat of a unit” or remove it.')
    return out
  }
  if (has(c, number)) out.push('give either a number or a reading from a unit, not both.')
  if (reads.length > 1) out.push('read only one thing from the unit (level, hit points, most hit points or a stat).')
  if (reads.includes('stat') && !has(c, 'of')) out.push('a stat reading needs the unit to read it from.')
  if (!reads.includes('stat') && has(c, 'of')) out.push('“Of” belongs to a stat reading only.')
  if (scene && reads.includes('hp')) out.push('a scene cannot read hit points (units have none between battles); read the most hit points instead.')
  return out
}
