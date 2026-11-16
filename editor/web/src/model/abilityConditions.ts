// The "Only if" rows of an ability (docs/SRPG.md section 3 "CONDITIONS", used by section 4). Skills (E2) will have their
// own, larger builder; an ability only needs the few that make sense for a menu command. A row the editor does not know
// (a formula, a terrain ...) is kept as it is and shown with its words, so a hand-written file is never damaged.
import type { Extra } from './types'

export type CondKind = 'hp' | 'near' | 'unit' | 'carrying' | 'round'
export const COND_KINDS: CondKind[] = ['hp', 'near', 'unit', 'carrying', 'round']
export type Op = '=' | '!=' | '<' | '<=' | '>' | '>='
export const OPS: Op[] = ['<=', '>=', '<', '>', '=', '!=']

const has = (o: object, k: string) => Object.prototype.hasOwnProperty.call(o, k)
export const condKind = (c: Extra): CondKind | null => COND_KINDS.find((k) => has(c, k)) ?? null
/** True when the row judges the target unit instead of the unit that uses the ability. */
export const isFoe = (c: Extra): boolean => c.who === 'foe'
/** hp and unit can judge the target; the others are about the user only. */
export const canJudgeTarget = (k: CondKind): boolean => k === 'hp' || k === 'unit'

export function newCondition(k: CondKind): Extra {
  switch (k) {
    case 'hp': return { hp: { op: '<=', percent: 50 } }
    case 'near': return { near: 'ally', within: 1, op: '>=', count: 1 }
    case 'unit': return { unit: '' }
    case 'carrying': return { carrying: true }
    case 'round': return { round: { op: '>=', value: 2 } }
  }
}

/** Changing the kind of a row starts a fresh row of that kind, but keeps "the target" when the new kind can have it. */
export const changeKind = (old: Extra, k: CondKind): Extra => (isFoe(old) && canJudgeTarget(k) ? { ...newCondition(k), who: 'foe' } : newCondition(k))

export const setWho = (c: Extra, foe: boolean): Extra => {
  const { who: _w, ...rest } = c
  return foe ? { ...rest, who: 'foe' } : rest
}

const OP_WORDS: Record<string, string> = { '=': 'exactly', '!=': 'not', '<': 'below', '<=': 'at most', '>': 'above', '>=': 'at least' }
export const opWords = (op: unknown): string => OP_WORDS[String(op ?? '>=')] ?? 'at least'

/** One plain sentence for the row ("its HP is at most 50%"). */
export function conditionText(c: Extra): string {
  const who = isFoe(c) ? 'the target’s' : 'its'
  const whoIs = isFoe(c) ? 'the target is' : 'it is'
  const kind = condKind(c)
  const o = (c[kind ?? ''] ?? {}) as Extra
  switch (kind) {
    case 'hp': return `${who} HP is ${opWords(o.op)} ${o.percent ?? 0}%`
    case 'near': return `${opWords(c.op)} ${c.count ?? 1} ${c.near === 'enemy' ? 'enemy' : 'ally'} within ${c.within ?? 1} ${c.within === 1 || c.within === undefined ? 'square' : 'squares'}`
    case 'unit': return `${whoIs} of the type or tag “${String(c.unit ?? '')}”`
    case 'carrying': return c.carrying === false ? 'it is not carrying anyone' : 'it is carrying someone'
    case 'round': return `the round is ${opWords(o.op)} ${o.value ?? 0}`
    default: return 'another condition the editor does not show (kept as it is)'
  }
}

export const conditionsText = (list: Extra[] | undefined): string => (list ?? []).map(conditionText).join(' and ')
