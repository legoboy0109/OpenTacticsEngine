// Editor-side checks for characters (docs/SRPG.md "Checks / Problems"), beside the loader's: the loader refuses what
// cannot load; these point at what loads but is probably not what the maker meant.
import { quoteChecks } from './quoteChecks'
import { forEachCommand } from './storyWalk'
import type { ProjectDoc } from './types'

export interface CharacterCheck { characterId: string; text: string }

/** Ids of the characters some map places or some event adds (an `addUnit` that says "character"). */
export function charactersInUse(doc: ProjectDoc): Set<string> {
  const used = new Set<string>()
  for (const m of Object.values(doc.maps)) {
    if (!m) continue
    for (const u of m.units ?? []) if (typeof u.character === 'string') used.add(u.character)
    for (const e of m.events ?? []) forEachCommand(e.do, '', (c) => { if (typeof c.addUnit?.character === 'string') used.add(c.addUnit.character) })
  }
  return used
}

export const NOT_USED = 'Not used on any map.'

export function characterChecks(doc: ProjectDoc): CharacterCheck[] {
  const list = doc.characters ?? []
  if (list.length === 0) return []
  const quotes = quoteChecks(doc)  // docs/EVENTS.md "Battle and defeat quotes"
  // A map that could not be read might place the character: say nothing rather than something wrong.
  if (Object.values(doc.maps).some((m) => m === null)) return quotes
  const used = charactersInUse(doc)
  return [...list.filter((c) => !used.has(c.id)).map((c) => ({ characterId: c.id, text: NOT_USED })), ...quotes]
}
