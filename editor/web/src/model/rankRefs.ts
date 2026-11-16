// What weapon ranks and stat caps (docs/SRPG.md section 7) leave behind in other places, and the clean-up when a rank, a weapon
// group, a stat or a whole feature goes: weapons that need a rank, starting ranks on unit types, characters and placed units, caps and
// cap adjustments. Pure functions returning new documents, so a delete and its clean-up are one undo step. An untouched part keeps
// its identity (so it is not saved again).
import type { Usage } from './references'
import type { RankMap } from './srpgTypes'
import type { MapFile, NumMap, ProjectDoc, Weapon } from './types'

const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id
const plural = (n: number, one: string, many: string) => (n === 1 ? one : many)

/** Same list when nothing changed. */
function mapKeep<T>(list: T[] | null | undefined, fn: (t: T) => T): T[] | null {
  if (!list) return null
  const next = list.map(fn)
  return next.some((x, i) => x !== list[i]) ? next : list
}
/** `o` with its `key` replaced by `value`, or removed when undefined; the same object when it was already so. */
function withKey<T extends object>(o: T, key: string, value: unknown): T {
  const cur = (o as Record<string, unknown>)[key]
  if (cur === value) return o
  const c: Record<string, unknown> = { ...(o as Record<string, unknown>) }
  if (value === undefined) delete c[key]; else c[key] = value
  return c as T
}

type Edit<M> = (m: M | undefined) => M | undefined
/** Runs `fn` over every starting-ranks map: unit types, characters, and units placed on the maps. */
function everyRankMap(doc: ProjectDoc, fn: Edit<RankMap>): ProjectDoc {
  const fix = <T extends object>(o: T): T => {
    const cur = (o as { ranks?: RankMap }).ranks
    if (cur === undefined) return o
    const next = fn(cur)
    return next === cur ? o : withKey(o, 'ranks', next)
  }
  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(doc.maps)) {
    const units = m && mapKeep(m.units, fix)
    maps[id] = m && units && units !== m.units ? ({ ...m, units } as MapFile) : m
  }
  return { ...doc, units: mapKeep(doc.units, fix), characters: mapKeep(doc.characters, fix), maps }
}
const trim = (m: RankMap): RankMap | undefined => (Object.keys(m).length ? m : undefined)
const dropWhere = (m: RankMap | undefined, gone: (group: string, v: RankMap[string]) => boolean): RankMap | undefined => {
  if (!m) return m
  const kept = Object.fromEntries(Object.entries(m).filter(([g, v]) => !gone(g, v)))
  return Object.keys(kept).length === Object.keys(m).length ? m : trim(kept)
}

// --- a rank goes or changes its id -------------------------------------------------------------------------------------

const editWeapons = (doc: ProjectDoc, fn: (w: Weapon) => Weapon): ProjectDoc => ({ ...doc, weapons: mapKeep(doc.weapons, fn) })

/** The rank is removed: a weapon that needed it needs nothing; a class, character or placed unit that started in it loses that entry
 *  (so it starts at the lowest rank, or at what the class says). */
export function rankRefsOnRemove(doc: ProjectDoc, id: string): ProjectDoc {
  return everyRankMap(editWeapons(doc, (w) => (w.rank === id ? withKey(w, 'rank', undefined) : w)), (m) => dropWhere(m, (_g, v) => v === id))
}

/** The rank's id changes: everything that named it follows. */
export function renameRankRefs(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const moved = editWeapons(doc, (w) => (w.rank === from ? { ...w, rank: to } : w))
  return everyRankMap(moved, (m) => (m && Object.values(m).includes(from) ? Object.fromEntries(Object.entries(m).map(([g, v]) => [g, v === from ? to : v])) : m))
}

// --- a weapon group goes or is renamed --------------------------------------------------------------------------------------

/** Starting ranks of a group no weapon has any more are dropped (the loader refuses them). */
export function dropGroupRanks(doc: ProjectDoc, group: string): ProjectDoc {
  return everyRankMap(doc, (m) => dropWhere(m, (g) => g === group))
}

/** A group's name changes on its only weapon: units that wield it and every starting rank in it follow. */
export function renameGroupRefs(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const renamed = everyRankMap(doc, (m) => (m && from in m ? Object.fromEntries(Object.entries(m).map(([g, v]) => [g === from ? to : g, v])) : m))
  return { ...renamed, units: mapKeep(renamed.units, (u) => (u.wields?.includes(from) ? { ...u, wields: u.wields.map((g) => (g === from ? to : g)) } : u)) }
}

// --- a feature goes -------------------------------------------------------------------------------------------------------

/** Weapon ranks switched off: no weapon needs a rank, no starting ranks are left. */
export function cleanAllRanks(doc: ProjectDoc): ProjectDoc {
  const noWeaponRanks = editWeapons(doc, (w) => (w.rank !== undefined || w.rankXp !== undefined ? withKey(withKey(w, 'rank', undefined), 'rankXp', undefined) : w))
  return everyRankMap(noWeaponRanks, () => undefined)
}

/** Stat caps switched off: no class cap and no cap adjustment is left (a class cap works even without the game-wide file, so it goes too). */
export function cleanAllCaps(doc: ProjectDoc): ProjectDoc {
  return { ...doc, units: mapKeep(doc.units, (u) => withKey(u, 'caps', undefined)), characters: mapKeep(doc.characters, (c) => withKey(c, 'capAdjust', undefined)) }
}

/** A stat is deleted: its game-wide cap, class caps and character adjustments go with it. */
export function cleanStatCaps(doc: ProjectDoc, stat: string): ProjectDoc {
  const drop = (m: NumMap | undefined): NumMap | undefined => {
    if (!m || !(stat in m)) return m
    const { [stat]: _gone, ...rest } = m
    return Object.keys(rest).length ? rest : undefined
  }
  return {
    ...doc,
    caps: doc.caps && doc.caps.caps && stat in doc.caps.caps ? { ...doc.caps, caps: drop(doc.caps.caps) ?? {} } : doc.caps,
    units: mapKeep(doc.units, (u) => withKey(u, 'caps', drop(u.caps))),
    characters: mapKeep(doc.characters, (c) => withKey(c, 'capAdjust', drop(c.capAdjust))),
  }
}

// --- "used by" lines for the delete questions ---------------------------------------------------------------------------------

function rankLines(doc: ProjectDoc, test: (m: RankMap) => boolean, verb: string): Usage[] {
  const out: Usage[] = []
  for (const u of doc.units ?? []) if (u.ranks && test(u.ranks)) out.push({ section: 'units', text: `Unit “${u.name}” ${verb}` })
  for (const c of doc.characters ?? []) if (c.ranks && test(c.ranks)) out.push({ section: 'characters', text: `Character “${c.name}” ${verb}` })
  for (const [mid, m] of Object.entries(doc.maps)) {
    const n = (m?.units ?? []).filter((u) => u.ranks && test(u.ranks)).length
    if (n) out.push({ section: 'maps', text: `Map “${mapName(doc, mid)}”: ${n} placed ${plural(n, 'unit has', 'units have')} its own rank for it` })
  }
  return out
}

/** Who needs this rank or starts in it. */
export function rankUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = (doc.weapons ?? []).filter((w) => w.rank === id).map((w) => ({ section: 'weapons' as const, text: `Weapon “${w.name}” needs this rank (it then needs none)` }))
  return [...out, ...rankLines(doc, (m) => Object.values(m).includes(id), 'starts in this rank (it then starts at the lowest rank)')]
}

/** Who has a starting rank in a weapon group: shown when the last weapon of the group is deleted. */
export function groupRankUsageLines(doc: ProjectDoc, group: string): Usage[] {
  return rankLines(doc, (m) => group in m, `has a starting rank in “${group}” (it goes, since no weapon is left in that group)`)
}

/** Who has a cap for the stat (all go when the stat is deleted). */
export function statCapUsageLines(doc: ProjectDoc, stat: string): Usage[] {
  const out: Usage[] = []
  if (doc.caps?.caps && stat in doc.caps.caps) out.push({ section: 'rules', text: 'The game-wide cap for it (Rules)' })
  const unitNames = (doc.units ?? []).filter((u) => u.caps && stat in u.caps).map((u) => u.name)
  if (unitNames.length) out.push({ section: 'units', text: `${unitNames.length} ${plural(unitNames.length, 'unit has', 'units have')} its own cap for it: ${unitNames.slice(0, 5).join(', ')}${unitNames.length > 5 ? ` and ${unitNames.length - 5} more` : ''}` })
  for (const c of doc.characters ?? []) if (c.capAdjust && stat in c.capAdjust) out.push({ section: 'characters', text: `Character “${c.name}” adjusts its cap` })
  return out
}
