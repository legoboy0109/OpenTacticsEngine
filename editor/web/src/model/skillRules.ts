// The tables of docs/SRPG.md section 3 as data: the eight "when" points, which conditions and effects each one allows,
// and the limits of the loader (engine/core/data/loader_skill_blocks.cpp). The block builder offers only what these
// tables allow, so what the editor writes loads. Wording lives in screens/skills/text.ts, not here.
import type { Extra } from './types'

export type When = 'always' | 'turnStart' | 'combat' | 'strike' | 'struck' | 'defeat' | 'afterCombat' | 'staffUse'
export const WHENS: When[] = ['always', 'turnStart', 'combat', 'strike', 'struck', 'defeat', 'afterCombat', 'staffUse']

/** Loader limits (`kMaxSkillBlocks`, `kMaxBlockConditions`, `kMaxBlockEffects` in engine/core/data/srpg.h). */
export const MAX_BLOCKS = 8
export const MAX_CONDITIONS = 8
export const MAX_EFFECTS = 8

export type CondKind = 'hp' | 'near' | 'weapon' | 'terrain' | 'structure' | 'unit' | 'carrying' | 'round' | 'initiating' | 'distance' | 'formula' | 'condition'
  // General blocks (docs/SRPG.md "More skill blocks"): a weapon's kind of damage, the turn number, "the fight has a counterattack", items carried.
  | 'weaponKind' | 'turn' | 'counterattack' | 'carries' | 'inventory'
  // Third set (docs/SRPG.md "Skill blocks, third set"): which strike of the fight (strike / struck blocks).
  | 'strike'
export const COND_KINDS: CondKind[] = ['hp', 'near', 'weapon', 'terrain', 'structure', 'unit', 'carrying', 'round', 'initiating', 'distance', 'formula', 'condition',
  'weaponKind', 'turn', 'counterattack', 'carries', 'inventory', 'strike']
/** Conditions that may say `"who": "foe"` (the foe points only). */
export const FOEABLE: CondKind[] = ['hp', 'weapon', 'terrain', 'structure', 'unit', 'carrying', 'condition', 'weaponKind', 'carries', 'inventory']
/** Only the "strike" and "struck" points know whether the foe answers and which strike it is. */
export const STRIKE_ONLY: CondKind[] = ['counterattack', 'strike']
/** Conditions that need a combat. */
export const FOE_POINT_ONLY: CondKind[] = ['initiating', 'distance']

export type EffectKind =
  | 'stats' | 'growths' | 'maxMoveCost' | 'pass' | 'moveAgain' | 'xpPercent' | 'ability'
  | 'hit' | 'crit' | 'damage' | 'avoid' | 'critAvoid' | 'damageTaken' | 'damagePercent' | 'damageTakenPercent'
  | 'strikes' | 'multiStrike' | 'strikeFirst' | 'counterAnyRange' | 'noCounter' | 'drain' | 'strikeAgain' | 'survive'
  | 'heal' | 'hurt' | 'actAgain'
  // Status conditions (docs/SRPG.md section 9): the first four belong to the blocks of a condition only.
  | 'cannotAct' | 'cannotMove' | 'forbid' | 'berserk' | 'immune' | 'inflict'
  // General blocks (docs/SRPG.md "More skill blocks").
  | 'followUp' | 'followShift' | 'trianglePercent' | 'weaponXpPercent' | 'noCarryPenalty' | 'cancelEffective' | 'cure'
  // Second set (docs/SRPG.md "Skill and ability blocks, second set").
  | 'nullifySkills' | 'staffRange' | 'carryBonus'
  // Third set: the wielder is hurt by its own strike.
  | 'recoil'
  // Fog (docs/ECONOMY.md "Fog options"): the unit sees further or less far. Never 0 (the loader refuses it).
  | 'vision'
const A: When = 'always', T: When = 'turnStart', C: When = 'combat', S: When = 'strike', R: When = 'struck', D: When = 'defeat', F: When = 'afterCombat', U: When = 'staffUse'
/** Every effect and the `when` points it is allowed in, in the order the contract lists them. */
export const EFFECT_WHENS: Record<EffectKind, When[]> = {
  stats: [A, C], growths: [A], maxMoveCost: [A], pass: [A], moveAgain: [A], xpPercent: [A], ability: [A],
  hit: [C, S], crit: [C, S], damage: [C, S], avoid: [C, R], critAvoid: [C, R], damageTaken: [C, R],
  damagePercent: [C, S], damageTakenPercent: [C, R], strikes: [C], multiStrike: [C], strikeFirst: [C], counterAnyRange: [C], noCounter: [C],
  drain: [S], strikeAgain: [S], survive: [R], heal: [T, D, F, U], hurt: [T, F], actAgain: [D, F],
  cannotAct: [A], cannotMove: [A], forbid: [A], berserk: [A], immune: [A], inflict: [S, R],
  followUp: [C], followShift: [C], trianglePercent: [C], weaponXpPercent: [A], noCarryPenalty: [A], cancelEffective: [A], cure: [T, D, F, U],
  nullifySkills: [C], staffRange: [A], carryBonus: [A], recoil: [S], vision: [A],
}
/** Effects only the blocks of a condition may have (the loader says they "belong to the blocks of a condition"). */
export const CONDITION_ONLY: EffectKind[] = ['cannotAct', 'cannotMove', 'forbid', 'berserk']
export const EFFECT_KINDS = Object.keys(EFFECT_WHENS) as EffectKind[]
/** What an aura may give. */
export const AURA_EFFECTS: EffectKind[] = ['stats', 'hit', 'avoid', 'crit', 'critAvoid', 'damage', 'damageTaken']
/** Effects that may be aimed at the foe (`"to": "foe"`, combat blocks only, never in an aura). */
export const FOE_EFFECTS: EffectKind[] = ['stats', 'hit', 'avoid', 'crit', 'critAvoid', 'damage', 'damageTaken', 'strikes', 'followUp', 'followShift', 'trianglePercent']
/** Combat numbers that may be written as a formula (text) instead of a whole number; "stats" may too, per stat, but only in combat. */
export const FORMULA_EFFECTS: EffectKind[] = ['hit', 'crit', 'damage', 'avoid', 'critAvoid', 'damageTaken', 'damagePercent', 'damageTakenPercent']
/** Effects without a number: they are written as `true`. */
export const FLAG_EFFECTS: EffectKind[] = ['moveAgain', 'strikeFirst', 'counterAnyRange', 'noCounter', 'strikeAgain', 'survive', 'actAgain', 'cannotMove', 'berserk', 'noCarryPenalty', 'cancelEffective']
/** Whole-number effects and the loader's range. */
export const NUMBER_RANGE: Partial<Record<EffectKind, [number, number]>> = {
  maxMoveCost: [1, 100], xpPercent: [1, 1000], hit: [-1000, 1000], crit: [-1000, 1000], damage: [-1000, 1000], avoid: [-1000, 1000],
  critAvoid: [-1000, 1000], damageTaken: [-1000, 1000], damagePercent: [0, 1000], damageTakenPercent: [0, 1000], strikes: [-7, 7],
  multiStrike: [2, 4], drain: [1, 100], followShift: [-100, 100], trianglePercent: [0, 1000], weaponXpPercent: [1, 1000], carryBonus: [-100, 100], vision: [-10, 10],
}
/** `staffRange`: a number from 1 to 10, or `{ amount, group }` (its own form, so it is not in NUMBER_RANGE). */
export const STAFF_RANGE_MAX = 10

export const hasChance = (w: When): boolean => w !== 'always' && w !== 'combat'
/** `"if"` in an `always` block: no combat, so no foe, no initiating / distance, no formula. A turn start has no foe either. */
export function condsFor(w: When, aura: boolean): CondKind[] {
  const plain = COND_KINDS.filter((k) => !STRIKE_ONLY.includes(k))
  if (w === 'always') return plain.filter((k) => !FOE_POINT_ONLY.includes(k) && k !== 'formula')
  if (w === 'turnStart' || w === 'staffUse' || aura) return plain.filter((k) => !FOE_POINT_ONLY.includes(k))
  return (w === 'strike' || w === 'struck') ? COND_KINDS : plain
}
/** Can this condition say "the foe"? Only where there is a foe, and never for an aura. */
export const canJudgeFoe = (k: CondKind, w: When, aura: boolean): boolean => FOEABLE.includes(k) && !aura && w !== 'always' && w !== 'turnStart' && w !== 'staffUse'
export function effectsFor(w: When, aura: boolean, forCondition = false): EffectKind[] {
  return EFFECT_KINDS.filter((k) => EFFECT_WHENS[k].includes(w) && (!aura || AURA_EFFECTS.includes(k)) && (forCondition || !CONDITION_ONLY.includes(k)))
}

const has = (o: object, k: string) => Object.prototype.hasOwnProperty.call(o, k)
export const condKindOf = (c: Extra): CondKind | null => COND_KINDS.find((k) => has(c, k)) ?? null
export const effectKindOf = (e: Extra): EffectKind | null => EFFECT_KINDS.find((k) => has(e, k)) ?? null
export const whenOf = (b: Extra): When | null => ((WHENS as string[]).includes(b.when as string) ? (b.when as When) : null)
