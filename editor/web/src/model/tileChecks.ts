// Checks on a map's tile layer and the tiles its changes and commands carry. They mirror what the loader refuses
// (engine/core/data/loader_map_tiles.cpp, loader_map_changes.cpp, loader_story_mapchange.cpp, loader_tilesets.cpp) in the same plain
// words, so the maker sees a problem before the game says no. `imageSize` (the image library's width and height of a file) lets the
// range of tile numbers be checked; without it that one check is skipped.
import { isHex, gridOfGame } from './gridSpec'
import { findSheet } from './graphics'
import { cellsOf, changesOf } from './mapChanges'
import { forEachCommand } from './storyWalk'
import { tileLayerOf } from './tiles'
import { findTileset, sheetTiles } from './tilesets'
import type { MapFile, ProjectDoc } from './types'

export interface TileCheck { mapId: string; text: string }
export type ImageSize = (file: string) => { width: number; height: number } | undefined

export function tileChecks(doc: ProjectDoc, mapId: string, imageSize?: ImageSize): TileCheck[] {
  const map = doc.maps[mapId]
  const out: TileCheck[] = []
  if (!map) return out
  const say = (text: string) => out.push({ mapId, text })
  const raw = (map as { tileLayer?: unknown }).tileLayer
  const layer = tileLayerOf(map)
  let count = 0  // tiles in the tileset (0 = unknown)
  if (raw !== undefined) {
    if (!layer) say('The tile layer is not written as { tileset, cells }. The game will not load until it is fixed or taken away.')
    else {
      if (isHex(gridOfGame(doc.game))) say('Tile layers are not supported on hexagon games. The game will not load until the tile layer is taken away.')
      const ts = findTileset(doc, layer.tileset)
      if (!ts) say(`The tile layer uses the tileset “${layer.tileset}”, which this game does not have (Art screen, Tilesets). The game will not load until it is added or the tile layer taken away.`)
      else {
        const sheet = findSheet(doc, ts.sheet)
        const size = sheet && imageSize ? imageSize(sheet.file) : undefined
        if (!sheet) say(`The tileset “${layer.tileset}” uses the image “${ts.sheet}”, which this game does not have.`)
        else if (size && size.width > 0) count = sheetTiles(sheet, size.width, size.height).count
      }
      const rows: unknown[] = Array.isArray(layer.cells) ? layer.cells : []
      const w = map.grid.width, h = map.grid.height
      if (!Array.isArray(layer.cells)) say('The tile layer has no list of rows. The game will not load until it is fixed.')
      else if (rows.length !== h) say(`The tile layer has ${rows.length} rows but the map is ${h} squares high. The game will not load until they match.`)
      let reported = 0
      rows.slice(0, h).forEach((row, y) => {
        if (reported >= 3) return
        if (typeof row === 'string') {
          if (row.length !== w * 4) { say(`Row ${y + 1} of the tile layer has ${row.length} characters but ${w * 4} are needed (${w} squares of four hex digits, or “....” for none).`); reported++; return }
          for (let x = 0; x < w && reported < 3; x++) {
            const tok = row.slice(x * 4, x * 4 + 4)
            if (tok === '....') continue
            if (!/^[0-9a-fA-F]{4}$/.test(tok)) { say(`Square ${x + 1}, ${y + 1} of the tile layer is “${tok}”: it should be four hex digits or “....”.`); reported++ }
            else if (count > 0 && parseInt(tok, 16) >= count) { say(`Square ${x + 1}, ${y + 1} uses tile ${parseInt(tok, 16)}, but the tileset “${layer.tileset}” has only ${count} tiles (0 to ${count - 1}).`); reported++ }
          }
        } else if (Array.isArray(row)) {
          if (row.length !== w) { say(`Row ${y + 1} of the tile layer has ${row.length} squares but the map is ${w} wide.`); reported++ }
        } else { say(`Row ${y + 1} of the tile layer is not a row of tiles.`); reported++ }
      })
    }
  }
  // Tiles carried by the changes and by setTerrain commands.
  const noLayer = !layer
  const checkTile = (v: unknown, at: string) => {
    if (v === undefined) return
    if (noLayer) { say(`${at}: a tile needs a tile layer, and this map has none. The game will not load until the tile is taken out or a tile layer added.`); return }
    if (v === null) return
    if (typeof v !== 'number' || !Number.isInteger(v) || v < -1 || (count > 0 && v >= count)) say(`${at}: the tile must be -1 (none) or a tile number${count > 0 ? ` from 0 to ${count - 1}` : ''}.`)
  }
  for (const [id, ch] of Object.entries(changesOf(map))) {
    cellsOf(ch).forEach((k, i) => {
      checkTile(k.tile, `The change “${id}”, square ${i + 1}`)
      if (k.keepTile !== undefined && typeof k.keepTile !== 'boolean') say(`The change “${id}”, square ${i + 1}: “keep the tile” must be on or off.`)
    })
    if (ch.keepTile !== undefined && typeof ch.keepTile !== 'boolean') say(`The change “${id}”: “keep the tiles” must be on or off.`)
  }
  forEachMapCommand(map, (c, where) => {
    if (typeof c.setTerrain !== 'string') return
    checkTile(c.tile, where)
    if (c.keepTile !== undefined && typeof c.keepTile !== 'boolean') say(`${where}: “keep the tile” must be on or off.`)
  })
  return out
}

function forEachMapCommand(map: MapFile, fn: (c: import('./storyTypes').Command, where: string) => void): void {
  for (const e of map.events ?? []) forEachCommand(e.do, `Event “${e.id}”`, fn)
}
