// The amount of a `heal` or `hurt` effect (docs/SRPG.md section 9): a whole number, { percent }, or a formula (a = the unit), and for
// `hurt` the "can defeat" switch (`canKill`), which turns the written form into { amount | percent | formula, canKill: true }.
// Pure functions on one effect; the Skills form and the Conditions form both use them.
import type { Extra } from './types'

export type AmountKind = 'number' | 'percent' | 'formula'
export interface Amount { kind: AmountKind; value: number; text: string; canKill: boolean }

export const AMOUNT_MAX = 1000000
const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

/** Reads the amount of `effect[key]` ("heal" or "hurt"). Anything it cannot read counts as the number 5. */
export function amountOf(effect: Extra, key: 'heal' | 'hurt'): Amount {
  const v = effect[key]
  const plain: Amount = { kind: 'number', value: 5, text: '', canKill: false }
  if (typeof v === 'number') return { ...plain, value: v }
  if (typeof v === 'string') return { ...plain, kind: 'formula', value: 0, text: v }
  if (v && typeof v === 'object') {
    const o = v as Extra
    const canKill = o.canKill === true
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
  if (a.canKill) {
    const base = a.kind === 'percent' ? { percent: a.value } : a.kind === 'formula' ? { formula: a.text } : { amount: a.value }
    return { ...base, canKill: true }
  }
  return a.kind === 'percent' ? { percent: a.value } : a.kind === 'formula' ? a.text : a.value
}

export const withAmount = (effect: Extra, key: 'heal' | 'hurt', a: Amount): Extra => ({ ...effect, [key]: amountValue(a) })

/** Another way of writing it, with a starting value that is legal. */
export function amountKind(prev: Amount, kind: AmountKind): Amount {
  if (kind === prev.kind) return prev
  if (kind === 'number') return { ...prev, kind, value: 5 }
  if (kind === 'percent') return { ...prev, kind, value: 10 }
  return { ...prev, kind, text: 'a.level * 2' }
}
export const amountNumber = (prev: Amount, n: number): Amount => ({ ...prev, value: clampInt(n, 1, AMOUNT_MAX) })
export const amountPercent = (prev: Amount, n: number): Amount => ({ ...prev, value: clampInt(n, 1, 100) })
