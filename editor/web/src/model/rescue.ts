// game.json `rules.rescue` (docs/SRPG.md section 5): carry or pair up, one mode per game, with who may carry whom, what
// the carrier loses while carrying (carry) and what the partner behind gives (pair up). Off = no `rescue` block at all.
// Defaults are never written; keys the editor does not know stay. The loader refuses `pairBonus` outside pair up, a
// bonus or penalty on the health stat, and an unknown stat, so the editor never writes those.
import { withGame } from './doc'
import { dropKey } from './util'
import type { RescueRules } from './srpgTypes'
import type { NumMap, ProjectDoc } from './types'

export type RescueMode = 'off' | 'carry' | 'pairUp'
export const rescueOf = (doc: ProjectDoc): RescueRules | undefined => doc.game?.rules?.rescue
export const rescueMode = (doc: ProjectDoc): RescueMode => rescueOf(doc)?.mode ?? 'off'

/** The stats a rescue rule may change: every stat except health. */
export const changeableStats = (doc: ProjectDoc) => (doc.game?.stats ?? []).filter((s) => s.id !== doc.game?.roles.hp)

function withRescue(doc: ProjectDoc, fn: (r: RescueRules) => RescueRules | undefined): ProjectDoc {
  return withGame(doc, (g) => {
    const rules = { ...(g.rules ?? {}) }
    const next = rules.rescue ? fn(rules.rescue) : undefined
    if (next) rules.rescue = next; else delete rules.rescue
    return { ...g, rules }
  })
}

/** How many numbers `setRescueMode` would throw away (the settings of the old mode, and every unit type's "gives when behind"). */
export function rescueModeLoss(doc: ProjectDoc, mode: RescueMode): number {
  const r = rescueOf(doc)
  if (!r || mode === r.mode) return 0
  const unitBonus = mode === 'pairUp' ? 0 : (doc.units ?? []).filter((u) => u.pairBonus).length
  if (mode === 'off') return Object.keys(r.carrierStats ?? {}).length + Object.keys(r.pairBonus ?? {}).length + (r.canCarry ? 1 : 0) + unitBonus
  return Object.keys(r.pairBonus ?? {}).length + unitBonus
}

/** Switches the mode. Leaving pair up drops the pair-up numbers (they are refused elsewhere); Off drops the whole block. */
export function setRescueMode(doc: ProjectDoc, mode: RescueMode): ProjectDoc {
  const old = rescueMode(doc)
  if (old === mode) return doc
  let out = doc
  if (mode !== 'pairUp') out = { ...out, units: out.units && out.units.map((u) => { if (!u.pairBonus) return u; const { pairBonus: _p, ...rest } = u; return rest }) }
  if (mode === 'off') return withGame(out, (g) => { const rules = { ...(g.rules ?? {}) }; delete rules.rescue; return { ...g, rules } })
  return withGame(out, (g) => {
    const rules = { ...(g.rules ?? {}) }
    const cur = rules.rescue ?? {}
    const { pairBonus: _pb, ...kept } = cur
    rules.rescue = { ...(mode === 'carry' ? kept : cur), mode }
    return { ...g, rules }
  })
}

// --- who can carry whom ------------------------------------------------------------------------------------------------

export type CanCarry =
  | { kind: 'anyone' }
  | { kind: 'stat'; mine: string; theirs: string }
  | { kind: 'tag'; tag: string }
  | { kind: 'custom'; formula: string }

const STAT_RE = /^a\.(\w+) >= b\.(\w+)$/
const TAG_RE = /^not b\.tags\.(\w+)$/

/** Reads the formula back into a preset; anything else is "custom" (kept exactly as written). */
export function readCanCarry(formula: string | undefined): CanCarry {
  if (formula === undefined || formula.trim() === '') return { kind: 'anyone' }
  const s = STAT_RE.exec(formula.trim())
  if (s) return { kind: 'stat', mine: s[1], theirs: s[2] }
  const t = TAG_RE.exec(formula.trim())
  if (t) return { kind: 'tag', tag: t[1] }
  return { kind: 'custom', formula }
}

export function writeCanCarry(c: CanCarry): string | undefined {
  switch (c.kind) {
    case 'anyone': return undefined
    case 'stat': return `a.${c.mine} >= b.${c.theirs}`
    case 'tag': return `not b.tags.${c.tag}`
    case 'custom': return c.formula
  }
}

export function setCanCarry(doc: ProjectDoc, formula: string | undefined): ProjectDoc {
  return withRescue(doc, (r) => {
    const { canCarry: _c, ...rest } = r
    return formula === undefined ? rest : { ...rest, canCarry: formula }
  })
}

/** A tag works in a preset formula only when it is a plain word (a formula cannot spell anything else). */
export const isFormulaWord = (s: string): boolean => /^[A-Za-z_]\w*$/.test(s)

// --- while carrying ----------------------------------------------------------------------------------------------------

export type ChangeMode = 'halved' | 'minus' | 'percent' | 'plus'
export interface CarrierChange { mode: ChangeMode; n: number }
type Raw = number | { percent: number }

export function readChange(v: Raw): CarrierChange {
  if (typeof v === 'object' && v) return v.percent === 50 ? { mode: 'halved', n: 50 } : { mode: 'percent', n: v.percent }
  return v < 0 ? { mode: 'minus', n: -v } : { mode: 'plus', n: v }
}
export function writeChange(c: CarrierChange): Raw {
  switch (c.mode) {
    case 'halved': return { percent: 50 }
    case 'percent': return { percent: Math.min(1000, Math.max(0, Math.round(c.n))) }
    case 'minus': return -Math.max(0, Math.round(c.n))
    case 'plus': return Math.max(0, Math.round(c.n))
  }
}

export const carrierRows = (doc: ProjectDoc): Array<{ stat: string; change: CarrierChange }> =>
  Object.entries(rescueOf(doc)?.carrierStats ?? {}).map(([stat, v]) => ({ stat, change: readChange(v) }))

/** Replaces the whole block, keeping the order of the rows; an emptied block loses its key. */
function writeRows(doc: ProjectDoc, rows: Array<[string, Raw]>): ProjectDoc {
  return withRescue(doc, (r) => {
    const { carrierStats: _c, ...rest } = r
    return rows.length ? { ...rest, carrierStats: Object.fromEntries(rows) } : rest
  })
}
const rawRows = (doc: ProjectDoc): Array<[string, Raw]> => Object.entries(rescueOf(doc)?.carrierStats ?? {})

export function addCarrierRow(doc: ProjectDoc): ProjectDoc {
  const used = new Set(rawRows(doc).map(([s]) => s))
  const stat = changeableStats(doc).find((s) => !used.has(s.id))
  return stat ? writeRows(doc, [...rawRows(doc), [stat.id, { percent: 50 }]]) : doc
}
export const removeCarrierRow = (doc: ProjectDoc, i: number): ProjectDoc => writeRows(doc, rawRows(doc).filter((_, j) => j !== i))
export const setCarrierRow = (doc: ProjectDoc, i: number, stat: string, change: CarrierChange): ProjectDoc =>
  writeRows(doc, rawRows(doc).map((row, j): [string, Raw] => (j === i ? [stat, writeChange(change)] : row)))

// --- what the partner behind gives -----------------------------------------------------------------------------------------

/** The game-wide "partner behind gives" numbers (pair up only). */
export function setRulesPairBonus(doc: ProjectDoc, stat: string, value: number | undefined): ProjectDoc {
  return withRescue(doc, (r) => {
    const next: NumMap | undefined = value === undefined ? dropKey(r.pairBonus, stat) : { ...(r.pairBonus ?? {}), [stat]: value }
    const { pairBonus: _p, ...rest } = r
    return next ? { ...rest, pairBonus: next } : rest
  })
}
