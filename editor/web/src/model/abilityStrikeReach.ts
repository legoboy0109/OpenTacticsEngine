// "reach": "equipped" on a strike ability (loader: data/abilities.json): the ability strikes with the strike weapon's damage but at
// the range of the weapon in the user's hand (the strike weapon's own range when the user holds none). Off = the key is left out.
import { patchObj } from './util'
import type { Ability } from './srpgTypes'

export const strikeReachEquipped = (a: Ability): boolean => a.effect?.reach === 'equipped'

/** Turns the reach on or off. Only a strike has one; the weapon (and any other key of the effect) stays. */
export function setStrikeReach(a: Ability, on: boolean): Ability {
  if (typeof a.effect?.strike !== 'string') return a
  return patchObj(a, { effect: patchObj(a.effect, { reach: on ? 'equipped' : undefined }) })
}

/** Mirrors the loader: "reach" only with "strike", and only the value "equipped". */
export function strikeReachChecks(a: Ability): string[] {
  const r = a.effect?.reach
  if (r === undefined) return []
  if (typeof a.effect?.strike !== 'string') return [`${a.name}: “reach” goes only with a strike. Remove it, or make the ability strike with a weapon.`]
  return r === 'equipped' ? [] : [`${a.name}: the reach of a strike can only be “equipped” (the weapon in hand). Leave it out for the weapon’s own reach.`]
}
