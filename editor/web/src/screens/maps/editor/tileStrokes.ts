// What one drag does with the Tiles tool (docs/EDITOR_UI.md "Tiles tool"): paint the chosen tile (or stamp the chosen block of the palette),
// erase, fill a rectangle, or pick the tile under the pointer. While the Changes tab has "Paint into this change" on, the squares go into the
// change instead of the map (the same way the Terrain tool does).
import { ctxOf, editMap } from '../../../model/mapCtx'
import { gridOfGame, isHex } from '../../../model/gridSpec'
import { lineCells } from '../../../model/mapPaint'
import { paintChangeTiles } from '../../../model/mapChanges'
import type { Cell } from '../../../model/mapGrid'
import { addTileLayer, NO_TILE, readTiles, tileLayerOf, writeTiles } from '../../../model/tiles'
import type { MapFile } from '../../../model/types'
import { STROKE_PREFIX } from '../../../store/history'
import type { Stroke, StrokeEnv, ToolState } from './tools'
import { TL } from './tilesText'

const NOOP: Stroke = { move() {}, end() {} }
let counter = 0

/** The palette's chosen tiles: a block of w x h tile numbers, row by row. */
export const blockOf = (t: ToolState): { w: number; h: number; tiles: number[] } => t.tileBlock ?? { w: 1, h: 1, tiles: [] }

/** The tile a pattern gives the square (x, y) when painted from (ox, oy): the block repeats from there. */
const patternTile = (b: { w: number; h: number; tiles: number[] }, x: number, y: number, ox: number, oy: number): number =>
  b.tiles[(((y - oy) % b.h) + b.h) % b.h * b.w + (((x - ox) % b.w) + b.w) % b.w] ?? NO_TILE

export function tileStroke(env: StrokeEnv, first: Cell, t: ToolState, opts: { alt?: boolean }): Stroke {
  const doc = env.store.getState().doc
  const map = doc.maps[env.mapId]
  const ctx = ctxOf(doc)
  if (!map || !ctx) return NOOP
  if (isHex(gridOfGame(doc.game))) { env.say(TL.noHex); return NOOP }
  const layer = tileLayerOf(map)
  const tileset = layer?.tileset ?? t.tileTileset ?? ''
  const mode = opts.alt ? 'pick' : t.tileMode ?? 'paint'

  if (mode === 'pick') {
    const tile = readTiles(map)?.[first.y * map.grid.width + first.x] ?? NO_TILE
    if (tile < 0) env.say(TL.pickedNone)
    else env.pickTile?.(tile)
    return NOOP
  }
  if (!tileset) { env.say(TL.needTileset); return NOOP }
  const block = blockOf(t)
  if (mode !== 'erase' && block.tiles.length === 0) { env.say(TL.needTile); return NOOP }

  const into = env.view.paintChange
  // Writes squares (x, y, tile) into the map's layer (created on the first tile), or into the selected change.
  const write = (entries: Array<{ x: number; y: number; tile: number }>, key: string) => {
    if (!entries.length) return
    if (into) {
      env.store.edit((d) => editMap(d, env.mapId, (m, c) => paintChangeTiles(m, c.terrain, into, entries)), key)
      return
    }
    env.store.edit((d) => editMap(d, env.mapId, (m) => {
      const base: MapFile = tileLayerOf(m) ? m : addTileLayer(m, tileset)
      const tiles = readTiles(base)!
      const w = base.grid.width, h = base.grid.height
      let changed = base !== m
      for (const e of entries) {
        if (e.x < 0 || e.y < 0 || e.x >= w || e.y >= h || tiles[e.y * w + e.x] === e.tile) continue
        tiles[e.y * w + e.x] = e.tile
        changed = true
      }
      return changed ? writeTiles(base, tiles) : m
    }), key)
  }
  const key = `${STROKE_PREFIX}${++counter}`

  if (mode === 'rect') {
    const set = (to: Cell) => env.view.set({ pickRect: { x: Math.min(first.x, to.x), y: Math.min(first.y, to.y), w: Math.abs(to.x - first.x) + 1, h: Math.abs(to.y - first.y) + 1 } })
    set(first)
    return {
      move: set,
      end(c) {
        env.view.set({ pickRect: null })
        const to = c ?? first
        const x0 = Math.min(first.x, to.x), x1 = Math.max(first.x, to.x), y0 = Math.min(first.y, to.y), y1 = Math.max(first.y, to.y)
        const entries: Array<{ x: number; y: number; tile: number }> = []
        for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) entries.push({ x, y, tile: patternTile(block, x, y, x0, y0) })
        write(entries, key)
      },
    }
  }
  if (mode === 'erase') {
    write([{ x: first.x, y: first.y, tile: NO_TILE }], key)
    let last = first
    return { move(c) { write(lineCells(last, c, gridOfGame(doc.game)).map((q) => ({ x: q.x, y: q.y, tile: NO_TILE })), key); last = c }, end() {} }
  }
  // Paint: a click stamps the whole block with its top-left square on the clicked square; dragging goes on painting the block's pattern.
  write(Array.from({ length: block.h }, (_, j) => Array.from({ length: block.w }, (_, i) => ({ x: first.x + i, y: first.y + j, tile: block.tiles[j * block.w + i] }))).flat().filter((e) => e.tile >= 0), key)
  let last = first
  return {
    move(c) {
      write(lineCells(last, c, gridOfGame(doc.game)).map((q) => ({ x: q.x, y: q.y, tile: patternTile(block, q.x, q.y, first.x, first.y) })).filter((e) => e.tile >= 0), key)
      last = c
    },
    end() {},
  }
}
