// Selling items back (docs/EVENTS.md "Preparation", docs/ITEMS.md): an item's "sellable" switch, the game's sell fraction
// (`rules.items.sellPercent`) and the preparation shop's "buys". Defaults are never written. The checks mirror what the
// game loader refuses (engine/core/data/loader_items.cpp, loader.cpp, loader_prep.cpp).
import { withGame } from './doc'
import { updateItem } from './items'
import type { ItemCheck } from './itemChecks'
import type { MapFile, ProjectDoc } from './types'

export const SELL_PERCENT_DEFAULT = 50
export const SELL_PERCENT_MIN = 0
export const SELL_PERCENT_MAX = 100

/** "Cannot be sold" is written as `sellable: false`; the default (it can be) is not written. */
export const setSellable = (doc: ProjectDoc, id: string, can: boolean): ProjectDoc => updateItem(doc, id, { sellable: can ? undefined : false })

export const sellPercent = (doc: ProjectDoc): number => doc.game?.rules?.items?.sellPercent ?? SELL_PERCENT_DEFAULT

/** A whole number from 0 to 100; the default (50) removes the key (and an all-default `rules.items` goes with it). */
export function setSellPercent(doc: ProjectDoc, n: number): ProjectDoc {
  const value = Math.min(SELL_PERCENT_MAX, Math.max(SELL_PERCENT_MIN, Math.round(n)))
  return withGame(doc, (g) => {
    const next = { ...(g.rules?.items ?? {}), sellPercent: value }
    if (value === SELL_PERCENT_DEFAULT) delete (next as { sellPercent?: number }).sellPercent
    const rules = { ...(g.rules ?? {}) }
    if (Object.keys(next).length) rules.items = next
    else delete rules.items
    return { ...g, rules }
  })
}

/** The preparation shop buys items back unless `buys` says false. */
export const shopBuys = (map: MapFile): boolean => map.prep?.shop?.buys !== false

/** `buys: false` is written for "no"; the default (it buys) is not written. A map without a shop is left alone. */
export function setShopBuys(map: MapFile, buys: boolean): MapFile {
  const prep = map.prep
  if (!prep?.shop) return map
  const { buys: _old, ...shop } = prep.shop
  return { ...map, prep: { ...prep, shop: buys ? shop : { ...shop, buys: false } } }
}

/** Does any map's preparation shop buy items? (The item form offers "Can be sold" then, or when an item already says no.) */
export const gameHasBuyingShop = (doc: ProjectDoc): boolean =>
  Object.values(doc.maps ?? {}).some((m) => !!m?.prep?.shop && m.prep.shop.buys !== false)

export function sellChecks(doc: ProjectDoc): ItemCheck[] {
  const out: ItemCheck[] = []
  const pct = doc.game?.rules?.items?.sellPercent as unknown
  if (pct !== undefined && (typeof pct !== 'number' || !Number.isInteger(pct) || pct < SELL_PERCENT_MIN || pct > SELL_PERCENT_MAX))
    out.push({ section: 'items', text: 'The share of an item’s price that a shop pays when it buys (Rules, Items) must be a whole number from 0 to 100. The game will not load until it is.' })
  const drops = doc.game?.rules?.items?.drops as unknown
  if (drops !== undefined && drops !== 'marked' && drops !== 'last')
    out.push({ section: 'items', text: 'What a defeated unit drops (Rules, Items) must be every marked item or only its last item. The game will not load until it is.' })
  for (const i of doc.items ?? [])
    if (i.sellable !== undefined && typeof i.sellable !== 'boolean') out.push({ section: 'items', text: `${i.name}: whether it can be sold must be on or off.` })
  return out
}
