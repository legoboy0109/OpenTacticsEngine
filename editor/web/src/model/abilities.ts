// Menu abilities (docs/SRPG.md section 4, data/abilities.json): create, change, and the rules that keep what the screen
// writes loadable. The loader refuses a push with a free square as target, a heal on an enemy, a summon that is not on a
// square, and so on; here the pickers only offer what the loader accepts, and changing the effect repairs the target.
// A default (cost "action", afterMove true, summon limit 1 ...) is never written; keys the editor does not know stay.
import { withAbilities } from './doc'
import { newAbilityId } from './srpgRefs'
import { moveItem, patchObj, updateById } from './util'
import type { Ability, AbilityEffect, AbilityTarget, StealOptions, Summon } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'

export type EffectKind = 'push' | 'pull' | 'swap' | 'moveBehind' | 'movePast' | 'heal' | 'summon' | 'steal' | 'refresh' | 'cure' | 'unlock'
export const EFFECT_KINDS: EffectKind[] = ['push', 'pull', 'swap', 'moveBehind', 'movePast', 'heal', 'summon', 'steal', 'refresh', 'cure', 'unlock']
/** The five that move units next to the user: the loader needs a unit target and the range [1, 1]. */
export const isMove = (k: EffectKind | null): boolean => k === 'push' || k === 'pull' || k === 'swap' || k === 'moveBehind' || k === 'movePast'
/** Effects with no range of their own to write: the moving ones and steal (a neighbour, range [1, 1]). */
export const hasFixedRange = (k: EffectKind | null): boolean => isMove(k) || k === 'steal'

export const PUSH_MAX = 5
export const HEAL_MAX = 999
export const USES_MAX = 99
export const RANGE_MAX = 20
export const LIMIT_MAX = 20
export const LIFETIME_MAX = 99

/** Which targets each effect accepts (the loader's "target against effect" rules). */
export const TARGETS_FOR: Record<EffectKind, AbilityTarget[]> = {
  push: ['ally', 'enemy', 'unit'], pull: ['ally', 'enemy', 'unit'], swap: ['ally', 'enemy', 'unit'], moveBehind: ['ally', 'enemy', 'unit'], movePast: ['ally', 'enemy', 'unit'],
  heal: ['self', 'ally', 'unit'], summon: ['cell'], steal: ['enemy'], refresh: ['ally'], cure: ['self', 'ally'], unlock: ['cell'],
}

export function effectKind(a: Ability): EffectKind | null {
  const e = a.effect
  if (!e || typeof e !== 'object') return null
  return EFFECT_KINDS.find((k) => Object.prototype.hasOwnProperty.call(e, k)) ?? null
}

export const targetOf = (a: Ability): AbilityTarget => a.target ?? 'unit'
export const rangeOf = (a: Ability): [number, number] => (Array.isArray(a.range) && a.range.length === 2 ? a.range : [1, 1])
export const summonOf = (a: Ability): Summon => (a.effect?.summon && typeof a.effect.summon === 'object' ? a.effect.summon : {})

// --- the list ----------------------------------------------------------------------------------------------------------

/** A new ability pushes a neighbour one square: valid as it stands, the maker changes it. */
export function addAbility(doc: ProjectDoc, name: string, id: string = newAbilityId(doc, name)): ProjectDoc {
  return withAbilities(doc, (list) => [...list, { id, name, target: 'unit', effect: { push: 1 } }])
}

export function duplicateAbility(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withAbilities(doc, (list) => {
    const i = list.findIndex((a) => a.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    const copy: Ability = { ...structuredClone(list[i]), id: newId ?? newAbilityId(doc, name), name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
}

export const moveAbility = (doc: ProjectDoc, from: number, to: number): ProjectDoc => withAbilities(doc, (list) => moveItem(list, from, to))

export const updateAbility = (doc: ProjectDoc, id: string, patch: Partial<Omit<Ability, 'id'>>): ProjectDoc =>
  withAbilities(doc, (list) => updateById(list, id, (a) => patchObj(a, patch)))

// --- target, range, effect -----------------------------------------------------------------------------------------------

const clamp = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

/** The range is written only when it is not the default [1, 1]; "itself" and the moving effects have none to write. */
function withRange(a: Ability, range: [number, number]): Ability {
  const kind = effectKind(a)
  const keep = targetOf(a) !== 'self' && !hasFixedRange(kind) && !(range[0] === 1 && range[1] === 1)
  return patchObj(a, { range: keep ? range : undefined })
}

export function setTarget(a: Ability, target: AbilityTarget): Ability {
  const kind = effectKind(a)
  if (kind && !TARGETS_FOR[kind].includes(target)) return a
  return withRange(patchObj(a, { target }), rangeOf(a))
}

export function setRange(a: Ability, min: number, max: number): Ability {
  const lo = clamp(min, 1, RANGE_MAX)
  return withRange(a, [lo, clamp(Math.max(max, lo), lo, RANGE_MAX)])
}

const defaultEffect = (k: EffectKind): AbilityEffect => {
  switch (k) {
    case 'push': return { push: 1 }
    case 'pull': return { pull: true }
    case 'swap': return { swap: true }
    case 'moveBehind': return { moveBehind: true }
    case 'movePast': return { movePast: true }
    case 'heal': return { heal: 10 }
    case 'summon': return { summon: { type: '' } }
    case 'steal': return { steal: true }
    case 'refresh': return { refresh: true }
    case 'cure': return { cure: 'all' }
    case 'unlock': return { unlock: true }
  }
}

/** Changing what it does keeps the target when the new effect accepts it, else picks the first that fits; a summon
 *  needs a square within reach, so it starts with range 1..2 (the summon's own default is the maker's to change). */
export function setEffectKind(doc: ProjectDoc, a: Ability, k: EffectKind): Ability {
  if (effectKind(a) === k) return a
  const target = TARGETS_FOR[k].includes(targetOf(a)) ? targetOf(a) : TARGETS_FOR[k][0]
  const effect = defaultEffect(k)
  if (k === 'summon') effect.summon = { type: doc.units?.[0]?.id ?? '' }
  if (k === 'cure' && doc.conditions?.[0]) effect.cure = [doc.conditions[0].id]
  const next = patchObj(a, { effect, target })
  return withRange(next, k === 'summon' && a.range === undefined ? [1, 2] : rangeOf(a))
}

/** push 1..5; heal 1..999 or "full". */
export function setEffectAmount(a: Ability, amount: number | 'full'): Ability {
  const k = effectKind(a)
  if (k === 'push') return patchObj(a, { effect: { ...a.effect, push: clamp(amount === 'full' ? 1 : amount, 1, PUSH_MAX) } })
  if (k === 'heal') return patchObj(a, { effect: { ...a.effect, heal: amount === 'full' ? 'full' : clamp(amount, 1, HEAL_MAX) } })
  return a
}

/** Summon fields: a value equal to the engine's default is removed rather than written. */
const SUMMON_DEFAULTS: Record<string, unknown> = { level: 'summoner', limit: 1, vanish: true, lifetime: 0, canAct: false, givesXp: true }
export function setSummon(a: Ability, patch: Partial<Summon>): Ability {
  if (effectKind(a) !== 'summon') return a
  const next: Record<string, unknown> = { ...summonOf(a), ...patch }
  for (const [k, def] of Object.entries(SUMMON_DEFAULTS)) if (next[k] === def || next[k] === undefined) delete next[k]
  return patchObj(a, { effect: { ...a.effect, summon: next as Summon } })
}

/** Steal options: the defaults (no condition, no weapons) are written as plain `true`. */
export function setSteal(a: Ability, patch: Partial<StealOptions>): Ability {
  if (effectKind(a) !== 'steal') return a
  const cur = a.effect?.steal && typeof a.effect.steal === 'object' ? a.effect.steal : {}
  const next: Record<string, unknown> = { ...cur, ...patch }
  if (next.if === undefined || next.if === '') delete next.if
  if (next.reason === undefined || next.reason === '' || next.if === undefined) delete next.reason  // the reason goes with a formula
  if (next.weapons !== true) { delete next.weapons; delete next.equipped }  // "equipped" needs weapons
  if (next.equipped !== true) delete next.equipped
  return patchObj(a, { effect: { steal: Object.keys(next).length ? (next as StealOptions) : true } })
}
export const stealOf = (a: Ability): StealOptions => (a.effect?.steal && typeof a.effect.steal === 'object' ? a.effect.steal : {})

/** Refresh: how many times a unit can be refreshed in one turn (1 is the default and is written as `true`). */
export const refreshPerTurn = (a: Ability): number => (a.effect?.refresh && typeof a.effect.refresh === 'object' ? a.effect.refresh.perTurn : 1)
export function setRefreshPerTurn(a: Ability, n: number): Ability {
  if (effectKind(a) !== 'refresh') return a
  const v = clamp(n, 1, REFRESH_MAX)
  return patchObj(a, { effect: { refresh: v === 1 ? true : { perTurn: v } } })
}
export const REFRESH_MAX = 9

/** Cure: "all", or the ids of the conditions it cures (the first one when the maker leaves "all"). */
export const setCure = (a: Ability, cure: string[] | 'all'): Ability => (effectKind(a) === 'cure' ? patchObj(a, { effect: { cure } }) : a)

export const setCost = (a: Ability, free: boolean): Ability => patchObj(a, { cost: free ? 'free' : undefined })
export const setUses = (a: Ability, n: number | undefined): Ability => patchObj(a, { uses: n === undefined ? undefined : clamp(n, 1, USES_MAX) })
export const setCooldown = (a: Ability, n: number | undefined): Ability => patchObj(a, { cooldown: n === undefined ? undefined : clamp(n, 1, USES_MAX) })
export const setAfterMove = (a: Ability, afterMove: boolean): Ability => patchObj(a, { afterMove: afterMove ? undefined : false })
export const setConditions = (a: Ability, list: Extra[]): Ability => patchObj(a, { if: list.length ? list : undefined })

/** Ability ids, so a story or a form can tell what exists. */
export const abilityName = (doc: ProjectDoc, id: string): string => doc.abilities?.find((a) => a.id === id)?.name ?? id

/** Unlock (docs/SRPG.md section 10): doors and chests without a key, optionally needing a lockpick item that wears out. `true` = both kinds. */
export type UnlockKinds = 'both' | 'door' | 'chest'
export interface UnlockSetting { kinds: UnlockKinds; needs: string }
export function unlockOf(a: Ability): UnlockSetting {
  const u = a.effect?.unlock
  const fromList = (l: unknown): UnlockKinds => (Array.isArray(l) && l.length === 1 ? (l[0] === 'door' ? 'door' : l[0] === 'chest' ? 'chest' : 'both') : 'both')
  if (Array.isArray(u)) return { kinds: fromList(u), needs: '' }
  if (u && typeof u === 'object') return { kinds: fromList((u as { kinds?: unknown }).kinds), needs: typeof (u as { needs?: unknown }).needs === 'string' ? (u as { needs: string }).needs : '' }
  return { kinds: 'both', needs: '' }
}
/** The simplest form that says it: true, a list of one kind, or an object when a lockpick is needed. */
export function setUnlock(a: Ability, patch: Partial<UnlockSetting>): Ability {
  if (effectKind(a) !== 'unlock') return a
  const cur = { ...unlockOf(a), ...patch }
  const list = cur.kinds === 'both' ? undefined : [cur.kinds]
  const value = cur.needs
    ? { ...(list ? { kinds: list } : {}), needs: cur.needs }
    : list ?? true
  return patchObj(a, { effect: { unlock: value as AbilityEffect['unlock'] } })
}
