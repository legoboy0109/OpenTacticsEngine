// The amount of a `heal` or `hurt` effect (docs/SRPG.md section 9): a whole number, { percent }, or a formula (a = the unit), and for
// `hurt` the "can defeat" switch (`canKill`), which turns the written form into { amount | percent | formula, canKill: true }.
// Pure functions on one effect; the Skills form and the Conditions form both use them.
import type { Extra } from './types'

/** 'range' (hurt only): a whole number from `value` to `max`, rolled each time (docs/SRPG.md section 9 "Damage that varies"). */
export type AmountKind = 'number' | 'percent' | 'formula' | 'range'
export interface Amount { kind: AmountKind; value: number; text: string; canKill: boolean; max?: number }

export const AMOUNT_MAX = 1000000
const isRange = (v: unknown): v is [number, number] => Array.isArray(v) && v.length === 2 && v.every((n) => typeof n === 'number')
const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

/** Reads the amount of `effect[key]` ("heal" or "hurt"). Anything it cannot read counts as the number 5. */
export function amountOf(effect: Extra, key: 'heal' | 'hurt'): Amount {
  const v = effect[key]
  const plain: Amount = { kind: 'number', value: 5, text: '', canKill: false }
  if (typeof v === 'number') return { ...plain, value: v }
  if (isRange(v)) return { ...plain, kind: 'range', value: v[0], max: v[1] }
  if (typeof v === 'string') return { ...plain, kind: 'formula', value: 0, text: v }
  if (v && typeof v === 'object') {
    const o = v as Extra
    const canKill = o.canKill === true || o.lethal === true  // "lethal" is another name for "canKill"
    if (isRange(o.amount)) return { ...plain, kind: 'range', value: o.amount[0], max: o.amount[1], canKill }
    if (typeof o.percent === 'number') return { kind: 'percent', value: o.percent, text: '', canKill }
    if (typeof o.formula === 'string') return { kind: 'formula', value: 0, text: o.formula, canKill }
    if (typeof o.amount === 'string') return { kind: 'formula', value: 0, text: o.amount, canKill }
    if (typeof o.amount === 'number') return { ...plain, value: o.amount, canKill }
    return { ...plain, canKill }
  }
  return plain
}

/** The effect's `heal` / `hurt` value for an amount: the shortest form that says it. */
export function amountValue(a: Amount): unknown {
  const lo = a.value, hi = a.max ?? a.value + 2
  if (a.kind === 'range' && !a.canKill) return [lo, hi]
  if (a.canKill) {
    const base = a.kind === 'percent' ? { percent: a.value } : a.kind === 'formula' ? { formula: a.text } : a.kind === 'range' ? { amount: [lo, hi] } : { amount: a.value }
    return { ...base, canKill: true }
  }
  return a.kind === 'percent' ? { percent: a.value } : a.kind === 'formula' ? a.text : a.value
}

export const withAmount = (effect: Extra, key: 'heal' | 'hurt', a: Amount): Extra => ({ ...effect, [key]: amountValue(a) })

/** Another way of writing it, with a starting value that is legal. */
export function amountKind(prev: Amount, kind: AmountKind): Amount {
  if (kind === prev.kind) return prev
  if (kind === 'number') return { ...prev, kind, value: 5 }
  if (kind === 'range') return { ...prev, kind, value: 1, max: 3 }
  if (kind === 'percent') return { ...prev, kind, value: 10 }
  return { ...prev, kind, text: 'a.level * 2' }
}
/** A range's two ends: from 1 up, the highest above the lowest (the loader refuses anything else). */
export function amountRange(prev: Amount, lo: number, hi: number): Amount {
  const a = clampInt(lo, 1, AMOUNT_MAX - 1)
  return { ...prev, kind: 'range', value: a, max: Math.max(a + 1, clampInt(hi, 1, AMOUNT_MAX)) }
}
export const amountNumber = (prev: Amount, n: number): Amount => ({ ...prev, value: clampInt(n, 1, AMOUNT_MAX) })
export const amountPercent = (prev: Amount, n: number): Amount => ({ ...prev, value: clampInt(n, 1, 100) })
