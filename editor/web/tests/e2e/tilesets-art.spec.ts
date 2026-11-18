// The Tilesets card of the Art screen (docs/GRAPHICS.md "Tile layer"): add, pick a tile, mark it animated, rename, delete with the maps that
// use it listed; at desktop width and at 360 px.
import { clickPiece, seedArt, sheetPng } from './artkit'
import { expect, expectNoHScroll, gotoSection, readDisk, saved, test } from './fixtures'

const graphics = (extra: object = {}) => ({
  sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 16, cellHeight: 16, name: 'Tiles' }],
  tilesets: { world: { sheet: 'tiles' } }, ...extra,
})

/** Gives the first map a tile layer that uses the tileset "world" (like a hand-painted map would have). */
async function tileFirstMap(request: import('@playwright/test').APIRequestContext, id: string): Promise<string> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const mid = doc.game.maps[0].id
  const m = doc.maps[mid]
  m.tileLayer = { tileset: 'world', cells: Array.from({ length: m.grid.height }, () => '0001'.repeat(m.grid.width)) }
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: { [mid]: m } } })
  expect(res.ok(), await res.text()).toBeTruthy()
  return doc.game.maps[0].name
}

for (const [label, size] of [['desktop', { width: 1280, height: 800 }], ['360 px', { width: 360, height: 740 }]] as const) {
  test(`tilesets: add, select a tile, animate, rename, delete (${label})`, async ({ page, request, game }) => {
    await page.setViewportSize(size)
    const id = await game.create('strategy-rpg', 'Tilesets')
    await seedArt(request, id, { tiles: sheetPng(8, 4, 16) }, graphics())
    const mapName = await tileFirstMap(request, id)
    await page.goto(`/p/${id}/art`)
    await gotoSection(page, 'Art')
    const card = page.getByTestId('tilesets')
    await card.scrollIntoViewIfNeeded()
    await expect(card.getByTestId('tileset-world')).toBeVisible()
    await expect(card).toContainText('32 tiles (8 across, 4 down)')

    // Add a second tileset, select tile 9 (column 1, row 1) on its grid.
    await card.getByPlaceholder('Name of a new tileset').fill('water')
    await card.getByRole('button', { name: 'Add tileset' }).click()
    const water = card.getByTestId('tileset-water')
    await expect(water).toBeVisible()
    await saved(page)
    expect(readDisk(id, 'data/graphics.json').tilesets.water).toEqual({ sheet: 'tiles' })
    await clickPiece(page, water.getByTestId('piece-grid'), 1, 1)
    await expect(water.getByTestId('tileset-water-selected')).toHaveText('Tile 9')
    // The grid is usable at this width: it scrolls inside its box, the page does not.
    await expectNoHScroll(page)
    const box = await water.getByTestId('piece-grid').boundingBox()
    expect(box!.width / 8).toBeGreaterThanOrEqual(16)

    // Mark it animated, give it frames and a time.
    await water.getByRole('switch', { name: 'Animated tile' }).check()
    await saved(page)
    expect(readDisk(id, 'data/graphics.json').tilesets.water.animations).toEqual({ 9: { frames: [9, 10] } })
    await water.getByTestId('tile-frames').fill('9, 10, 11')
    await water.getByTestId('tile-ms').fill('350')
    await saved(page)
    expect(readDisk(id, 'data/graphics.json').tilesets.water.animations).toEqual({ 9: { frames: [9, 10, 11], ms: 350 } })
    // Bad input shows why and does not reach the file.
    await water.getByTestId('tile-frames').fill('9, 99')
    await expect(water.getByRole('alert')).toContainText('whole tile numbers from 0 to 31')
    await water.getByTestId('tile-frames').fill('9, 10, 11')
    await expect(water).toContainText('Animated tiles: 9')
    // Choosing another tile by number works too.
    await water.getByTestId('tileset-water-tile-input').fill('12')
    await expect(water.getByTestId('tileset-water-selected')).toHaveText('Tile 12')

    // Rename.
    await water.getByRole('textbox', { name: 'Name of tileset 2' }).fill('lava')
    await saved(page)
    expect(Object.keys(readDisk(id, 'data/graphics.json').tilesets)).toEqual(['world', 'lava'])

    // Deleting a tileset a map uses asks first and lists the map; confirming removes the layer too.
    await card.getByRole('button', { name: 'Delete tileset world' }).click()
    const dlg = page.getByRole('dialog', { name: /Delete the tileset/ })
    await expect(dlg).toContainText(mapName)
    await dlg.getByRole('button', { name: 'Delete tileset' }).click()
    await saved(page)
    expect(Object.keys(readDisk(id, 'data/graphics.json').tilesets)).toEqual(['lava'])
    const mid = (await (await request.get(`/api/projects/${id}`)).json()).game.maps[0].id
    expect(readDisk(id, `maps/${mid}.json`).tileLayer).toBeUndefined()
    // One undo step brings both back.
    await page.getByRole('button', { name: 'Undo' }).click()
    await saved(page)
    expect(Object.keys(readDisk(id, 'data/graphics.json').tilesets)).toEqual(['world', 'lava'])
    expect(readDisk(id, `maps/${mid}.json`).tileLayer.tileset).toBe('world')
    await expectNoHScroll(page)
  })
}
