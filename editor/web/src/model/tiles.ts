// A map's tile layer (docs/GRAPHICS.md "Tile layer (hand-painted maps)"): per square, a tile of the map's tileset drawn INSTEAD of the terrain's
// look. Presentation only. In the file:
//
//   "tileLayer": { "tileset": "world", "cells": [ "000c000f....", ... ] }
//
// one string per map row, four hex digits per square ("...." = no tile), so a row has 4 x width characters. In the editor the layer is an
// Int32Array of tile numbers (-1 = none), row by row. Every function returns the very same map when nothing changed, so a stroke over
// squares that already have the tile adds no undo step. The checks that mirror the loader are in tileChecks.ts.
import type { Cell } from './mapGrid'
import type { MapFile, TileLayer } from './types'

export const NO_TILE = -1
/** A tile number is four hex digits in the file. */
export const MAX_TILES = 65536

export const tileLayerOf = (map: MapFile | null | undefined): TileLayer | null => {
  const t = (map as { tileLayer?: unknown } | null | undefined)?.tileLayer
  return t && typeof t === 'object' && !Array.isArray(t) && typeof (t as TileLayer).tileset === 'string' ? (t as TileLayer) : null
}

const isNo = (v: unknown): boolean => v === null || v === undefined || v === NO_TILE

/** Reads the layer into tile numbers (squares the file does not describe properly read as "none"); null when the map has no layer. */
export function readTiles(map: MapFile): Int32Array | null {
  const layer = tileLayerOf(map)
  if (!layer) return null
  const w = map.grid.width, h = map.grid.height
  const out = new Int32Array(w * h).fill(NO_TILE)
  const rows: unknown[] = Array.isArray(layer.cells) ? layer.cells : []
  for (let y = 0; y < h; y++) {
    const row = rows[y]
    if (typeof row === 'string') {
      for (let x = 0; x < w; x++) {
        const tok = row.slice(x * 4, x * 4 + 4)
        if (/^[0-9a-fA-F]{4}$/.test(tok)) out[y * w + x] = parseInt(tok, 16)
      }
    } else if (Array.isArray(row)) {
      for (let x = 0; x < w; x++) { const v = row[x]; if (!isNo(v) && Number.isInteger(v) && (v as number) >= 0) out[y * w + x] = v as number }
    }
  }
  return out
}

/** The file's text for a row of tile numbers. */
export const encodeRow = (tiles: ArrayLike<number>, width: number, y: number): string => {
  let s = ''
  for (let x = 0; x < width; x++) {
    const v = tiles[y * width + x]
    s += v < 0 || !Number.isInteger(v) ? '....' : Math.min(v, 0xffff).toString(16).padStart(4, '0')
  }
  return s
}

/** The layer with these tiles (keeps the tileset). */
export function writeTiles(map: MapFile, tiles: ArrayLike<number>): MapFile {
  const layer = tileLayerOf(map)
  if (!layer) return map
  const w = map.grid.width, h = map.grid.height
  return { ...map, tileLayer: { ...layer, cells: Array.from({ length: h }, (_, y) => encodeRow(tiles, w, y)) } }
}

/** Starts a layer for the tileset: every square without a tile. */
export function addTileLayer(map: MapFile, tileset: string): MapFile {
  const w = map.grid.width, h = map.grid.height
  const row = '....'.repeat(w)
  return { ...map, tileLayer: { tileset, cells: Array.from({ length: h }, () => row) } }
}

/** Takes the layer away (and the tiles changes and commands of the map carry; see tilesets.ts stripMapTiles for that). */
export function dropTileLayer(map: MapFile): MapFile {
  if (!tileLayerOf(map) && !(map as { tileLayer?: unknown }).tileLayer) return map
  const { tileLayer: _gone, ...rest } = map as MapFile & { tileLayer?: unknown }
  return rest as MapFile
}

/** Another tileset for the layer; the tile numbers stay (the checks say when they no longer fit). */
export function setLayerTileset(map: MapFile, tileset: string): MapFile {
  const layer = tileLayerOf(map)
  return layer && layer.tileset !== tileset ? { ...map, tileLayer: { ...layer, tileset } } : map
}

export const tileAt = (map: MapFile, x: number, y: number): number => {
  const t = readTiles(map)
  return !t || x < 0 || y < 0 || x >= map.grid.width || y >= map.grid.height ? NO_TILE : t[y * map.grid.width + x]
}

/** Gives the squares this tile (-1 = erase). Squares outside the map are ignored. */
export function setTiles(map: MapFile, cells: Cell[], tile: number): MapFile {
  const t = readTiles(map)
  if (!t) return map
  const w = map.grid.width, h = map.grid.height
  let changed = false
  for (const { x, y } of cells) {
    if (x < 0 || y < 0 || x >= w || y >= h) continue
    if (t[y * w + x] !== tile) { t[y * w + x] = tile; changed = true }
  }
  return changed ? writeTiles(map, t) : map
}

/** The squares of the rectangle between two corners (inclusive, either order), clipped to the map. */
export function rectCells(map: MapFile, a: Cell, b: Cell): Cell[] {
  const x0 = Math.max(0, Math.min(a.x, b.x)), x1 = Math.min(map.grid.width - 1, Math.max(a.x, b.x))
  const y0 = Math.max(0, Math.min(a.y, b.y)), y1 = Math.min(map.grid.height - 1, Math.max(a.y, b.y))
  const out: Cell[] = []
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) out.push({ x, y })
  return out
}

/** A block of tiles picked from the palette (`w` x `h`, row by row; -1 = leave the square as it is when stamped). */
export interface TileBlock { w: number; h: number; tiles: number[] }

/** Stamps the block with its top-left square at `at` (squares outside the map are skipped; "none" parts leave the map alone). */
export function stampTiles(map: MapFile, at: Cell, block: TileBlock): MapFile {
  const t = readTiles(map)
  if (!t) return map
  const w = map.grid.width, h = map.grid.height
  let changed = false
  for (let j = 0; j < block.h; j++) {
    for (let i = 0; i < block.w; i++) {
      const v = block.tiles[j * block.w + i]
      const x = at.x + i, y = at.y + j
      if (v === undefined || v < 0 || x < 0 || y < 0 || x >= w || y >= h) continue
      if (t[y * w + x] !== v) { t[y * w + x] = v; changed = true }
    }
  }
  return changed ? writeTiles(map, t) : map
}

/** The layer for a resized map: the old squares move by (dx, dy), new squares have no tile. */
export function resizeTileLayer(map: MapFile, newW: number, newH: number, dx: number, dy: number): TileLayer | undefined {
  const layer = tileLayerOf(map)
  const old = readTiles(map)
  if (!layer || !old) return undefined
  const ow = map.grid.width, oh = map.grid.height
  const tiles = new Int32Array(newW * newH).fill(NO_TILE)
  for (let y = 0; y < newH; y++) for (let x = 0; x < newW; x++) {
    const ox = x - dx, oy = y - dy
    if (ox >= 0 && oy >= 0 && ox < ow && oy < oh) tiles[y * newW + x] = old[oy * ow + ox]
  }
  return { ...layer, cells: Array.from({ length: newH }, (_, y) => encodeRow(tiles, newW, y)) }
}

/** How many squares of the layer have a tile. */
export const tileCount = (map: MapFile): number => { const t = readTiles(map); return t ? t.reduce((n, v) => n + (v >= 0 ? 1 : 0), 0) : 0 }
