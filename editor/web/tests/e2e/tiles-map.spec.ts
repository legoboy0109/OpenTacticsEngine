// The map editor's Tiles tool (docs/EDITOR_UI.md "Tiles tool", docs/GRAPHICS.md "Tile layer"): choose tiles in the palette, paint / erase / fill a
// rectangle / stamp a block / pick with the eyedropper, the "show terrain colours" aid, one undo step per stroke, tiles recorded into a map
// change, and the whole thing at desktop width and at 360 px (the palette scrolls inside its box; the page never does).
import type { Page } from '@playwright/test'
import { hex, pieceColor, seedArt, sheetPng, tilesGraphics, expectPixel, mapPixel } from './artkit'
import { expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { clickCell, dragThrough, openMap, cellPoint, canvas } from './mapkit'

const MAP = 'ford'
const tilesTool = (page: Page) => page.getByRole('toolbar', { name: 'Map tools' }).getByRole('button', { name: /^Tiles/ })
const palette = (page: Page) => page.getByTestId('tile-palette-canvas')
const mode = (page: Page, m: 'paint' | 'erase' | 'rect' | 'pick') => page.getByTestId(`tile-mode-${m}`)
const tileColor = (n: number) => hex(pieceColor(n % 8, Math.floor(n / 8)))
/** The square's four-hex-digit text in the saved layer ("...." = none). */
const diskTile = (id: string, x: number, y: number): string => readDisk(id, `maps/${MAP}.json`).tileLayer.cells[y].slice(x * 4, x * 4 + 4)

/** Presses on the palette's tile (col, row) (and drags to (col2, row2) for a block), like a person with a mouse. On a phone the map stays at the
 *  top of the page while the page scrolls, so the pointer is first brought to the tile by Playwright (which scrolls it clear of that). */
async function choose(page: Page, col: number, row: number, col2 = col, row2 = row): Promise<void> {
  const cv = palette(page)
  const cell = Number(await cv.getAttribute('data-cell')), cellh = Number(await cv.getAttribute('data-cellh'))
  const at = (c: number, r: number) => ({ x: (c + 0.5) * cell, y: (r + 0.5) * cellh })
  await cv.hover({ position: at(col, row) })
  await page.mouse.down()
  if (col2 !== col || row2 !== row) {
    const box = (await cv.boundingBox())!
    await page.mouse.move(box.x + at(col2, row2).x, box.y + at(col2, row2).y, { steps: 4 })
  }
  await page.mouse.up()
}

async function setUp(page: Page, request: import('@playwright/test').APIRequestContext, game: { create(t: string, n?: string): Promise<string> }, name: string) {
  const id = await game.create('strategy-rpg', name)
  await seedArt(request, id, { tiles: sheetPng(8, 4, 16) }, tilesGraphics({ tilesets: { world: { sheet: 'tiles' } } }, 16))
  await openMap(page, id, MAP)
  await tilesTool(page).click()
  await expect(page.getByTestId('tiles-tool')).toBeVisible()
  await expect(palette(page)).toBeVisible()
  return id
}

for (const [label, size] of [['desktop', { width: 1280, height: 800 }], ['360 px', { width: 360, height: 740 }]] as const) {
  test(`tiles: choose, paint, drag, erase, rectangle, stamp, eyedropper, terrain colours, undo (${label})`, async ({ page, request, game }) => {
    await page.setViewportSize(size)
    const id = await setUp(page, request, game, `Tiles ${label}`)
    await expectNoHScroll(page)
    // The palette: 8 x 4 tiles, shown big enough for a finger, scrolling inside its box.
    expect(Number(await palette(page).getAttribute('data-cols'))).toBe(8)
    expect(Number(await palette(page).getAttribute('data-cell'))).toBeGreaterThanOrEqual(24)
    await expect(page.getByTestId('tile-chosen')).toHaveText('No tile chosen.')
    // Painting without a chosen tile says so instead of doing something.
    await clickCell(page, 2, 2)
    await expect(page.getByTestId('map-notice')).toContainText('Pick a tile in the palette first')

    // Choose tile 10 (column 2, row 1), then click a square: the layer is created with that one tile.
    await choose(page, 2, 1)
    await expect(page.getByTestId('tile-chosen')).toContainText('Tile 10')
    await expect(palette(page)).toHaveAttribute('data-sel', '2,1,1,1')
    await clickCell(page, 2, 2)
    await saved(page)
    const layer = readDisk(id, `maps/${MAP}.json`).tileLayer
    expect(layer.tileset).toBe('world')
    expect(diskTile(id, 2, 2)).toBe('000a')
    expect(layer.cells.flatMap((r: string) => r.match(/.{4}/g)!).filter((t: string) => t !== '....')).toEqual(['000a'])
    await page.mouse.move(1, 1)  // (the pointer's own highlight is not part of the picture)
    await expectPixel(page, 2, 2, 0.5, 0.5, tileColor(10), 'the square shows the tile')
    expect(await game.problems(id)).toEqual([])

    // One drag = one undo step.
    await dragThrough(page, [[3, 4], [4, 4], [5, 4]])
    await saved(page)
    expect([3, 4, 5].map((x) => diskTile(id, x, 4))).toEqual(['000a', '000a', '000a'])
    await page.getByRole('button', { name: 'Undo' }).click()
    await saved(page)
    expect([3, 4, 5].map((x) => diskTile(id, x, 4))).toEqual(['....', '....', '....'])
    expect(diskTile(id, 2, 2)).toBe('000a')
    await page.getByRole('button', { name: 'Redo' }).click()
    await saved(page)

    // Terrain colours over the tiles (the rules layer).
    const plain = await mapPixel(page, 3, 4)
    await page.getByRole('switch', { name: 'Show terrain colours' }).check()
    await expect.poll(async () => hex(await mapPixel(page, 3, 4)), { intervals: [50, 100, 200] }).not.toBe(hex(plain))
    await page.getByRole('switch', { name: 'Show terrain colours' }).uncheck()
    await expectPixel(page, 3, 4, 0.5, 0.5, tileColor(10))

    // Rectangle: a new tile (col 0, row 3 = tile 24) fills the dragged rectangle.
    await mode(page, 'rect').click()
    await choose(page, 0, 3)
    await dragThrough(page, [[0, 0], [2, 1]])
    await saved(page)
    for (const [x, y] of [[0, 0], [1, 0], [2, 0], [0, 1], [1, 1], [2, 1]]) expect(diskTile(id, x, y), `${x},${y}`).toBe('0018')
    expect(diskTile(id, 3, 0)).toBe('....')

    // A block chosen by dragging in the palette is stamped as a whole by one click (paint mode).
    await mode(page, 'paint').click()
    await choose(page, 0, 0, 1, 1)
    await expect(page.getByTestId('tile-chosen')).toContainText('block of 2 × 2')
    await clickCell(page, 6, 1)
    await saved(page)
    expect([[6, 1], [7, 1], [6, 2], [7, 2]].map(([x, y]) => diskTile(id, x, y))).toEqual(['0000', '0001', '0008', '0009'])

    // Erase takes a tile away (the square shows its terrain again); Alt-click picks the tile under the pointer.
    await mode(page, 'erase').click()
    await clickCell(page, 2, 2)
    await saved(page)
    expect(diskTile(id, 2, 2)).toBe('....')
    await mode(page, 'paint').click()
    await clickCell(page, 7, 2, { modifiers: ['Alt'] })
    await expect(page.getByTestId('tile-chosen')).toContainText('Tile 9')
    await expect(palette(page)).toHaveAttribute('data-sel', '1,1,1,1')
    // The pick mode does the same with a plain click, and then goes back to painting.
    await mode(page, 'pick').click()
    await clickCell(page, 0, 0)
    await expect(page.getByTestId('tile-chosen')).toContainText('Tile 24')
    await expect(mode(page, 'paint')).toHaveAttribute('aria-checked', 'true')

    // The layer count and the whole game still loads.
    await expect(page.getByTestId('tile-layer-count')).toBeVisible()
    expect(await game.problems(id)).toEqual([])
    await expectNoHScroll(page)
  })
}

test('tiles: the zoom of the palette, and the tool says what is missing', async ({ page, request, game }) => {
  const id = await setUp(page, request, game, 'Tiles zoom')
  const before = Number(await palette(page).getAttribute('data-cell'))
  await page.getByRole('button', { name: 'Zoom the palette in' }).click()
  expect(Number(await palette(page).getAttribute('data-cell'))).toBeGreaterThan(before)
  await page.getByRole('button', { name: 'Zoom the palette out' }).click()
  expect(Number(await palette(page).getAttribute('data-cell'))).toBe(before)
  // A game with no tileset: the tool explains where to make one.
  const bare = await game.create('strategy-rpg', 'Tiles none')
  await seedArt(request, bare, { tiles: sheetPng(8, 4, 16) }, tilesGraphics({}, 16))
  await openMap(page, bare, MAP)
  await tilesTool(page).click()
  await expect(page.getByTestId('tiles-tool')).toContainText('no tileset yet')
  expect(id).not.toBe(bare)
})

test('tiles: a change carries tiles (paint into it, record, forget) and the map is not touched', async ({ page, request, game }) => {
  const id = await setUp(page, request, game, 'Tiles change')
  // Paint one tile into the map first (the layer starts), then open the Changes tab.
  await choose(page, 2, 1)
  await clickCell(page, 1, 1)
  await saved(page)
  await page.getByRole('tab', { name: 'Changes', exact: true }).click()
  await page.getByTestId('change-add').click()
  const panel = page.getByTestId('change-tiles')
  await expect(panel).toBeVisible()
  await panel.getByRole('switch', { name: 'Paint tiles into this change' }).check()
  await choose(page, 3, 0)
  await clickCell(page, 4, 3)
  await saved(page)
  const file = () => readDisk(id, `maps/${MAP}.json`)
  const change = Object.values(file().changes)[0] as { cells: Array<{ x: number; y: number; terrain: string; tile?: number }> }
  expect(change.cells).toEqual([expect.objectContaining({ x: 4, y: 3, tile: 3 })])
  expect(diskTile(id, 4, 3)).toBe('....')  // the map itself is untouched
  await expect(page.getByTestId('change-tile-count')).toContainText('1 square has a tile of its own')
  // "Also record the tiles": every square of the change gets the tile the map has now (here: none = -1).
  await panel.getByTestId('change-record-tiles').click()
  await saved(page)
  expect((Object.values(file().changes)[0] as typeof change).cells[0].tile).toBe(-1)
  // Forget them: the squares then clear their tile when the terrain changes, as the game does.
  await panel.getByTestId('change-forget-tiles').click()
  await saved(page)
  expect(Object.values(file().changes)[0]).toEqual({ cells: [expect.not.objectContaining({ tile: expect.anything() })] })
  await panel.getByRole('switch', { name: 'Keep the tiles' }).check()
  await saved(page)
  expect((Object.values(file().changes)[0] as { keepTile?: boolean }).keepTile).toBe(true)
  expect(await game.problems(id)).toEqual([])
  void canvas
  void cellPoint
})
