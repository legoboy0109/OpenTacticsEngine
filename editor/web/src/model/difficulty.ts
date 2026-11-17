// Difficulties (docs/DIFFICULTY.md): the game's list (game.json "difficulties"), a map's own numbers, and who a placement is for.
// Defaults are never written: no "change" when it is lowerOnly, no zero level shift, no 100 percent, no empty "enemy". A game
// that does not use difficulties keeps a game.json without the key. The ids are made from a name once and never change.
import { withGame } from './doc'
import type { DifficultyChange, DifficultyDef, DifficultyEnemy, MapDifficulties, MapDifficulty } from './difficultyTypes'
import { makeId } from './ids'
import { scrubDoc } from './storyRefs'
import type { MapFile, MapUnit, ProjectDoc } from './types'
import { moveItem, patchObj, updateById } from './util'

export const CHANGE_RULES: DifficultyChange[] = ['never', 'lowerOnly', 'free']
export const MAX_DIFFICULTIES = 12
export const MAX_LEVEL_SHIFT = 98
export const MAX_PERCENT = 1000
export const MAX_FLAT_STAT = 999

export const difficultyList = (doc: ProjectDoc): DifficultyDef[] => doc.game?.difficulties?.list ?? []
export const hasDifficulties = (doc: ProjectDoc): boolean => difficultyList(doc).length > 0
export const difficultyById = (doc: ProjectDoc, id: string): DifficultyDef | undefined => difficultyList(doc).find((d) => d.id === id)
export const defaultDifficulty = (doc: ProjectDoc): string => doc.game?.difficulties?.default ?? difficultyList(doc)[0]?.id ?? ''
export const changeRule = (doc: ProjectDoc): DifficultyChange => doc.game?.difficulties?.change ?? 'lowerOnly'
export const percentStats = (doc: ProjectDoc): string[] | undefined => doc.game?.difficulties?.percentStats
export const difficultyName = (doc: ProjectDoc, id: string): string => difficultyById(doc, id)?.name ?? id

const newId = (doc: ProjectDoc, name: string): string => makeId(name, difficultyList(doc).map((d) => d.id), { fallback: 'difficulty' })

/** The starting set: one difficulty, "Normal", as the default; or the usual three. */
export function startDifficulties(doc: ProjectDoc, three: boolean): ProjectDoc {
  if (hasDifficulties(doc)) return doc
  const list: DifficultyDef[] = three
    ? [{ id: 'easy', name: 'Easy', description: 'Gentler foes for a relaxed story.', enemy: { levels: -1 } },
       { id: 'normal', name: 'Normal', description: 'The game as its maker meant it.' },
       { id: 'hard', name: 'Hard', description: 'Foes a level stronger, and a little tougher.', enemy: { levels: 1 }, xpPercent: 90 }]
    : [{ id: 'normal', name: 'Normal' }]
  return withGame(doc, (g) => ({ ...g, difficulties: { default: 'normal', list } }))
}

/** Switches difficulties off for good: the game, its maps, placements and events forget them (the delete question lists what goes). */
export function removeAllDifficulties(doc: ProjectDoc): ProjectDoc {
  let out = doc
  for (const d of difficultyList(doc)) out = removeDifficulty(out, d.id)
  return out
}

export function addDifficulty(doc: ProjectDoc, name: string): ProjectDoc {
  if (!hasDifficulties(doc) || difficultyList(doc).length >= MAX_DIFFICULTIES) return doc
  const entry: DifficultyDef = { id: newId(doc, name), name }
  return withGame(doc, (g) => ({ ...g, difficulties: { ...g.difficulties!, list: [...g.difficulties!.list, entry] } }))
}

export const renameDifficulty = (doc: ProjectDoc, id: string, name: string): ProjectDoc =>
  withGame(doc, (g) => (g.difficulties ? { ...g, difficulties: { ...g.difficulties, list: updateById(g.difficulties.list, id, (d) => ({ ...d, name })) } } : g))

/** The list order is the order of difficulty: easiest first. It decides what "lower" means. */
export const moveDifficulty = (doc: ProjectDoc, from: number, to: number): ProjectDoc =>
  withGame(doc, (g) => (g.difficulties ? { ...g, difficulties: { ...g.difficulties, list: moveItem(g.difficulties.list, from, to) } } : g))

export const setDefaultDifficulty = (doc: ProjectDoc, id: string): ProjectDoc =>
  withGame(doc, (g) => (g.difficulties && difficultyById(doc, id) ? { ...g, difficulties: { ...g.difficulties, default: id } } : g))

export function setChangeRule(doc: ProjectDoc, rule: DifficultyChange): ProjectDoc {
  return withGame(doc, (g) => {
    if (!g.difficulties) return g
    const { change: _old, ...rest } = g.difficulties
    return { ...g, difficulties: rule === 'lowerOnly' ? rest : { ...rest, change: rule } }
  })
}

/** The stats "statsPercent" scales; an empty / missing choice = every stat but the movement stat (the key is left out). */
export function setPercentStats(doc: ProjectDoc, stats: string[] | undefined): ProjectDoc {
  return withGame(doc, (g) => {
    if (!g.difficulties) return g
    const { percentStats: _old, ...rest } = g.difficulties
    return { ...g, difficulties: stats && stats.length ? { ...rest, percentStats: stats } : rest }
  })
}

export type DifficultyField = 'levels' | 'statsPercent' | 'xpPercent' | 'resourcePercent'

/** A number of the enemy block of an entry or of a map's override. `keepDefaults`: a map's override keeps 0 / 100 (it replaces the
 *  game's number, so "no shift here" is a real choice); the game's own list drops them (they mean nothing). */
export function withEnemy(enemy: DifficultyEnemy | undefined, field: 'levels' | 'statsPercent', value: number | undefined, keepDefaults: boolean): DifficultyEnemy | undefined {
  const dflt = field === 'levels' ? 0 : 100
  const next: DifficultyEnemy = { ...(enemy ?? {}) }
  if (value === undefined || (!keepDefaults && value === dflt)) delete next[field]
  else next[field] = value
  return Object.keys(next).length ? next : undefined
}

export function withEnemyStat(enemy: DifficultyEnemy | undefined, stat: string, value: number | undefined, keepDefaults: boolean): DifficultyEnemy | undefined {
  const next: DifficultyEnemy = { ...(enemy ?? {}) }
  const stats = { ...(next.stats ?? {}) }
  if (value === undefined || (!keepDefaults && value === 0)) delete stats[stat]
  else stats[stat] = value
  if (Object.keys(stats).length) next.stats = stats
  else delete next.stats
  return Object.keys(next).length ? next : undefined
}

const setKey = <T extends object>(o: T, key: string, v: unknown): T => {
  const next = { ...o } as Record<string, unknown>
  if (v === undefined) delete next[key]
  else next[key] = v
  return next as T
}

export function setDifficultyText(doc: ProjectDoc, id: string, field: 'description', value: string): ProjectDoc {
  return withGame(doc, (g) => (g.difficulties ? { ...g, difficulties: { ...g.difficulties, list: updateById(g.difficulties.list, id, (d) => setKey(d, field, value.trim() ? value : undefined)) } } : g))
}

export function setDifficultyNumber(doc: ProjectDoc, id: string, field: DifficultyField, value: number | undefined): ProjectDoc {
  return withGame(doc, (g) => {
    if (!g.difficulties) return g
    const list = updateById(g.difficulties.list, id, (d) => {
      if (field === 'xpPercent' || field === 'resourcePercent') return setKey(d, field, value === undefined || value === 100 ? undefined : value)
      return setKey(d, 'enemy', withEnemy(d.enemy, field, value, false))
    })
    return { ...g, difficulties: { ...g.difficulties, list } }
  })
}

export function setDifficultyStat(doc: ProjectDoc, id: string, stat: string, value: number | undefined): ProjectDoc {
  return withGame(doc, (g) => (g.difficulties
    ? { ...g, difficulties: { ...g.difficulties, list: updateById(g.difficulties.list, id, (d) => setKey(d, 'enemy', withEnemyStat(d.enemy, stat, value, false))) } }
    : g))
}

// --- a map's own numbers --------------------------------------------------------------------------------------------------------

export const mapOverride = (map: MapFile, id: string): MapDifficulty | undefined => map.difficulty?.[id]

function putOverride(map: MapFile, id: string, next: MapDifficulty | undefined): MapFile {
  const all: MapDifficulties = { ...(map.difficulty ?? {}) }
  if (next === undefined || Object.keys(next).length === 0) delete all[id]
  else all[id] = next
  const { difficulty: _old, ...rest } = map
  return Object.keys(all).length ? { ...rest, difficulty: all } : (rest as MapFile)
}

export function setMapOverrideNumber(map: MapFile, id: string, field: DifficultyField, value: number | undefined): MapFile {
  const cur = mapOverride(map, id) ?? {}
  const next = field === 'xpPercent' || field === 'resourcePercent' ? setKey(cur, field, value) : setKey(cur, 'enemy', withEnemy(cur.enemy, field, value, true))
  return putOverride(map, id, next)
}

export function setMapOverrideStat(map: MapFile, id: string, stat: string, value: number | undefined): MapFile {
  const cur = mapOverride(map, id) ?? {}
  return putOverride(map, id, setKey(cur, 'enemy', withEnemyStat(cur.enemy, stat, value, true)))
}

export const clearMapOverride = (map: MapFile, id: string): MapFile => putOverride(map, id, undefined)

// --- who a placement is for -----------------------------------------------------------------------------------------------------

/** Ticks / unticks one difficulty in a placement's "only on" list. Every difficulty ticked is the same as none (it appears on all):
 *  the key is left out. `all` = the game's ids in list order, so the list is written in that order. */
export function toggleOnlyOn(current: string[] | undefined, all: string[], id: string, on: boolean): string[] | undefined {
  const now = current === undefined ? all : current
  const set = new Set(now)
  if (on) set.add(id)
  else set.delete(id)
  const next = all.filter((x) => set.has(x))
  // Unticking the last one would leave "no difficulty": back to "all" rather than a list the loader refuses.
  return next.length === 0 || next.length === all.length ? undefined : next
}

/** `ids` the unit is limited to that no longer exist in the game (a hand-edited file). */
export const unknownOnlyOn = (doc: ProjectDoc, list: string[] | undefined): string[] => (list ?? []).filter((id) => !difficultyById(doc, id))

// --- deleting one ---------------------------------------------------------------------------------------------------------------

/** Removes the entry and everything that named it, in one edit: a map's override, the id in a placement's "only on" list (a
 *  placement left with none appears on every difficulty), and events' conditions and added units (storyRefs / storyWalk). The
 *  campaign's scenes are cleaned by the document store (campaignRefs.reconcileCampaign). Deleting the last one switches
 *  difficulties off (the key goes). The default moves to the first remaining entry. */
export function removeDifficulty(doc: ProjectDoc, id: string): ProjectDoc {
  if (!difficultyById(doc, id)) return doc
  let out = scrubDoc(doc, [{ kind: 'difficulty', id }])
  const maps: ProjectDoc['maps'] = {}
  for (const [mid, m] of Object.entries(out.maps)) {
    if (!m) { maps[mid] = m; continue }
    let next: MapFile = clearMapOverride(m, id)
    if (next.units?.some((u) => u.difficulty?.includes(id))) {
      next = { ...next, units: next.units.map((u): MapUnit => {
        if (!u.difficulty?.includes(id)) return u
        const left = u.difficulty.filter((x) => x !== id)
        return patchObj(u, { difficulty: left.length ? left : undefined })
      }) }
    }
    maps[mid] = next === m ? m : next
  }
  out = { ...out, maps }
  return withGame(out, (g) => {
    const set = g.difficulties
    if (!set) return g
    const list = set.list.filter((d) => d.id !== id)
    const { difficulties: _old, ...others } = g
    if (!list.length) return others as typeof g
    return { ...others, difficulties: { ...set, list, default: set.default === id ? list[0].id : set.default } } as typeof g
  })
}
