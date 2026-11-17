// Map weapons (docs/SRPG.md "Map weapons"): a structure kind's usable weapon ("A weapon to use here" on the Structures screen), a placed structure's own
// number of uses, and the checks that mirror what the loader refuses (engine/core/data/loader_map_weapon.cpp). Pure functions on the document.
import { sameFootprint } from './places'
import { updateStructure } from './structures'
import type { Item, MapFile, ProjectDoc, Structure, StructureWeaponBy } from './types'

export const WEAPON_USES_MAX = 9999

export interface WeaponCheck { section: 'structures' | 'maps'; /** The structure kind a check is about. */ kind?: string; text: string; mapId?: string; cell?: { x: number; y: number }; note?: boolean }

const q = (s: string) => `“${s}”`

/** The items that can be a map weapon: weapon items. */
export const weaponItems = (doc: ProjectDoc): Item[] => (doc.items ?? []).filter((i) => i.kind === 'weapon')

export const hasWeapon = (s: Structure | undefined): boolean => typeof s?.weapon === 'string'

/** The unit types that may use it: every type when no list, else the types named and those with one of the tags. */
export function byTypeIds(doc: ProjectDoc, s: Structure): string[] | null {
  if (!s.by) return null
  const types = new Set(s.by.types ?? [])
  for (const u of doc.units ?? []) if ((u.tags ?? []).some((t) => (s.by!.tags ?? []).includes(t))) types.add(u.id)
  return [...types]
}

/** Tags used by some unit type (what "by" may name). */
export const unitTags = (doc: ProjectDoc): string[] => [...new Set((doc.units ?? []).flatMap((u) => u.tags ?? []))].sort()

/** Switching the weapon off clears everything that belongs with it (the loader refuses "uses", "by", "ignoreRank" and "becomes" without a weapon). */
export function setWeapon(doc: ProjectDoc, id: string, item: string | undefined): ProjectDoc {
  if (item !== undefined) return updateStructure(doc, id, { weapon: item })
  return updateStructure(doc, id, { weapon: undefined, uses: undefined, by: undefined, ignoreRank: undefined, becomes: undefined })
}

/** The first weapon item of the game (what switching the card on picks), '' when there is none. */
export const starterWeapon = (doc: ProjectDoc): string => weaponItems(doc)[0]?.id ?? ''

export const setWeaponUses = (doc: ProjectDoc, id: string, uses: number | undefined): ProjectDoc =>
  updateStructure(doc, id, { uses: uses === undefined ? undefined : Math.min(WEAPON_USES_MAX, Math.max(1, Math.round(uses))) })

export const setWeaponIgnoreRank = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateStructure(doc, id, { ignoreRank: on ? true : undefined })

export const setWeaponBecomes = (doc: ProjectDoc, id: string, to: string): ProjectDoc => updateStructure(doc, id, { becomes: to === '' ? undefined : to })

/** Who may use it: empty lists remove "by" (anyone who can stand there). */
export function setWeaponBy(doc: ProjectDoc, id: string, types: string[], tags: string[]): ProjectDoc {
  const by: StructureWeaponBy = {}
  if (types.length) by.types = types
  if (tags.length) by.tags = tags
  return updateStructure(doc, id, { by: types.length || tags.length ? by : undefined })
}

/** Kinds it may become when spent: same footprint, not itself. */
export const weaponTwins = (doc: ProjectDoc, s: Structure): Structure[] => (doc.structures ?? []).filter((o) => o.id !== s.id && sameFootprint(o, s))

/** A placed structure's own uses (the key is removed with undefined). */
export function setPlacedUses(map: MapFile, at: { x: number; y: number }, uses: number | undefined): MapFile {
  const list = (map.structures ?? []).map((s) => {
    if (s.x !== at.x || s.y !== at.y) return s
    const { uses: _old, ...rest } = s
    return uses === undefined ? rest : { ...rest, uses: Math.min(WEAPON_USES_MAX, Math.max(1, Math.round(uses))) }
  })
  return { ...map, structures: list }
}

const whole = (v: unknown, lo: number, hi: number): boolean => typeof v === 'number' && Number.isInteger(v) && v >= lo && v <= hi

/** A kind that stops every movement type: nothing can stand on it (the loader refuses a weapon there). */
function nobodyCanStand(doc: ProjectDoc, s: Structure): boolean {
  const types = (doc.game?.moveTypes ?? []).map((m) => m.id)
  return types.length > 0 && types.every((m) => s.moveCost?.[m] === 0)
}

function kindChecks(doc: ProjectDoc, s: Structure, say: (text: string, note?: boolean) => void): void {
  if (s.weapon === undefined) {
    for (const [key, label] of [['uses', 'uses'], ['by', 'who may use it'], ['ignoreRank', 'ignore rank'], ['becomes', 'what it becomes']] as const)
      if ((s as Record<string, unknown>)[key] !== undefined) say(`“${label}” belongs with a weapon to use here, but it has none.`)
    return
  }
  const item = doc.items?.find((i) => i.id === s.weapon)
  if (!item) say(`the weapon ${q(String(s.weapon))} is not an item of this game.`)
  else if (item.kind !== 'weapon') say(`the weapon ${q(item.name)} is not a weapon item.`)
  if (s.uses !== undefined && !whole(s.uses, 1, WEAPON_USES_MAX)) say(`its uses must be a whole number from 1 to ${WEAPON_USES_MAX} (leave it empty for no limit).`)
  if (s.ignoreRank !== undefined && typeof s.ignoreRank !== 'boolean') say('“ignore rank” must be on or off.')
  if (s.by !== undefined) {
    const by = s.by as Record<string, unknown>
    if (!by || typeof by !== 'object' || Array.isArray(by)) say('who may use it is not in a form the game can read.')
    else {
      const types = Array.isArray(by.types) ? by.types.map(String) : []
      const tags = Array.isArray(by.tags) ? by.tags.map(String) : []
      if (!types.length && !tags.length) say('the list of who may use it is empty. Name unit types or tags, or leave it out for any unit.')
      for (const t of types) if (!doc.units?.some((u) => u.id === t)) say(`who may use it names the unit type ${q(t)}, which is not in this game.`)
      for (const t of tags) if (!unitTags(doc).includes(t)) say(`who may use it names the tag ${q(t)}, which no unit type has.`)
      const ids = byTypeIds(doc, s) ?? []
      const moveOf = (id: string) => doc.units?.find((u) => u.id === id)?.moveType
      if (ids.length && ids.every((id) => { const m = moveOf(id); return m !== undefined && s.moveCost?.[m] === 0 })) say('none of the unit types that may use it can stand on this structure (see “Moving through it”).')
    }
  }
  if (nobodyCanStand(doc, s)) say('it has a weapon but no kind of unit can stand on it (every way of moving is set to “Cannot enter”).')
  if (s.becomes !== undefined) {
    const t = doc.structures?.find((x) => x.id === s.becomes)
    if (!t) say(`what it becomes when the weapon is spent, ${q(String(s.becomes))}, is not a structure kind of this game.`)
    else if (t.id === s.id) say('what it becomes when the weapon is spent is the structure itself. It must become another kind.')
    else if (!sameFootprint(s, t)) say(`what it becomes when the weapon is spent, ${t.name}, has a different size. The two must cover the same squares.`)
  }
}

/** What the loader refuses about map weapons, and a note or two. Structure kinds first, then the placed ones on each map. */
export function mapWeaponChecks(doc: ProjectDoc): WeaponCheck[] {
  const out: WeaponCheck[] = []
  for (const s of doc.structures ?? []) kindChecks(doc, s, (text, note) => out.push({ section: 'structures', kind: s.id, text: `${s.name}: ${text}`, note }))
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    for (const p of m.structures ?? []) {
      if (p.uses === undefined) continue
      const kind = doc.structures?.find((k) => k.id === p.type)
      const where = `The ${kind?.name ?? p.type} at ${p.x + 1}, ${p.y + 1}`
      const say = (text: string) => out.push({ section: 'maps', mapId, cell: { x: p.x, y: p.y }, text: `${where}: ${text}` })
      if (!kind || !hasWeapon(kind)) say('its own uses only belong to a structure whose kind has a weapon to use here.')
      else if (!whole(p.uses, 1, WEAPON_USES_MAX)) say(`its own uses must be a whole number from 1 to ${WEAPON_USES_MAX}.`)
    }
  }
  return out
}

/** The placed structures of a map that have a weapon (the Structures tool shows their uses). */
export const placedWithWeapon = (doc: ProjectDoc, map: MapFile) => (map.structures ?? []).filter((p) => hasWeapon(doc.structures?.find((k) => k.id === p.type)))
