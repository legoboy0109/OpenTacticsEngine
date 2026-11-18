// The map editor's side of the tile layer (docs/GRAPHICS.md "Tile layer", docs/EVENTS.md "Changing the map"): tiles in a change (painting into it,
// recording, forgetting, keepTile), the "after" view, resizing, the checks that mirror the loader and the canvas's tile scene.
import { describe, expect, it } from 'vitest'
import { buildTileScene, hasAnimatedTiles, tileFrame } from '../../src/art/tileScene'
import { addChange, applyToMap, baseTerrainAt, cellsOf, changesOf, changeTileCount, clearChangeTiles, paintChange, paintChangeTiles, recordChangeTiles, setChangeKeepTile } from '../../src/model/mapChanges'
import { mapWarnings } from '../../src/model/mapChecks'
import { editMap } from '../../src/model/mapCtx'
import { resizeMap } from '../../src/model/mapResize'
import { tileChecks } from '../../src/model/tileChecks'
import { addTileLayer, NO_TILE, readTiles, tileAt, writeTiles } from '../../src/model/tiles'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const SIZE = () => ({ width: 128, height: 64 })  // 8 x 4 tiles of 16 px = 32 tiles

function docWithTiles(): { doc: ProjectDoc; id: string } {
  const d = loadDemo('demo-srpg')
  const id = d.game!.maps[0].id
  const graphics = { sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 16, cellHeight: 16 }], tilesets: { test: { sheet: 'tiles', animations: { '8': { frames: [8, 9, 10], ms: 200 } } } } }
  let doc: ProjectDoc = { ...d, graphics }
  doc = editMap(doc, id, (m) => addTileLayer(m, 'test'))
  return { doc, id }
}
const mapOf = (d: ProjectDoc, id: string): MapFile => d.maps[id]!
const terrains = (d: ProjectDoc) => d.terrain!.map((t) => t.id)

describe('tiles in a change', () => {
  it('painting tiles into a change adds the square with its present terrain, and the same as the map leaves it again', () => {
    const { doc, id } = docWithTiles()
    let m = addChange(mapOf(doc, id), 'dry')
    m = writeTiles(m, new Int32Array(m.grid.width * m.grid.height).fill(8))
    m = paintChangeTiles(m, doc.terrain, 'dry', [{ x: 1, y: 1, tile: 5 }, { x: 2, y: 1, tile: NO_TILE }])
    const cells = cellsOf(changesOf(m).dry)
    expect(cells).toHaveLength(2)
    expect(cells[0]).toMatchObject({ x: 1, y: 1, tile: 5 })
    expect(cells[1]).toMatchObject({ x: 2, y: 1, tile: -1 })
    // The same tile the map has leaves the change again.
    const back = paintChangeTiles(m, doc.terrain, 'dry', [{ x: 1, y: 1, tile: 8 }])
    expect(cellsOf(changesOf(back).dry).map((k) => `${k.x},${k.y}`)).toEqual(['2,1'])
    // Nothing happens without a layer, off the map, or when nothing changes.
    expect(paintChangeTiles({ ...m, tileLayer: undefined } as MapFile, doc.terrain, 'dry', [{ x: 0, y: 0, tile: 1 }])).toEqual({ ...m, tileLayer: undefined })
    expect(paintChangeTiles(m, doc.terrain, 'dry', [{ x: 99, y: 0, tile: 1 }])).toBe(m)
    expect(paintChangeTiles(m, doc.terrain, 'dry', [{ x: 1, y: 1, tile: 5 }])).toBe(m)
    expect(changeTileCount(changesOf(m).dry)).toBe(2)
  })

  it('painting terrain into a change keeps a square that carries a tile', () => {
    const { doc, id } = docWithTiles()
    let m = writeTiles(addChange(mapOf(doc, id), 'dry'), new Int32Array(mapOf(doc, id).grid.width * mapOf(doc, id).grid.height).fill(3))
    m = paintChangeTiles(m, doc.terrain, 'dry', [{ x: 0, y: 0, tile: 5 }])
    const base = cellsOf(changesOf(m).dry)[0].terrain
    // painting its own base terrain back would drop a plain entry, but this one has a tile
    const again = paintChange(m, doc.terrain, 'dry', [{ x: 0, y: 0 }], base)
    expect(cellsOf(changesOf(again).dry)).toHaveLength(1)
  })

  it('records the map\'s present tiles, forgets them, and keepTile is a switch', () => {
    const { doc, id } = docWithTiles()
    const t = new Int32Array(mapOf(doc, id).grid.width * mapOf(doc, id).grid.height).fill(NO_TILE)
    t[0] = 12
    let m = writeTiles(addChange(mapOf(doc, id), 'c'), t)
    m = paintChange(m, doc.terrain, 'c', [{ x: 0, y: 0 }, { x: 1, y: 0 }], terrains(doc).find((x) => x !== cellsOf(changesOf(m).c)[0]?.terrain && x !== 'plain') ?? terrains(doc)[1])
    expect(cellsOf(changesOf(m).c)).toHaveLength(2)
    const rec = recordChangeTiles(m, 'c')
    expect(cellsOf(changesOf(rec).c).map((k) => k.tile)).toEqual([12, -1])
    expect(recordChangeTiles(rec, 'c')).toBe(rec)
    const forgot = clearChangeTiles(rec, 'c')
    expect(cellsOf(changesOf(forgot).c).some((k) => 'tile' in k)).toBe(false)
    expect(clearChangeTiles(forgot, 'c')).toBe(forgot)
    const keep = setChangeKeepTile(m, 'c', true)
    expect(changesOf(keep).c.keepTile).toBe(true)
    expect(setChangeKeepTile(keep, 'c', true)).toBe(keep)
    expect('keepTile' in changesOf(setChangeKeepTile(keep, 'c', false)).c).toBe(false)
    expect(clearChangeTiles(keep, 'c').changes!.c).not.toHaveProperty('keepTile')
  })

  it('the "after" view follows the game: a given tile, a cleared tile, keepTile', () => {
    const { doc, id } = docWithTiles()
    const w = mapOf(doc, id).grid.width
    let m = writeTiles(addChange(mapOf(doc, id), 'c'), new Int32Array(w * mapOf(doc, id).grid.height).fill(8))
    const other = terrains(doc).find((x) => x !== terrains(doc)[0])!
    m = { ...m, changes: { c: { cells: [
      { x: 0, y: 0, terrain: other, tile: 5 },
      { x: 1, y: 0, terrain: other },
      { x: 2, y: 0, terrain: other, keepTile: true },
      { x: 3, y: 0, terrain: baseTerrainAt(m, doc.terrain, 3, 0)! },  // the terrain does not really change: the tile stays
      { x: 4, y: 0, terrain: baseTerrainAt(m, doc.terrain, 4, 0)!, tile: null },
    ] } } }

    const after = applyToMap(m, doc.terrain, 'c')
    const t = readTiles(after)!
    expect([t[0], t[1], t[2], t[3], t[4], t[5]]).toEqual([5, -1, 8, 8, -1, 8])
    const wholeKeep = applyToMap({ ...m, changes: { c: { keepTile: true, cells: [{ x: 1, y: 0, terrain: other }] } } }, doc.terrain, 'c')
    expect(tileAt(wholeKeep, 1, 0)).toBe(8)
    expect(applyToMap(addChange(mapOf(doc, id), 'empty'), doc.terrain, 'empty')).toEqual(addChange(mapOf(doc, id), 'empty'))
  })
})

describe('resizing keeps the tile layer in step', () => {
  it('moves the tiles with the map and gives new squares none', () => {
    const { doc, id } = docWithTiles()
    const m0 = mapOf(doc, id)
    const w = m0.grid.width, h = m0.grid.height
    const t = new Int32Array(w * h).fill(NO_TILE)
    t[0] = 7
    t[w * h - 1] = 9
    const m = writeTiles(m0, t)
    const grown = resizeMap(m, doc.terrain!, doc.structures ?? [], w + 2, h + 1, 'se')
    const g = readTiles(grown)!
    expect(grown.tileLayer!.cells).toHaveLength(h + 1)
    expect(g.length).toBe((w + 2) * (h + 1))
    expect(g[2 + (w + 2) * 1]).toBe(7)  // the old top-left moved by (2, 1)
    expect(g[(w + 2) * (h + 1) - 1]).toBe(9)
    expect(g[0]).toBe(NO_TILE)
    const cropped = resizeMap(m, doc.terrain!, doc.structures ?? [], w - 1, h - 1, 'nw')
    expect(readTiles(cropped)![0]).toBe(7)
    expect(cropped.tileLayer!.cells[0]).toHaveLength((w - 1) * 4)
    // A map with no layer stays without one.
    expect((resizeMap(m0 as MapFile & { tileLayer?: unknown }, doc.terrain!, [], w + 1, h, 'nw') as MapFile).tileLayer).toBeDefined()
  })
})

describe('checks mirror the loader', () => {
  const say = (d: ProjectDoc, id: string) => tileChecks(d, id, SIZE).map((c) => c.text)

  it('a good layer has nothing to say', () => {
    const { doc, id } = docWithTiles()
    expect(say(doc, id)).toEqual([])
    expect(mapWarnings(doc, mapOf(doc, id), SIZE).filter((x) => x.key.startsWith('tiles:'))).toEqual([])
  })

  it('names an unknown tileset, a missing image, wrong rows and bad squares', () => {
    const { doc, id } = docWithTiles()
    const m = mapOf(doc, id)
    const set = (patch: Partial<NonNullable<MapFile['tileLayer']>>) => ({ ...doc, maps: { ...doc.maps, [id]: { ...m, tileLayer: { ...m.tileLayer!, ...patch } } } }) as ProjectDoc
    expect(say(set({ tileset: 'nope' }), id)[0]).toMatch(/tileset “nope”, which this game does not have/)
    expect(say({ ...doc, graphics: { ...doc.graphics!, tilesets: { test: { sheet: 'gone' } } } }, id)[0]).toMatch(/uses the image “gone”/)
    expect(say(set({ cells: m.tileLayer!.cells.slice(1) }), id)[0]).toMatch(/has \d+ rows but the map is \d+ squares high/)
    expect(say(set({ cells: m.tileLayer!.cells.map((r, i) => (i === 0 ? r.slice(4) : r)) }), id)[0]).toMatch(/Row 1 of the tile layer has \d+ characters but \d+ are needed/)
    expect(say(set({ cells: m.tileLayer!.cells.map((r, i) => (i === 0 ? 'zz' + r.slice(2) : r)) }), id)[0]).toMatch(/Square 1, 1 of the tile layer is “zz\.\.”/)
    expect(say(set({ cells: m.tileLayer!.cells.map((r, i) => (i === 0 ? '0020' + r.slice(4) : r)) }), id)[0]).toMatch(/uses tile 32, but the tileset “test” has only 32 tiles \(0 to 31\)/)
    expect(say(set({ cells: 5 as unknown as string[] }), id).join(' ')).toMatch(/no list of rows/)
    expect(say({ ...doc, maps: { ...doc.maps, [id]: { ...m, tileLayer: 'x' } as unknown as MapFile } }, id)[0]).toMatch(/not written as/)
  })

  it('checks the tiles carried by changes and setTerrain commands, and needs a layer for them', () => {
    const { doc, id } = docWithTiles()
    const m = mapOf(doc, id)
    const other = terrains(doc)[1]
    const withChange = (tile: unknown, keep?: unknown) => ({ ...doc, maps: { ...doc.maps, [id]: { ...m, changes: { c: { cells: [{ x: 0, y: 0, terrain: other, tile, keepTile: keep }] } } } as unknown as MapFile } }) as ProjectDoc
    expect(say(withChange(5), id)).toEqual([])
    expect(say(withChange(null), id)).toEqual([])
    expect(say(withChange(99), id)[0]).toMatch(/The change “c”, square 1: the tile must be -1 \(none\) or a tile number from 0 to 31/)
    expect(say(withChange('five'), id)[0]).toMatch(/the tile must be/)
    expect(say(withChange(undefined, 'yes'), id)[0]).toMatch(/“keep the tile” must be on or off/)
    const cmd = (tile: unknown) => ({ ...doc, maps: { ...doc.maps, [id]: { ...m, events: [{ id: 'e', when: { turn: 2 }, do: [{ setTerrain: other, area: { x: 0, y: 0, w: 1, h: 1 }, tile }] }] } as unknown as MapFile } }) as ProjectDoc
    expect(say(cmd(4), id)).toEqual([])
    expect(say(cmd(-5), id)[0]).toMatch(/Event “e”, command 1: the tile must be/)
    // No layer: a tile needs one.
    const none = { ...withChange(5), maps: { ...doc.maps, [id]: { ...(withChange(5).maps[id] as MapFile), tileLayer: undefined } } } as ProjectDoc
    expect(say(none, id)[0]).toMatch(/a tile needs a tile layer/)
  })

  it('without image sizes the range of tile numbers is not checked', () => {
    const { doc, id } = docWithTiles()
    const m = mapOf(doc, id)
    const d = { ...doc, maps: { ...doc.maps, [id]: { ...m, tileLayer: { ...m.tileLayer!, cells: m.tileLayer!.cells.map((r, i) => (i === 0 ? 'ffff' + r.slice(4) : r)) } } } } as ProjectDoc
    expect(tileChecks(d, id)).toEqual([])
    expect(tileChecks(d, id, SIZE)).toHaveLength(1)
  })
})

describe('the canvas tile scene', () => {
  const src = (ready = true) => ({ image: () => ({ status: ready ? 'ready' : 'loading', img: ready ? ({} as CanvasImageSource) : null, width: 128, height: 64 }) as ReturnType<import('../../src/art/scene').ImageSource['image']> })

  it('is null without a layer, a tileset, or a loaded picture', () => {
    const { doc, id } = docWithTiles()
    expect(buildTileScene(doc, mapOf(doc, id), src(), false)).not.toBeNull()
    expect(buildTileScene(doc, mapOf(doc, id), src(false), false)).toBeNull()
    expect(buildTileScene(doc, { ...mapOf(doc, id), tileLayer: undefined } as MapFile, src(), false)).toBeNull()
    expect(buildTileScene({ ...doc, graphics: { ...doc.graphics!, tilesets: {} } }, mapOf(doc, id), src(), false)).toBeNull()
  })

  it('animated tiles cycle through their frames and only count when a square shows one', () => {
    const { doc, id } = docWithTiles()
    const w = mapOf(doc, id).grid.width
    const s0 = buildTileScene(doc, mapOf(doc, id), src(), true)!
    expect(s0.cols).toBe(8)
    expect(s0.rows).toBe(4)
    expect(s0.terrainOver).toBe(true)
    expect(hasAnimatedTiles(s0)).toBe(false)
    const m = writeTiles(mapOf(doc, id), new Int32Array(w * mapOf(doc, id).grid.height).fill(8))
    const s = buildTileScene(doc, m, src(), false)!
    expect(hasAnimatedTiles(s)).toBe(true)
    expect([0, 200, 450, 600].map((t) => tileFrame(s, 8, t))).toEqual([8, 9, 10, 8])
    expect(tileFrame(s, 13, 450)).toBe(13)
  })
})
