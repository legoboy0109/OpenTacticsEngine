import { describe, expect, it } from 'vitest'
import {
  addTileLayer, dropTileLayer, encodeRow, MAX_TILES, NO_TILE, readTiles, rectCells, resizeTileLayer, setLayerTileset, setTiles, stampTiles, tileAt, tileCount, tileLayerOf,
  writeTiles,
} from '../../src/model/tiles'
import type { MapFile } from '../../src/model/types'

const map = (w = 4, h = 3): MapFile => ({ name: 'm', grid: { type: 'square', width: w, height: h }, legend: {}, tiles: [] })
const tiled = (w = 4, h = 3) => addTileLayer(map(w, h), 'world')

describe('tile layer text', () => {
  it('a new layer has one row of "...." per map row', () => {
    const m = tiled()
    expect(m.tileLayer).toEqual({ tileset: 'world', cells: ['................', '................', '................'] })
    expect(Array.from(readTiles(m)!)).toEqual(new Array(12).fill(NO_TILE))
    expect(readTiles(map())).toBeNull()
    expect(tileCount(m)).toBe(0)
  })

  it('round trips numbers as four hex digits', () => {
    const t = new Int32Array([0, 255, -1, 65535, 4096, 10, 11, 12, -1, -1, -1, 1])
    const m = writeTiles(tiled(), t)
    expect(m.tileLayer!.cells[0]).toBe('000000ff....ffff')
    expect(m.tileLayer!.cells[1]).toBe('1000000a000b000c')
    expect(Array.from(readTiles(m)!)).toEqual(Array.from(t))
    expect(encodeRow(t, 4, 2)).toBe('............0001')
    expect(MAX_TILES).toBe(65536)
    expect(tileCount(m)).toBe(8)
    expect(tileAt(m, 1, 0)).toBe(255)
    expect(tileAt(m, 9, 9)).toBe(NO_TILE)
  })

  it('reads rows given as numbers (null and -1 are none) and ignores bad text', () => {
    const m = { ...map(3, 2), tileLayer: { tileset: 'w', cells: [[1, null, -1], 'zzzz0005....'] } } as unknown as MapFile
    expect(Array.from(readTiles(m)!)).toEqual([1, -1, -1, -1, 5, -1])
    const short = { ...map(3, 2), tileLayer: { tileset: 'w', cells: ['0001'] } } as unknown as MapFile
    expect(Array.from(readTiles(short)!)).toEqual([1, -1, -1, -1, -1, -1])
  })

  it('writeTiles needs a layer; drop removes it', () => {
    const m = map()
    expect(writeTiles(m, new Int32Array(12))).toBe(m)
    const t = tiled()
    expect(dropTileLayer(t).tileLayer).toBeUndefined()
    expect(dropTileLayer(m)).toBe(m)
    expect(tileLayerOf(t)!.tileset).toBe('world')
    expect(tileLayerOf(m)).toBeNull()
  })

  it('switching the tileset keeps the tiles', () => {
    const m = setTiles(tiled(), [{ x: 0, y: 0 }], 7)
    const s = setLayerTileset(m, 'dungeon')
    expect(s.tileLayer!.tileset).toBe('dungeon')
    expect(s.tileLayer!.cells).toEqual(m.tileLayer!.cells)
    expect(setLayerTileset(s, 'dungeon')).toBe(s)
  })
})

describe('painting tiles', () => {
  it('setTiles paints, erases, ignores outside squares and returns the same map when nothing changes', () => {
    const m = tiled()
    const a = setTiles(m, [{ x: 1, y: 1 }, { x: 9, y: 9 }, { x: -1, y: 0 }], 20)
    expect(tileAt(a, 1, 1)).toBe(20)
    expect(tileCount(a)).toBe(1)
    expect(setTiles(a, [{ x: 1, y: 1 }], 20)).toBe(a)
    const e = setTiles(a, [{ x: 1, y: 1 }], NO_TILE)
    expect(tileCount(e)).toBe(0)
    expect(setTiles(e, [{ x: 1, y: 1 }], NO_TILE)).toBe(e)
    const plain = map()
    expect(setTiles(plain, [{ x: 0, y: 0 }], 1)).toBe(plain)
  })

  it('rectCells covers the rectangle in either order, clipped to the map', () => {
    const m = map(4, 3)
    expect(rectCells(m, { x: 2, y: 1 }, { x: 1, y: 0 }).map((c) => `${c.x},${c.y}`)).toEqual(['1,0', '2,0', '1,1', '2,1'])
    expect(rectCells(m, { x: 2, y: 2 }, { x: 9, y: 9 }).map((c) => `${c.x},${c.y}`)).toEqual(['2,2', '3,2'])
    expect(rectCells(m, { x: -3, y: -3 }, { x: 0, y: 0 })).toHaveLength(1)
  })

  it('stampTiles places a block, skips "none" parts and outside squares, identity when unchanged', () => {
    const m = tiled()
    const block = { w: 2, h: 2, tiles: [1, -1, 3, 4] }
    const a = stampTiles(m, { x: 2, y: 1 }, block)
    expect(tileAt(a, 2, 1)).toBe(1)
    expect(tileAt(a, 3, 1)).toBe(NO_TILE)
    expect(tileAt(a, 2, 2)).toBe(3)
    expect(tileAt(a, 3, 2)).toBe(4)
    expect(stampTiles(a, { x: 2, y: 1 }, block)).toBe(a)
    const edge = stampTiles(m, { x: 3, y: 2 }, block)
    expect(tileCount(edge)).toBe(1)
    expect(tileAt(edge, 3, 2)).toBe(1)
    expect(stampTiles(map(), { x: 0, y: 0 }, block)).toBeTruthy()
  })
})

describe('resizing', () => {
  it('moves old squares by the offset and leaves new ones empty', () => {
    const m = setTiles(setTiles(tiled(2, 2), [{ x: 0, y: 0 }], 5), [{ x: 1, y: 1 }], 6)
    const l = resizeTileLayer(m, 4, 3, 1, 1)!
    const r = { ...m, grid: { ...m.grid, width: 4, height: 3 }, tileLayer: l }
    expect(l.cells).toHaveLength(3)
    expect(tileAt(r, 1, 1)).toBe(5)
    expect(tileAt(r, 2, 2)).toBe(6)
    expect(tileCount(r)).toBe(2)
    // Cropping: the offset pushes the first column out.
    const c = resizeTileLayer(m, 1, 2, -1, 0)!
    const rc = { ...m, grid: { ...m.grid, width: 1, height: 2 }, tileLayer: c }
    expect(tileAt(rc, 0, 1)).toBe(6)
    expect(tileCount(rc)).toBe(1)
    expect(resizeTileLayer(map(), 3, 3, 0, 0)).toBeUndefined()
  })
})
