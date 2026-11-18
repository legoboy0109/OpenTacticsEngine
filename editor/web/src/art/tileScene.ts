// What the map canvas needs to draw a map's tile layer (docs/GRAPHICS.md "Tile layer (hand-painted maps)"): the tile of every square,
// the tileset's image already decoded and its animated tiles. Rebuilt when the map, the tileset or a picture changes.
import { findSheet } from '../model/graphics'
import { readTiles } from '../model/tiles'
import { animationOf, findTileset, sheetTiles } from '../model/tilesets'
import { tileLayerOf } from '../model/tiles'
import type { MapFile, ProjectDoc } from '../model/types'
import type { ImageSource } from './scene'

export interface TileScene {
  tileset: string
  /** Per square, row by row: the tile number or -1. */
  cells: Int32Array
  img: CanvasImageSource
  cw: number
  ch: number
  cols: number
  rows: number
  /** Animated tiles: tile -> frames and ms per frame. */
  anims: Map<number, { frames: number[]; ms: number }>
  /** The terrain's colour is drawn over the tiles (the "show terrain colours" aid). */
  terrainOver: boolean
}

/** Null when the map has no tile layer, its tileset is gone or the picture is not loaded yet (the canvas then draws as without tiles). */
export function buildTileScene(doc: ProjectDoc, map: MapFile, src: ImageSource, terrainOver: boolean): TileScene | null {
  const layer = tileLayerOf(map)
  const cells = readTiles(map)
  if (!layer || !cells) return null
  const ts = findTileset(doc, layer.tileset)
  const sheet = ts ? findSheet(doc, ts.sheet) : undefined
  if (!ts || !sheet) return null
  const im = src.image(sheet.file)
  if (im.status !== 'ready' || !im.img) return null
  const { cols, rows } = sheetTiles(sheet, im.width, im.height)
  if (cols < 1 || rows < 1) return null
  const anims = new Map<number, { frames: number[]; ms: number }>()
  for (const k of Object.keys(ts.animations ?? {})) {
    const a = animationOf(ts, Number(k))
    if (a && a.frames.length > 0) anims.set(Number(k), a)
  }
  return { tileset: layer.tileset, cells, img: im.img, cw: sheet.cellWidth, ch: sheet.cellHeight, cols, rows, anims, terrainOver }
}

/** The tile to draw for `tile` at running time `timeMs` (animated tiles cycle through their frames, like the player's TilesetDef::frame). */
export function tileFrame(s: TileScene, tile: number, timeMs: number): number {
  const a = s.anims.get(tile)
  return a ? a.frames[Math.floor(timeMs / Math.max(1, a.ms)) % a.frames.length] : tile
}

/** Some square shows an animated tile. */
export function hasAnimatedTiles(s: TileScene | null): boolean {
  if (!s || s.anims.size === 0) return false
  for (const v of s.cells) if (v >= 0 && s.anims.has(v)) return true
  return false
}

/** Draws tile `tile` stretched into a rectangle (hard pixels). False when the tile lies outside the image. */
export function drawTile(g: CanvasRenderingContext2D, s: TileScene, tile: number, dx: number, dy: number, dw: number, dh: number): boolean {
  if (tile < 0 || tile >= s.cols * s.rows || dw < 1 || dh < 1) return false
  g.imageSmoothingEnabled = false
  g.drawImage(s.img, (tile % s.cols) * s.cw, Math.floor(tile / s.cols) * s.ch, s.cw, s.ch, dx, dy, dw, dh)
  return true
}
