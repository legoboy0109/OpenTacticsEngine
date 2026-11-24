// An ability's "xp" and "inflict" (docs/SRPG.md section 11) and a staff's "xpMiss": small edits and checks mirroring the loader.
// Defaults are never written; unknown keys are kept.
import { amountFromText } from './amountText'
import { inflictEntries, setInflictChance, setInflictCondition, setInflictTurns, INFLICT_MAX } from './abilityInflicts'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

export { amountFromText, amountText, type Amount } from './amountText'

export function setAbilityXp(a: Ability, text: string): Ability {
  const next: Ability = { ...a }
  const v = amountFromText(text)
  if (v === undefined) delete next.xp
  else next.xp = v
  return next
}

export type { InflictEntry as AbilityInflict } from './abilityInflicts'
/** The first (or only) condition entry; the form lists all of them (abilityInflicts.ts). */
export const abilityInflict = (a: Ability) => inflictEntries(a)[0]

/** condition '' removes the first entry; turns undefined = the condition's own. A chance (and any other key) already there is kept. */
export function setAbilityInflict(a: Ability, condition: string, turns?: number): Ability {
  return setInflictTurns(setInflictCondition(a, 0, condition), 0, turns)
}

/** The chance it takes hold (docs/SRPG.md "Ability conditions") of the first entry: "60" -> 60, other text = a formula, '' -> removed (always takes hold). */
export const setAbilityInflictChance = (a: Ability, text: string): Ability => setInflictChance(a, 0, text)

/** One condition entry's problems; `at` ('' for a single entry, "Condition 2: " in a list) goes in front of each. */
function entryProblems(doc: ProjectDoc, e: Record<string, unknown>, at: string): string[] {
  const out: string[] = []
  if (typeof e.condition !== 'string') return [`${at}"Gives a condition" must name a condition.`]
  const cond = (doc.conditions ?? []).find((c) => c.id === e.condition)
  const turns = e.turns
  if (!cond) out.push(`${at}The condition "${e.condition}" does not exist (Conditions screen).`)
  else if (turns !== undefined && (cond.until === 'cured' || cond.until === 'mapEnd')) out.push(`${at}"${cond.name}" does not count turns, so a number of turns means nothing.`)
  if (turns !== undefined && (typeof turns !== 'number' || !Number.isInteger(turns) || turns < 1 || turns > 99)) out.push(`${at}Turns must be a whole number from 1 to 99.`)
  const ch = e.chance
  if (ch !== undefined && !(typeof ch === 'string' && ch.trim() !== '') && !(typeof ch === 'number' && Number.isInteger(ch) && ch >= 1 && ch <= 100))
    out.push(`${at}The chance must be a percent from 1 to 100 or a formula (a = the user, b = the target).`)
  return out
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
    if (Array.isArray(inf)) {
      if (inf.length < 1 || inf.length > INFLICT_MAX) out.push(`The conditions it gives must be one entry or a list of 1 to ${INFLICT_MAX}.`)
      const seen = new Set<unknown>()
      inf.forEach((e, i) => {
        const at = `Condition ${i + 1}: `
        if (!e || typeof e !== 'object' || Array.isArray(e)) { out.push(`${at}"Gives a condition" must name a condition.`); return }
        out.push(...entryProblems(doc, e as Record<string, unknown>, at))
        const c = (e as Record<string, unknown>).condition
        if (typeof c === 'string' && seen.has(c)) out.push(`It gives "${c}" twice. List each condition once.`)
        seen.add(c)
      })
    } else if (inf && typeof inf === 'object') out.push(...entryProblems(doc, inf as Record<string, unknown>, ''))
    else out.push('"Gives a condition" must name a condition.')
    if (a.target === 'cell' || (a.effect && ('summon' in a.effect || 'unlock' in a.effect)))
      out.push('An ability that gives a condition needs a unit to give it to (not a summon, an unlock or a square).')
  }
  return out
}
