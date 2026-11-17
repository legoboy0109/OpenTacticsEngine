// Battle and defeat quotes of a character (docs/EVENTS.md "Battle and defeat quotes", docs/SRPG.md "Characters"): the edits
// behind the Quotes card of the Characters screen, what the delete question lists, and the clean-up when a character goes.
// Pure functions on the document. The lines are ordinary `say` commands, edited with the same forms as the story's.
import { withCharacters } from './doc'
import { fallenRule } from './campaign'
import type { Usage } from './references'
import type { Character, CharacterQuotes, Quote, QuoteOnce } from './srpgTypes'
import type { Command } from './storyTypes'
import type { ProjectDoc } from './types'
import { moveItem, patchObj, updateById } from './util'

export type { Quote, QuoteOnce }

export const QUOTE_ONCE: QuoteOnce[] = ['map', 'campaign', 'always']
/** The `once` a quote has when the key is absent. */
export const DEFAULT_ONCE: QuoteOnce = 'map'
export const MAX_LINES = 200

export const quotesOf = (doc: ProjectDoc, id: string): CharacterQuotes => doc.characters?.find((c) => c.id === id)?.quotes ?? {}
export const battleQuotes = (doc: ProjectDoc, id: string): Quote[] => quotesOf(doc, id).battle ?? []
export const onceOf = (q: Quote): QuoteOnce => q.once ?? DEFAULT_ONCE

/** Which defeat sets the game uses (docs/EVENTS.md): without a campaign, or when fallen units are gone, every defeat "falls"; when
 *  the campaign brings fallen units back, a unit of the player's retreats and every other falls. */
export function defeatSets(doc: ProjectDoc): Array<'falls' | 'retreats'> {
  return doc.campaign && fallenRule(doc.campaign) === 'return' ? ['falls', 'retreats'] : ['falls']
}

/** A character's quotes with empty parts removed: no empty list, no empty object, and the whole key goes when nothing is left. */
function tidy(q: CharacterQuotes): CharacterQuotes | undefined {
  const out: CharacterQuotes = { ...q }
  if (!out.battle?.length) delete out.battle
  if (out.defeat) {
    const d = { ...out.defeat }
    if (!d.falls?.length) delete d.falls
    if (!d.retreats?.length) delete d.retreats
    if (Object.keys(d).length) out.defeat = d
    else delete out.defeat
  }
  return Object.keys(out).length ? out : undefined
}

function withQuotes(doc: ProjectDoc, id: string, fn: (q: CharacterQuotes) => CharacterQuotes): ProjectDoc {
  return withCharacters(doc, (list) => updateById(list, id, (c: Character) => patchObj(c, { quotes: tidy(fn(c.quotes ?? {})) })))
}

/** A new battle quote at the end: against anyone, once per map, one empty line the maker fills in. The line has no speaker, so the
 *  character itself says it. */
export const addQuote = (doc: ProjectDoc, id: string): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, battle: [...(q.battle ?? []), { lines: [{ say: '' }] }] }))

export const removeQuote = (doc: ProjectDoc, id: string, index: number): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, battle: (q.battle ?? []).filter((_, i) => i !== index) }))

export const moveQuote = (doc: ProjectDoc, id: string, from: number, to: number): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, battle: moveItem(q.battle ?? [], from, to) }))

/** Who the quote is against: a character id, or undefined = anyone. */
export const setQuoteAgainst = (doc: ProjectDoc, id: string, index: number, against: string | undefined): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, battle: (q.battle ?? []).map((x, i) => (i === index ? patchObj(x, { against }) : x)) }))

/** How often: the default ("map") is not written. */
export const setQuoteOnce = (doc: ProjectDoc, id: string, index: number, once: QuoteOnce): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, battle: (q.battle ?? []).map((x, i) => (i === index ? patchObj(x, { once: once === DEFAULT_ONCE ? undefined : once }) : x)) }))

/** The lines of one quote (the list may be empty: the check and the loader say so). */
export const setQuoteLines = (doc: ProjectDoc, id: string, index: number, lines: Command[]): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, battle: (q.battle ?? []).map((x, i) => (i === index ? { ...x, lines } : x)) }))

export const defeatLines = (doc: ProjectDoc, id: string, set: 'falls' | 'retreats'): Command[] => quotesOf(doc, id).defeat?.[set] ?? []

/** One defeat set; an empty list removes it. */
export const setDefeatLines = (doc: ProjectDoc, id: string, set: 'falls' | 'retreats', lines: Command[]): ProjectDoc =>
  withQuotes(doc, id, (q) => ({ ...q, defeat: { ...(q.defeat ?? {}), [set]: lines } }))

// --- who refers to a character --------------------------------------------------------------------------------------------

const speaks = (lines: Command[] | undefined, id: string) => !!lines?.some((l) => l.speaker === id)

/** Quotes of OTHER characters that name this one: a quote against it, or a line it speaks. */
export function quoteUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const c of doc.characters ?? []) {
    if (c.id === id || !c.quotes) continue
    for (const q of c.quotes.battle ?? []) {
      if (q.against === id) out.push({ section: 'characters', text: `${c.name}’s battle quote against it (the quote goes with it)` })
      else if (speaks(q.lines, id)) out.push({ section: 'characters', text: `${c.name}’s battle quote has lines it speaks (those lines go with it)` })
    }
    for (const set of ['falls', 'retreats'] as const)
      if (speaks(c.quotes.defeat?.[set], id)) out.push({ section: 'characters', text: `${c.name}’s defeat quote has lines it speaks (those lines go with it)` })
  }
  return out
}

/** After a character is deleted: other characters' quotes against it go, and so do the lines it speaks (a quote left without
 *  lines goes too, and an emptied defeat set). The deleted character's own quotes go with its entry. */
export function cleanQuotes(doc: ProjectDoc, id: string): ProjectDoc {
  const list = doc.characters
  if (!list || quoteUsageLines(doc, id).length === 0) return doc
  const keep = (lines: Command[]) => lines.filter((l) => l.speaker !== id)
  const characters = list.map((c) => {
    if (c.id === id || !c.quotes) return c
    const battle = (c.quotes.battle ?? [])
      .filter((q) => q.against !== id)
      .map((q) => (speaks(q.lines, id) ? { ...q, lines: keep(q.lines) } : q))
      .filter((q) => q.lines.length > 0)
    const defeat = c.quotes.defeat && { ...c.quotes.defeat, ...(speaks(c.quotes.defeat.falls, id) ? { falls: keep(c.quotes.defeat.falls ?? []) } : {}), ...(speaks(c.quotes.defeat.retreats, id) ? { retreats: keep(c.quotes.defeat.retreats ?? []) } : {}) }
    return patchObj(c, { quotes: tidy({ ...c.quotes, battle, defeat }) })
  })
  return { ...doc, characters }
}
