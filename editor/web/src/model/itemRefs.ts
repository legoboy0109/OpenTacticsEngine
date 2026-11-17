// What other parts of a game hold about items, and the clean-up when an item (or a stat, resource or weapon an item
// points at) is deleted. Pure functions returning new documents, so a delete and its clean-up are one undo step.
import { allGear } from './gear'
import { entryId, withoutItem } from './itemGrants'
import { placeItemUsageLines } from './placeRefs'
import { mapWeaponItemUsageLines } from './mapWeaponRefs'
import { cleanItemPromotions, itemPromotionUsageLines } from './promotionRefs'
import { groupRankUsageLines } from './rankRefs'
import { scrubMap, storyUsages } from './storyRefs'
import { dropKey } from './util'
import type { Usage } from './references'
import type { Item, ItemEntry, MapFile, ProjectDoc } from './types'

const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id
const holds = (list: ItemEntry[] | undefined, id: string) => !!list?.some((e) => entryId(e) === id)
const plural = (n: number, one: string, many: string) => (n === 1 ? one : many)

/** Lines for the delete confirmation: who starts with it, who sells it, which map units and storages hold it. */
export function itemUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const u of doc.units ?? []) if (holds(u.items, id)) out.push({ section: 'units', text: `Unit “${u.name}” starts with it` })
  for (const s of doc.structures ?? []) if (s.sells?.includes(id)) out.push({ section: 'structures', text: `Structure “${s.name}” sells it` })
  for (const [mid, m] of Object.entries(doc.maps)) {
    if (!m) continue
    const carriers = (m.units ?? []).filter((u) => holds(u.items, id)).length
    if (carriers) out.push({ section: 'maps', text: `Map “${mapName(doc, mid)}”: ${carriers} placed ${plural(carriers, 'unit carries', 'units carry')} it` })
    for (const s of m.sides ?? []) if (holds(s.storage, id)) out.push({ section: 'maps', text: `Map “${mapName(doc, mid)}”: side “${s.name || s.id}” starts with it in storage` })
  }
  out.push(...itemPromotionUsageLines(doc, id))
  out.push(...placeItemUsageLines(doc, id))
  out.push(...mapWeaponItemUsageLines(doc, id))
  // The last staff of a group takes the starting ranks in that group with it (docs/SRPG.md section 8).
  const group = doc.items?.find((i) => i.id === id && i.kind === 'staff')?.group
  if (group && !allGear(doc).some((g) => !(g.owner === 'item' && g.id === id) && g.keys.group === group)) out.push(...groupRankUsageLines(doc, group))
  // Events and objectives that give it, check for it or add units carrying it.
  out.push(...storyUsages(doc, [{ kind: 'item', id }]))
  return out
}

/** Takes the item out of every list. A placed unit that is left with no items keeps an empty list (it now has none). */
export function cleanItem(doc: ProjectDoc, id: string): ProjectDoc {
  return cleanItemPromotions(cleanItemLists(doc, id), id)
}

function cleanItemLists(doc: ProjectDoc, id: string): ProjectDoc {
  const maps: ProjectDoc['maps'] = {}
  for (const [mid, m] of Object.entries(doc.maps)) maps[mid] = m === null ? null : scrubMap(doc, cleanMap(m, id), [{ kind: 'item', id }])
  return {
    ...doc,
    units: doc.units && doc.units.map((u) => (holds(u.items, id) ? dropList(u, 'items', withoutItem(u.items!, id)) : u)),
    structures: doc.structures && doc.structures.map((s) => (s.sells?.includes(id) ? dropList(s, 'sells', s.sells.filter((x) => x !== id)) : s)),
    maps,
  }
}

function cleanMap(m: MapFile, id: string): MapFile {
  const unitsHold = m.units?.some((u) => holds(u.items, id))
  const sidesHold = m.sides?.some((s) => holds(s.storage, id))
  if (!unitsHold && !sidesHold) return m
  return {
    ...m,
    units: m.units && m.units.map((u) => (holds(u.items, id) ? { ...u, items: withoutItem(u.items!, id) } : u)),
    sides: m.sides && m.sides.map((s) => (holds(s.storage, id) ? dropList(s, 'storage', withoutItem(s.storage!, id)) : s)),
  }
}

/** A type's own list or a side's storage that ends up empty loses its key (nothing is then the same as no list). */
function dropList<T extends object>(o: T, key: string, list: unknown[]): T {
  const c: Record<string, unknown> = { ...(o as Record<string, unknown>) }
  if (list.length) c[key] = list; else delete c[key]
  return c as T
}

/** A deleted stat leaves what items raise or give. */
export function cleanStatInItems(doc: ProjectDoc, stat: string): ProjectDoc['items'] {
  return sameIfUnchanged(doc.items, (i) => {
    const hit = (i.raise && stat in i.raise) || (i.bonus && stat in i.bonus)
    if (!hit) return i
    const c: Record<string, unknown> = { ...i }
    for (const f of ['raise', 'bonus'] as const) {
      if (!i[f] || !(stat in i[f]!)) continue
      const v = dropKey(i[f], stat)
      if (v) c[f] = v; else delete c[f]
    }
    return c as unknown as typeof i
  })
}

/** A deleted resource leaves every item's price. */
export function cleanResourceInItems(doc: ProjectDoc, resource: string): ProjectDoc['items'] {
  return sameIfUnchanged(doc.items, (i) => {
    if (!i.price || !(resource in i.price)) return i
    const c: Record<string, unknown> = { ...i }
    const v = dropKey(i.price, resource)
    if (v === undefined) delete c.price; else c.price = v
    return c as unknown as typeof i
  })
}

/** Maps the list, but hands back the very same list when nothing changed (so the edit is not seen as a change). */
function sameIfUnchanged(list: ProjectDoc['items'], fn: (i: Item) => Item): ProjectDoc['items'] {
  if (!list) return list
  const next = list.map(fn)
  return next.some((x, k) => x !== list[k]) ? next : list
}
