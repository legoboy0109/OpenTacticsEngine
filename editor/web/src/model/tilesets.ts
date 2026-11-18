// Tilesets (data/graphics.json "tilesets": { "<id>": { "sheet": "<image id>", "animations": { "<tile>": { "frames": [..], "ms": 200 } } } }):
// a sheet whose cells are the tiles of hand-painted maps, numbered row by row from 0 (docs/GRAPHICS.md "Tile layer"). Pure edits.
// Removing a tileset (or the image it uses) takes the tile layers of the maps that use it away, in the same edit (one undo step); the
// delete confirmation lists those maps first (tilesetUsageLines).
import { removeSheet, sheetsOf, withGraphics } from './graphics'
import { dropTileLayer, tileLayerOf } from './tiles'
import { isStoryId } from './storyIds'
import type { Command } from './storyTypes'
import type { MapFile, ProjectDoc, Sheet, Tileset } from './types'
import { patchObj } from './util'

export const TILE_ANIM_MAX_FRAMES = 64
export const TILE_ANIM_MIN_MS = 20
export const TILE_ANIM_MAX_MS = 60000
export const TILE_ANIM_DEFAULT_MS = 200

export const tilesetList = (doc: ProjectDoc): Array<{ id: string; tileset: Tileset }> =>
  Object.entries(doc.graphics?.tilesets ?? {}).map(([id, tileset]) => ({ id, tileset }))
export const tilesetIds = (doc: ProjectDoc): string[] => Object.keys(doc.graphics?.tilesets ?? {})
export const findTileset = (doc: ProjectDoc, id: string): Tileset | undefined => doc.graphics?.tilesets?.[id]

const setTilesets = (doc: ProjectDoc, map: Record<string, Tileset>): ProjectDoc =>
  withGraphics(doc, (g) => patchObj(g, { tilesets: Object.keys(map).length ? map : undefined }))

/** Why `id` cannot be a tileset id (null when it can). `except` = the tileset being renamed. */
export function tilesetIdProblem(doc: ProjectDoc, id: string, except?: string): string | null {
  if (id === '') return 'Type a name.'
  if (!isStoryId(id)) return 'Use letters, digits, _ and - only (no spaces).'
  if (id !== except && doc.graphics?.tilesets && id in doc.graphics.tilesets) return 'There is already a tileset with this name.'
  return null
}

/** The tiles of a sheet whose image is `width` x `height` pixels: columns, rows and how many. 0 when unknown. */
export function sheetTiles(sheet: Sheet | undefined, imageW: number, imageH: number): { cols: number; rows: number; count: number } {
  if (!sheet || !imageW || !imageH || sheet.cellWidth < 1 || sheet.cellHeight < 1) return { cols: 0, rows: 0, count: 0 }
  const cols = Math.floor(imageW / sheet.cellWidth), rows = Math.floor(imageH / sheet.cellHeight)
  return { cols, rows, count: cols * rows }
}

/** A new tileset starts on the first image. */
export function addTileset(doc: ProjectDoc, id: string): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet || tilesetIdProblem(doc, id)) return doc
  return setTilesets(doc, { ...(doc.graphics?.tilesets ?? {}), [id]: { sheet } })
}

export function setTilesetSheet(doc: ProjectDoc, id: string, sheet: string): ProjectDoc {
  const cur = findTileset(doc, id)
  if (!cur || cur.sheet === sheet) return doc
  return setTilesets(doc, { ...doc.graphics!.tilesets!, [id]: patchObj(cur, { sheet }) })
}

/** Renames in place (the order is kept); the maps' tile layers follow. */
export function renameTileset(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const all = doc.graphics?.tilesets
  if (!all || !(from in all) || from === to || tilesetIdProblem(doc, to, from)) return doc
  const out = setTilesets(doc, Object.fromEntries(Object.entries(all).map(([k, v]) => [k === from ? to : k, v])))
  let maps = out.maps
  for (const [mid, m] of Object.entries(out.maps)) {
    const layer = tileLayerOf(m)
    if (m && layer?.tileset === from) maps = { ...maps, [mid]: { ...m, tileLayer: { ...layer, tileset: to } } }
  }
  return maps === out.maps ? out : { ...out, maps }
}

export function moveTileset(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  const entries = Object.entries(doc.graphics?.tilesets ?? {})
  if (from === to || from < 0 || to < 0 || from >= entries.length || to >= entries.length) return doc
  const [it] = entries.splice(from, 1)
  entries.splice(to, 0, it)
  return setTilesets(doc, Object.fromEntries(entries))
}

// ---- animated tiles ----

/** The frames of an animated tile, or null when it is an ordinary one. */
export const animationOf = (ts: Tileset | undefined, tile: number): { frames: number[]; ms: number } | null => {
  const a = ts?.animations?.[String(tile)]
  return a && Array.isArray(a.frames) ? { frames: a.frames, ms: typeof a.ms === 'number' ? a.ms : TILE_ANIM_DEFAULT_MS } : null
}

const setAnimations = (doc: ProjectDoc, id: string, anims: Record<string, { frames: number[]; ms?: number }>): ProjectDoc => {
  const cur = findTileset(doc, id)
  if (!cur) return doc
  const sorted = Object.fromEntries(Object.entries(anims).sort((a, b) => Number(a[0]) - Number(b[0])))
  return setTilesets(doc, { ...doc.graphics!.tilesets!, [id]: patchObj(cur, { animations: Object.keys(sorted).length ? sorted : undefined }) })
}

/** Makes the tile animated (it starts as its own two frames: itself and the next tile, so it already does something). */
export function markAnimated(doc: ProjectDoc, id: string, tile: number, count: number): ProjectDoc {
  const cur = findTileset(doc, id)
  if (!cur || animationOf(cur, tile) || tile < 0 || (count > 0 && tile >= count)) return doc
  return setAnimations(doc, id, { ...(cur.animations ?? {}), [String(tile)]: { frames: [tile, count > 0 && tile + 1 < count ? tile + 1 : tile] } })
}

export function unmarkAnimated(doc: ProjectDoc, id: string, tile: number): ProjectDoc {
  const cur = findTileset(doc, id)
  if (!cur?.animations || !(String(tile) in cur.animations)) return doc
  const { [String(tile)]: _gone, ...rest } = cur.animations
  return setAnimations(doc, id, rest)
}

/** The frames list (tile numbers) and the time per frame of one animated tile; ms undefined = the default (200). */
export function setAnimation(doc: ProjectDoc, id: string, tile: number, frames: number[], ms?: number): ProjectDoc {
  const cur = findTileset(doc, id)
  if (!cur?.animations || !(String(tile) in cur.animations)) return doc
  const entry = ms === undefined || ms === TILE_ANIM_DEFAULT_MS ? { frames } : { frames, ms }
  return setAnimations(doc, id, { ...cur.animations, [String(tile)]: entry })
}

/** Frames typed as "8, 9 10" -> the tile numbers (null when it is not a list of whole numbers). */
export function parseFrames(text: string): number[] | null {
  const parts = text.split(/[\s,;]+/).filter((p) => p !== '')
  if (parts.length === 0 || !parts.every((p) => /^\d+$/.test(p))) return null
  return parts.map(Number)
}

// ---- who uses a tileset ----

/** The maps whose tile layer uses the tileset (ids). */
export const mapsUsingTileset = (doc: ProjectDoc, id: string): string[] =>
  Object.entries(doc.maps).filter(([, m]) => m && tileLayerOf(m)?.tileset === id).map(([mid]) => mid)

export const tilesetsUsingSheet = (doc: ProjectDoc, sheetId: string): string[] =>
  tilesetList(doc).filter((t) => t.tileset.sheet === sheetId).map((t) => t.id)

function stripCommands(list: Command[] | undefined): Command[] | undefined {
  if (!Array.isArray(list)) return list
  let changed = false
  const out = list.map((c) => {
    let r: Command = c
    if (typeof r.setTerrain === 'string' && ('tile' in r || 'keepTile' in r)) { const { tile: _t, keepTile: _k, ...rest } = r; r = rest as Command }
    for (const branch of ['then', 'else'] as const) {
      const inner = r[branch]
      const next = stripCommands(inner)
      if (next !== inner) r = { ...r, [branch]: next }
    }
    if (Array.isArray(r.answers)) {
      const answers = r.answers.map((a) => { const d = stripCommands(a.do); return d === a.do ? a : { ...a, do: d! } })
      if (answers.some((a, i) => a !== r.answers![i])) r = { ...r, answers }
    }
    if (r !== c) changed = true
    return r
  })
  return changed ? out : list
}

/** The map without its tile layer and without the tiles its changes and `setTerrain` commands carry (they mean nothing without it). */
export function stripMapTiles(map: MapFile): MapFile {
  let out = dropTileLayer(map)
  const changes = out.changes
  if (changes) {
    let changed = false
    const next = Object.fromEntries(Object.entries(changes).map(([k, ch]) => {
      const cells = Array.isArray(ch.cells) ? ch.cells.map((c) => { if (!('tile' in c) && !('keepTile' in c)) return c; const { tile: _t, keepTile: _k, ...rest } = c; return rest as typeof c }) : ch.cells
      const { keepTile: _k, ...rest } = ch
      const same = cells && Array.isArray(ch.cells) && cells.every((c, i) => c === ch.cells![i]) && !('keepTile' in ch)
      if (same) return [k, ch]
      changed = true
      return [k, { ...rest, ...(cells ? { cells } : {}) }]
    }))
    if (changed) out = { ...out, changes: next as MapFile['changes'] }
  }
  if (Array.isArray(out.events)) {
    const events = out.events.map((e) => { const d = stripCommands(e.do); return d === e.do ? e : { ...e, do: d! } })
    if (events.some((e, i) => e !== out.events![i])) out = { ...out, events }
  }
  return out
}

function stripMaps(doc: ProjectDoc, mapIds: string[]): ProjectDoc {
  if (!mapIds.length) return doc
  const maps = { ...doc.maps }
  for (const mid of mapIds) { const m = maps[mid]; if (m) maps[mid] = stripMapTiles(m) }
  return { ...doc, maps }
}

/** Removes the tileset; the tile layers that use it go with it (one edit). */
export function removeTileset(doc: ProjectDoc, id: string): ProjectDoc {
  const all = doc.graphics?.tilesets
  if (!all || !(id in all)) return doc
  const users = mapsUsingTileset(doc, id)
  const { [id]: _gone, ...rest } = all
  return stripMaps(setTilesets(doc, rest), users)
}

/** The tilesets that draw from the image go (and the layers that use them): called when an image is removed. */
export function removeTilesetsOfSheet(doc: ProjectDoc, sheetId: string): ProjectDoc {
  let out = doc
  for (const id of tilesetsUsingSheet(doc, sheetId)) out = removeTileset(out, id)
  return out
}

/** Lines for the delete confirmation: the maps whose tile layer goes with the tileset. */
export const tilesetUsageLines = (doc: ProjectDoc, ids: string[]): Array<{ id: string; mapId: string; mapName: string }> =>
  ids.flatMap((id) => mapsUsingTileset(doc, id).map((mid) => ({ id, mapId: mid, mapName: doc.game?.maps.find((m) => m.id === mid)?.name ?? mid })))

/** Removes the image (graphics.ts removeSheet) and, with it, the tilesets that draw from it and the tile layers that use them (one edit). */
export function removeSheetAndTilesets(doc: ProjectDoc, sheetId: string): ProjectDoc {
  return removeTilesetsOfSheet(removeSheet(doc, sheetId), sheetId)
}
