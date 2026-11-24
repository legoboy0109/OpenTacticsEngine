// Checks for "Downed units and revive" (docs/SRPG.md), each mirroring one refusal of the loader (engine/core/data: rules.downed, an ability's
// effect.revive, a usable item's revive, a staff's { revive } effect, a condition's autoRevive), in plain words. Every sentence about a thing
// starts with its name, like the other checks. The wording is a placeholder for the owner to review.
import { REMOVE_AFTER_MAX, REVIVE_MAX, gameHasDowned, reviveAmountProblem } from './downed'
import type { Ability } from './srpgTypes'
import type { Condition } from './srpgTypes'
import type { Item, ProjectDoc } from './types'

const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v)

const NEEDS_RULE = 'it needs the “Downed units” rule to be switched on (Rules screen).'
const AMOUNT_PLAIN = `a whole number from 1 to ${REVIVE_MAX}, “full” or a percent from 1 to 100`

/** The amount of an ability or a staff: also a formula; a bare word is refused. */
function formulaAmount(v: unknown, who: string): string[] {
  const p = reviveAmountProblem(v, true)
  if (p === 'bare') return [`${who}: “${String(v)}” is not a formula. Write “full”, or a formula that reads the units, such as “a.mag * 2”.`]
  return p ? [`${who}: reviving must give back ${AMOUNT_PLAIN}, or a formula.`] : []
}
/** The amount of a usable item or a condition: no formula. */
function plainAmount(v: unknown, who: string, what: string): string[] {
  return reviveAmountProblem(v, false) ? [`${who}: ${what} must be ${AMOUNT_PLAIN} (no formula).`] : []
}

// --- game.json rules.downed -------------------------------------------------------------------------------------------------------

/** The rule block itself: "afterBattle" return or fallen, "removeAfter" 0 to 99, nothing else. */
export function downedRuleChecks(doc: ProjectDoc): string[] {
  const d = doc.game?.rules?.downed as unknown
  if (d === undefined) return []
  if (!isObj(d)) return ['Downed units: the rule must be a block of settings (an empty one switches it on with the usual settings).']
  const out: string[] = []
  for (const k of Object.keys(d)) if (k !== 'afterBattle' && k !== 'removeAfter') out.push(`Downed units: there is no setting “${k}”. The rule takes “afterBattle” and “removeAfter”.`)
  if (d.afterBattle !== undefined && d.afterBattle !== 'return' && d.afterBattle !== 'fallen') out.push('Downed units: what happens after the battle must be “return” (back for the next battle) or “fallen” (counts as fallen).')
  const r = d.removeAfter
  if (r !== undefined && !(typeof r === 'number' && Number.isInteger(r) && r >= 0 && r <= REMOVE_AFTER_MAX)) out.push(`Downed units: the turns before a downed unit is gone must be a whole number from 0 to ${REMOVE_AFTER_MAX} (0 = never).`)
  return out
}

// --- an ability ------------------------------------------------------------------------------------------------------------------

/** effect.revive: the rule is on, it is the only effect, the target is an ally or any unit, no inflict, no charge, a valid amount. An area is allowed. */
export function reviveAbilityChecks(doc: ProjectDoc, a: Ability): string[] {
  if (!a.effect || typeof a.effect !== 'object' || !('revive' in a.effect)) return []
  const out: string[] = []
  if (!gameHasDowned(doc)) out.push(`${a.name}: ${NEEDS_RULE}`)
  if (Object.keys(a.effect).some((k) => k !== 'revive')) out.push(`${a.name}: reviving must be the only thing it does. It cannot go together with healing, curing, pushing and the like.`)
  const target = a.target ?? 'unit'
  if (target !== 'ally' && target !== 'unit') out.push(`${a.name}: reviving works on an ally or any unit (a downed unit), not on an enemy, itself or a square.`)
  if (a.inflict !== undefined) out.push(`${a.name}: reviving cannot also give a condition. Make a second ability for the condition.`)
  if ((a as Record<string, unknown>).chargeSpeed !== undefined) out.push(`${a.name}: reviving cannot be a charged ability.`)
  out.push(...formulaAmount(a.effect.revive, a.name))
  return out
}

// --- a usable item ---------------------------------------------------------------------------------------------------------------

const CLASH_WORDS: Array<[keyof Item, string]> = [['heal', 'healing'], ['raise', 'raising stats'], ['cure', 'curing'], ['inflict', 'giving a condition'], ['promotes', 'promoting'], ['opens', 'opening doors and chests'], ['light', 'giving light']]

/** An item's `revive`: only a usable item, the rule on, nothing it cannot go with, used on an ally, a valid amount (no formula). */
export function reviveItemChecks(doc: ProjectDoc): Array<{ section: 'items'; text: string }> {
  const out: Array<{ section: 'items'; text: string }> = []
  for (const i of doc.items ?? []) {
    if (i.revive === undefined) continue
    const say = (text: string) => out.push({ section: 'items', text: `${i.name}: ${text}` })
    if (i.kind !== 'usable') say('only a usable item can revive (a staff lists “revive” among its effects).')
    if (!gameHasDowned(doc)) say(NEEDS_RULE)
    for (const [k, word] of CLASH_WORDS) if (i[k] !== undefined) say(`reviving cannot go together with ${word}.`)
    if (i.target !== 'ally') say('reviving is used on a downed neighbour, so the item must be used on an ally.')
    for (const t of plainAmount(i.revive, i.name, 'reviving')) out.push({ section: 'items', text: t })
  }
  return out
}

// --- a staff ---------------------------------------------------------------------------------------------------------------------

/** A staff's `{ revive }` effect: the rule on, a valid amount (formula allowed), and (`only`: it is the one effect) used on an ally, the team or anyone. */
export function staffReviveProblems(doc: ProjectDoc, i: Item, v: unknown, only: boolean): string[] {
  const out: string[] = []
  if (!gameHasDowned(doc)) out.push(NEEDS_RULE)
  if (!only) out.push('reviving must be the only effect of the staff.')
  if (i.target === 'foe' || i.target === 'self') out.push('reviving cannot be used on foes or on the user. Pick an ally, the team or anyone.')
  const p = reviveAmountProblem(v, true)
  if (p === 'bare') out.push(`“${String(v)}” is not a formula. Write “full”, or a formula that reads the units, such as “a.mag * 2”.`)
  else if (p) out.push(`reviving must give back ${AMOUNT_PLAIN}, or a formula.`)
  return out
}

// --- a condition -----------------------------------------------------------------------------------------------------------------

/** `autoRevive`: the rule on, a valid amount (no formula). */
export function autoReviveChecks(doc: ProjectDoc, c: Condition): string[] {
  if (c.autoRevive === undefined) return []
  const out: string[] = []
  if (!gameHasDowned(doc)) out.push(NEEDS_RULE)
  out.push(...plainAmount(c.autoRevive, 'it', 'the HP it stands the unit up with').map((t) => t.replace(/^it: /, '')))
  return out
}
