// Edits of one rule ("block") of a skill, pure functions on plain JSON. Every function returns a block the loader accepts
// when it was given one: changing "when" repairs what the new point does not allow, an effect or condition that needs a
// thing of the game (a weapon, an ability) is only made when the game has one, and the keys keep the file's usual order.
// Keys this editor does not know stay (the loader will tell the maker about them).
import {
  AURA_EFFECTS, FLAG_EFFECTS, MAX_CONDITIONS, MAX_EFFECTS, canJudgeFoe, condKindOf, condsFor, effectKindOf, effectsFor, hasChance,
  type CondKind, type EffectKind, type When,
} from './skillRules'
import type { Extra, ProjectDoc } from './types'

const KEY_ORDER = ['when', 'if', 'chance', 'aura', 'do']
const list = (v: unknown): Extra[] => (Array.isArray(v) ? (v as Extra[]) : [])

/** The block with its keys in the file's usual order and empty `if` / `aura` / `chance` left out. */
export function tidy(b: Extra): Extra {
  const out: Extra = {}
  for (const k of KEY_ORDER) if (b[k] !== undefined) out[k] = b[k]
  for (const [k, v] of Object.entries(b)) if (!(k in out) && v !== undefined) out[k] = v
  if (Array.isArray(out.if) && out.if.length === 0) delete out.if
  return out
}
export const patchBlock = (b: Extra, patch: Extra): Extra => tidy({ ...b, ...patch })

// --- stats of the game ---------------------------------------------------------------------------------------------------

export const statIds = (doc: ProjectDoc): string[] => (doc.game?.stats ?? []).map((s) => s.id)
/** The stats a condition's stat bonus may change: not hit points (a maximum that flickers would cut the current hit points). */
export const conditionStatIds = (doc: ProjectDoc): string[] => statIds(doc).filter((s) => s !== doc.game?.roles?.hp)
/** A combat bonus may not change the hit points or the movement stat (the loader refuses it). */
export const combatStatIds = (doc: ProjectDoc): string[] => statIds(doc).filter((s) => s !== doc.game?.roles?.hp && s !== doc.game?.roles?.move)

// --- new conditions and effects ------------------------------------------------------------------------------------------

/** Something to start from, or null when the game has nothing for it yet (no weapon, no terrain, no unit type). */
export function newCondition(kind: CondKind, doc: ProjectDoc): Extra | null {
  switch (kind) {
    case 'hp': return { hp: { op: '<=', percent: 50 } }
    case 'near': return { near: 'ally', within: 1, op: '>=', count: 1 }
    case 'weapon': return doc.weapons?.[0] ? { weapon: doc.weapons[0].id } : null
    case 'terrain': return doc.terrain?.[0] ? { terrain: doc.terrain[0].id } : null
    case 'structure': return { structure: true }
    case 'unit': return doc.units?.[0] ? { unit: doc.units[0].id } : null
    case 'carrying': return { carrying: true }
    case 'round': return { round: { op: '>=', value: 2 } }
    case 'initiating': return { initiating: true }
    case 'distance': return { distance: { op: '=', value: 1 } }
    case 'formula': return { formula: 'a.level >= 5' }
    case 'condition': return doc.conditions?.[0] ? { condition: doc.conditions[0].id } : null
  }
}

/** A starting effect for the point, or null when there is none to make (no stat, no ability). */
export function newEffect(kind: EffectKind, doc: ProjectDoc, when: When): Extra | null {
  const stats = when === 'combat' ? combatStatIds(doc) : statIds(doc)
  const plain = combatStatIds(doc)
  switch (kind) {
    case 'stats': { const s = plain[0] ?? stats[0]; return s ? { stats: { [s]: 1 } } : null }
    case 'growths': { const s = plain[0] ?? statIds(doc)[0]; return s ? { growths: { [s]: 10 } } : null }
    case 'ability': return doc.abilities?.[0] ? { ability: doc.abilities[0].id } : null
    case 'maxMoveCost': return { maxMoveCost: 1 }
    case 'pass': return { pass: 'enemies' }
    case 'xpPercent': return { xpPercent: 150 }
    case 'hit': return { hit: 10 }
    case 'crit': return { crit: 10 }
    case 'damage': return { damage: 3 }
    case 'avoid': return { avoid: 10 }
    case 'critAvoid': return { critAvoid: 10 }
    case 'damageTaken': return { damageTaken: -2 }
    case 'damagePercent': return { damagePercent: 150 }
    case 'damageTakenPercent': return { damageTakenPercent: 50 }
    case 'strikes': return { strikes: 1 }
    case 'multiStrike': return { multiStrike: 2 }
    case 'drain': return { drain: 50 }
    case 'heal': return { heal: 5 }
    case 'hurt': return { hurt: 5 }
    case 'immune': return doc.conditions?.[0] ? { immune: [doc.conditions[0].id] } : null
    case 'inflict': return doc.conditions?.[0] ? { inflict: { condition: doc.conditions[0].id } } : null
    case 'cannotAct': return { cannotAct: true }
    case 'forbid': return { forbid: { kinds: ['staff'] } }
    default: return FLAG_EFFECTS.includes(kind) ? { [kind]: true } : null
  }
}

/** The effect kinds to offer in a block: those its point allows, that can be made in this game, and that the block's
 *  conditions allow (growths and an ability cannot have conditions). */
export function offeredEffects(doc: ProjectDoc, b: Extra): EffectKind[] {
  const when = b.when as When
  const conds = list(b.if).length > 0
  // A condition's four own effects (cannot act, cannot move, forbid, berserk) have their own simple controls on the Conditions form.
  return effectsFor(when, !!b.aura).filter((k) => newEffect(k, doc, when) !== null && !(conds && (k === 'growths' || k === 'ability')))
}

/** The condition kinds to offer in a block, and why one is missing when it cannot be had (for the picker's help). */
export function offeredConditions(doc: ProjectDoc, b: Extra): CondKind[] {
  return condsFor(b.when as When, !!b.aura).filter((k) => newCondition(k, doc) !== null)
}

/** Why a condition cannot be added now (the block already has what forbids conditions), or null. */
export function noConditionsWhy(doc: ProjectDoc, b: Extra): 'growths' | 'ability' | 'hp' | 'limit' | null {
  const conds = list(b.if)
  if (conds.length >= MAX_CONDITIONS) return 'limit'
  const eff = list(b.do)
  if (eff.some((e) => e.growths !== undefined)) return 'growths'
  if (eff.some((e) => e.ability !== undefined)) return 'ability'
  const hp = doc.game?.roles?.hp
  if (b.when === 'always' && hp && eff.some((e) => e.stats && typeof e.stats === 'object' && (e.stats as Extra)[hp] !== undefined)) return 'hp'
  return null
}

// --- editing a block ---------------------------------------------------------------------------------------------------------

export const addCondition = (b: Extra, c: Extra): Extra => patchBlock(b, { if: [...list(b.if), c] })
export const setCondition = (b: Extra, i: number, c: Extra): Extra => patchBlock(b, { if: list(b.if).map((x, j) => (j === i ? c : x)) })
export const removeCondition = (b: Extra, i: number): Extra => patchBlock(b, { if: list(b.if).filter((_, j) => j !== i) })
export const setConditions = (b: Extra, next: Extra[]): Extra => patchBlock(b, { if: next })

export function addEffect(b: Extra, e: Extra): Extra {
  return list(b.do).length >= MAX_EFFECTS ? b : patchBlock(b, { do: [...list(b.do), e] })
}
export const setEffect = (b: Extra, i: number, e: Extra): Extra => patchBlock(b, { do: list(b.do).map((x, j) => (j === i ? e : x)) })
export const setEffects = (b: Extra, next: Extra[]): Extra => patchBlock(b, { do: next })

/** Changing "Reaches": "itself" removes the aura; allies / enemies make an aura and drop what an aura cannot give. */
export function setAura(b: Extra, aura: { to: 'allies' | 'enemies'; within: number } | null, doc: ProjectDoc): Extra {
  if (aura === null) return patchBlock(b, { aura: undefined })
  const eff = list(b.do).flatMap((e): Extra[] => {
    const k = effectKindOf(e)
    if (!k || !AURA_EFFECTS.includes(k)) return []
    const { to: _to, ...rest } = e
    return [rest]
  })
  // An aura's conditions are judged for its owner alone: nothing about a foe or the fight itself.
  const ok = condsFor('combat', true)
  const conds = list(b.if).filter((c) => { const k = condKindOf(c); return (k === null || ok.includes(k)) && c.who !== 'foe' })
  return patchBlock(b, { aura, if: conds, do: eff.length ? eff : [newEffect('damage', doc, 'combat')!] })
}

/** The effect with `"to": "foe"` set or cleared (only where the effect may have it). */
export function setEffectTarget(e: Extra, foe: boolean): Extra {
  const { to: _to, ...rest } = e
  return foe ? { ...rest, to: 'foe' } : rest
}

/** Changes the point in time. What the new point does not allow is taken out (the maker can Undo): the chance, the aura,
 *  conditions about a foe or a combat, effects of another point; a rule left with no effect gets one that fits. */
export function setWhen(b: Extra, when: When, doc: ProjectDoc): Extra {
  const aura = when === 'combat' && b.aura ? b.aura : undefined
  const okConds = condsFor(when, !!aura)
  const conds = list(b.if).filter((c) => {
    const k = condKindOf(c)
    return k === null || (okConds.includes(k) && (c.who !== 'foe' || canJudgeFoe(k, when, !!aura)))
  })
  const okEffects = effectsFor(when, !!aura)
  let eff = list(b.do).filter((e) => { const k = effectKindOf(e); return k === null || okEffects.includes(k) })
  // "to foe" belongs to the combat point. A hurt's targets differ between the two timed points, so its own are dropped
  // and the default of the new point applies.
  eff = eff.map((e) => {
    if (effectKindOf(e) === 'hurt') return withoutKey(withoutKey(e, 'to'), 'within')
    if (e.to === 'foe' && (when !== 'combat' || aura)) return setEffectTarget(e, false)
    return e
  })
  if (eff.length === 0) { const first = okEffects.map((k) => newEffect(k, doc, when)).find((x) => x !== null); eff = first ? [first] : [] }
  const next: Extra = { ...b, when, if: conds, do: eff }
  if (!hasChance(when)) delete next.chance
  if (!aura) delete next.aura
  return tidy(next)
}
const withoutKey = (o: Extra, k: string): Extra => { const c = { ...o }; delete c[k]; return c }

/** A new skill rule, valid as it stands. */
export function newBlock(doc: ProjectDoc, when: When = 'combat'): Extra {
  const prefer: Record<When, EffectKind> = { always: 'stats', turnStart: 'heal', combat: 'damage', strike: 'damage', struck: 'damageTaken', defeat: 'heal', afterCombat: 'heal' }
  const e = [prefer[when], ...effectsFor(when, false)].map((k) => newEffect(k, doc, when)).find((x) => x !== null)
  return { when, do: e ? [e] : [] }
}
