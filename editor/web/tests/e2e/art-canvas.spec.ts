// The map canvas draws art by the rules in docs/GRAPHICS.md ("Drawing rules"): pixel checks on the real canvas.
import type { APIRequestContext } from '@playwright/test'
import { cellHash } from '../../src/model/pickCell'
import { canvasPixel, expectPixel, hex, makePng, mapPixel, near, pieceColor, seedArt, sheetPng, tilesGraphics, type Rgba } from './artkit'
import { expect, test } from './fixtures'
import { canvas, diskGrid, openMap, pickTerrain, pickUnitType, tool } from './mapkit'

// The first map of the strategy-rpg template ("ford", 10 x 8): forest at (2,0) (1,1) (7,2); mountain at (8,0) (9,0) (9,1);
// blue lord (1,3), blue cavalier (1,4), red archer (8,3), red soldier (8,4).
const MAP = 'ford'
const rgb = (c: string): Rgba => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16), 255]

async function colors(request: APIRequestContext, id: string): Promise<Record<string, Rgba>> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  return Object.fromEntries(doc.terrain.map((t: any) => [t.id, rgb(t.color)]))
}

test('terrain with a picture: the piece fills the square, the letter and outline are gone, other terrain keeps its look @smoke', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas One')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({ terrain: { forest: { sheet: 'tiles', cell: [2, 1] } } }))
  const col = await colors(request, id)
  await openMap(page, id, MAP)
  const want = hex(pieceColor(2, 1))
  for (const [fx, fy] of [[0.5, 0.5], [0.1, 0.1], [0.9, 0.9], [0.5, 0.1], [0.05, 0.5]]) {
    await expectPixel(page, 2, 0, fx, fy, want, `forest at ${fx},${fy}`)
  }
  await expectPixel(page, 1, 1, 0.5, 0.5, want)
  // A square without a picture next to it is unchanged (colour; the letter sits in the middle, so look near the corner).
  await expectPixel(page, 0, 0, 0.2, 0.2, hex(col.plain))
})

test('a random mix shows, on every square, the piece the engine would pick (same hash)', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Mix')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({ terrain: { plain: { sheet: 'tiles', variants: [[0, 0], [1, 0], [2, 0]] } } }))
  await openMap(page, id, MAP)
  const grid = diskGrid(id, MAP)
  let seen = new Set<string>()
  for (let y = 0; y < 8; y++) for (let x = 0; x < 10; x++) {
    if (grid[y][x] !== 'plain') continue
    const want = hex(pieceColor(cellHash(x, y) % 3, 0))
    seen.add(want)
    // Away from the unit blocks: only squares without a unit.
    if ([[1, 3], [1, 4], [8, 3], [8, 4]].some(([ux, uy]) => ux === x && uy === y)) continue
    await expectPixel(page, x, y, 0.5, 0.5, want, `(${x}, ${y})`)
  }
  expect(seen.size).toBe(3) // all three variants really occur on this map
})

test('animation: frames alternate on the canvas, and stay on the first frame when the system asks for less motion', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Anim')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({ terrain: { forest: { sheet: 'tiles', frames: [[0, 0], [1, 0]], frameMs: 100 } } }))
  await openMap(page, id, MAP)
  // The image loads a moment after the map opens: wait until a frame (either one) is on the canvas before sampling.
  const frameColors = [hex(pieceColor(0, 0)), hex(pieceColor(1, 0))]
  await expect.poll(async () => frameColors.includes(hex(await mapPixel(page, 2, 0)))).toBe(true)
  const seen = new Set<string>()
  // Sample often, at a rhythm that does not line up with the 100 ms frames (the default poll is too sparse to be sure).
  for (let i = 0; i < 100 && seen.size < 2; i++) { seen.add(hex(await mapPixel(page, 2, 0))); await page.waitForTimeout(37) }
  expect(seen.size).toBe(2)
  expect([...seen].sort()).toEqual([hex(pieceColor(0, 0)), hex(pieceColor(1, 0))].sort())

  await page.emulateMedia({ reducedMotion: 'reduce' })
  await openMap(page, id, MAP)
  // The image loads a moment after the map opens: wait for the first frame before watching that it stays.
  await expectPixel(page, 2, 0, 0.5, 0.5, hex(pieceColor(0, 0)))
  const still = new Set<string>()
  for (let i = 0; i < 8; i++) { still.add(hex(await mapPixel(page, 2, 0))); await page.waitForTimeout(120) }
  expect([...still]).toEqual([hex(pieceColor(0, 0))])
})

test('joining pictures choose the piece by neighbours: N1 E2 S4 W8, the map edge, and "also joins with"', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Join')
  const join = (extra: object) => ({ terrain: { mountain: { sheet: 'tiles', join: [0, 0], ...extra } } })
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics(join({ joinEdge: false })))
  const piece = (n: number) => hex(pieceColor(n % 4, Math.floor(n / 4)))
  await openMap(page, id, MAP)
  // Mountains at (8,0) (9,0) (9,1): (8,0) has only an east neighbour, (9,0) west and south, (9,1) only north.
  await expectPixel(page, 8, 0, 0.5, 0.5, piece(2))
  await expectPixel(page, 9, 0, 0.5, 0.5, piece(8 | 4))
  await expectPixel(page, 9, 1, 0.5, 0.5, piece(1))
  // With the map edge counting, (8,0) also joins up; (9,0) joins up and right too; (9,1) joins up (a mountain) and right (the edge).
  await request.put(`/api/projects/${id}`, { data: { baseRevision: (await (await request.get(`/api/projects/${id}`)).json()).revision, graphics: { ...tilesGraphics(join({ joinEdge: true })) } } })
  await openMap(page, id, MAP)
  await expectPixel(page, 8, 0, 0.5, 0.5, piece(1 | 2))
  await expectPixel(page, 9, 0, 0.5, 0.5, piece(1 | 2 | 4 | 8))
  await expectPixel(page, 9, 1, 0.5, 0.5, piece(1 | 2))
  // Plain counts as the same terrain: (8,0) now also has plain to the west and south.
  await request.put(`/api/projects/${id}`, { data: { baseRevision: (await (await request.get(`/api/projects/${id}`)).json()).revision, graphics: { ...tilesGraphics(join({ joinEdge: false, joinWith: ['plain'] })) } } })
  await openMap(page, id, MAP)
  await expectPixel(page, 8, 0, 0.5, 0.5, piece(2 | 4 | 8))
})

test('transparent pixels show the terrain colour; pieces are stretched without smoothing', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Alpha')
  const A: Rgba = [250, 0, 0, 255], B: Rgba = [0, 250, 0, 255], C: Rgba = [0, 0, 250, 255]
  // One 2 x 2 piece: top-left transparent, then A, B, C.
  const img = makePng(2, 2, (x, y) => (x === 0 && y === 0 ? [0, 0, 0, 0] : x === 1 && y === 0 ? A : x === 0 ? B : C))
  await seedArt(request, id, { tiles: img }, { sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 2, cellHeight: 2 }], terrain: { forest: { sheet: 'tiles', cell: [0, 0] } } })
  const col = await colors(request, id)
  await openMap(page, id, MAP)
  const [scale] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
  expect(scale).toBeGreaterThan(20)
  await expectPixel(page, 2, 0, 0.25, 0.25, hex(col.forest)) // the hole shows the terrain colour
  await expectPixel(page, 2, 0, 0.75, 0.25, hex(A))
  await expectPixel(page, 2, 0, 0.25, 0.75, hex(B))
  await expectPixel(page, 2, 0, 0.75, 0.75, hex(C))
  // Right at the edge between pieces' pixels the colour is one of the two, never a blend.
  const [, vx, vy] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
  const mid = Math.round(vx + 2 * scale + scale / 2)
  for (const dx of [-1, 0, 1]) {
    const p = await canvasPixel(canvas(page), mid + dx, Math.round(vy + scale * 0.25))
    expect([hex(col.forest), hex(A)], `x offset ${dx}`).toContain(hex(p))
  }
})

test('units: a sprite on a coloured base, tall sprites reach above their square, lower units are drawn over higher ones', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Units')
  // Piece 0 (24 x 48): solid orange, the bottom 8 pixel rows transparent so the base shows. Piece 1: solid green.
  const ORANGE: Rgba = [240, 130, 20, 255], GREEN: Rgba = [20, 200, 90, 255]
  const tall = makePng(48, 48, (x, y) => (x < 24 ? (y < 40 ? ORANGE : [0, 0, 0, 0]) : GREEN))
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps[MAP].units = [...doc.maps[MAP].units].reverse() // the lower units come first in the file
  await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })
  await seedArt(request, id, { men: tall }, {
    sheets: [{ id: 'men', file: 'art/men.png', cellWidth: 24, cellHeight: 48 }],
    units: { lord: { sheet: 'men', cell: [0, 0] }, cavalier: { sheet: 'men', cell: [1, 0] }, archer: { sheet: 'men', cell: [0, 0] } },
  })
  await openMap(page, id, MAP)
  // Blue lord at (1,3): twice as tall as its square, so its head is in the square above (1,2).
  await expectPixel(page, 1, 2, 0.5, 0.5, hex(ORANGE))
  // The blue cavalier at (1,4) is taller than the square too and reaches into the lord's square: lower units are drawn over
  // higher ones (the file lists the cavalier first, so this is the sort by row, not the file order).
  await expectPixel(page, 1, 3, 0.5, 0.5, hex(GREEN))
  // Red archer at (8,3): a flat base in the side colour (red #cc0000) shows through the transparent feet.
  await expectPixel(page, 8, 2, 0.5, 0.5, hex(ORANGE))
  expect(near(await mapPixel(page, 8, 3, 0.5, 21.5 / 24), rgb('#cc0000'), 2)).toBe(true)
  // The base is inset: its outline is dark and the corner of the square next to it is plain terrain.
  expect(near(await mapPixel(page, 8, 3, 0.5, 23.7 / 24), rgb('#cc0000'), 30)).toBe(false)
})

test('the coloured base can be switched off in the Art section', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Marker')
  const ORANGE: Rgba = [240, 130, 20, 255]
  const img = makePng(24, 24, (_x, y) => (y < 18 ? ORANGE : [0, 0, 0, 0]))
  await seedArt(request, id, { men: img }, { sheets: [{ id: 'men', file: 'art/men.png', cellWidth: 24, cellHeight: 24 }], units: { lord: { sheet: 'men', cell: [0, 0] } } })
  const col = await colors(request, id)
  await openMap(page, id, MAP)
  await expectPixel(page, 1, 3, 0.5, 0.3, hex(ORANGE)) // wait for the picture to load
  expect(near(await mapPixel(page, 1, 3, 0.5, 21.5 / 24), rgb('#3d85c6'), 2)).toBe(true)
  await page.getByRole('link', { name: 'Art', exact: true }).click()
  await page.getByRole('switch', { name: /coloured base under units/ }).uncheck()
  await page.getByRole('link', { name: 'Maps', exact: true }).click()
  await expect(canvas(page)).toHaveAttribute('data-view', /.+/)
  await expectPixel(page, 1, 3, 0.5, 21.5 / 24, hex(col.plain))
})

test('palettes show pictures too: terrain and unit buttons draw the art, others keep their chip', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Palette')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({ terrain: { forest: { sheet: 'tiles', cell: [2, 1] } }, units: { lord: { sheet: 'tiles', cell: [1, 1] } } }))
  await openMap(page, id, MAP)
  await pickTerrain(page, 'Forest').locator('canvas').waitFor()
  await expect(pickTerrain(page, 'Plain').locator('canvas')).toHaveCount(0)
  const px = await pickTerrain(page, 'Forest').locator('canvas').evaluate((el) => { const c = el as HTMLCanvasElement; const d = c.getContext('2d')!.getImageData(c.width / 2, c.height / 2, 1, 1).data; return [d[0], d[1], d[2]] })
  expect(hex(px)).toBe(hex(pieceColor(2, 1)))
  await tool(page, 'Units').click()
  await expect(pickUnitType(page, 'Lord').locator('canvas')).toHaveCount(1)
  await expect(pickUnitType(page, 'Archer').locator('canvas')).toHaveCount(0)
})

test('the Look selector previews a style: its pictures, terrain colours and background', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Canvas Looks')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({
    terrain: { forest: { sheet: 'tiles', cell: [0, 0] } },
    styles: { night: { terrain: { forest: { sheet: 'tiles', cell: [3, 3] }, water: null } } },
  }))
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.game.styles = [{ id: 'day', name: 'Day' }, { id: 'night', name: 'Night', background: '#112233', terrainColors: { plain: '#ff00ff' } }]
  await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game } })
  const col = await colors(request, id)
  await openMap(page, id, MAP)
  const look = page.getByLabel('Style to preview')
  await expect(look).toHaveValue('day') // the player starts with the first look
  await expectPixel(page, 2, 0, 0.5, 0.5, hex(pieceColor(0, 0)))
  await expectPixel(page, 0, 0, 0.2, 0.2, hex(col.plain))
  await look.selectOption('night')
  await expectPixel(page, 2, 0, 0.5, 0.5, hex(pieceColor(3, 3))) // its own picture
  await expectPixel(page, 0, 0, 0.2, 0.2, '#ff00ff') // its own colour
  await expectPixel(page, 5, 0, 0.1, 0.1, hex(col.water)) // "no picture in this style" keeps the coloured square
  const box = (await canvas(page).boundingBox())!
  expect(hex(await canvasPixel(canvas(page), 2, Math.floor(box.height) - 3))).toBe('#112233') // background around the map
  // The choice is remembered.
  await page.reload()
  await expect(page.getByLabel('Style to preview')).toHaveValue('night')
})
