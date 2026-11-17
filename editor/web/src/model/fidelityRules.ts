// The fidelity options of game.json "rules" (docs/SRPG.md section 11): rules.hitRoll, and rules.progression's xpNoDamage and
// computerGains. Defaults are never written ("single", no xpNoDamage, computers gain experience), so a game that does not touch
// them keeps a game.json without the keys. Unknown keys are kept.
import { withGame } from './doc'
import type { ProjectDoc } from './types'

export type HitRoll = 'single' | 'average2'

export const hitRoll = (doc: ProjectDoc): HitRoll => (doc.game?.rules?.hitRoll === 'average2' ? 'average2' : 'single')

export function setHitRoll(doc: ProjectDoc, roll: HitRoll): ProjectDoc {
  return withGame(doc, (g) => {
    const rules = { ...(g.rules ?? {}) }
    if (roll === 'average2') rules.hitRoll = 'average2'
    else delete rules.hitRoll
    return { ...g, rules }
  })
}

/** The "xpNoDamage" text as the form shows it: a number or a formula; '' = off. */
export function xpNoDamageText(doc: ProjectDoc): string {
  const v = doc.game?.rules?.progression?.xpNoDamage
  return typeof v === 'number' || typeof v === 'string' ? String(v) : ''
}

/** A whole number is written as a number, anything else as formula text; '' removes the key. */
export function setXpNoDamage(doc: ProjectDoc, text: string): ProjectDoc {
  const t = text.trim()
  return withGame(doc, (g) => {
    const progression = { ...(g.rules?.progression ?? {}) }
    if (t === '') delete progression.xpNoDamage
    else progression.xpNoDamage = /^\d+$/.test(t) ? Number(t) : t
    return { ...g, rules: { ...(g.rules ?? {}), progression } }
  })
}

export const computerGains = (doc: ProjectDoc): boolean => doc.game?.rules?.progression?.computerGains !== false

export function setComputerGains(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withGame(doc, (g) => {
    const progression = { ...(g.rules?.progression ?? {}) }
    if (on) delete progression.computerGains
    else progression.computerGains = false
    return { ...g, rules: { ...(g.rules ?? {}), progression } }
  })
}

/** Checks mirroring the loader: "xpNoDamage" must be a whole number 0-1000 or formula text; "hitRoll" one of two words. */
export function fidelityRuleProblems(doc: ProjectDoc): string[] {
  const out: string[] = []
  const v = doc.game?.rules?.progression?.xpNoDamage
  if (v !== undefined && !(typeof v === 'string' && v.trim() !== '') && !(typeof v === 'number' && Number.isInteger(v) && v >= 0 && v <= 1000))
    out.push('Experience for a fight that did no damage must be a whole number from 0 to 1000 or a formula.')
  const roll = doc.game?.rules?.hitRoll
  if (roll !== undefined && roll !== 'single' && roll !== 'average2') out.push('The hit roll must be "single" or "average2".')
  return out
}
