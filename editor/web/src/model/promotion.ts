// Promotion (docs/SRPG.md section 2): a unit type's `promotions` rows, promotion items, and `rules.promotion`.
// A default is never written (fromLevel 1, atLevel "off", no levelAfter of its own, no gains); keys the editor does not
// know stay. The loader refuses a promotion into the unit type itself, "offer" / "auto" without experience and levels,
// and items that are not promotion items, so the screens only offer what loads.
import { withGame, withUnits } from './doc'
import { moveItem, patchObj, updateById } from './util'
import type { Promotion, PromotionRules } from './srpgTypes'
import type { Item, NumMap, ProjectDoc, Unit } from './types'

export type AtLevel = 'off' | 'offer' | 'auto'
export type LevelAfter = 'reset' | 'keep'

export const promotionsOf = (u: Unit | undefined): Promotion[] => u?.promotions ?? []
export const progressionOn = (doc: ProjectDoc): boolean => doc.game?.rules?.progression?.enabled === true
export const maxLevelOf = (doc: ProjectDoc): number => doc.game?.rules?.progression?.maxLevel ?? 20
/** What a promotion without its own rule does to the level: the game's rule, "reset" when it has none. */
export const gameLevelAfter = (doc: ProjectDoc): LevelAfter => doc.game?.rules?.promotion?.levelAfter ?? 'reset'
export const promotionItems = (doc: ProjectDoc): Item[] => (doc.items ?? []).filter((i) => i.kind === 'usable' && i.promotes === true)

// --- rows of one unit type -------------------------------------------------------------------------------------------

const write = (doc: ProjectDoc, unitId: string, fn: (list: Promotion[]) => Promotion[]): ProjectDoc => {
  const u = doc.units?.find((x) => x.id === unitId)
  if (!u) return doc
  const next = fn(promotionsOf(u))
  return withUnits(doc, (list) => updateById(list, unitId, (x) => patchObj(x, { promotions: next.length ? next : undefined })))
}

/** The first other unit type: a promotion must name one (never the type itself). */
export function addPromotion(doc: ProjectDoc, unitId: string): ProjectDoc {
  const to = (doc.units ?? []).find((u) => u.id !== unitId)?.id
  return to ? write(doc, unitId, (l) => [...l, { to }]) : doc
}
export const removePromotion = (doc: ProjectDoc, unitId: string, i: number): ProjectDoc => write(doc, unitId, (l) => l.filter((_, j) => j !== i))
export const movePromotion = (doc: ProjectDoc, unitId: string, from: number, to: number): ProjectDoc => write(doc, unitId, (l) => moveItem(l, from, to))
export const patchPromotion = (doc: ProjectDoc, unitId: string, i: number, patch: Record<string, unknown>): ProjectDoc =>
  write(doc, unitId, (l) => l.map((p, j) => (j === i ? patchObj(p, patch) : p)))

/** Each setter writes nothing for the default value. */
export const promotionPatches = {
  to: (to: string) => ({ to }),
  fromLevel: (n: number) => ({ fromLevel: n <= 1 ? undefined : n }),
  items: (ids: string[]) => ({ items: ids.length ? ids : undefined }),
  atLevel: (v: AtLevel) => ({ atLevel: v === 'off' ? undefined : v }),
  levelAfter: (v: LevelAfter | 'game') => ({ levelAfter: v === 'game' ? undefined : v }),
}

// --- stat changes ---------------------------------------------------------------------------------------------------------

/** What the engine does when a promotion lists no `gains`: each stat changes by the difference of the two types' base stats. */
export function baseDifference(doc: ProjectDoc, from: string, to: string): NumMap {
  const a = doc.units?.find((u) => u.id === from)?.stats ?? {}
  const b = doc.units?.find((u) => u.id === to)?.stats ?? {}
  const out: NumMap = {}
  for (const s of doc.game?.stats ?? []) {
    const d = (b[s.id] ?? 0) - (a[s.id] ?? 0)
    if (d !== 0) out[s.id] = d
  }
  return out
}

export function setGain(gains: NumMap, stat: string, value: number | undefined): NumMap {
  const next = { ...gains }
  if (value === undefined) delete next[stat]; else next[stat] = value
  return next
}

// --- promotion items ----------------------------------------------------------------------------------------------------------

/** Takes the item out of every promotion that lists it (an item that stops being a promotion item, or is deleted). */
export function unlistPromotionItem(doc: ProjectDoc, itemId: string): ProjectDoc {
  if (!doc.units?.some((u) => promotionsOf(u).some((p) => p.items?.includes(itemId)))) return doc
  return {
    ...doc,
    units: doc.units.map((u) => {
      if (!promotionsOf(u).some((p) => p.items?.includes(itemId))) return u
      const list = promotionsOf(u).map((p) => {
        if (!p.items?.includes(itemId)) return p
        const rest = p.items.filter((x) => x !== itemId)
        return patchObj(p, { items: rest.length ? rest : undefined })
      })
      return { ...u, promotions: list }
    }),
  }
}

export function setItemPromotes(doc: ProjectDoc, itemId: string, on: boolean): ProjectDoc {
  const items = (doc.items ?? []).map((i) => (i.id === itemId ? patchObj(i, { promotes: on ? true : undefined }) : i))
  const next = { ...doc, items }
  return on ? next : unlistPromotionItem(next, itemId)
}

// --- rules.promotion --------------------------------------------------------------------------------------------------------------

/** "Starts again at 1" is the default and is not written; an empty block is removed. */
export function setLevelAfterRule(doc: ProjectDoc, v: LevelAfter): ProjectDoc {
  return withGame(doc, (g) => {
    const rules = { ...(g.rules ?? {}) }
    const block: PromotionRules = { ...(rules.promotion ?? {}) }
    if (v === 'reset') delete block.levelAfter; else block.levelAfter = v
    if (Object.keys(block).length) rules.promotion = block; else delete rules.promotion
    return { ...g, rules }
  })
}
