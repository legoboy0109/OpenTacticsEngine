// The battle scene's backdrop and platforms by terrain, per map and for the game; which fighter's square decides; which side each fighter
// stands on; and weather (docs/BATTLE_SCENE.md "Which background and platform", docs/ECONOMY.md "Weather and movement cost").
// Pure edits on the document; the loader's refusals said in plain words (engine/core/data/loader_battle_art.cpp, loader_weather.cpp).
// Defaults are never written; unknown keys stay.
import { backgroundList, battleOf } from './battleArt'
import { findSheet, withGraphics } from './graphics'
import { patchObj } from './util'
import type { BattleSection, CellRef, Graphics, MapFile, ProjectDoc } from './types'

export type TableKind = 'battleBackgrounds' | 'battlePlatforms'
export type Weather = 'rain' | 'snow'
export const WEATHERS: Weather[] = ['rain', 'snow']
/** The key of a table that stands for every terrain it does not name. */
export const TABLE_DEFAULT = 'default'

const prune = <T,>(m: Record<string, T> | undefined): Record<string, T> | undefined => (m && Object.keys(m).length ? m : undefined)
const withMap = (doc: ProjectDoc, mapId: string, fn: (m: MapFile) => MapFile): ProjectDoc => {
  const m = doc.maps[mapId]
  return m ? { ...doc, maps: { ...doc.maps, [mapId]: fn(m) } } : doc
}

// --- the game's choices (game.json) ------------------------------------------------------------------------------------------
export type BackgroundFrom = 'defender' | 'attacker'
export const backgroundFrom = (doc: ProjectDoc): BackgroundFrom => (doc.game?.battleBackgroundFrom === 'attacker' ? 'attacker' : 'defender')
/** "The struck fighter's square" is the default and is written as NO key. */
export function setBackgroundFrom(doc: ProjectDoc, v: BackgroundFrom): ProjectDoc {
  if (!doc.game || backgroundFrom(doc) === v) return doc
  return { ...doc, game: patchObj(doc.game, { battleBackgroundFrom: v === 'attacker' ? 'attacker' : undefined }) }
}
export type SceneSides = 'map' | 'playerRight' | 'playerLeft'
export const SCENE_SIDES: SceneSides[] = ['map', 'playerRight', 'playerLeft']
export const sceneSides = (doc: ProjectDoc): SceneSides => (doc.game?.battleSceneSides === 'playerRight' || doc.game?.battleSceneSides === 'playerLeft' ? doc.game.battleSceneSides : 'map')
export function setSceneSides(doc: ProjectDoc, v: SceneSides): ProjectDoc {
  if (!doc.game || sceneSides(doc) === v) return doc
  return { ...doc, game: patchObj(doc.game, { battleSceneSides: v === 'map' ? undefined : v }) }
}

// --- platforms (data/graphics.json "battle") -----------------------------------------------------------------------------------
export interface PlatformRow { id: string; sheet: string; cell: CellRef; far?: CellRef; right?: CellRef; farRight?: CellRef }
export const platformList = (doc: ProjectDoc): PlatformRow[] => Object.entries(battleOf(doc).platforms ?? {}).map(([id, p]) => ({ id, sheet: p.sheet, cell: p.cell, far: p.far, right: p.right, farRight: p.farRight }))

export const withBattle = (doc: ProjectDoc, fn: (b: BattleSection) => BattleSection): ProjectDoc =>
  withGraphics(doc, (g) => {
    const next = fn(g.battle ?? {})
    return patchObj(g, { battle: Object.keys(next).length ? next : undefined }) as Graphics
  })

export function newPlatformId(doc: ProjectDoc, name: string): string {
  const base = name.replace(/[^A-Za-z0-9_-]/g, '') || 'platform'
  const taken = new Set([...platformList(doc).map((p) => p.id), TABLE_DEFAULT])
  let id = base
  for (let n = 2; taken.has(id); n += 1) id = `${base}${n}`
  return id
}

/** A new platform takes the first image's first piece (the loader needs a picture). */
export function addPlatform(doc: ProjectDoc, id: string): ProjectDoc {
  const sheet = doc.graphics?.sheets?.[0]
  if (!sheet || platformList(doc).some((p) => p.id === id)) return doc
  return withBattle(doc, (b) => ({ ...b, platforms: { ...(b.platforms ?? {}), [id]: { sheet: sheet.id, cell: [0, 0] } } }))
}
export function setPlatformPicture(doc: ProjectDoc, id: string, sheet: string, cell: CellRef, far?: boolean): ProjectDoc {
  const cur = battleOf(doc).platforms?.[id]
  if (!cur) return doc
  return withBattle(doc, (b) => ({ ...b, platforms: { ...b.platforms, [id]: far ? { ...cur, sheet, far: cell } : patchObj(cur, { sheet, cell }) } }))
}
/** The right-hand fighter's own picture of a platform (a fixed stage only); without one the left picture is drawn mirrored. */
export function setPlatformRight(doc: ProjectDoc, id: string, cell: CellRef | undefined, far?: boolean): ProjectDoc {
  const cur = battleOf(doc).platforms?.[id]
  if (!cur || (far && !cur.far)) return doc
  return withBattle(doc, (b) => ({ ...b, platforms: { ...b.platforms, [id]: patchObj(cur, far ? { farRight: cell } : { right: cell }) } }))
}
export function clearPlatformFar(doc: ProjectDoc, id: string): ProjectDoc {
  const cur = battleOf(doc).platforms?.[id]
  if (!cur || !cur.far) return doc
  return withBattle(doc, (b) => ({ ...b, platforms: { ...b.platforms, [id]: patchObj(cur, { far: undefined, farRight: undefined }) } }))
}
/** Removes the platform and every choice that named it (terrain table, default, a map's table). */
export function removePlatform(doc: ProjectDoc, id: string): ProjectDoc {
  const kept = withBattle(doc, (b) => {
    const { [id]: _gone, ...rest } = b.platforms ?? {}
    return patchObj(b, {
      platforms: prune(rest),
      terrainPlatform: prune(Object.fromEntries(Object.entries(b.terrainPlatform ?? {}).filter(([, v]) => v !== id))),
      defaultPlatform: b.defaultPlatform === id ? undefined : b.defaultPlatform,
    })
  })
  return dropFromMapTables(kept, 'battlePlatforms', (v) => v === id)
}
export const terrainPlatform = (doc: ProjectDoc, terrain: string): string => battleOf(doc).terrainPlatform?.[terrain] ?? ''
export function setTerrainPlatform(doc: ProjectDoc, terrain: string, id: string): ProjectDoc {
  return withBattle(doc, (b) => patchObj(b, {
    terrainPlatform: prune(id === '' ? Object.fromEntries(Object.entries(b.terrainPlatform ?? {}).filter(([k]) => k !== terrain)) : { ...(b.terrainPlatform ?? {}), [terrain]: id }),
  }))
}
export const defaultPlatform = (doc: ProjectDoc): string => battleOf(doc).defaultPlatform ?? ''
export const setDefaultPlatform = (doc: ProjectDoc, id: string): ProjectDoc => withBattle(doc, (b) => patchObj(b, { defaultPlatform: id === '' ? undefined : id }))

// --- a map's own tables ---------------------------------------------------------------------------------------------------------------
export const mapTable = (map: MapFile, kind: TableKind): Record<string, string> => map[kind] ?? {}
/** `key` = a terrain id or "default"; an empty id removes the entry (and an empty table removes the key). */
export function setMapTableEntry(doc: ProjectDoc, mapId: string, kind: TableKind, key: string, id: string): ProjectDoc {
  return withMap(doc, mapId, (m) => {
    const cur = { ...(m[kind] ?? {}) }
    if (id === '') delete cur[key]
    else cur[key] = id
    return patchObj(m, { [kind]: prune(cur) })
  })
}
/** Entries a background / platform that was removed leaves behind go too, on every map. */
export function dropFromMapTables(doc: ProjectDoc, kind: TableKind, gone: (id: string) => boolean): ProjectDoc {
  let next = doc
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m || !m[kind]) continue
    const kept = Object.fromEntries(Object.entries(m[kind] ?? {}).filter(([, v]) => !gone(v)))
    if (Object.keys(kept).length !== Object.keys(m[kind] ?? {}).length) next = withMap(next, mapId, (x) => patchObj(x, { [kind]: prune(kept) }))
  }
  return next
}
/** A terrain was deleted: the tables of the maps and the game forget it. */
export function dropTerrainFromTables(doc: ProjectDoc, terrain: string): ProjectDoc {
  let next = doc
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    for (const kind of ['battleBackgrounds', 'battlePlatforms'] as const)
      if (m[kind] && terrain in (m[kind] ?? {})) next = setMapTableEntry(next, mapId, kind, terrain, '')
  }
  return terrainPlatform(next, terrain) ? setTerrainPlatform(next, terrain, '') : next
}

// --- weather ----------------------------------------------------------------------------------------------------------------------------
export const mapWeather = (map: MapFile): Weather | '' => (map.weather === 'rain' || map.weather === 'snow' ? map.weather : '')
export const setMapWeather = (doc: ProjectDoc, mapId: string, w: Weather | ''): ProjectDoc => withMap(doc, mapId, (m) => patchObj(m, { weather: w === '' ? undefined : w }))

export const WEATHER_COST_MAX = 99
export const weatherCostOf = (doc: ProjectDoc, terrain: string, weather: Weather, moveType: string): number | undefined => doc.terrain?.find((t) => t.id === terrain)?.weatherCost?.[weather]?.[moveType]
/** undefined removes the entry; an emptied weather or terrain key goes too (no key = the terrain does not care about the weather). */
export function setWeatherCost(doc: ProjectDoc, terrain: string, weather: Weather, moveType: string, cost: number | undefined): ProjectDoc {
  if (!doc.terrain) return doc
  return {
    ...doc,
    terrain: doc.terrain.map((t) => {
      if (t.id !== terrain) return t
      const wc = { ...(t.weatherCost ?? {}) }
      const row = { ...(wc[weather] ?? {}) }
      if (cost === undefined) delete row[moveType]
      else row[moveType] = Math.min(WEATHER_COST_MAX, Math.max(1, Math.round(cost)))
      if (Object.keys(row).length) wc[weather] = row
      else delete wc[weather]
      return patchObj(t, { weatherCost: Object.keys(wc).length ? wc : undefined })
    }),
  }
}

// --- checks (what the loader refuses) --------------------------------------------------------------------------------------------------
export function mapTableProblems(doc: ProjectDoc, map: MapFile): string[] {
  const out: string[] = []
  const bgs = new Set(backgroundList(doc).map((b) => b.id))
  const plats = new Set(platformList(doc).map((p) => p.id))
  for (const [kind, ids, what] of [['battleBackgrounds', bgs, 'background'], ['battlePlatforms', plats, 'platform']] as const) {
    for (const [key, id] of Object.entries(map[kind] ?? {})) {
      if (key !== TABLE_DEFAULT && !doc.terrain?.some((t) => t.id === key)) out.push(`The ${what} table names the terrain “${key}”, which this game does not have.`)
      if (!ids.has(id)) out.push(`The ${what} table gives ${key === TABLE_DEFAULT ? 'every other terrain' : `“${key}”`} the ${what} “${id}”, which does not exist.`)
    }
  }
  if (map.weather !== undefined && map.weather !== 'rain' && map.weather !== 'snow') out.push('The weather must be rain, snow or clear.')
  return out
}

export function weatherCostProblems(doc: ProjectDoc, terrain: string): string[] {
  const t = doc.terrain?.find((x) => x.id === terrain)
  const out: string[] = []
  const wc = t?.weatherCost
  if (!wc) return out
  for (const [w, row] of Object.entries(wc)) {
    if (w !== 'rain' && w !== 'snow') { out.push(`The weather “${w}” is not rain or snow.`); continue }
    for (const [mt, cost] of Object.entries(row ?? {})) {
      if (!doc.game?.moveTypes.some((m) => m.id === mt)) out.push(`The ${w} cost names the movement type “${mt}”, which this game does not have.`)
      if (!Number.isInteger(cost) || cost < 1 || cost > WEATHER_COST_MAX) out.push(`The ${w} cost for “${mt}” must be a whole number from 1 to ${WEATHER_COST_MAX}.`)
    }
  }
  return out
}

export function platformProblems(doc: ProjectDoc): string[] {
  const b = battleOf(doc)
  const out: string[] = []
  for (const p of platformList(doc)) {
    if (p.id === TABLE_DEFAULT) out.push('A platform cannot be called “default”.')
    if (!findSheet(doc, p.sheet)) out.push(`Platform “${p.id}” uses an image that is not in the game.`)
  }
  for (const [terrain, id] of Object.entries(b.terrainPlatform ?? {})) {
    if (!doc.terrain?.some((t) => t.id === terrain)) out.push(`A platform is set for the terrain “${terrain}”, which no longer exists.`)
    if (!b.platforms?.[id]) out.push(`The terrain “${terrain}” uses the platform “${id}”, which does not exist.`)
  }
  if (b.defaultPlatform && !b.platforms?.[b.defaultPlatform]) out.push(`The default platform “${b.defaultPlatform}” does not exist.`)
  return out
}

/** An image was removed: the platforms that used it go, with the terrain choices and the default that named them (maps' tables: left to the checks). */
export function droppedPlatforms(b: BattleSection, sheetId: string): Partial<BattleSection> {
  if (!b.platforms) return {}
  const kept = Object.fromEntries(Object.entries(b.platforms).filter(([, p]) => p.sheet !== sheetId))
  const gone = new Set(Object.keys(b.platforms).filter((k) => !(k in kept)))
  if (gone.size === 0) return {}
  return {
    platforms: prune(kept),
    terrainPlatform: prune(Object.fromEntries(Object.entries(b.terrainPlatform ?? {}).filter(([, v]) => !gone.has(v)))),
    defaultPlatform: b.defaultPlatform && gone.has(b.defaultPlatform) ? undefined : b.defaultPlatform,
  }
}
