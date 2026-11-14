// Checks that mirror what the game loader refuses about items (engine/core/data/loader_items.cpp), so a maker sees
// them before pressing Play. The wording is plain; the loader's own messages still show in the Problems list after a save.
import { entryId, entryUses } from './itemGrants'
import { carriedWeapons, inventorySize, storageMode } from './itemRules'
import type { Item, ItemEntry, ProjectDoc, Section } from './types'

export interface ItemCheck { section: Section; text: string; mapId?: string }

const nonZero = (m: Record<string, number> | undefined) => !!m && Object.values(m).some((v) => v !== 0)

/** Problems with the item list itself. */
function itemListChecks(doc: ProjectDoc): ItemCheck[] {
  const out: ItemCheck[] = []
  const carried = carriedWeapons(doc)
  const stats = new Set((doc.game?.stats ?? []).map((s) => s.id))
  const say = (i: Item, text: string) => out.push({ section: 'items', text: `${i.name}: ${text}` })
  for (const i of doc.items ?? []) {
    if (i.kind === 'usable' && i.heal === undefined && !nonZero(i.raise)) say(i, 'a usable item needs to heal or raise something.')
    if (i.kind === 'accessory' && !nonZero(i.bonus)) say(i, 'an accessory needs a bonus to at least one stat.')
    if (i.kind === 'weapon') {
      if (!carried) say(i, 'weapon items need weapons to be “Carried as items” on the Rules screen.')
      if (!i.weapon || !doc.weapons?.some((w) => w.id === i.weapon)) say(i, i.weapon ? `its weapon “${i.weapon}” does not exist.` : 'pick which weapon it is.')
    }
    for (const stat of [...Object.keys(i.raise ?? {}), ...Object.keys(i.bonus ?? {})])
      if (!stats.has(stat)) say(i, `“${stat}” is not a stat of this game.`)
  }
  return out
}

/** Problems in one list of item entries; `who` starts each message. */
function grantChecks(doc: ProjectDoc, list: ItemEntry[] | undefined, opts: { section: Section; mapId?: string; who: string; max?: number }): ItemCheck[] {
  const out: ItemCheck[] = []
  const say = (text: string) => out.push({ section: opts.section, mapId: opts.mapId, text: `${opts.who}: ${text}` })
  if (!list?.length) return out
  if (opts.max !== undefined && list.length > opts.max) say(`${list.length} items, but a unit can carry only ${opts.max}.`)
  for (const e of list) {
    const id = entryId(e)
    const def = doc.items?.find((x) => x.id === id)
    if (!def) { say(`“${id}” is not an item of this game.`); continue }
    const uses = entryUses(e)
    if (uses !== undefined && def.uses === undefined) say(`${def.name} has unlimited uses, so “uses left” cannot be set.`)
    else if (uses !== undefined && uses > def.uses!) say(`${def.name} has only ${def.uses} uses, but ${uses} are left.`)
  }
  return out
}

export function itemChecks(doc: ProjectDoc): ItemCheck[] {
  const out = itemListChecks(doc)
  const max = inventorySize(doc)
  for (const u of doc.units ?? [])
    out.push(...grantChecks(doc, u.items, { section: 'units', who: `${u.name} starts with`, max }))
  for (const s of doc.structures ?? []) {
    for (const id of s.sells ?? []) {
      const def = doc.items?.find((x) => x.id === id)
      if (!def) out.push({ section: 'structures', text: `${s.name}: sells “${id}”, which is not an item of this game.` })
      else if (!def.price || Object.keys(def.price).length === 0) out.push({ section: 'structures', text: `${s.name}: sells ${def.name}, which has no price.` })
    }
  }
  const storageOff = storageMode(doc) === 'off'
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    for (const u of m.units ?? [])
      out.push(...grantChecks(doc, u.items, { section: 'maps', mapId, who: `${doc.units?.find((t) => t.id === u.type)?.name ?? u.type} at ${u.x + 1},${u.y + 1}`, max }))
    for (const s of m.sides ?? []) {
      if (s.storage?.length && storageOff) out.push({ section: 'maps', mapId, text: `${s.name || s.id}: has starting storage, but the game’s shared storage is Off (Rules screen).` })
      out.push(...grantChecks(doc, s.storage, { section: 'maps', mapId, who: `${s.name || s.id} storage` }))
    }
  }
  return out
}
