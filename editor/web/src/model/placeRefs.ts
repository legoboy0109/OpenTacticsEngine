// What doors, chests, places and keys hold about other parts of the game (docs/SRPG.md section 10), and the follow-through when one of those is deleted:
//  - a structure kind: a door or chest that opens into it stops being one (the Door / Chest part is switched off), a place loses its "becomes" / "ruin", a key
//    stops listing it;
//  - an item: a door's or chest's list of keys, the contents of placed chests, an unlock ability's lockpick;
//  - a resource: the amounts in placed chests.
// `reconcilePlaces` is run by the document store after EVERY edit (like reconcileEconomy), so no delete path can forget it and the delete and its clean-up are one
// Undo step. A story name that disappears with a placed structure is NOT scrubbed from the events (that would silently delete a maker's events): the checks flag
// an event that names a place that is no longer there. Renaming a story name follows (mapPlaces.ts changePlaceId).
import { placeIsNamedByEvents } from './mapPlaces'
import { placeBlockOf, placeKindOf } from './places'
import type { Usage } from './references'
import type { ChestLoot, Item, MapFile, ProjectDoc, Structure } from './types'

const idsOf = (list: Array<{ id: string }> | null | undefined): Set<string> => new Set((list ?? []).map((x) => x.id))
const gone = (before: Array<{ id: string }> | null | undefined, after: Array<{ id: string }> | null | undefined): Set<string> => {
  const now = idsOf(after)
  return new Set([...idsOf(before)].filter((id) => !now.has(id)))
}
const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id
const lootItem = (e: ChestLoot): string | undefined => (typeof e === 'string' ? e : 'item' in e ? e.item : undefined)
const lootResource = (e: ChestLoot): string | undefined => (typeof e === 'object' && 'resource' in e ? e.resource : undefined)

function dropKind(s: Structure, kinds: Set<string>): Structure {
  const k = placeKindOf(s)
  if (!k) return s
  const b = placeBlockOf(s) as { opens?: string; becomes?: string; ruin?: string }
  const c: Record<string, unknown> = { ...s }
  if ((k === 'door' || k === 'chest') && b.opens !== undefined && kinds.has(b.opens)) { delete c[k]; if (k === 'door') delete c.blocksSight; return c as unknown as Structure }
  if (k === 'visit' && ((b.becomes !== undefined && kinds.has(b.becomes)) || (b.ruin !== undefined && kinds.has(b.ruin)))) {
    const nb: Record<string, unknown> = { ...b }
    if (b.becomes !== undefined && kinds.has(b.becomes)) delete nb.becomes
    if (b.ruin !== undefined && kinds.has(b.ruin)) delete nb.ruin
    c.visit = nb
    return c as unknown as Structure
  }
  return s
}

function dropItems(s: Structure, items: Set<string>): Structure {
  const k = placeKindOf(s)
  if (k !== 'door' && k !== 'chest') return s
  const b = s[k]!
  if (!b.keys?.some((x) => items.has(x))) return s
  const keys = b.keys.filter((x) => !items.has(x))
  const nb: Record<string, unknown> = { ...b }
  if (keys.length) nb.keys = keys; else delete nb.keys
  return { ...s, [k]: nb } as Structure
}

function cleanMapLoot(m: MapFile, items: Set<string>, resources: Set<string>): MapFile {
  if (!m.structures?.some((s) => s.contents?.some((e) => items.has(lootItem(e) ?? '') || resources.has(lootResource(e) ?? '')))) return m
  return {
    ...m,
    structures: m.structures.map((s) => {
      if (!s.contents?.some((e) => items.has(lootItem(e) ?? '') || resources.has(lootResource(e) ?? ''))) return s
      const contents = s.contents.filter((e) => !items.has(lootItem(e) ?? '') && !resources.has(lootResource(e) ?? ''))
      const { contents: _old, ...rest } = s
      return contents.length ? { ...rest, contents } : rest
    }),
  }
}

/** Takes what points at a deleted kind, item or resource out of doors, chests, places, keys, lockpicks and placed chests; `after` itself when nothing did. */
export function reconcilePlaces(before: ProjectDoc, after: ProjectDoc): ProjectDoc {
  if (before === after) return after
  const kinds = gone(before.structures, after.structures)
  const items = gone(before.items, after.items)
  const resourcesGone = gone(before.game?.resources, after.game?.resources)
  if (!kinds.size && !items.size && !resourcesGone.size) return after
  let structures = after.structures
  if (structures && (kinds.size || items.size)) {
    const next = structures.map((s) => dropItems(dropKind(s, kinds), items))
    if (next.some((s, i) => s !== structures![i])) structures = next
  }
  let list = after.items
  if (list && kinds.size) {
    const next = list.map((i): Item => {
      if (!i.opens?.some((w) => kinds.has(w))) return i
      const opens = i.opens.filter((w) => !kinds.has(w))
      const { opens: _old, ...rest } = i
      return (opens.length ? { ...rest, opens } : rest) as Item
    })
    if (next.some((i, n) => i !== list![n])) list = next
  }
  let abilities = after.abilities
  if (abilities && items.size) {
    const next = abilities.map((a) => {
      const u = a.effect?.unlock
      if (!u || typeof u !== 'object' || Array.isArray(u) || !u.needs || !items.has(u.needs)) return a
      const { needs: _n, ...rest } = u
      const k = rest.kinds
      const simple = Array.isArray(k) && k.length === 1 ? k : true // the lockpick goes, and the simplest form that says the rest is written
      return { ...a, effect: { ...a.effect, unlock: simple } } as typeof a
    })
    if (next.some((a, n) => a !== abilities![n])) abilities = next
  }
  let maps = after.maps
  if (items.size || resourcesGone.size) {
    const next: ProjectDoc['maps'] = {}
    let changed = false
    for (const [id, m] of Object.entries(after.maps)) {
      const c = m ? cleanMapLoot(m, items, resourcesGone) : m
      if (c !== m) changed = true
      next[id] = c
    }
    if (changed) maps = next
  }
  if (structures === after.structures && list === after.items && abilities === after.abilities && maps === after.maps) return after
  return { ...after, structures, items: list, abilities, maps }
}

// --- "used by" lines for the delete confirmations ----------------------------------------------------------------------------------

/** What deleting a structure kind changes in doors, places and keys (and the events that name its placed ones). */
export function placeKindUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const s of doc.structures ?? []) {
    if (s.id === id) continue
    const k = placeKindOf(s)
    const b = placeBlockOf(s) as { opens?: string; becomes?: string; ruin?: string } | undefined
    if ((k === 'door' || k === 'chest') && b?.opens === id) out.push({ section: 'structures', text: `${k === 'door' ? 'Door' : 'Chest'} “${s.name}” opens into it (it stops being a ${k})` })
    if (k === 'visit' && b?.becomes === id) out.push({ section: 'structures', text: `Place “${s.name}” becomes it after a visit (that setting is cleared)` })
    if (k === 'visit' && b?.ruin === id) out.push({ section: 'structures', text: `Place “${s.name}” becomes it when destroyed (that setting is cleared)` })
  }
  for (const i of doc.items ?? []) if (i.opens?.includes(id)) out.push({ section: 'items', text: `Key “${i.name}” opens it (it is taken off the key’s list${i.opens.length === 1 ? '; the item then does nothing' : ''})` })
  for (const [mid, m] of Object.entries(doc.maps)) {
    if (!m) continue
    for (const s of m.structures ?? []) {
      if (s.type === id && s.id && placeIsNamedByEvents(m, s.id)) out.push({ section: 'maps', text: `Map “${mapName(doc, mid)}”: an event names “${s.id}”, which goes with it (the event stays and is flagged until you change it)` })
    }
  }
  return out
}

/** What deleting an item changes in doors, chests and lockpicks. */
export function placeItemUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const s of doc.structures ?? []) {
    const k = placeKindOf(s)
    if ((k === 'door' || k === 'chest') && s[k]!.keys?.includes(id)) out.push({ section: 'structures', text: `${k === 'door' ? 'Door' : 'Chest'} “${s.name}” lists it as a key that fits (it is taken off the list)` })
  }
  for (const [mid, m] of Object.entries(doc.maps)) {
    for (const s of m?.structures ?? []) {
      if (s.contents?.some((e) => lootItem(e) === id)) out.push({ section: 'maps', text: `Map “${mapName(doc, mid)}”: ${s.id ? `chest “${s.id}”` : `the chest at ${s.x + 1}, ${s.y + 1}`} holds it (it is taken out)` })
    }
  }
  for (const a of doc.abilities ?? []) {
    const u = a.effect?.unlock
    if (u && typeof u === 'object' && !Array.isArray(u) && u.needs === id) out.push({ section: 'abilities', text: `Ability “${a.name}” needs it as a lockpick (that is cleared)` })
  }
  return out
}
