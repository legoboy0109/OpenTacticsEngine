// An ability that spends its item (docs/SRPG.md "An ability that spends its item"): which items give an ability, and the checks
// that mirror the loader. An item gives an ability through a skill with `{ "ability": id }` in an `always` block: an accessory's
// skills, or the skills of the weapon a weapon item is.
import type { Ability } from './srpgTypes'
import type { Item, ProjectDoc } from './types'

function skillGives(doc: ProjectDoc, skillId: string, abilityId: string): boolean {
  const skill = (doc.skills ?? []).find((s) => s.id === skillId)
  return (skill?.blocks ?? []).some((b) => b.when === 'always' && Array.isArray(b.do) && b.do.some((e) => e && typeof e === 'object' && (e as { ability?: unknown }).ability === abilityId))
}

/** The items that give the ability. */
export function abilityGivers(doc: ProjectDoc, abilityId: string): Item[] {
  return (doc.items ?? []).filter((it) => {
    if (it.kind === 'accessory') return (it.skills ?? []).some((s) => skillGives(doc, s, abilityId))
    if (it.kind === 'weapon' && it.weapon) {
      const w = (doc.weapons ?? []).find((x) => x.id === it.weapon)
      return (w?.skills ?? []).some((s) => skillGives(doc, s, abilityId))
    }
    return false
  })
}

export function setSpendsItem(a: Ability, on: boolean): Ability {
  const next: Ability = { ...a }
  if (on) next.spendsItem = true
  else delete next.spendsItem
  return next
}

/** Plain problems of "spends its item" (the loader refuses the same things). */
export function spendsItemProblems(doc: ProjectDoc, a: Ability): string[] {
  if (a.spendsItem === undefined) return []
  if (typeof a.spendsItem !== 'boolean') return ['"Spends its item" must be on or off.']
  if (!a.spendsItem) return []
  const out: string[] = []
  const givers = abilityGivers(doc, a.id)
  if (givers.length === 0) out.push('No item gives this ability yet: give a skill with this ability to an accessory or a weapon (Skills and Items screens).')
  for (const it of givers) if (it.uses === undefined) out.push(`"${it.name}" gives it but has unlimited uses, so nothing would be spent. Give the item a number of uses.`)
  for (const t of doc.units ?? []) if ((t.abilities ?? []).includes(a.id)) out.push(`The unit type "${t.name}" has this ability by itself; only an item may give it.`)
  for (const c of doc.characters ?? []) if ((c.abilities ?? []).includes(a.id)) out.push(`${c.name} has this ability by itself; only an item may give it.`)
  return out
}
