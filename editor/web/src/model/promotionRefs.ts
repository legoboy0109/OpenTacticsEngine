// What promotion, abilities and carrying hold about unit types, items and stats, and the clean-up when one of those is deleted
// (E3 of docs/SRPG.md). Pure functions returning new documents, so a delete and its clean-up are one undo step. The shared
// delete code (references.ts, itemRefs.ts) only calls the four functions at the bottom.
import { summonOf, effectKind } from './abilities'
import { promotionsOf, unlistPromotionItem } from './promotion'
import { deleteAbility } from './srpgRefs'
import type { Usage } from './references'
import type { Promotion } from './srpgTypes'
import type { NumMap, ProjectDoc } from './types'
import { dropKey, patchObj } from './util'

const summonsType = (doc: ProjectDoc, type: string) => (doc.abilities ?? []).filter((a) => effectKind(a) === 'summon' && summonOf(a).type === type)

/** Deleting a unit type: promotions into it and abilities that summon it. */
export function unitPromotionUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const u of doc.units ?? []) {
    const n = promotionsOf(u).filter((p) => p.to === id).length
    if (n) out.push({ section: 'units', text: `Unit “${u.name}” can promote into it (that promotion is removed)` })
  }
  for (const a of summonsType(doc, id)) out.push({ section: 'abilities', text: `Ability “${a.name}” summons it (the ability is deleted too)` })
  return out
}

export function cleanUnitPromotions(doc: ProjectDoc, id: string): ProjectDoc {
  let out: ProjectDoc = {
    ...doc,
    units: doc.units && doc.units.map((u) => {
      if (!promotionsOf(u).some((p) => p.to === id)) return u
      const rest = promotionsOf(u).filter((p) => p.to !== id)
      return patchObj(u, { promotions: rest.length ? rest : undefined })
    }),
  }
  for (const a of summonsType(doc, id)) out = deleteAbility(out, a.id)
  return out
}

/** Deleting an item: promotions that list it. */
export function itemPromotionUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const u of doc.units ?? []) if (promotionsOf(u).some((p) => p.items?.includes(id))) out.push({ section: 'units', text: `Unit “${u.name}” can promote with it (it is taken off that promotion)` })
  return out
}
export const cleanItemPromotions = unlistPromotionItem

/** A deleted stat leaves promotion stat changes, "gives when behind" numbers and the carrying rows. */
export function cleanStatPromotions(doc: ProjectDoc, stat: string): ProjectDoc {
  const gains = (p: Promotion): Promotion => {
    const g = p.gains as NumMap | undefined
    return g && stat in g ? patchObj(p, { gains: dropKey(g, stat) }) : p
  }
  const rescue = doc.game?.rules?.rescue
  const touchesRescue = !!rescue && (!!rescue.pairBonus && stat in rescue.pairBonus || !!rescue.carrierStats && stat in rescue.carrierStats)
  const game = touchesRescue && doc.game
    ? { ...doc.game, rules: { ...doc.game.rules, rescue: patchObj(rescue!, { pairBonus: dropKey(rescue!.pairBonus, stat), carrierStats: dropCarrier(rescue!.carrierStats, stat) }) } }
    : doc.game
  return {
    ...doc,
    game,
    units: doc.units && doc.units.map((u) => {
      const promos = promotionsOf(u).map(gains)
      const changed = promos.some((p, i) => p !== promotionsOf(u)[i])
      const bonus = u.pairBonus && stat in u.pairBonus
      if (!changed && !bonus) return u
      return patchObj(u, { ...(changed ? { promotions: promos } : {}), ...(bonus ? { pairBonus: dropKey(u.pairBonus, stat) } : {}) })
    }),
  }
}

function dropCarrier<T extends Record<string, unknown>>(m: T | undefined, stat: string): T | undefined {
  if (!m || !(stat in m)) return m
  const rest = { ...m }
  delete rest[stat]
  return Object.keys(rest).length ? rest : undefined
}
