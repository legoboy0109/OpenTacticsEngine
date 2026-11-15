// Who uses a stat / resource / move type / unit / weapon / terrain, and the
// clean-up that removes those references in the same edit as the delete.
import { dropArtOf } from './graphicsClean'
import { economyUsageLines } from './researchRefs'
import { deleteItem } from './items'
import { cleanResourceInItems, cleanStatInItems } from './itemRefs'
import { readGrid, writeGrid } from './mapGrid'
import { scrubMap, storyUsages } from './storyRefs'
import { cleanMoveTypeInStructures, cleanResourceInStructures, cleanUnitInStructures } from './structuresRefs'
import { dropKey, without } from './util'
import type { MapFile, ProjectDoc, Section, Unit } from './types'

export type RefKind = 'stat' | 'resource' | 'moveType' | 'unit' | 'weapon' | 'terrain'
export interface Usage { section: Section; text: string }

const mapEntries = (doc: ProjectDoc): Array<[string, MapFile]> =>
  Object.entries(doc.maps).filter((e): e is [string, MapFile] => e[1] !== null)

const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id

function mapMembers(doc: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc['maps'] {
  const out: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(doc.maps)) out[id] = m === null ? null : fn(m)
  return out
}

/** Everything that mentions `id`, as plain-language lines. */
export function findUsages(doc: ProjectDoc, kind: RefKind, id: string): Usage[] {
  const out: Usage[] = []
  const units = doc.units ?? []
  switch (kind) {
    case 'stat':
      {
        // One line for all units: a stat is on every unit, so a per-unit list would be long.
        const names = units.filter((u) => (u.stats && id in u.stats) || (u.growths && id in u.growths)).map((u) => u.name)
        if (names.length) out.push({ section: 'units', text: `${names.length} unit${names.length === 1 ? '' : 's'} ${names.length === 1 ? 'has' : 'have'} a value for it: ${names.slice(0, 5).join(', ')}${names.length > 5 ? ` and ${names.length - 5} more` : ''}` })
      }
      for (const [mid, m] of mapEntries(doc))
        for (const mu of m.units ?? []) if (mu.stats && id in mu.stats) out.push({ section: 'maps', text: `Map "${mapName(doc, mid)}": a ${nameOfUnit(units, mu.type)} has its own value` })
      for (const i of doc.items ?? []) if ((i.raise && id in i.raise) || (i.bonus && id in i.bonus)) out.push({ section: 'items', text: `Item "${i.name}" changes it` })
      out.push(...economyUsageLines(doc, 'stat', id))
      break
    case 'resource':
      for (const u of units) if (u.cost && id in u.cost) out.push({ section: 'units', text: `Unit "${u.name}" has a cost in it` })
      for (const t of doc.structures ?? []) if (t.income && id in t.income) out.push({ section: 'structures', text: `Structure "${t.name}" gives it as income` })
      for (const i of doc.items ?? []) if (i.price && id in i.price) out.push({ section: 'items', text: `Item "${i.name}" has a price in it` })
      for (const [mid, m] of mapEntries(doc))
        for (const s of m.sides ?? []) if (s.resources && id in s.resources) out.push({ section: 'maps', text: `Map "${mapName(doc, mid)}": side "${s.name}" starts with some` })
      out.push(...economyUsageLines(doc, 'resource', id))
      out.push(...storyUsages(doc, [{ kind: 'resource', id }]))
      break
    case 'moveType':
      for (const u of units) if (u.moveType === id) out.push({ section: 'units', text: `Unit "${u.name}" moves this way` })
      for (const t of doc.terrain ?? []) if (t.moveCost && id in t.moveCost) out.push({ section: 'terrain', text: `Terrain "${t.name}" has a movement cost for it` })
      for (const t of doc.structures ?? []) if (t.moveCost && id in t.moveCost) out.push({ section: 'structures', text: `Structure "${t.name}" has its own movement rule for it` })
      break
    case 'unit':
      for (const w of doc.weapons ?? []) if (w.versus && id in w.versus && !tagStillUsed(units, id, id)) out.push({ section: 'weapons', text: `Weapon "${w.name}" has an effectiveness row for it` })
      for (const t of doc.structures ?? []) if (t.produces?.includes(id)) out.push({ section: 'structures', text: `Structure "${t.name}" can build it` })
      for (const [mid, m] of mapEntries(doc)) {
        const n = (m.units ?? []).filter((u) => u.type === id).length
        if (n) out.push({ section: 'maps', text: `Map "${mapName(doc, mid)}" has ${n} of them placed` })
      }
      out.push(...economyUsageLines(doc, 'unit', id))
      out.push(...storyUsages(doc, [{ kind: 'type', id }, ...placedIdsOfType(doc, id)]))
      if (hasPicture(doc, 'units', id)) out.push({ section: 'art', text: 'Its picture is cleared (the image file stays, under Art)' })
      break
    case 'weapon':
      for (const u of units) if (u.weapons?.includes(id)) out.push({ section: 'units', text: `Unit "${u.name}" carries it` })
      for (const i of doc.items ?? []) if (i.kind === 'weapon' && i.weapon === id) out.push({ section: 'items', text: `Item "${i.name}" is this weapon (the item is deleted too)` })
      break
    case 'terrain':
      for (const [mid, m] of mapEntries(doc))
        if (Object.values(m.legend).includes(id)) out.push({ section: 'maps', text: `Map "${mapName(doc, mid)}" has tiles of it (they become ${firstOther(doc, id)})` })
      for (const s of doc.game?.styles ?? []) if (s.terrainColors && id in s.terrainColors) out.push({ section: 'game', text: `Style "${s.name}" recolours it` })
      if (hasPicture(doc, 'terrain', id)) out.push({ section: 'art', text: 'Its picture is cleared (the image file stays, under Art)' })
      break
  }
  return out
}

/** The story names of placed units of this type: they go with it, and so does what events say about them. */
const placedIdsOfType = (doc: ProjectDoc, type: string): Array<{ kind: 'unit'; id: string }> =>
  mapEntries(doc).flatMap(([, m]) => (m.units ?? []).filter((u) => u.type === type && u.id).map((u) => ({ kind: 'unit' as const, id: u.id! })))

const hasPicture = (doc: ProjectDoc, kind: 'terrain' | 'units', id: string): boolean =>
  !!doc.graphics && (id in (doc.graphics[kind] ?? {}) || Object.values(doc.graphics.styles ?? {}).some((st) => id in (st[kind] ?? {})))
const nameOfUnit = (units: Unit[], type: string) => units.find((u) => u.id === type)?.name ?? type
const firstOther = (doc: ProjectDoc, id: string) => doc.terrain?.find((t) => t.id !== id)?.name ?? 'nothing'
/** A "versus" key can be a unit id or a tag; keep it if another unit still has that tag. */
const tagStillUsed = (units: Unit[], key: string, removedUnit: string) => units.some((u) => u.id !== removedUnit && u.tags?.includes(key))

export function cleanStat(doc: ProjectDoc, id: string): ProjectDoc {
  const has = (m?: Record<string, number>) => !!m && id in m
  return {
    ...doc,
    units: doc.units && doc.units.map((u) => (has(u.stats) || has(u.growths)
      ? stripKey(stripKey(u, 'stats', dropKey(u.stats, id)), 'growths', dropKey(u.growths, id)) : u)),
    items: cleanStatInItems(doc, id),
    maps: mapMembers(doc, (m) => (m.units?.some((u) => has(u.stats))
      ? { ...m, units: m.units.map((u) => (has(u.stats) ? stripKey(u, 'stats', dropKey(u.stats, id)) : u)) } : m)),
  }
}

/** Objects with an emptied optional map lose the key rather than keep `{}`. */
function stripKey<T extends object>(o: T, key: string, value: unknown): T {
  const c: Record<string, unknown> = { ...(o as Record<string, unknown>) }
  if (value === undefined) delete c[key]; else c[key] = value
  return c as T
}

export function cleanResource(doc: ProjectDoc, id: string): ProjectDoc {
  return {
    ...doc,
    units: doc.units && doc.units.map((u) => (u.cost && id in u.cost ? stripKey(u, 'cost', dropKey(u.cost, id)) : u)),
    // Old (unconverted) terrain may still list income; the structures' list is the live one.
    terrain: doc.terrain && doc.terrain.map((t) => (t.income && id in t.income ? stripKey(t, 'income', dropKey(t.income, id)) : t)),
    structures: cleanResourceInStructures(doc, id),
    items: cleanResourceInItems(doc, id),
    maps: mapMembers(doc, (m0) => {
      const m = m0.sides?.some((s) => s.resources && id in s.resources)
        ? { ...m0, sides: m0.sides.map((s) => (s.resources && id in s.resources ? stripKey(s, 'resources', dropKey(s.resources, id)) : s)) } : m0
      return scrubMap(doc, m, [{ kind: 'resource', id }])
    }),
  }
}

/** Units on the removed move type switch to `reassignTo`; terrain forgets its cost. */
export function cleanMoveType(doc: ProjectDoc, id: string, reassignTo: string): ProjectDoc {
  return {
    ...doc,
    units: doc.units && doc.units.map((u) => (u.moveType === id ? { ...u, moveType: reassignTo } : u)),
    terrain: doc.terrain && doc.terrain.map((t) => (t.moveCost && id in t.moveCost ? stripKey(t, 'moveCost', dropKey(t.moveCost, id)) : t)),
    structures: cleanMoveTypeInStructures(doc, id),
  }
}

export function cleanUnit(doc: ProjectDoc, id: string): ProjectDoc {
  const remaining = (doc.units ?? []).filter((u) => u.id !== id)
  return dropArtOf({
    ...doc,
    weapons: doc.weapons && doc.weapons.map((w) => (w.versus && id in w.versus && !tagStillUsed(remaining, id, id)
      ? stripKey(w, 'versus', dropKey(w.versus, id)) : w)),
    // Old (unconverted) terrain may still list what it builds; the structures' list is the live one.
    terrain: doc.terrain && doc.terrain.map((t) => (t.produces?.includes(id) ? stripKey(t, 'produces', without(t.produces, id)) : t)),
    structures: cleanUnitInStructures(doc, id),
    maps: mapMembers(doc, (m) => {
      if (!m.units?.some((u) => u.type === id)) return scrubMap(doc, m, [{ kind: 'type', id }])
      // The units go, and with them whatever events and objectives said about their story names.
      const gone = m.units.filter((u) => u.type === id && u.id).map((u) => ({ kind: 'unit' as const, id: u.id! }))
      return scrubMap(doc, { ...m, units: m.units.filter((u) => u.type !== id) }, [{ kind: 'type', id }, ...gone])
    }),
  }, 'units', id)
}

/** A weapon item that names the weapon cannot stay, so it goes too (with its own references). */
export function cleanWeapon(doc: ProjectDoc, id: string): ProjectDoc {
  const gone = (doc.items ?? []).filter((i) => i.kind === 'weapon' && i.weapon === id)
  const base = gone.reduce((d, i) => deleteItem(d, i.id), doc)
  return {
    ...base,
    units: base.units && base.units.map((u) => (u.weapons?.includes(id) ? { ...u, weapons: u.weapons.filter((w) => w !== id) } : u)),
  }
}

/** Map squares of the removed terrain become `replacement`; the legend and tiles are rebuilt so no two symbols mean the same terrain. Looks forget its colour. */
export function cleanTerrain(doc: ProjectDoc, id: string, replacement: string): ProjectDoc {
  const game = doc.game && doc.game.styles?.some((s) => s.terrainColors && id in s.terrainColors)
    ? { ...doc.game, styles: updateStyles(doc.game.styles, id) } : doc.game
  const remaining = (doc.terrain ?? []).filter((t) => t.id !== id)
  return dropArtOf({
    ...doc,
    game,
    maps: mapMembers(doc, (m) => {
      if (!Object.values(m.legend).includes(id)) return m
      const swapped = { ...m, legend: Object.fromEntries(Object.entries(m.legend).map(([ch, t]) => [ch, t === id ? replacement : t])) }
      return remaining.length ? writeGrid(swapped, readGrid(swapped, remaining), remaining) : swapped
    }),
  }, 'terrain', id)
}

function updateStyles(styles: NonNullable<ProjectDoc['game']>['styles'], id: string) {
  return (styles ?? []).map((s) => {
    if (!s.terrainColors || !(id in s.terrainColors)) return s
    const rest = { ...s.terrainColors }
    delete rest[id]
    return stripKey(s, 'terrainColors', Object.keys(rest).length ? rest : undefined)
  })
}
