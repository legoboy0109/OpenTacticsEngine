// An ability's "xp" and "inflict" (docs/SRPG.md section 11) and a staff's "xpMiss": small edits and checks mirroring the loader.
// Defaults are never written; unknown keys are kept.
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

export type Amount = number | string | undefined

/** "10" -> 10 (a whole number), other text stays a formula, '' -> removed. */
export function amountFromText(text: string): Amount {
  const t = text.trim()
  if (t === '') return undefined
  return /^\d+$/.test(t) ? Number(t) : t
}
export const amountText = (v: unknown): string => (typeof v === 'number' || typeof v === 'string' ? String(v) : '')

export function setAbilityXp(a: Ability, text: string): Ability {
  const next: Ability = { ...a }
  const v = amountFromText(text)
  if (v === undefined) delete next.xp
  else next.xp = v
  return next
}

export interface AbilityInflict { condition: string; turns?: number; chance?: number | string }
export const abilityInflict = (a: Ability): AbilityInflict | undefined => {
  const v = a.inflict as { condition?: unknown; turns?: unknown; chance?: unknown } | undefined
  return v && typeof v === 'object' && typeof v.condition === 'string'
    ? { condition: v.condition, turns: typeof v.turns === 'number' ? v.turns : undefined,
        chance: typeof v.chance === 'number' || typeof v.chance === 'string' ? v.chance : undefined }
    : undefined
}

/** condition '' removes the key; turns undefined = the condition's own. A chance (and any other key) already there is kept. */
export function setAbilityInflict(a: Ability, condition: string, turns?: number): Ability {
  const next: Ability = { ...a }
  if (condition === '') {
    delete next.inflict
    return next
  }
  const was = (a.inflict && typeof a.inflict === 'object' ? a.inflict : {}) as Record<string, unknown>
  const v: Record<string, unknown> = { ...was, condition }
  if (turns === undefined) delete v.turns
  else v.turns = turns
  next.inflict = v as Ability['inflict']
  return next
}

/** The chance it takes hold (docs/SRPG.md "Ability conditions"): "60" -> 60, other text = a formula, '' -> removed (always takes hold). */
export function setAbilityInflictChance(a: Ability, text: string): Ability {
  if (!a.inflict || typeof a.inflict !== 'object') return a
  const v: Record<string, unknown> = { ...(a.inflict as Record<string, unknown>) }
  const amount = amountFromText(text)
  if (amount === undefined) delete v.chance
  else v.chance = amount
  return { ...a, inflict: v as Ability['inflict'] }
}

export function abilityExtraProblems(doc: ProjectDoc, a: Ability): string[] {
  const out: string[] = []
  const progression = doc.game?.rules?.progression?.enabled === true
  if (a.xp !== undefined) {
    if (!progression) out.push('Experience for this ability needs experience to be switched on (Rules screen).')
    else if (!(typeof a.xp === 'string' && a.xp.trim() !== '') && !(typeof a.xp === 'number' && Number.isInteger(a.xp) && a.xp >= 0 && a.xp <= 1000))
      out.push('Experience must be a whole number from 0 to 1000 or a formula.')
  }
  const inf = a.inflict
  if (inf !== undefined) {
    const i = abilityInflict(a)
    if (!i) out.push('"Gives a condition" must name a condition.')
    else {
      const cond = (doc.conditions ?? []).find((c) => c.id === i.condition)
      if (!cond) out.push(`The condition "${i.condition}" does not exist (Conditions screen).`)
      else if (i.turns !== undefined && (cond.until === 'cured' || cond.until === 'mapEnd')) out.push(`"${cond.name}" does not count turns, so a number of turns means nothing.`)
      if (i.turns !== undefined && (!Number.isInteger(i.turns) || i.turns < 1 || i.turns > 99)) out.push('Turns must be a whole number from 1 to 99.')
      const ch = (inf as { chance?: unknown }).chance
      if (ch !== undefined && !(typeof ch === 'string' && ch.trim() !== '') && !(typeof ch === 'number' && Number.isInteger(ch) && ch >= 1 && ch <= 100))
        out.push('The chance must be a percent from 1 to 100 or a formula (a = the user, b = the target).')
    }
    if (a.target === 'cell' || (a.effect && ('summon' in a.effect || 'unlock' in a.effect)))
      out.push('An ability that gives a condition needs a unit to give it to (not a summon, an unlock or a square).')
  }
  return out
}
