// Several preparation shops (docs/EVENTS.md "Preparation"): a map's `prep.shops` list of `{ id, name, sells, buys, music }`, beside the
// older single `prep.shop`. Giving both is refused by the game, so the editor keeps exactly one of them. Pure edits; defaults are not written
// (`buys` true, no music). The checks mirror engine/core/data/loader_prep.cpp.
import type { PrepShopEntry } from './prepTypes'
import type { MapFile, ProjectDoc } from './types'

export const MAX_SHOPS = 8

export const isSeveralShops = (map: MapFile): boolean => Array.isArray(map.prep?.shops)
export const shopsOf = (map: MapFile): PrepShopEntry[] => (Array.isArray(map.prep?.shops) ? map.prep!.shops! : [])

const withShops = (map: MapFile, shops: PrepShopEntry[]): MapFile => {
  if (!map.prep) return map
  const { shops: _o, shop: _s, ...rest } = map.prep
  return { ...map, prep: shops.length ? { ...rest, shops } : rest }
}

/** From the single shop (or from none) to a list with one shop in it, named "Shop". */
export function toSeveralShops(map: MapFile): MapFile {
  const prep = map.prep
  if (!prep || isSeveralShops(map)) return map
  const one: PrepShopEntry = { id: 'shop', name: 'Shop', sells: prep.shop?.sells ?? [] }
  if (prep.shop?.buys === false) one.buys = false
  if (prep.shop?.music) one.music = prep.shop.music
  return withShops(map, [one])
}

/** A list with exactly one shop goes back to the single `shop` (its id and name are lost: the single shop has none). */
export function toSingleShop(map: MapFile): MapFile {
  const prep = map.prep
  const list = shopsOf(map)
  if (!prep || list.length !== 1) return map
  const { shops: _o, ...rest } = prep
  const s = list[0]
  return { ...map, prep: { ...rest, shop: { sells: s.sells, ...(s.buys === false ? { buys: false } : {}), ...(s.music ? { music: s.music } : {}) } } }
}

/** A new shop with a free id ("shop2"...) and a free name ("Shop 2"...). */
export function addShop(map: MapFile): MapFile {
  const list = shopsOf(map)
  if (!map.prep || list.length >= MAX_SHOPS) return map
  let n = list.length + 1
  while (list.some((s) => s.id === `shop${n}` || s.name === `Shop ${n}`)) n += 1
  return withShops(map, [...list, { id: `shop${n}`, name: `Shop ${n}`, sells: [] }])
}

export const removeShop = (map: MapFile, i: number): MapFile => withShops(map, shopsOf(map).filter((_, j) => j !== i))

/** Changes keys of one shop; `undefined` removes the key. `id` is never changed (it names the shop for good). */
export function patchShop(map: MapFile, i: number, patch: Partial<Omit<PrepShopEntry, 'id'>>): MapFile {
  const list = shopsOf(map)
  if (!list[i]) return map
  const next: PrepShopEntry = { ...list[i], ...patch }
  for (const k of Object.keys(patch) as Array<keyof typeof patch>) if (patch[k] === undefined) delete next[k]
  if (next.buys === true) delete next.buys  // the default is not written
  return withShops(map, list.map((s, j) => (j === i ? next : s)))
}

/** The music of the single shop; undefined removes it. A map without a single shop is left alone. */
export function setSingleShopMusic(map: MapFile, music: string | undefined): MapFile {
  const prep = map.prep
  if (!prep?.shop) return map
  const { music: _old, ...shop } = prep.shop
  return { ...map, prep: { ...prep, shop: music ? { ...shop, music } : shop } }
}

/** Plain problems of the shops of a map (the loader refuses the same). */
export function shopChecks(doc: ProjectDoc, map: MapFile): string[] {
  const prep = map.prep
  if (!prep) return []
  const out: string[] = []
  if (prep.shop && prep.shops) out.push('The map has both one shop and a list of shops. The game will not load until only one is kept.')
  const list = prep.shops
  if (list === undefined) return out
  if (!Array.isArray(list) || list.length === 0 || list.length > MAX_SHOPS) { out.push(`The preparation shops must be a list of 1 to ${MAX_SHOPS}. The game will not load until they are.`); return out }
  const musicIds = new Set(Object.keys(doc.audio?.music ?? {}))
  list.forEach((s, i) => {
    const who = s.name || s.id || `shop ${i + 1}`
    if (!s.id || !/^[A-Za-z0-9_-]+$/.test(s.id)) out.push(`The shop “${who}” has no usable id (letters, digits, _ and -).`)
    if (list.some((o, j) => j < i && o.id === s.id)) out.push(`The shop id “${s.id}” is used twice. The game will not load until each shop has its own.`)
    if (list.some((o, j) => j < i && o.name === s.name)) out.push(`Two shops are called “${who}”. The game will not load until each has its own name.`)
    if (s.name !== undefined && s.name === '') out.push(`The shop “${s.id}” has an empty name. The game will not load until it has one.`)
    if (s.sells.length === 0) out.push(`The shop “${who}” sells nothing. The game will not load until it sells something or is removed.`)
    for (const id of s.sells) {
      const item = doc.items?.find((x) => x.id === id)
      if (!item) out.push(`The shop “${who}” sells “${id}”, which is not an item of this game.`)
      else if (!item.price || Object.keys(item.price).length === 0) out.push(`The shop “${who}” sells ${item.name ?? id}, which has no price (Items).`)
    }
    if (s.music !== undefined && (typeof s.music !== 'string' || s.music === '')) out.push(`The music of the shop “${who}” must be one of the game’s music tracks.`)
    else if (s.music && !musicIds.has(s.music)) out.push(`The shop “${who}” plays “${s.music}”, which is not a music track of this game (Sound).`)
  })
  return out
}
