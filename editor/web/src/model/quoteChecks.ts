// Editor-side checks of characters' quotes (docs/EVENTS.md "Battle and defeat quotes"). They mirror what the loader refuses (an
// empty quote, an unknown or own character in `against`, two quotes against the same character, a line spoken by an unknown
// character) in plain words, so the maker sees them next to the card before the Problems panel does; plus one that only the
// editor knows: lines in a defeat set the game never uses.
import { defeatSets } from './quotes'
import type { Command } from './storyTypes'
import type { ProjectDoc } from './types'

export interface QuoteCheck { characterId: string; text: string }

export const QUOTE_TEXT = {
  noLines: (n: number) => `Battle quote ${n} has no lines.`,
  emptyLine: (where: string, n: number) => `${where}, line ${n} is empty.`,
  unknownAgainst: (n: number, id: string) => `Battle quote ${n} is against “${id}”, who is not a character.`,
  againstSelf: (n: number) => `Battle quote ${n} is against the character itself.`,
  duplicate: (n: number, who: string) => `Battle quote ${n} is a second one ${who}; only the first can ever be said.`,
  unknownSpeaker: (where: string, id: string) => `${where} has a line spoken by “${id}”, who is not a character.`,
  unusedSet: (set: string) => `The ${set} lines are never used by this game (see the campaign’s “When a unit is defeated” setting).`,
}

/** What a battle quote looks like straight after "Add a battle quote": against anyone, the default frequency and one line that is just an
 *  empty `say`. The check stays quiet about its empty line until the maker has done anything to the quote (typed, picked who it is
 *  against, how often, a speaker or a portrait, added a line); the file is the same either way. */
const justAdded = (b: { against?: string; once?: string; lines: Command[] }) =>
  b.against === undefined && b.once === undefined && b.lines.length === 1 && b.lines[0].say === '' && Object.keys(b.lines[0]).length === 1

const blank = (l: Command) => !(typeof l.say === 'string' && l.say.trim() !== '')

export function quoteChecks(doc: ProjectDoc): QuoteCheck[] {
  const out: QuoteCheck[] = []
  const ids = new Set((doc.characters ?? []).map((c) => c.id))
  const sets = defeatSets(doc)
  const lineChecks = (characterId: string, where: string, lines: Command[], fresh = false) => {
    lines.forEach((l, i) => {
      if (blank(l) && !fresh) out.push({ characterId, text: QUOTE_TEXT.emptyLine(where, i + 1) })
      if (typeof l.speaker === 'string' && !ids.has(l.speaker)) out.push({ characterId, text: QUOTE_TEXT.unknownSpeaker(where, l.speaker) })
    })
  }
  for (const c of doc.characters ?? []) {
    const q = c.quotes
    if (!q) continue
    const seen = new Set<string>()
    ;(q.battle ?? []).forEach((b, i) => {
      const n = i + 1
      const where = `Battle quote ${n}`
      if (b.lines.length === 0) out.push({ characterId: c.id, text: QUOTE_TEXT.noLines(n) })
      lineChecks(c.id, where, b.lines, justAdded(b))
      if (b.against === c.id) out.push({ characterId: c.id, text: QUOTE_TEXT.againstSelf(n) })
      else if (b.against !== undefined && !ids.has(b.against)) out.push({ characterId: c.id, text: QUOTE_TEXT.unknownAgainst(n, b.against) })
      const key = b.against ?? ''
      if (seen.has(key)) {
        const who = b.against === undefined ? 'against anyone' : `against ${doc.characters?.find((x) => x.id === b.against)?.name ?? b.against}`
        out.push({ characterId: c.id, text: QUOTE_TEXT.duplicate(n, who) })
      }
      seen.add(key)
    })
    for (const set of ['falls', 'retreats'] as const) {
      const lines = q.defeat?.[set]
      if (!lines) continue
      lineChecks(c.id, set === 'falls' ? 'Defeat quote' : 'Retreat quote', lines)
      if (!sets.includes(set) && lines.length > 0) out.push({ characterId: c.id, text: QUOTE_TEXT.unusedSet(set) })
    }
  }
  return out
}
