// Doors, chests and visitable places on the Structures screen (docs/SRPG.md section 10). A kind has at most one of the three blocks
// ("door", "chest", "visit"); the form shows each as a part that is switched on. The loader's rules (engine/core/data/loader_places.cpp)
// are kept as refusals here: an edit that would write a file the loader refuses is not made, and the screen greys the same choice out
// with the reason. Placing them on a map is mapPlaces.ts; the checks are placeChecks.ts; clean-up after a delete is placeRefs.ts.
import { patchObj, setNum } from './util'
import { ringsOf, sizeOf, updateStructure } from './structures'
import type { ChestBlock, DoorBlock, Item, OpenWay, PlaceFrom, ProjectDoc, Structure, VisitBlock } from './types'

export type PlaceKind = 'door' | 'chest' | 'visit'
export const PLACE_KINDS: PlaceKind[] = ['door', 'chest', 'visit']
export const DOOR_HP_MAX = 9999
export const DEFAULT_DOOR_HP = 20
export type PlaceBlock = DoorBlock | ChestBlock | VisitBlock

const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v)

/** Which of the three a kind is (the first it has), or null for an ordinary structure. */
export function placeKindOf(s: Structure | undefined): PlaceKind | null {
  if (!s) return null
  return PLACE_KINDS.find((k) => isObj((s as Record<string, unknown>)[k])) ?? null
}

export const placeBlockOf = (s: Structure | undefined): PlaceBlock | undefined => {
  const k = placeKindOf(s)
  return k ? (s![k] as PlaceBlock) : undefined
}

/** The same footprint (the loader compares width, height and rings): a door opens into a kind of the same size. */
export const sameFootprint = (a: Structure, b: Structure): boolean => {
  const sa = sizeOf(a), sb = sizeOf(b)
  return ringsOf(a) === ringsOf(b) && sa.w === sb.w && sa.h === sb.h
}

/** The other kinds a door / chest / place may become: same footprint, not itself. */
export const twinKinds = (doc: ProjectDoc, s: Structure): Structure[] => (doc.structures ?? []).filter((o) => o.id !== s.id && sameFootprint(o, s))

export const kindName = (doc: ProjectDoc, id: string): string => doc.structures?.find((s) => s.id === id)?.name ?? id

/** True when the kind stops at least one movement type (cost 0): what makes a shut door a door. */
export const blocksSomeone = (s: Structure): boolean => Object.values(s.moveCost ?? {}).some((c) => c === 0)

// --- the three parts -----------------------------------------------------------------------------------------------------------

export type PartBlock = '' | 'other' | 'twin'
/** Why a part cannot be switched on now: another of the three is on ('other'), a door or chest has no other kind of the same size to open
 *  into ('twin'), or '' when it can. A part that is on is never blocked (it can be switched off). */
export function partBlock(doc: ProjectDoc, s: Structure, part: PlaceKind): PartBlock {
  const cur = placeKindOf(s)
  if (cur === part) return ''
  if (cur) return 'other'
  if (part !== 'visit' && twinKinds(doc, s).length === 0) return 'twin'
  return ''
}

/** Switches a part on (a new block that is valid straight away) or off (the block goes; the kinds it pointed at stay). A door that stops
 *  nobody is refused by the loader, so switching a door on also makes the closed look stop every movement type that has no rule yet. */
export function setPlacePart(doc: ProjectDoc, id: string, part: PlaceKind, on: boolean): ProjectDoc {
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return doc
  if (!on) return placeKindOf(s) === part ? updateStructure(doc, id, { [part]: undefined }) : doc
  if (partBlock(doc, s, part) || placeKindOf(s) === part) return doc
  if (part === 'visit') return updateStructure(doc, id, { visit: {} })
  const first = twinKinds(doc, s)[0]
  const block: DoorBlock | ChestBlock = { opens: first.id }
  if (part === 'chest') return updateStructure(doc, id, { chest: block })
  // A shut door stops every movement type that has no rule of its own yet; the maker refines it under "Moving through".
  const cost = { ...(s.moveCost ?? {}) }
  if (!blocksSomeone(s)) for (const m of doc.game?.moveTypes ?? []) if (cost[m.id] === undefined || cost[m.id] < 0) cost[m.id] = 0
  return updateStructure(doc, id, { door: block as DoorBlock, ...(Object.keys(cost).length ? { moveCost: cost } : {}) })
}

/** Changes keys of the kind's block; undefined removes one. */
export function patchPlace(doc: ProjectDoc, id: string, patch: Record<string, unknown>): ProjectDoc {
  const s = doc.structures?.find((x) => x.id === id)
  const k = placeKindOf(s)
  if (!s || !k) return doc
  return updateStructure(doc, id, { [k]: patchObj(s[k] as PlaceBlock, patch) })
}

export const defaultFrom = (part: PlaceKind): PlaceFrom => (part === 'visit' ? 'on' : 'either')
export const fromOf = (s: Structure): PlaceFrom => {
  const k = placeKindOf(s)
  const f = k ? (s[k] as { from?: unknown }).from : undefined
  return f === 'on' || f === 'next' || f === 'either' ? f : defaultFrom(k ?? 'door')
}
/** The default is not written. */
export function setPlaceFrom(doc: ProjectDoc, id: string, from: PlaceFrom): ProjectDoc {
  const k = placeKindOf(doc.structures?.find((x) => x.id === id))
  return k ? patchPlace(doc, id, { from: from === defaultFrom(k) ? undefined : from }) : doc
}

/** A door or chest is locked unless it says false. */
export const lockedByDefault = (s: Structure): boolean => {
  const k = placeKindOf(s)
  return !(k && (k === 'door' || k === 'chest') && (s[k] as { locked?: unknown }).locked === false)
}
export const setLockedByDefault = (doc: ProjectDoc, id: string, locked: boolean): ProjectDoc => patchPlace(doc, id, { locked: locked ? undefined : false })

/** The kind a door / chest becomes when opened. */
export const setOpensKind = (doc: ProjectDoc, id: string, to: string): ProjectDoc => patchPlace(doc, id, { opens: to })

// --- what opens a door or chest ------------------------------------------------------------------------------------------------

export const WAYS: OpenWay[] = ['key', 'trait', 'break']

/** The ways a unit can open it: the block's `by` list, or the default (a key and a trait, and breaking when the door has hit points). */
export function waysOf(s: Structure): OpenWay[] {
  const k = placeKindOf(s)
  if (k !== 'door' && k !== 'chest') return []
  const b = s[k] as DoorBlock
  if (Array.isArray(b.by)) return WAYS.filter((w) => b.by!.includes(w))
  return k === 'door' && typeof b.hp === 'number' ? ['key', 'trait', 'break'] : ['key', 'trait']
}

/** Sets the ways. "Breaking it down" is the door's hit points (switching it on gives it some, off removes them and what its damage reads),
 *  and a list of keys makes no sense without "a key". The list is written only when it is not what the loader would assume. */
export function setWays(doc: ProjectDoc, id: string, ways: OpenWay[]): ProjectDoc {
  const s = doc.structures?.find((x) => x.id === id)
  const k = placeKindOf(s)
  if (!s || (k !== 'door' && k !== 'chest')) return doc
  const b = s[k] as DoorBlock
  const w = WAYS.filter((x) => ways.includes(x) && (x !== 'break' || k === 'door'))
  const patch: Record<string, unknown> = {}
  if (k === 'door') {
    const hp = w.includes('break') ? (typeof b.hp === 'number' ? b.hp : DEFAULT_DOOR_HP) : undefined
    patch.hp = hp
    if (hp === undefined) patch.stats = undefined
  }
  if (!w.includes('key')) patch.keys = undefined
  const dflt: OpenWay[] = k === 'door' && w.includes('break') ? ['key', 'trait', 'break'] : ['key', 'trait']
  patch.by = w.length === dflt.length && dflt.every((x) => w.includes(x)) ? undefined : w
  return patchPlace(doc, id, patch)
}

export const setDoorHp = (doc: ProjectDoc, id: string, hp: number): ProjectDoc =>
  patchPlace(doc, id, { hp: Math.min(DOOR_HP_MAX, Math.max(1, Math.round(hp))) })

/** One number its damage formula reads (b.<stat>); empty stats are not written. */
export function setDoorStat(doc: ProjectDoc, id: string, stat: string, value: number | undefined): ProjectDoc {
  const s = doc.structures?.find((x) => x.id === id)
  if (!s?.door) return doc
  return patchPlace(doc, id, { stats: setNum(s.door.stats, stat, value) })
}

/** The keys that fit this kind: a key item that opens "door" / "chest" (as the kind is) or names the kind. */
export const isKeyItem = (i: Item): boolean => i.kind === 'usable' && Array.isArray(i.opens) && i.opens.length > 0
export function keyFits(i: Item, s: Structure): boolean {
  const k = placeKindOf(s)
  if (!isKeyItem(i) || (k !== 'door' && k !== 'chest')) return false
  return i.opens!.includes(k) || i.opens!.includes(s.id)
}
export const fittingKeys = (doc: ProjectDoc, s: Structure): Item[] => (doc.items ?? []).filter((i) => keyFits(i, s))

/** Only these keys fit; an empty list is not written (then any key that fits does). */
export const setPlaceKeys = (doc: ProjectDoc, id: string, keys: string[]): ProjectDoc => patchPlace(doc, id, { keys: keys.length ? keys : undefined })

// --- a place that is visited ----------------------------------------------------------------------------------------------------

/** "Nothing" (empty) removes the key. A place that changes kind cannot be visited again, so the loader refuses both: choosing one is not offered while the other is on. */
export const setVisitBecomes = (doc: ProjectDoc, id: string, to: string): ProjectDoc => patchPlace(doc, id, { becomes: to === '' ? undefined : to })
export const setVisitRuin = (doc: ProjectDoc, id: string, to: string): ProjectDoc => patchPlace(doc, id, { ruin: to === '' ? undefined : to })
export function setVisitRepeat(doc: ProjectDoc, id: string, repeat: boolean): ProjectDoc {
  const b = doc.structures?.find((x) => x.id === id)?.visit
  if (!b || (repeat && b.becomes !== undefined)) return doc
  return patchPlace(doc, id, { repeat: repeat ? true : undefined })
}

/** Fog of war: units do not see past it (a door that hides what is behind it). */
export const setBlocksSight = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateStructure(doc, id, { blocksSight: on ? true : undefined })

/** Does anything in the game still need this kind to exist as a door / chest / place (for the plain notes)? */
export const placedAnywhere = (doc: ProjectDoc, kindId: string): boolean =>
  Object.values(doc.maps).some((m) => !!m?.structures?.some((s) => s.type === kindId))
