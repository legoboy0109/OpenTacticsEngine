import { describe, expect, it } from 'vitest'
import { loadTemplate } from './fixtures'
import {
  addTileset, animationOf, findTileset, mapsUsingTileset, markAnimated, moveTileset, parseFrames, removeSheetAndTilesets, removeTileset, renameTileset,
  setAnimation, setTilesetSheet, sheetTiles, stripMapTiles, tilesetIdProblem, tilesetList, tilesetUsageLines, unmarkAnimated,
} from '../../src/model/tilesets'
import { addTileLayer, tileLayerOf } from '../../src/model/tiles'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import type { Command } from '../../src/model/storyTypes'

const blank = (): ProjectDoc => {
  const d = loadTemplate('strategy-rpg')
  return { ...d, graphics: { sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 16, cellHeight: 16 }, { id: 'other', file: 'art/o.png', cellWidth: 16, cellHeight: 16 }] } }
}
const mapId = (d: ProjectDoc) => Object.keys(d.maps)[0]
/** A doc with tilesets "world" (sheet tiles) and "dungeon" (sheet other), the first map tiled with "world". */
const tiled = (): ProjectDoc => {
  let d = addTileset(addTileset(blank(), 'world'), 'dungeon')
  d = setTilesetSheet(d, 'dungeon', 'other')
  const id = mapId(d)
  const m = addTileLayer(d.maps[id]!, 'world')
  const nested: Command[] = [{ setTerrain: 'plain', cells: [[0, 0]], tile: 3, keepTile: true }, { setFog: true }]
  const events = [{ id: 'e1', when: { turn: 2 }, do: [{ setTerrain: 'plain', area: { x: 0, y: 0, w: 1, h: 1 }, tile: 4 }, { if: [], then: nested, else: [{ setTerrain: 'plain', cells: [[1, 1]], tile: null }] }] }]
  const withStuff = { ...m, changes: { c1: { keepTile: true, cells: [{ x: 0, y: 0, terrain: 'plain', tile: 5, keepTile: false }, { x: 1, y: 0, terrain: 'plain' }] } }, events } as unknown as MapFile
  return { ...d, maps: { ...d.maps, [id]: withStuff } }
}

describe('tileset edits', () => {
  it('adds on the first image, refuses bad ids, renames in place and moves', () => {
    let d = blank()
    expect(addTileset({ ...d, graphics: { sheets: [] } }, 'x').graphics!.tilesets).toBeUndefined()
    d = addTileset(d, 'a')
    d = addTileset(d, 'b')
    expect(tilesetList(d).map((t) => t.id)).toEqual(['a', 'b'])
    expect(findTileset(d, 'a')).toEqual({ sheet: 'tiles' })
    expect(tilesetIdProblem(d, '')).toMatch(/name/)
    expect(tilesetIdProblem(d, 'a b')).toMatch(/letters/)
    expect(tilesetIdProblem(d, 'a')).toMatch(/already/)
    expect(tilesetIdProblem(d, 'a', 'a')).toBeNull()
    expect(addTileset(d, 'a')).toBe(d)
    d = moveTileset(d, 0, 1)
    expect(tilesetList(d).map((t) => t.id)).toEqual(['b', 'a'])
    expect(moveTileset(d, 0, 5)).toBe(d)
    d = renameTileset(d, 'b', 'bb')
    expect(tilesetList(d).map((t) => t.id)).toEqual(['bb', 'a'])
    expect(renameTileset(d, 'bb', 'a')).toBe(d)
  })

  it('renaming follows the tile layers of the maps; changing the image keeps the rest', () => {
    let d = tiled()
    d = renameTileset(d, 'world', 'overworld')
    expect(tileLayerOf(d.maps[mapId(d)])!.tileset).toBe('overworld')
    expect(mapsUsingTileset(d, 'overworld')).toEqual([mapId(d)])
    expect(mapsUsingTileset(d, 'dungeon')).toEqual([])
    d = setTilesetSheet(d, 'overworld', 'other')
    expect(findTileset(d, 'overworld')!.sheet).toBe('other')
    expect(setTilesetSheet(d, 'overworld', 'other')).toBe(d)
  })

  it('sheetTiles counts whole cells', () => {
    const sh = { id: 's', file: 'a', cellWidth: 16, cellHeight: 16 }
    expect(sheetTiles(sh, 128, 64)).toEqual({ cols: 8, rows: 4, count: 32 })
    expect(sheetTiles(sh, 130, 70)).toEqual({ cols: 8, rows: 4, count: 32 })
    expect(sheetTiles(sh, 0, 0).count).toBe(0)
    expect(sheetTiles(undefined, 128, 64).count).toBe(0)
  })
})

describe('animated tiles', () => {
  it('mark, set, unmark; entries stay sorted; default ms is not written', () => {
    let d = addTileset(blank(), 'w')
    d = markAnimated(d, 'w', 9, 32)
    d = markAnimated(d, 'w', 8, 32)
    expect(Object.keys(findTileset(d, 'w')!.animations!)).toEqual(['8', '9'])
    expect(animationOf(findTileset(d, 'w'), 8)).toEqual({ frames: [8, 9], ms: 200 })
    expect(markAnimated(d, 'w', 8, 32)).toBe(d)
    expect(markAnimated(d, 'w', 40, 32)).toBe(d)
    d = setAnimation(d, 'w', 8, [8, 9, 10], 300)
    expect(findTileset(d, 'w')!.animations!['8']).toEqual({ frames: [8, 9, 10], ms: 300 })
    d = setAnimation(d, 'w', 8, [8, 9, 10], 200)
    expect(findTileset(d, 'w')!.animations!['8']).toEqual({ frames: [8, 9, 10] })
    expect(setAnimation(d, 'w', 20, [1, 2])).toBe(d)
    d = unmarkAnimated(unmarkAnimated(d, 'w', 8), 'w', 9)
    expect(findTileset(d, 'w')).toEqual({ sheet: 'tiles' })
    expect(unmarkAnimated(d, 'w', 9)).toBe(d)
    expect(animationOf(findTileset(d, 'w'), 1)).toBeNull()
  })

  it('the last tile of the sheet animates with itself, parseFrames reads lists', () => {
    const d = markAnimated(addTileset(blank(), 'w'), 'w', 31, 32)
    expect(animationOf(findTileset(d, 'w'), 31)!.frames).toEqual([31, 31])
    expect(parseFrames('8, 9 10;11')).toEqual([8, 9, 10, 11])
    expect(parseFrames('')).toBeNull()
    expect(parseFrames('8, x')).toBeNull()
    expect(parseFrames('-1, 2')).toBeNull()
    expect(parseFrames('1.5')).toBeNull()
  })
})

describe('removal', () => {
  it('stripMapTiles removes the layer and every tile key, in nested commands too', () => {
    const d = tiled()
    const m = stripMapTiles(d.maps[mapId(d)]!)
    expect(tileLayerOf(m)).toBeNull()
    expect(m.changes!.c1).toEqual({ cells: [{ x: 0, y: 0, terrain: 'plain' }, { x: 1, y: 0, terrain: 'plain' }] })
    const e = m.events![0].do as Command[]
    expect(e[0]).toEqual({ setTerrain: 'plain', area: { x: 0, y: 0, w: 1, h: 1 } })
    expect(e[1].then![0]).toEqual({ setTerrain: 'plain', cells: [[0, 0]] })
    expect(e[1].then![1]).toEqual({ setFog: true })
    expect(e[1].else![0]).toEqual({ setTerrain: 'plain', cells: [[1, 1]] })
    expect(stripMapTiles(m)).toBe(m)
  })

  it('removeTileset takes its maps tile layers with it, and only those', () => {
    const d = tiled()
    expect(tilesetUsageLines(d, ['world'])).toEqual([{ id: 'world', mapId: mapId(d), mapName: expect.any(String) }])
    expect(tilesetUsageLines(d, ['dungeon'])).toEqual([])
    const keep = removeTileset(d, 'dungeon')
    expect(keep.maps[mapId(d)]).toBe(d.maps[mapId(d)])
    expect(tilesetList(keep).map((t) => t.id)).toEqual(['world'])
    const gone = removeTileset(d, 'world')
    expect(tileLayerOf(gone.maps[mapId(d)])).toBeNull()
    expect(tilesetList(gone).map((t) => t.id)).toEqual(['dungeon'])
    expect(removeTileset(gone, 'world')).toBe(gone)
    expect(removeTileset(removeTileset(d, 'world'), 'dungeon').graphics!.tilesets).toBeUndefined()
  })

  it('removing the image removes its tilesets and their layers in one edit', () => {
    const d = tiled()
    const out = removeSheetAndTilesets(d, 'tiles')
    expect(out.graphics!.sheets.map((s) => s.id)).toEqual(['other'])
    expect(tilesetList(out).map((t) => t.id)).toEqual(['dungeon'])
    expect(tileLayerOf(out.maps[mapId(d)])).toBeNull()
    const other = removeSheetAndTilesets(d, 'other')
    expect(tilesetList(other).map((t) => t.id)).toEqual(['world'])
    expect(tileLayerOf(other.maps[mapId(d)])!.tileset).toBe('world')
  })
})
