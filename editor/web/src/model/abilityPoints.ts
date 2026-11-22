// game.json rules.abilityPoints (docs/JOBS.md "Learning from equipment"): what units earn to master what their items teach.
// The block is written only while it is switched on; defaults (name "AP", perAction 0, perDefeat 0) are left out, perBattle stays so the block is never empty.
import { withGame } from './doc'
import { anyTeaches } from './teaches'
import type { AbilityPointRules, ProjectDoc } from './types'

export const AP_NAME_DEFAULT = 'AP'
export const AP_NAME_MAX = 8
export const AP_MAX = 9999
export const AP_PER_BATTLE_DEFAULT = 10

export const abilityPointsOf = (doc: ProjectDoc): AbilityPointRules | undefined => doc.game?.rules?.abilityPoints
export const apName = (doc: ProjectDoc): string => abilityPointsOf(doc)?.name ?? AP_NAME_DEFAULT

const clamp = (n: number) => Math.min(AP_MAX, Math.max(0, Math.round(n)))

function write(doc: ProjectDoc, block: AbilityPointRules | undefined): ProjectDoc {
  return withGame(doc, (g) => {
    const { abilityPoints: _old, ...rules } = g.rules ?? {}
    return { ...g, rules: block ? { ...rules, abilityPoints: block } : rules }
  })
}

export const setAbilityPointsOn = (doc: ProjectDoc, on: boolean): ProjectDoc => write(doc, on ? { perBattle: AP_PER_BATTLE_DEFAULT } : undefined)

export interface AbilityPointsPatch { name?: string; perBattle?: number; perAction?: number; perDefeat?: number }
export function updateAbilityPoints(doc: ProjectDoc, p: AbilityPointsPatch): ProjectDoc {
  const cur = abilityPointsOf(doc)
  if (!cur) return doc
  const next: AbilityPointRules = { ...cur }
  if (p.name !== undefined) { if (p.name === '' || p.name === AP_NAME_DEFAULT) delete next.name; else next.name = p.name }
  if (p.perBattle !== undefined) next.perBattle = clamp(p.perBattle)
  for (const k of ['perAction', 'perDefeat'] as const) {
    const v = p[k]
    if (v !== undefined) { if (clamp(v) === 0) delete next[k]; else next[k] = clamp(v) }
  }
  return write(doc, next)
}

const isWhole = (n: unknown): boolean => typeof n === 'number' && Number.isInteger(n) && n >= 0 && n <= AP_MAX

export function abilityPointsChecks(doc: ProjectDoc): string[] {
  const r = abilityPointsOf(doc)
  if (r === undefined) return []
  if (!r || typeof r !== 'object' || Array.isArray(r)) return ['Ability points must be a block of settings.']
  const out: string[] = []
  if (r.name !== undefined && (typeof r.name !== 'string' || r.name.length < 1 || r.name.length > AP_NAME_MAX)) out.push(`Ability points: the name must be 1 to ${AP_NAME_MAX} characters.`)
  for (const [k, label] of [['perBattle', 'per battle'], ['perAction', 'per action'], ['perDefeat', 'per defeat']] as const)
    if (r[k] !== undefined && !isWhole(r[k])) out.push(`Ability points: “${label}” must be a whole number from 0 to ${AP_MAX}.`)
  if (!anyTeaches(doc)) out.push('Ability points do nothing yet: no item teaches anything. Add a “Teaches” entry to a weapon or accessory (Items screen), or switch ability points off.')
  return out
}
