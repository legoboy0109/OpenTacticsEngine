// Checks on an ability's new keys, mirroring the loader (docs/SRPG.md "Abilities that strike", "Healing by formula", "Unit resources"):
// the strike weapon, the heal amount and what the ability spends. Each returns plain sentences that start with the ability's name.
import { spendOf } from './abilities'
import { unitResourceIds } from './unitResources'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

const isWhole = (n: unknown, lo: number, hi: number): n is number => typeof n === 'number' && Number.isInteger(n) && n >= lo && n <= hi

/** "strike": a weapon that exists and does not charge; target and range left out; the action is used. */
export function strikeChecks(doc: ProjectDoc, a: Ability): string[] {
  const w = a.effect?.strike
  if (w === undefined) return []
  const out: string[] = []
  if (typeof w !== 'string' || w === '') out.push(`${a.name}: pick the weapon it strikes with.`)
  else {
    const weapon = doc.weapons?.find((x) => x.id === w)
    if (!weapon) out.push(`${a.name}: it strikes with “${w}”, which is not a weapon of this game.`)
    else if (weapon.chargeSpeed !== undefined) out.push(`${a.name}: the weapon ${weapon.name} charges by itself. A strike charges through the ability, so take the charge off the weapon (Weapons screen) and give it to the ability.`)
  }
  if (a.target !== undefined || a.range !== undefined) out.push(`${a.name}: a strike takes its targets and reach from the weapon. Leave out the target and the range.`)
  if (a.cost === 'free') out.push(`${a.name}: a strike ends the unit’s turn, so it cannot be free.`)
  return out
}

/** "heal": whole 1 to 999, "full", { percent 1 to 100 } or a formula (text). */
export function healChecks(a: Ability): string[] {
  if (!a.effect || !('heal' in a.effect)) return []
  const h = a.effect.heal
  // A bare word ("ful", "most") is a slip, not a formula: the loader refuses it (a formula reads the units, such as "a.mag * 2").
  if (typeof h === 'string' && h !== 'full' && /^[A-Za-z_]+$/.test(h))
    return [`${a.name}: “${h}” is not a formula. Write “full”, or a formula that reads the units, such as “a.mag * 2”.`]
  const ok = h === 'full' || (typeof h === 'string' && h.trim() !== '') || isWhole(h, 1, 999) ||
    (!!h && typeof h === 'object' && !Array.isArray(h) && Object.keys(h).length === 1 && isWhole(h.percent, 1, 100))
  return ok ? [] : [`${a.name}: healing must be a whole number from 1 to 999, “full”, a percent from 1 to 100, or a formula.`]
}

/** "spend": resource ids of the game (side resources or unit resources), amounts whole 1 to 999. */
export function spendChecks(doc: ProjectDoc, a: Ability): string[] {
  if (a.spend === undefined) return []
  if (typeof a.spend !== 'object' || a.spend === null || Array.isArray(a.spend)) return [`${a.name}: what it costs must list resources with an amount each.`]
  const known = new Set([...(doc.game?.resources ?? []).map((r) => r.id), ...unitResourceIds(doc)])
  const out: string[] = []
  for (const [id, n] of Object.entries(spendOf(a))) {
    if (!known.has(id)) out.push(`${a.name}: it costs “${id}”, which is not a resource of this game (Rules screen: Resources or Unit resources).`)
    if (!isWhole(n, 1, 999)) out.push(`${a.name}: the cost in “${id}” must be a whole number from 1 to 999.`)
  }
  return out
}
