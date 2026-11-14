// Writes an old-format project (before structures) into a game folder, for the Convert tests and screenshots.
import type { APIRequestContext } from '@playwright/test'
import { rmSync } from 'node:fs'
import { OLD_MAP, OLD_TERRAIN } from '../unit/oldProject'
import { seedArt, sheetPng, tilesGraphics } from './artkit'
import { diskPath, readDisk, writeDisk } from './fixtures'

/** Turns a fresh economy-tactics game into an old-format one: buildings are terrain, owners are a list on the map, a night style recolours
 *  two of the buildings, the city and the headquarters have pictures on the terrain, and there is no data/structures.json. */
export async function seedOldGame(request: APIRequestContext, id: string, opts: { art?: boolean } = {}): Promise<void> {
  const art = opts.art !== false // `art: false` = no pictures at all (the screenshots look natural that way)
  await seedArt(request, id, art ? { tiles: sheetPng(6, 6) } : {}, art ? tilesGraphics({
    terrain: { city: { sheet: 'tiles', cell: [3, 1] }, hq: { sheet: 'tiles', frames: [[0, 2], [1, 2]], frameMs: 300 } },
    styles: { night: { terrain: { city: { sheet: 'tiles', cell: [4, 1] } } } },
  }) : { sheets: [] })
  const game = readDisk(id, 'game.json')
  game.styles = [{ id: 'classic', name: 'Classic' }, { id: 'night', name: 'Night', background: '#05060f', terrainColors: { plain: '#2f4f3a', city: '#6a6a78', hq: '#8a7a30' } }]
  writeDisk(id, 'game.json', JSON.stringify(game, null, 2))
  writeDisk(id, 'data/terrain.json', JSON.stringify({ terrain: OLD_TERRAIN }, null, 2))
  writeDisk(id, 'maps/crossroads.json', JSON.stringify(OLD_MAP, null, 2))
  rmSync(diskPath(id, 'data/structures.json'))
}
