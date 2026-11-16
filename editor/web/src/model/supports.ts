// Supports (docs/SRPG.md section 6): the edits behind the Supports screen. Pure functions on the document, like `characters.ts`.
// A pair has no id of its own: the screen finds it by its two characters (`pairKey`), which the loader makes unique.
// Edits keep the file loadable while the person types (levels always rise, level names never fall short of a pair's levels).
import { withSupports } from './doc'
import type { Command } from './storyTypes'
import type { Character, SupportBonus, SupportLevel, SupportPair, SupportRules, SupportTalks, Supports } from './srpgTypes'
import type { ProjectDoc } from './types'
import { moveItem, patchObj } from './util'

/** The loader's limits (engine/core/data/srpg.h). */
export const MAX_LEVELS = 5
export const MAX_POINTS = 1_000_000
export const MAX_RANGE = 10
export const DEFAULT_RANGE = 3
/** Points a new level asks for beyond the one before it. */
const STEP = 20

export const pairsOf = (doc: ProjectDoc): SupportPair[] => doc.supports?.pairs ?? []
/** "eirik.lyra": character ids hold letters, digits, _ and - only, so a dot cannot be part of one. */
export const pairKey = (p: Pick<SupportPair, 'a' | 'b'>): string => `${p.a}.${p.b}`
export const pairIndex = (doc: ProjectDoc, key: string | undefined): number => pairsOf(doc).findIndex((p) => pairKey(p) === key)
export const characterName = (doc: ProjectDoc, id: string): string => doc.characters?.find((c) => c.id === id)?.name || id
export const pairLabel = (doc: ProjectDoc, p: Pick<SupportPair, 'a' | 'b'>): string => `${characterName(doc, p.a)} and ${characterName(doc, p.b)}`
export const samePair = (p: Pick<SupportPair, 'a' | 'b'>, a: string, b: string): boolean => (p.a === a && p.b === b) || (p.a === b && p.b === a)

/** What a level is called: its name from the rules, else its number. */
export const levelName = (rules: SupportRules | undefined, i: number): string => rules?.levelNames?.[i] || String(i + 1)

/** The first two characters that are not a pair yet, or null when there are none. */
export function nextFreePair(doc: ProjectDoc): [string, string] | null {
  const cs = doc.characters ?? []
  for (let i = 0; i < cs.length; i++) for (let j = i + 1; j < cs.length; j++) if (!pairsOf(doc).some((p) => samePair(p, cs[i].id, cs[j].id))) return [cs[i].id, cs[j].id]
  return null
}

/** Characters that can stand in a pair slot: not the other one, and not forming a pair that exists (except the one being edited). */
export function partnerChoices(doc: ProjectDoc, index: number, slot: 'a' | 'b'): Character[] {
  const p = pairsOf(doc)[index]
  if (!p) return []
  const other = slot === 'a' ? p.b : p.a
  return (doc.characters ?? []).filter((c) => c.id !== other && (c.id === p[slot] || !pairsOf(doc).some((q, i) => i !== index && samePair(q, c.id, other))))
}

const withPairs = (doc: ProjectDoc, fn: (l: SupportPair[]) => SupportPair[]): ProjectDoc => withSupports(doc, (s) => ({ ...s, pairs: fn(s.pairs ?? []) }))
const withPair = (doc: ProjectDoc, i: number, fn: (p: SupportPair) => SupportPair): ProjectDoc => withPairs(doc, (l) => l.map((p, k) => (k === i ? fn(p) : p)))

/** A new pair of the first two characters that are not a pair yet, with one level. */
export function addPair(doc: ProjectDoc): ProjectDoc {
  const free = nextFreePair(doc)
  if (!free) return doc
  return withPairs(doc, (l) => [...l, { a: free[0], b: free[1], levels: [{ points: STEP }] }])
}
export const deletePair = (doc: ProjectDoc, i: number): ProjectDoc => withPairs(doc, (l) => l.filter((_, k) => k !== i))
export const movePair = (doc: ProjectDoc, from: number, to: number): ProjectDoc => withPairs(doc, (l) => moveItem(l, from, to))

export const setPairCharacter = (doc: ProjectDoc, i: number, slot: 'a' | 'b', id: string): ProjectDoc => withPair(doc, i, (p) => patchObj(p, { [slot]: id }))
/** Points at the start of a playthrough; 0 (or empty) is the default, so the key goes. */
export const setPairStart = (doc: ProjectDoc, i: number, v: number | undefined): ProjectDoc =>
  withPair(doc, i, (p) => patchObj(p, { start: v ? Math.min(MAX_POINTS, Math.max(0, v)) : undefined }))

// --- levels ---------------------------------------------------------------------------------------------------------

const withLevels = (p: SupportPair, fn: (l: SupportLevel[]) => SupportLevel[]): SupportPair => ({ ...p, levels: fn(p.levels ?? []) })
const withLevel = (doc: ProjectDoc, i: number, lv: number, fn: (l: SupportLevel) => SupportLevel): ProjectDoc =>
  withPair(doc, i, (p) => withLevels(p, (l) => l.map((x, k) => (k === lv ? fn(x) : x))))

export const canAddLevel = (p: SupportPair): boolean => (p.levels?.length ?? 0) < MAX_LEVELS && (p.levels?.at(-1)?.points ?? 0) < MAX_POINTS

/** Adds a level that asks for 20 more points than the last; level names (if the game has them) get the number for it. */
export function addLevel(doc: ProjectDoc, i: number): ProjectDoc {
  const p = pairsOf(doc)[i]
  if (!p || !canAddLevel(p)) return doc
  const next = p.levels.length + 1
  const points = Math.min(MAX_POINTS, (p.levels.at(-1)?.points ?? 0) + STEP)
  const names = doc.supports?.rules?.levelNames
  let out = withPair(doc, i, (x) => withLevels(x, (l) => [...l, { points }]))
  if (names && names.length < next) out = setRules(out, { levelNames: [...names, ...Array.from({ length: next - names.length }, (_, k) => String(names.length + k + 1))] })
  return out
}
export const removeLevel = (doc: ProjectDoc, i: number, lv: number): ProjectDoc =>
  ((pairsOf(doc)[i]?.levels?.length ?? 0) <= 1 ? doc : withPair(doc, i, (p) => withLevels(p, (l) => l.filter((_, k) => k !== lv))))

/** The points a level needs. Levels must rise: a value not above the level before is raised to just above it, and
 *  later levels that would no longer be above this one are moved up with it. */
export function setLevelPoints(doc: ProjectDoc, i: number, lv: number, value: number | undefined): ProjectDoc {
  return withPair(doc, i, (p) => withLevels(p, (levels) => {
    const prev = lv > 0 ? levels[lv - 1].points : -1
    let at = Math.min(MAX_POINTS - (levels.length - 1 - lv), Math.max(prev + 1, value ?? 0))
    return levels.map((x, k) => {
      if (k < lv) return x
      if (k === lv) return patchObj(x, { points: at })
      if (x.points > at) { at = x.points; return x }
      at += 1
      return patchObj(x, { points: at })
    })
  }))
}

// --- bonus and conversation -------------------------------------------------------------------------------------------

export const BONUS_KEYS = ['hit', 'avoid', 'crit', 'critAvoid', 'damage', 'damageTaken'] as const
/** Row keys of the bonus table: a combat number, or "stat:<id>". */
export const STAT_PREFIX = 'stat:'

/** One bonus number; undefined removes it (a level with no bonus loses the key). Stats are `stat:<id>`. */
export function setBonus(doc: ProjectDoc, i: number, lv: number, key: string, value: number | undefined): ProjectDoc {
  return withLevel(doc, i, lv, (l) => {
    const cur: SupportBonus = l.bonus ?? {}
    const next: SupportBonus = { ...cur }
    if (key.startsWith(STAT_PREFIX)) {
      const stats = { ...(cur.stats ?? {}) }
      if (value === undefined) delete stats[key.slice(STAT_PREFIX.length)]; else stats[key.slice(STAT_PREFIX.length)] = value
      if (Object.keys(stats).length) next.stats = stats; else delete next.stats
    } else if (value === undefined) delete next[key]
    else next[key] = value
    return patchObj(l, { bonus: Object.keys(next).length ? next : undefined })
  })
}

/** The conversation: only `say` commands; an empty one removes the key. */
export const setTalk = (doc: ProjectDoc, i: number, lv: number, talk: Command[]): ProjectDoc =>
  withLevel(doc, i, lv, (l) => (l.talk === talk ? l : patchObj(l, { talk: talk.length ? talk : undefined })))

// --- the rules --------------------------------------------------------------------------------------------------------

const isEmpty = (o: object) => Object.keys(o).length === 0

/** Merges into `rules`; an undefined value removes the key, and `rules` goes when nothing is left. */
export function setRules(doc: ProjectDoc, patch: Partial<SupportRules>): ProjectDoc {
  return withSupports(doc, (s): Supports => {
    const rules = patchObj(s.rules ?? {}, patch as Record<string, unknown>)
    return patchObj(s, { rules: isEmpty(rules) ? undefined : rules })
  })
}

/** One point source: 0 or empty switches it off (the key goes). */
export function setPointSource(doc: ProjectDoc, key: 'endTurnAdjacent' | 'fightTogether' | 'heal', value: number | undefined): ProjectDoc {
  const points = { ...(doc.supports?.rules?.points ?? {}) }
  if (value) points[key] = Math.min(MAX_POINTS, Math.max(1, value)); else delete points[key]
  return setRules(doc, { points: isEmpty(points) ? undefined : points })
}

/** "Near" = 3 squares by default, so that number is not written. */
export const setRange = (doc: ProjectDoc, v: number | undefined): ProjectDoc => setRules(doc, { range: v && v !== DEFAULT_RANGE ? Math.min(MAX_RANGE, Math.max(1, v)) : undefined })
export const setMaxLevels = (doc: ProjectDoc, v: number | undefined): ProjectDoc => setRules(doc, { maxLevels: v ? Math.min(99, Math.max(1, v)) : undefined })
/** "Both" is the default, so it is not written. */
export const setConversations = (doc: ProjectDoc, v: SupportTalks): ProjectDoc => setRules(doc, { conversations: v === 'both' ? undefined : v })

/** How many levels the pair with the most levels has (what level names must cover). */
export const mostLevels = (doc: ProjectDoc): number => Math.max(1, ...pairsOf(doc).map((p) => p.levels?.length ?? 0))

/** The names of levels 1..n: typed names, "" for none yet. All empty removes `levelNames`; otherwise empty ones are written as their number
 *  (the loader needs a name for every level of the biggest pair) and the list is at least as long as that pair. */
export function setLevelNames(doc: ProjectDoc, names: string[]): ProjectDoc {
  if (names.every((n) => n.trim() === '')) return setRules(doc, { levelNames: undefined })
  const n = Math.max(names.length, mostLevels(doc))
  return setRules(doc, { levelNames: Array.from({ length: Math.min(n, MAX_LEVELS) }, (_, k) => names[k]?.trim() || String(k + 1)) })
}
