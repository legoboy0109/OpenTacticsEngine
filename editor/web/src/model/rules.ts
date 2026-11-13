// Battle rules and feature switches. Turning a feature off keeps its
// settings in the file (only "enabled" changes), so turning it on again
// brings everything back.
import { withGame } from './doc'
import type { Feature, ProjectDoc, Rules } from './types'

export type FeatureKey = 'economy' | 'production' | 'capture' | 'progression'
export type DefeatCondition = 'noUnits' | 'leaderDead' | 'lostHq'
export const DEFEAT_ORDER: DefeatCondition[] = ['noUnits', 'leaderDead', 'lostHq']

/** Values written the first time a feature is switched on (the engine's own defaults). */
const FEATURE_DEFAULTS: Record<FeatureKey, Feature> = {
  economy: {},
  production: { builtUnitsCanAct: false },
  capture: { amount: 'a.hp' },
  progression: { xpPerLevel: 100, maxLevel: 20, xpHit: '10', xpKill: '30' },
}

const withRules = (doc: ProjectDoc, fn: (r: Rules) => Rules): ProjectDoc =>
  withGame(doc, (g) => ({ ...g, rules: fn(g.rules ?? {}) }))

export function setCounterAttacks(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withRules(doc, (r) => ({ ...r, counterAttacks: on }))
}

export function defeatConditions(doc: ProjectDoc): string[] {
  return doc.game?.rules?.defeat ?? ['noUnits']
}

/** At least one way to lose must stay on, or battles could never end. */
export function setDefeatCondition(doc: ProjectDoc, cond: DefeatCondition, on: boolean): ProjectDoc {
  const cur = defeatConditions(doc)
  if (cur.includes(cond) === on) return doc
  const next = on ? [...cur, cond] : cur.filter((c) => c !== cond)
  if (next.length === 0) return doc
  // Known conditions in a stable order; unknown ones the file had keep their place at the end.
  const known = DEFEAT_ORDER.filter((c) => next.includes(c))
  const extra = next.filter((c) => !DEFEAT_ORDER.includes(c as DefeatCondition))
  return withRules(doc, (r) => ({ ...r, defeat: [...known, ...extra] }))
}

export function featureEnabled(doc: ProjectDoc, key: FeatureKey): boolean {
  return doc.game?.rules?.[key]?.enabled === true
}

export function setFeature(doc: ProjectDoc, key: FeatureKey, on: boolean): ProjectDoc {
  return withRules(doc, (r) => {
    const cur = r[key]
    const base = cur ?? (on ? FEATURE_DEFAULTS[key] : {})
    // Fill settings the file lacks, but never overwrite what is there.
    const merged = on ? { ...FEATURE_DEFAULTS[key], ...base } : base
    return { ...r, [key]: { ...merged, enabled: on } }
  })
}

export function setFeatureField(doc: ProjectDoc, key: FeatureKey, field: string, value: string | number | boolean): ProjectDoc {
  return withRules(doc, (r) => ({ ...r, [key]: { ...r[key], [field]: value } }))
}
