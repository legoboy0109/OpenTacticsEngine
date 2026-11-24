// A heal that also cures: "effect": { "heal": ..., "cure": ... } (heal first, then cure). Only this pair may go together.
// It is read as a heal (effectKind), so the target rules are a heal's.
import { effectKind } from './abilities'
import { patchObj } from './util'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

export const alsoCures = (a: Ability): boolean => effectKind(a) === 'heal' && !!a.effect && 'cure' in a.effect

/** On starts from the game's first condition, or every harmful one when there are none yet; off removes the cure and keeps the heal. */
export function setAlsoCures(doc: ProjectDoc, a: Ability, on: boolean): Ability {
  if (effectKind(a) !== 'heal' || alsoCures(a) === on) return a
  const e = { ...a.effect }
  if (on) e.cure = doc.conditions?.[0] ? [doc.conditions[0].id] : 'harmful'
  else delete e.cure
  return patchObj(a, { effect: e })
}
