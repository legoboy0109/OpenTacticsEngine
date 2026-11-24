// Menu abilities (docs/SRPG.md section 4, data/abilities.json): create, change, and the rules that keep what the screen
// writes loadable. The loader refuses a push with a free square as target, a heal on an enemy, a summon that is not on a
// square, and so on; here the pickers only offer what the loader accepts, and changing the effect repairs the target.
// A default (cost "action", afterMove true, summon limit 1 ...) is never written; keys the editor does not know stay.
import { withAbilities } from './doc'
import { hasArea, kindAllowsArea } from './abilityArea'
import { newAbilityId } from './srpgRefs'
import { moveItem, patchObj, updateById } from './util'
import type { Ability, AbilityEffect, AbilityTarget, StealOptions, Summon } from './srpgTypes'
import type { CureValue, Extra, ProjectDoc } from './types'

export type EffectKind = 'push' | 'pull' | 'swap' | 'moveBehind' | 'movePast' | 'heal' | 'summon' | 'steal' | 'refresh' | 'cure' | 'unlock' | 'strike' | 'revive' | 'none'
export const EFFECT_KINDS: EffectKind[] = ['push', 'pull', 'swap', 'moveBehind', 'movePast', 'heal', 'summon', 'steal', 'refresh', 'cure', 'unlock', 'strike', 'revive']
/** 'none' = no "effect" key at all: the ability only gives a condition ("inflict"). Not in EFFECT_KINDS (those are the kinds with a key of their own). */
export const NO_EFFECT: EffectKind = 'none'
/** The five that move units next to the user: the loader needs a unit target and the range [1, 1]. */
export const isMove = (k: EffectKind | null): boolean => k === 'push' || k === 'pull' || k === 'swap' || k === 'moveBehind' || k === 'movePast'
/** Effects with no range of their own to write: the moving ones and steal (a neighbour, range [1, 1]). */
export const hasFixedRange = (k: EffectKind | null): boolean => isMove(k) || k === 'steal' || k === 'strike'

export const PUSH_MAX = 5
export const HEAL_MAX = 999
export const USES_MAX = 99
export const RANGE_MAX = 20
export const LIMIT_MAX = 20
export const LIFETIME_MAX = 99

/** Which targets each effect accepts (the loader's "target against effect" rules). */
export const TARGETS_FOR: Record<Exclude<EffectKind, 'none'>, AbilityTarget[]> = {
  push: ['ally', 'enemy', 'unit'], pull: ['ally', 'enemy', 'unit'], swap: ['ally', 'enemy', 'unit'], moveBehind: ['ally', 'enemy', 'unit'], movePast: ['ally', 'enemy', 'unit'],
  heal: ['self', 'ally', 'unit'], summon: ['cell'], steal: ['enemy'], refresh: ['ally'], cure: ['self', 'ally', 'enemy', 'unit'], unlock: ['cell'],
  /** A strike has no target of its own: the weapon decides whom it hits and how far (the loader refuses "target" and "range"). */
  strike: [],
  /** Stands a downed unit up (model/downed.ts): an ally or any unit, never itself, an enemy or a square. */
  revive: ['ally', 'unit'],
}

/** What an ability with only an inflict may be aimed at: any unit (not a square). */
export const targetsFor = (k: EffectKind): AbilityTarget[] => (k === 'none' ? ['self', 'ally', 'enemy', 'unit'] : TARGETS_FOR[k])

export function effectKind(a: Ability): EffectKind | null {
  const e = a.effect
  if (!e || typeof e !== 'object') return a.inflict && typeof a.inflict === 'object' ? 'none' : null  // an object or a list of conditions
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
  if (!hasArea(a) && range[0] < 1) range = [1, Math.max(1, range[1])]  // a min of 0 (the user's own square) needs an area
  const keep = targetOf(a) !== 'self' && effectKind(a) !== 'strike' && !hasFixedRange(kind) && !(range[0] === 1 && range[1] === 1)
  return patchObj(a, { range: keep ? range : undefined })
}

export function setTarget(a: Ability, target: AbilityTarget): Ability {
  const kind = effectKind(a)
  if (kind && !targetsFor(kind).includes(target)) return a
  if (hasArea(a) && (target === 'self' || target === 'cell')) return a  // an area needs to say who in it is affected
  return withRange(patchObj(a, { target }), rangeOf(a))
}

export function setRange(a: Ability, min: number, max: number): Ability {
  const lo = clamp(min, hasArea(a) ? 0 : 1, RANGE_MAX)  // 0 = the user's own square, only with an area
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
    case 'strike': return { strike: '' }
    case 'revive': return { revive: { percent: 50 } }
    case 'none': return {}
  }
}

/** Changing what it does keeps the target when the new effect accepts it, else picks the first that fits; a summon
 *  needs a square within reach, so it starts with range 1..2 (the summon's own default is the maker's to change). */
export function setEffectKind(doc: ProjectDoc, a: Ability, k: EffectKind): Ability {
  if (effectKind(a) === k) return a
  if (k === 'strike') return patchObj(a, { effect: { strike: doc.weapons?.[0]?.id ?? '' }, target: undefined, range: undefined, cost: undefined, area: undefined })  // the weapon gives the target and reach; a strike always uses the action
  const keepsArea = hasArea(a) && kindAllowsArea(k)  // any other effect cannot have an area: it goes
  const target = targetsFor(k).includes(targetOf(a)) ? targetOf(a) : targetsFor(k).find((t) => !keepsArea || (t !== 'self' && t !== 'cell')) ?? targetsFor(k)[0]
  a = keepsArea ? a : patchObj(a, { area: undefined })
  if (k === 'none') {  // no "effect" key; "inflict" is then required, so one is started
    const next = patchObj(a, { effect: undefined, target, inflict: a.inflict ?? (doc.conditions?.[0] ? { condition: doc.conditions[0].id } : undefined) })
    return withRange(next, rangeOf(a))
  }
  const effect = defaultEffect(k)
  if (k === 'summon') effect.summon = { type: doc.units?.[0]?.id ?? '' }
  if (k === 'cure' && doc.conditions?.[0]) effect.cure = [doc.conditions[0].id]
  const next = patchObj(a, { effect, target })
  return withRange(next, k === 'summon' && a.range === undefined ? [1, 2] : rangeOf(a))
}

/** "Acts on an area": starts a burst (radius 1) when the effect allows one. Off removes it and the range goes back to starting at 1.
 *  A target that cannot be used with an area (itself) becomes "ally". */
export function setArea(a: Ability, on: boolean): Ability {
  if (on === hasArea(a)) return a
  if (!on) return withRange(patchObj(a, { area: undefined }), rangeOf(a))
  if (!kindAllowsArea(effectKind(a))) return a
  const target = targetOf(a)
  return withRange(patchObj(a, { area: { shape: 'burst' }, target: target === 'self' || target === 'cell' ? 'ally' : a.target }), rangeOf(a))
}

/** The weapon a strike attacks with ('' = none picked yet). */
export const strikeWeapon = (a: Ability): string => (typeof a.effect?.strike === 'string' ? a.effect.strike : '')
export const setStrikeWeapon = (a: Ability, weapon: string): Ability => (effectKind(a) === 'strike' ? patchObj(a, { effect: { strike: weapon } }) : a)

/** How a heal is given: a number of hit points, everything, a share of the maximum, or a formula (docs/SRPG.md "Healing by formula"). */
export type HealKind = 'amount' | 'full' | 'percent' | 'formula'
export function healKind(a: Ability): HealKind {
  const h = a.effect?.heal
  if (h === 'full') return 'full'
  if (typeof h === 'string') return 'formula'
  if (h && typeof h === 'object') return 'percent'
  return 'amount'
}
export const PERCENT_MAX = 100
/** Changing how it heals starts from a value that is valid for the new kind. */
export function setHealKind(a: Ability, kind: HealKind): Ability {
  if (effectKind(a) !== 'heal' || healKind(a) === kind) return a
  const heal: AbilityEffect['heal'] = kind === 'full' ? 'full' : kind === 'percent' ? { percent: 25 } : kind === 'formula' ? 'a.mag' : 10
  return patchObj(a, { effect: { ...a.effect, heal } })
}
/** The percent or the formula text, for the kinds that have them. */
export function setHealValue(a: Ability, value: number | string): Ability {
  if (effectKind(a) !== 'heal') return a
  const k = healKind(a)
  if (k === 'percent' && typeof value === 'number') return patchObj(a, { effect: { ...a.effect, heal: { percent: clamp(value, 1, PERCENT_MAX) } } })
  if (k === 'formula' && typeof value === 'string') return patchObj(a, { effect: { ...a.effect, heal: value } })
  return a
}

/** push 1..5; heal 1..999 or "full". */
export function setEffectAmount(a: Ability, amount: number | 'full'): Ability {
  const k = effectKind(a)
  if (k === 'push') return patchObj(a, { effect: { ...a.effect, push: clamp(amount === 'full' ? 1 : amount, 1, PUSH_MAX) } })
  if (k === 'heal') return patchObj(a, { effect: { ...a.effect, heal: amount === 'full' ? 'full' : clamp(amount, 1, HEAL_MAX) } })
  return a
}

/** Summon fields: a value equal to the engine's default is removed rather than written. */
const SUMMON_DEFAULTS: Record<string, unknown> = { level: 'summoner', limit: 1, vanish: true, lifetime: 0, canAct: false, givesXp: true, replace: false, pick: 'highest' }
export function setSummon(a: Ability, patch: Partial<Summon>): Ability {
  if (effectKind(a) !== 'summon') return a
  const next: Record<string, unknown> = { ...summonOf(a), ...patch }
  for (const [k, def] of Object.entries(SUMMON_DEFAULTS)) if (next[k] === def || next[k] === undefined) delete next[k]
  if (!Array.isArray(next.items) || next.items.length === 0) { delete next.items; delete next.pick }  // a pick goes with the items
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

/** Cure: "all", "harmful" / "helpful" (every condition of that kind), or the ids of the conditions it cures (the first one when the maker leaves "all").
 *  A heal that also cures keeps its heal; turning "also cures" on or off is in abilityHealCure.ts. */
export const setCure = (a: Ability, cure: CureValue): Ability =>
  effectKind(a) === 'cure' ? patchObj(a, { effect: { cure } }) : effectKind(a) === 'heal' && a.effect && 'cure' in a.effect ? patchObj(a, { effect: { ...a.effect, cure } }) : a

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

// --- spend: what using the ability costs (docs/SRPG.md "Unit resources") --------------------------------------------------

export const SPEND_MAX = 999
/** An amount is whole and 1 to 999; a resource that was listed is kept even when it no longer exists, so a check can name it. */
export const spendOf = (a: Ability): Record<string, number> => (a.spend && typeof a.spend === 'object' && !Array.isArray(a.spend) ? a.spend : {})

/** Sets (or, with undefined, removes) the price in one resource; an empty price removes the key. */
export function setSpend(a: Ability, resource: string, amount: number | undefined): Ability {
  const next = { ...spendOf(a) }
  if (amount === undefined) delete next[resource]
  else next[resource] = clamp(amount, 1, SPEND_MAX)
  return patchObj(a, { spend: Object.keys(next).length ? next : undefined })
}
