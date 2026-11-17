// What map weapons hold about other parts of the game (docs/SRPG.md "Map weapons"), and the follow-through when one of those is deleted:
//  - a weapon item: the structure kinds that fire it lose their weapon (and its uses, who may use it and what it becomes), and so do the placed
//    structures' own uses;
//  - a structure kind: a kind that "becomes" it stays as it is when spent;
//  - a unit type: it leaves the lists of who may use a weapon (a list that is left empty is removed, so any unit may then use it).
// `reconcileMapWeapons` is run by the document store after EVERY edit (like reconcilePlaces), so the delete and its clean-up are one Undo step.
import type { Usage } from './references'
import type { MapFile, ProjectDoc, Structure } from './types'

const idsOf = (list: Array<{ id: string }> | null | undefined): Set<string> => new Set((list ?? []).map((x) => x.id))
const gone = (before: Array<{ id: string }> | null | undefined, after: Array<{ id: string }> | null | undefined): Set<string> => {
  const now = idsOf(after)
  return new Set([...idsOf(before)].filter((id) => !now.has(id)))
}

function dropWeapon(s: Structure): Structure {
  const { weapon: _w, uses: _u, by: _b, ignoreRank: _i, becomes: _c, ...rest } = s
  return rest
}

function dropBecomes(s: Structure): Structure {
  const { becomes: _c, ...rest } = s
  return rest
}

function dropUnitTypes(s: Structure, types: Set<string>): Structure {
  if (!s.by?.types?.some((t) => types.has(t))) return s
  const left = s.by.types.filter((t) => !types.has(t))
  const { types: _t, ...others } = s.by
  const by = left.length ? { ...others, types: left } : others
  const { by: _old, ...rest } = s
  return by.tags?.length || left.length ? { ...rest, by } : rest
}

export function reconcileMapWeapons(before: ProjectDoc, after: ProjectDoc): ProjectDoc {
  if (before === after) return after
  const items = gone(before.items, after.items)
  const kinds = gone(before.structures, after.structures)
  const units = gone(before.units, after.units)
  if (!items.size && !kinds.size && !units.size) return after
  let structures = after.structures
  const loses = new Set<string>()
  if (structures) {
    const next = structures.map((s) => {
      let r = s
      if (r.weapon !== undefined && items.has(r.weapon)) { loses.add(r.id); r = dropWeapon(r) }
      if (r.becomes !== undefined && kinds.has(r.becomes)) r = dropBecomes(r)
      if (units.size) r = dropUnitTypes(r, units)
      return r
    })
    if (next.some((s, i) => s !== structures![i])) structures = next
  }
  let maps = after.maps
  if (loses.size) {
    const mapped: ProjectDoc['maps'] = {}
    for (const [id, m] of Object.entries(after.maps)) {
      mapped[id] = m && m.structures?.some((p) => loses.has(p.type) && p.uses !== undefined)
        ? ({ ...m, structures: m.structures.map((p) => { if (!loses.has(p.type) || p.uses === undefined) return p; const { uses: _u, ...rest } = p; return rest }) } as MapFile)
        : m
    }
    maps = mapped
  }
  return structures === after.structures && maps === after.maps ? after : { ...after, structures, maps }
}

/** Deleting a weapon item: the kinds that fire it. */
export function mapWeaponItemUsageLines(doc: ProjectDoc, id: string): Usage[] {
  return (doc.structures ?? []).filter((s) => s.weapon === id).map((s) => ({ section: 'structures' as const, text: `Structure “${s.name}” fires it as its weapon (it loses its weapon)` }))
}

/** Deleting a structure kind: kinds that become it when their weapon is spent. */
export function mapWeaponKindUsageLines(doc: ProjectDoc, id: string): Usage[] {
  return (doc.structures ?? []).filter((s) => s.becomes === id && s.id !== id).map((s) => ({ section: 'structures' as const, text: `Structure “${s.name}” becomes it when its weapon is spent (it stays as it is instead)` }))
}

/** Deleting a unit type: weapons only it may use. */
export function mapWeaponUnitUsageLines(doc: ProjectDoc, id: string): Usage[] {
  return (doc.structures ?? []).filter((s) => s.by?.types?.includes(id)).map((s) => ({
    section: 'structures' as const,
    text: s.by!.types!.length === 1 && !s.by!.tags?.length
      ? `Structure “${s.name}”: only it may use the weapon (the limit is removed, so any unit may)`
      : `Structure “${s.name}” lists it among who may use the weapon (it is taken off that list)`,
  }))
}
