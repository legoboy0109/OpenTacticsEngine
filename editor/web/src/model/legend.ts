// The map file's "legend" (one character per terrain) is an implementation
// detail the user never sees. These functions keep it stable across edits.
import type { Terrain } from './types'

const LETTERS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
const DIGITS = '0123456789'
// Never space, quote or backslash: they make the JSON hard to read and edit by hand.
const SYMBOLS = ".,:;!@#$%^&*()-_=+[]{}<>/?|~'"
const POOL = LETTERS + DIGITS + SYMBOLS

/** Printable ASCII that is safe to show inside a JSON string. */
export const isLegendChar = (c: unknown): c is string =>
  typeof c === 'string' && c.length === 1 && c > ' ' && c <= '~' && c !== '"' && c !== '\\'

/**
 * Chooses a character for every terrain id in `used`.
 * Characters the old legend already gave to a terrain are kept; new terrains
 * prefer their own letter/glyph, then the first free character of a fixed
 * pool, so the result only depends on the inputs (never on chance).
 */
export function assignLegend(used: string[], oldLegend: Record<string, string>, terrain: Terrain[]): Record<string, string> {
  const want = new Set(used)
  const charOf = new Map<string, string>()
  const taken = new Set<string>()
  for (const [ch, id] of Object.entries(oldLegend)) {
    if (!want.has(id) || charOf.has(id) || !isLegendChar(ch) || taken.has(ch)) continue
    charOf.set(id, ch)
    taken.add(ch)
  }
  const pos = (id: string) => terrain.findIndex((t) => t.id === id)
  const order = [...used].sort((a, b) => pos(a) - pos(b))
  for (const id of order) {
    if (charOf.has(id)) continue
    const def = terrain.find((t) => t.id === id)
    const first = id[0] ?? ''
    const pick = [def?.glyph, first, first.toUpperCase(), first.toLowerCase()].find((c) => isLegendChar(c) && !taken.has(c))
      ?? [...POOL].find((c) => !taken.has(c))
    if (pick === undefined) throw new Error('more terrains in one map than there are symbols for')
    charOf.set(id, pick)
    taken.add(pick)
  }
  // Legend keys in the order the terrain list has them: a tidy, predictable file.
  return Object.fromEntries(order.map((id) => [charOf.get(id)!, id]))
}
