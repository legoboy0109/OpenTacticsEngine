// "Downed units and revive" (docs/SRPG.md; optional, off by default). One rule block in game.json, and four places that name an amount of
// HP to stand a downed unit up with: an ability's effect, a usable item, a staff's effect and a condition's autoRevive.
//   rules.downed: { afterBattle: "return" | "fallen", removeAfter: 0..99 }   (present = on; an empty object = on with the defaults)
// Defaults are never written, and turning the rule off removes the block. Keys the editor does not know stay.
import { withGame } from './doc'
import { patchObj } from './util'
import type { Ability } from './srpgTypes'
import type { DownedRules, Item, ProjectDoc } from './types'

export const REMOVE_AFTER_MAX = 99
export const REVIVE_MAX = 999
export const REVIVE_PERCENT_MAX = 100

/** An amount of HP as the files have it. */
export type ReviveValue = number | 'full' | { percent: number } | string

const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v)

// --- the game rule ---------------------------------------------------------------------------------------------------------

/** Is rules.downed there? (An empty object counts: it turns the rule on.) */
export const gameHasDowned = (doc: ProjectDoc): boolean => isObj(doc.game?.rules?.downed)
const block = (doc: ProjectDoc): DownedRules => (isObj(doc.game?.rules?.downed) ? doc.game!.rules!.downed : {})

export const downedAfterBattle = (doc: ProjectDoc): 'return' | 'fallen' => (block(doc).afterBattle === 'fallen' ? 'fallen' : 'return')
export const downedRemoveAfter = (doc: ProjectDoc): number => (typeof block(doc).removeAfter === 'number' ? block(doc).removeAfter! : 0)

/** Writes the whole block (undefined removes it) without touching the other rules. */
function putBlock(doc: ProjectDoc, next: DownedRules | undefined): ProjectDoc {
  return withGame(doc, (g) => {
    const rules = { ...(g.rules ?? {}) }
    if (next === undefined) delete rules.downed
    else rules.downed = next
    return { ...g, rules }
  })
}

/** On writes an empty block; off removes it (and what was in it). */
export function setDownedOn(doc: ProjectDoc, on: boolean): ProjectDoc {
  if (gameHasDowned(doc) === on) return doc
  return putBlock(doc, on ? {} : undefined)
}
export function setDownedAfterBattle(doc: ProjectDoc, v: 'return' | 'fallen'): ProjectDoc {
  if (!gameHasDowned(doc)) return doc
  return putBlock(doc, patchObj(block(doc), { afterBattle: v === 'fallen' ? 'fallen' : undefined }))
}
export function setDownedRemoveAfter(doc: ProjectDoc, n: number): ProjectDoc {
  if (!gameHasDowned(doc)) return doc
  const v = Math.min(REMOVE_AFTER_MAX, Math.max(0, Math.round(n)))
  return putBlock(doc, patchObj(block(doc), { removeAfter: v === 0 ? undefined : v }))
}

// --- an amount of HP -------------------------------------------------------------------------------------------------------

/** How a revive amount is given: a number of HP, everything, a share of the maximum, or (abilities and staves only) a formula. */
export type ReviveKind = 'amount' | 'full' | 'percent' | 'formula'
export function reviveKindOf(v: unknown): ReviveKind {
  if (v === 'full') return 'full'
  if (typeof v === 'string') return 'formula'
  if (isObj(v)) return 'percent'
  return 'amount'
}
/** What a kind starts with when the maker switches to it (always valid). */
export const reviveStart = (kind: ReviveKind): ReviveValue => (kind === 'full' ? 'full' : kind === 'percent' ? { percent: 50 } : kind === 'formula' ? 'a.mag * 2' : 10)
/** The number shown in the box of an "amount" or "percent" value. */
export const reviveNumber = (v: unknown): number | undefined => (typeof v === 'number' ? v : isObj(v) && typeof v.percent === 'number' ? v.percent : undefined)
/** A typed number for the kind, clamped to what the loader accepts. */
export function reviveWithNumber(kind: ReviveKind, n: number): ReviveValue {
  const whole = Math.round(n)
  return kind === 'percent' ? { percent: Math.min(REVIVE_PERCENT_MAX, Math.max(1, whole)) } : Math.min(REVIVE_MAX, Math.max(1, whole))
}

const whole = (n: unknown, lo: number, hi: number): n is number => typeof n === 'number' && Number.isInteger(n) && n >= lo && n <= hi

/** The loader's rule for an amount: 1..999, "full", { percent: 1..100 } and, when a formula is allowed, text that is a formula.
 *  'bare' = an empty text or a bare word ("most"), a slip the loader refuses; 'bad' = anything else wrong; null = fine. */
export function reviveAmountProblem(v: unknown, formula: boolean): 'bare' | 'bad' | null {
  if (v === 'full' || whole(v, 1, REVIVE_MAX)) return null
  if (isObj(v) && Object.keys(v).length === 1 && whole(v.percent, 1, REVIVE_PERCENT_MAX)) return null
  if (formula && typeof v === 'string') return v.trim() === '' || /^[A-Za-z_]+$/.test(v) ? 'bare' : null
  return 'bad'
}

/** The sentence part for an amount: "with 50% of its HP", "with 10 HP", "with all its HP" ("" for a formula, which has no short wording). */
export function reviveWithText(v: unknown): string {
  if (v === 'full') return 'with all its HP'
  if (typeof v === 'number') return `with ${v} HP`
  if (isObj(v) && typeof v.percent === 'number') return `with ${v.percent}% of its HP`
  return ''
}

// --- an ability's revive ----------------------------------------------------------------------------------------------------

const hasRevive = (a: Ability): boolean => !!a.effect && 'revive' in a.effect
export function setAbilityReviveKind(a: Ability, kind: ReviveKind): Ability {
  if (!hasRevive(a) || reviveKindOf(a.effect!.revive) === kind) return a
  return patchObj(a, { effect: { revive: reviveStart(kind) } })
}
export const setAbilityReviveValue = (a: Ability, v: ReviveValue): Ability => (hasRevive(a) ? patchObj(a, { effect: { ...a.effect, revive: v } }) : a)

// --- a usable item's revive -------------------------------------------------------------------------------------------------

/** What a revive item cannot go with (the loader refuses each). Turning revive on removes them. */
export const ITEM_REVIVE_CLASH = ['heal', 'raise', 'cure', 'inflict', 'promotes', 'opens', 'light'] as const

const mapItem = (doc: ProjectDoc, id: string, fn: (i: Item) => Item): ProjectDoc => ({ ...doc, items: (doc.items ?? []).map((i) => (i.id === id ? fn(i) : i)) })

/** On: a usable item that stands a downed neighbour up (target "ally"); the keys it cannot go with are removed. Off: back to a small heal so the item stays valid. */
export function setItemRevive(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return mapItem(doc, id, (i) => {
    if (i.kind !== 'usable' || (i.revive !== undefined) === on) return i
    if (!on) return patchObj(i, { revive: undefined, heal: 10 })
    const drop: Record<string, undefined> = {}
    for (const k of ITEM_REVIVE_CLASH) drop[k] = undefined
    return patchObj(i, { ...drop, revive: { percent: 50 }, target: 'ally' })
  })
}
export const setItemReviveValue = (doc: ProjectDoc, id: string, v: Item['revive']): ProjectDoc =>
  mapItem(doc, id, (i) => (i.kind === 'usable' && i.revive !== undefined ? patchObj(i, { revive: v }) : i))

// --- a staff's revive -------------------------------------------------------------------------------------------------------

export const isReviveStaff = (item: Item | undefined): boolean => Array.isArray(item?.effects) && item!.effects.some((x) => isObj(x) && 'revive' in x)
/** The staff's revive amount (its effect), or undefined when it has none. */
export const staffRevive = (item: Item | undefined): unknown => (item?.effects ?? []).find((x) => isObj(x) && 'revive' in x)?.revive

/** On: the staff's only effect is a revive (it must be the only one); "foe" and "self" are not allowed as the target, so they are reset (unset = ally).
 *  Off: back to a small heal so the staff stays valid. */
export function setStaffReviveOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return mapItem(doc, id, (i) => {
    if (i.kind !== 'staff' || isReviveStaff(i) === on) return i
    if (!on) return patchObj(i, { effects: [{ heal: 10 }] })
    const bad = i.target === 'foe' || i.target === 'self'
    // A staff used on itself has no reach; its new target "ally" needs one.
    return patchObj(i, { effects: [{ revive: { percent: 50 } }], ...(bad ? { target: undefined } : {}), ...(i.target === 'self' ? { range: 2 } : {}) })
  })
}
export const setStaffReviveValue = (doc: ProjectDoc, id: string, v: ReviveValue): ProjectDoc =>
  mapItem(doc, id, (i) => (isReviveStaff(i) ? patchObj(i, { effects: (i.effects ?? []).map((e) => (isObj(e) && 'revive' in e ? { ...e, revive: v } : e)) }) : i))

// --- a condition's autoRevive -----------------------------------------------------------------------------------------------

/** undefined removes the key. A condition with it may have no rules of its own. */
export const setAutoRevive = (doc: ProjectDoc, id: string, v: Item['revive'] | undefined): ProjectDoc =>
  ({ ...doc, conditions: (doc.conditions ?? []).map((c) => (c.id === id ? patchObj(c, { autoRevive: v }) : c)) })
