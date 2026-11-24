// Checks on an ability's "area", its range with an area, and the heal + cure pair, in plain words; each mirrors one refusal of the
// loader (data/abilities.json). Every sentence starts with the ability's name, like the other ability checks.
import { AREA_MAX, AREA_SHAPES, VERTICAL_MAX, kindAllowsArea } from './abilityArea'
import { isFoe } from './abilityConditions'
import { EFFECT_KINDS, effectKind, hasFixedRange, targetOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'

const whole = (n: unknown, lo: number, hi: number): boolean => typeof n === 'number' && Number.isInteger(n) && n >= lo && n <= hi
const KNOWN_KEYS = ['shape', 'radius', 'length', 'vertical']

/** Is the game's "rules": { "height": ... } there? (An empty object counts: it turns the rules on.) */
export const gameHasHeight = (doc: ProjectDoc): boolean => {
  const h = (doc.game?.rules as Extra | undefined)?.height
  return !!h && typeof h === 'object'
}

/** An "effect" holds one thing, except heal and cure together. */
export function effectPairChecks(a: Ability): string[] {
  if (!a.effect || typeof a.effect !== 'object') return []
  const keys = EFFECT_KINDS.filter((k) => k !== 'revive' && Object.prototype.hasOwnProperty.call(a.effect, k))  // a revive's own check says it must be alone
  const pair = keys.length === 2 && keys.includes('heal') && keys.includes('cure')
  return keys.length > 1 && !pair ? [`${a.name}: it can do only one thing. Only healing and curing may go together.`] : []
}

/** The range: from 1 to 20, or from 0 (the user's own square) with an area. */
function rangeChecks(a: Ability, withArea: boolean): string[] {
  const kind = effectKind(a)
  if (a.range === undefined || kind === 'strike' || hasFixedRange(kind) || targetOf(a) === 'self') return []
  const [lo, hi] = Array.isArray(a.range) ? a.range : [NaN, NaN]
  const min = withArea ? 0 : 1
  return Array.isArray(a.range) && a.range.length === 2 && whole(lo, min, 20) && whole(hi, min, 20) && lo <= hi ? []
    : [`${a.name}: the reach must be two whole numbers from ${min} to 20${withArea ? ' (0 is the unit’s own square)' : ''}, the first not above the second.`]
}

export function areaChecks(doc: ProjectDoc, a: Ability): string[] {
  if (a.area === undefined) return rangeChecks(a, false)
  const out: string[] = []
  const kind = effectKind(a)
  if (kind === 'strike') return [`${a.name}: a strike takes its area from the weapon. Leave out the area.`]
  if (kind !== null && !kindAllowsArea(kind)) out.push(`${a.name}: an area goes with healing, curing (or both), reviving or an ability that only gives a condition. It cannot push, pull, swap, summon, unlock, steal or refresh.`)
  const target = targetOf(a)
  if (target === 'self' || target === 'cell') out.push(`${a.name}: an area needs the target to be an ally, an enemy or any unit (who in the area is affected). A reach of 0 to 0 centres it on the unit.`)
  out.push(...rangeChecks(a, true))
  if (typeof a.area !== 'object' || a.area === null || Array.isArray(a.area)) return [...out, `${a.name}: the area must be a shape with a size.`]
  const o = a.area
  for (const k of Object.keys(o)) {
    if (k === 'friendlyFire') out.push(`${a.name}: the area has no “friendly fire” setting. The target (ally, enemy or unit) says who in the area is affected.`)
    else if (k === 'apExtra') out.push(`${a.name}: the area has no extra action point cost. That is for weapons.`)
    else if (!KNOWN_KEYS.includes(k)) out.push(`${a.name}: the area has an unknown setting “${k}”. An ability’s area takes a shape, a radius or length, and a vertical reach.`)
  }
  const shape = o.shape
  if (shape === undefined) out.push(`${a.name}: the area needs a shape (burst, radius, cross or line).`)
  else if (!AREA_SHAPES.includes(shape as never)) out.push(`${a.name}: the shape of the area must be burst, radius, cross or line.`)
  if (shape === 'radius' || shape === 'cross') {
    if (o.radius === undefined) out.push(`${a.name}: an area of shape “${shape}” needs a radius from 1 to ${AREA_MAX}.`)
    else if (!whole(o.radius, 1, AREA_MAX)) out.push(`${a.name}: the radius of the area must be a whole number from 1 to ${AREA_MAX}.`)
  } else if (o.radius !== undefined) out.push(`${a.name}: a radius goes only with the shapes “radius” and “cross”.`)
  if (shape === 'line') {
    if (o.length === undefined) out.push(`${a.name}: an area of shape “line” needs a length from 1 to ${AREA_MAX}.`)
    else if (!whole(o.length, 1, AREA_MAX)) out.push(`${a.name}: the length of the area must be a whole number from 1 to ${AREA_MAX}.`)
  } else if (o.length !== undefined) out.push(`${a.name}: a length goes only with the shape “line”.`)
  if (o.vertical !== undefined) {
    if (!whole(o.vertical, 0, VERTICAL_MAX)) out.push(`${a.name}: the vertical reach of the area must be a whole number from 0 to ${VERTICAL_MAX}.`)
    if (!gameHasHeight(doc)) out.push(`${a.name}: a vertical reach needs the height rules to be switched on for the game.`)
  }
  const raw = a as Record<string, unknown>
  if (raw.chargeSpeed !== undefined && raw.chargeTargets === 'unit') out.push(`${a.name}: a charged ability with an area keeps the square it aims at. “Who is hit” must be the square (or left out).`)
  if ((a.if ?? []).some((c) => isFoe(c))) out.push(`${a.name}: an area ability has no single target unit, so its “only if” cannot be about the target.`)
  return out
}
