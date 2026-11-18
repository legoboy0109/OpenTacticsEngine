// A map's named changes (docs/EVENTS.md "Changing the map"): prepared sets of squares that get another terrain, and placed structures that
// become another kind, applied during the battle by the story commands applyChange / revertChange. Pure edits; the checks that mirror the
// loader are in mapChangeChecks.ts.
//
//   "changes": { "bridge": { "cells": [ { "x": 4, "y": 1, "terrain": "bridge" } ], "structures": [ { "id": "gate", "kind": "gate_open" } ] } }
import { editMap } from './mapCtx'
import { readGrid, writeGrid, type Cell } from './mapGrid'
import { NO_TILE, readTiles, tileLayerOf, writeTiles } from './tiles'
import { sizeOf, ringsOf } from './structures'
import type { Usage } from './references'
import type { Overlay } from './storyOverlays'
import { scrubMap, storyUsages } from './storyRefs'
import { forEachCommand, walkStory } from './storyWalk'
import type { MapFile, ProjectDoc, Structure } from './types'

/** `tile`: the square's tile in the tile layer once the change is applied (-1 / null = none); missing = a changed terrain clears it, unless `keepTile`. */
export interface ChangeCell { x: number; y: number; terrain: string; tile?: number | null; keepTile?: boolean }
export interface ChangeStructure { id: string; kind: string }
export interface MapChange { cells?: ChangeCell[]; structures?: ChangeStructure[]; keepTile?: boolean }

/** The loader's limits. */
export const CHANGE_MAX = 64
export const CHANGE_CELLS_MAX = 4096
const ID_RE = /^[A-Za-z0-9_-]+$/

export const isChangeId = (id: string): boolean => ID_RE.test(id)

/** The map's changes in the order they are written (a missing or malformed key reads as none). */
export function changesOf(map: MapFile | null | undefined): Record<string, MapChange> {
  const c = (map as { changes?: unknown } | null | undefined)?.changes
  return c && typeof c === 'object' && !Array.isArray(c) ? (c as Record<string, MapChange>) : {}
}
export const changeIds = (map: MapFile | null | undefined): string[] => Object.keys(changesOf(map))
export const cellsOf = (c: MapChange | undefined): ChangeCell[] => (Array.isArray(c?.cells) ? c!.cells! : [])
export const swapsOf = (c: MapChange | undefined): ChangeStructure[] => (Array.isArray(c?.structures) ? c!.structures! : [])

function withChanges(map: MapFile, next: Record<string, MapChange>): MapFile {
  if (Object.keys(next).length === 0) { const { changes: _gone, ...rest } = map as MapFile & { changes?: unknown }; return rest as MapFile }
  return { ...map, changes: next } as MapFile
}

/** Why an id cannot be used for a new change on this map, or null. */
export function changeIdProblem(map: MapFile, id: string, ignore?: string): string | null {
  if (!id) return 'Give the change a name.'
  if (!isChangeId(id)) return 'A change’s name may only have letters, digits, “_” and “-”.'
  if (id !== ignore && id in changesOf(map)) return `This map already has a change called “${id}”.`
  return null
}

/** A free id like "change1", "change2". */
export function nextChangeId(map: MapFile): string {
  const have = changesOf(map)
  for (let n = 1; ; n++) if (!(`change${n}` in have)) return `change${n}`
}

export function addChange(map: MapFile, id: string): MapFile {
  if (changeIdProblem(map, id) || changeIds(map).length >= CHANGE_MAX) return map
  return withChanges(map, { ...changesOf(map), [id]: { cells: [] } })
}

export function deleteChange(map: MapFile, id: string): MapFile {
  const have = changesOf(map)
  if (!(id in have)) return map
  const { [id]: _gone, ...rest } = have
  return withChanges(map, rest)
}

/** Renames keeping the order of the changes. */
export function renameChangeKey(map: MapFile, from: string, to: string): MapFile {
  const have = changesOf(map)
  if (from === to || !(from in have) || changeIdProblem(map, to, from)) return map
  return withChanges(map, Object.fromEntries(Object.entries(have).map(([k, v]) => [k === from ? to : k, v])))
}

const patchChange = (map: MapFile, id: string, fn: (c: MapChange) => MapChange): MapFile => {
  const have = changesOf(map)
  if (!(id in have)) return map
  const next = fn(have[id])
  return next === have[id] ? map : withChanges(map, { ...have, [id]: next })
}

/** The terrain a square has in the base map (what painting "back" to it would mean). */
export function baseTerrainAt(map: MapFile, terrain: ProjectDoc['terrain'], x: number, y: number): string | undefined {
  const g = readGrid(map, terrain ?? [])
  return x < 0 || y < 0 || x >= g.width || y >= g.height ? undefined : g.cells[y * g.width + x]
}

/** Paints squares INTO the change: a square gets the terrain in the change; painting it with the terrain it already has in the base map
 *  takes it out of the change again (the change then leaves that square alone). Squares outside the map are ignored. */
export function paintChange(map: MapFile, terrain: ProjectDoc['terrain'], id: string, cells: Cell[], terrainId: string): MapFile {
  if (!(terrain ?? []).some((t) => t.id === terrainId)) return map
  const g = readGrid(map, terrain ?? [])
  return patchChange(map, id, (c) => {
    let list = cellsOf(c)
    let changed = false
    for (const { x, y } of cells) {
      if (x < 0 || y < 0 || x >= g.width || y >= g.height) continue
      const at = list.findIndex((k) => k.x === x && k.y === y)
      const same = g.cells[y * g.width + x] === terrainId
      if (same && at >= 0 && 'tile' in list[at]) { if (list[at].terrain !== terrainId) { list = list.map((k, i) => (i === at ? { ...k, terrain: terrainId } : k)); changed = true } }  // it still carries a tile: stays
      else if (same && at >= 0) { list = list.filter((_, i) => i !== at); changed = true }
      else if (!same && at < 0 && list.length < CHANGE_CELLS_MAX) { list = [...list, { x, y, terrain: terrainId }]; changed = true }
      else if (!same && at >= 0 && list[at].terrain !== terrainId) { list = list.map((k, i) => (i === at ? { ...k, terrain: terrainId } : k)); changed = true }
    }
    return changed ? { ...c, cells: list } : c
  })
}

export function clearChangeCells(map: MapFile, id: string): MapFile {
  return patchChange(map, id, (c) => (cellsOf(c).length ? { ...c, cells: [] } : c))
}

/** Adds a structure swap (a placed structure by its id becomes `kind`); one swap per structure. */
export function setSwap(map: MapFile, id: string, structureId: string, kind: string): MapFile {
  return patchChange(map, id, (c) => {
    const list = swapsOf(c)
    const at = list.findIndex((s) => s.id === structureId)
    const next = at >= 0 ? list.map((s, i) => (i === at ? { ...s, kind } : s)) : [...list, { id: structureId, kind }]
    return { ...c, structures: next }
  })
}

export function removeSwap(map: MapFile, id: string, structureId: string): MapFile {
  return patchChange(map, id, (c) => {
    const list = swapsOf(c).filter((s) => s.id !== structureId)
    if (list.length === swapsOf(c).length) return c
    const { structures: _gone, ...rest } = c
    return list.length ? { ...rest, structures: list } : rest
  })
}

/** The map as it looks once the change is applied (for the "after" view): the change's squares carry its terrain, and (with a tile layer)
 *  its tiles: a square with a `tile` gets it, one whose terrain really changes loses its tile unless `keepTile` (the game's rule). */
export function applyToMap(map: MapFile, terrain: ProjectDoc['terrain'], id: string): MapFile {
  const c = changesOf(map)[id]
  const list = cellsOf(c)
  if (!c || list.length === 0 || !terrain?.length) return map
  const g = readGrid(map, terrain)
  const tiles = readTiles(map)
  let any = false
  for (const k of list) {
    if (k.x < 0 || k.y < 0 || k.x >= g.width || k.y >= g.height || !terrain.some((t) => t.id === k.terrain)) continue
    const i = k.y * g.width + k.x
    const differs = g.cells[i] !== k.terrain
    if (tiles) {
      if (k.tile !== undefined) tiles[i] = typeof k.tile === 'number' && k.tile >= 0 ? k.tile : NO_TILE
      else if (differs && !(k.keepTile ?? c.keepTile ?? false)) tiles[i] = NO_TILE
    }
    g.cells[i] = k.terrain
    any = true
  }
  if (!any) return map
  const out = writeGrid(map, g, terrain)
  return tiles ? writeTiles(out, tiles) : out
}

// --- tiles in a change (docs/GRAPHICS.md "Tile layer") ---

/** Gives the squares of a change their tile (-1 = none) while the Tiles tool paints "into the change": a square not in the change yet joins it
 *  with the terrain it has in the base map. A square whose terrain and tile would stay as in the map leaves the change again. */
export function paintChangeTiles(map: MapFile, terrain: ProjectDoc['terrain'], id: string, entries: Array<{ x: number; y: number; tile: number }>): MapFile {
  const layer = tileLayerOf(map)
  const base = readTiles(map)
  if (!layer || !base) return map
  const g = readGrid(map, terrain ?? [])
  return patchChange(map, id, (c) => {
    let list = cellsOf(c)
    let changed = false
    for (const { x, y, tile } of entries) {
      if (x < 0 || y < 0 || x >= g.width || y >= g.height) continue
      const i = y * g.width + x
      const at = list.findIndex((k) => k.x === x && k.y === y)
      if (at >= 0) {
        const k = list[at]
        if (k.tile === tile) continue
        if (k.terrain === g.cells[i] && tile === base[i]) list = list.filter((_, j) => j !== at)
        else list = list.map((q, j) => (j === at ? { ...q, tile } : q))
        changed = true
      } else if (tile !== base[i] && list.length < CHANGE_CELLS_MAX) {
        list = [...list, { x, y, terrain: g.cells[i], tile }]
        changed = true
      }
    }
    return changed ? { ...c, cells: list } : c
  })
}

/** "Also record the tiles of these squares": every square of the change gets the tile it has in the map now. */
export function recordChangeTiles(map: MapFile, id: string): MapFile {
  const base = readTiles(map)
  if (!base) return map
  const w = map.grid.width
  return patchChange(map, id, (c) => {
    const list = cellsOf(c)
    const next = list.map((k) => (k.x >= 0 && k.y >= 0 && k.x < w && k.y < map.grid.height ? { ...k, tile: base[k.y * w + k.x] } : k))
    return next.every((k, i) => k.tile === list[i].tile) ? c : { ...c, cells: next }
  })
}

/** Takes every tile (and keepTile) out of the change: its squares then clear their tile when the terrain changes, as the game does. */
export function clearChangeTiles(map: MapFile, id: string): MapFile {
  return patchChange(map, id, (c) => {
    const list = cellsOf(c)
    if (!list.some((k) => 'tile' in k || 'keepTile' in k) && !('keepTile' in c)) return c
    const { keepTile: _k, ...rest } = c
    return { ...rest, cells: list.map((k) => { const { tile: _t, keepTile: _q, ...q } = k; return q as ChangeCell }) }
  })
}

/** "Keep the tiles": squares of the change whose terrain changes keep the tile they have (unless they carry a tile of their own). */
export function setChangeKeepTile(map: MapFile, id: string, on: boolean): MapFile {
  return patchChange(map, id, (c) => {
    if ((c.keepTile ?? false) === on) return c
    const { keepTile: _k, ...rest } = c
    return on ? { ...rest, keepTile: true } : rest
  })
}

/** How many squares of the change carry a tile of their own. */
export const changeTileCount = (c: MapChange | undefined): number => cellsOf(c).filter((k) => k.tile !== undefined).length

/** Edits through the document (one undo step). */
export const editChanges = (doc: ProjectDoc, mapId: string, fn: (m: MapFile) => MapFile): ProjectDoc => editMap(doc, mapId, (m) => fn(m))

/** Structure kinds a placed structure can be swapped into: another kind of the same size. */
export function swapKinds(kinds: Structure[], placedType: string): Structure[] {
  const from = kinds.find((k) => k.id === placedType)
  if (!from) return []
  const a = sizeOf(from)
  return kinds.filter((k) => k.id !== placedType && sizeOf(k).w === a.w && sizeOf(k).h === a.h && ringsOf(k) === ringsOf(from))
}

/** The placed structures that have a story name (only those can be swapped). */
export const namedStructures = (map: MapFile): Array<{ id: string; type: string; x: number; y: number }> =>
  (map.structures ?? []).filter((s) => typeof s.id === 'string' && s.id !== '').map((s) => ({ id: s.id as string, type: s.type, x: s.x, y: s.y }))

// --- follow-through into events ---

/** Renames a change and every applyChange / revertChange command of the map that names it (one edit). */
export function renameChange(doc: ProjectDoc, mapId: string, from: string, to: string): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m || from === to || changeIdProblem(m, to, from)) return doc
  let next = renameChangeKey(m, from, to)
  next = walkStory(next, { ref: (r) => (r.kind === 'change' && r.id === from ? to : r.id) })
  return next === m ? doc : { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

/** Deletes a change; the commands that apply or take it back are removed with it (one edit, one undo). */
export function deleteChangeAndStory(doc: ProjectDoc, mapId: string, id: string): ProjectDoc {
  const m = doc.maps[mapId]
  if (!m) return doc
  const next = scrubMap(doc, deleteChange(m, id), [{ kind: 'change', id }])
  return next === m ? doc : { ...doc, maps: { ...doc.maps, [mapId]: next } }
}

/** Lines for the delete confirmation: the commands that would go with the change. */
export const changeUsages = (doc: ProjectDoc, mapId: string, id: string): Usage[] => storyUsages(doc, [{ kind: 'change', id }], mapId)

/** How many commands of the map's events apply the change. */
export function applyCount(map: MapFile, id: string): number {
  let n = 0
  for (const e of map.events ?? []) forEachCommand(e.do, '', (c) => { if (c.applyChange === id) n++ })
  return n
}

/** The squares of a change as map overlays (outlined while the Changes tab has it selected), and a swapped structure's footprint corner. */
export function changeOutline(map: MapFile, id: string): Overlay[] {
  const ch = changesOf(map)[id]
  return cellsOf(ch).map((k) => ({ x: k.x, y: k.y, w: 1, h: 1, label: '', tone: 'change' as const }))
}
